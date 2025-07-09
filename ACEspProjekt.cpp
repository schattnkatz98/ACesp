#include <windows.h>
#include <tlhelp32.h>
#include <cfloat>
#include <cmath>
#include <iostream>

// Hilfsfunktion: Prozess-ID anhand des Namens ermitteln
DWORD GetProcessID(const wchar_t *processName)
{
    PROCESSENTRY32 pe32{};
    pe32.dwSize = sizeof(pe32);
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
        return 0;
    if (Process32First(hSnap, &pe32))
    {
        do
        {
            if (_wcsicmp(pe32.szExeFile, processName) == 0)
            {
                CloseHandle(hSnap);
                return pe32.th32ProcessID;
            }
        } while (Process32Next(hSnap, &pe32));
    }
    CloseHandle(hSnap);
    return 0;
}

struct Vec3
{
    float x, y, z;
};
struct Vec2
{
    float x, y;
};

// WorldToScreen Funktion wie oben definiert
bool WorldToScreen(const Vec3 &world, Vec2 &screen, float m[16], int width, int height)
{
    float clipX = world.x * m[0] + world.y * m[4] + world.z * m[8] + m[12];
    float clipY = world.x * m[1] + world.y * m[5] + world.z * m[9] + m[13];
    float clipW = world.x * m[3] + world.y * m[7] + world.z * m[11] + m[15];
    if (clipW < 0.001f)
        return false;
    float ndcX = clipX / clipW;
    float ndcY = clipY / clipW;
    screen.x = (width / 2.0f) * (1 + ndcX);
    screen.y = (height / 2.0f) * (1 - ndcY);
    return true;
}

// ==========================================================
//  F U N K T I O N   CreateOverlayWindowSameSize
//      – legt ein rahmenloses, durchsichtiges Overlay
//        exakt über das AssaultCube-Fenster
// ==========================================================
LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, msg, w, l);
    }
    return 0;
}

HWND CreateOverlayWindowSameSize(HWND hGameWnd, int width, int height)
{
    HINSTANCE hInst = GetModuleHandle(nullptr);

    // ░1) Fensterklasse (einmalig) registrieren
    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEX wc{sizeof(wc)};
        wc.lpfnWndProc = OverlayWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"AC_ESP_OVERLAY";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassEx(&wc);
        classRegistered = true;
    }

    // ░2) Position des Spiel-Fensters in Bildschirm-Koordinaten
    RECT rc{};
    POINT pt{0, 0};
    ClientToScreen(hGameWnd, &pt);


   
    /*GetWindowRect(hGameWnd, &rc);*/ // absolute Koordinaten

    // ░3) Overlay erzeugen  (klick-durchlässig + transparent)
    HWND hOvl =
        CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT, L"AC_ESP_OVERLAY", L"", // Klasse / Titel
                       WS_POPUP,                                                                  // rahmenlos
                       pt.x, pt.y, width, height, nullptr, nullptr, hInst, nullptr);        



    if (!hOvl)
        return nullptr;

    // ░4) Schwarz als „Color-Key“ = komplett durchsichtig
    SetLayeredWindowAttributes(hOvl, RGB(0, 0, 0), 0, LWA_COLORKEY);

    ShowWindow(hOvl, SW_SHOW);
    UpdateWindow(hOvl);
    return hOvl;
}


constexpr uintptr_t LOCAL_PLAYER_PTR = 0x58AC00;
constexpr uintptr_t ENTITY_LIST_PTR = 0x58AC04;
constexpr uintptr_t ENTITY_COUNT_PTR = 0x58AC0C;
constexpr uintptr_t OFF_HEAD_POS = 0x04; // Vec3 Kopfposition (x)
constexpr uintptr_t OFF_FEET_POS = 0x28; // Vec3 Fußposition (x)
constexpr uintptr_t OFF_YAW = 0x34;
constexpr uintptr_t OFF_PITCH = 0x38;
constexpr uintptr_t OFF_HEALTH = 0xEC;
constexpr uintptr_t OFF_TEAM = 0x30C;
constexpr uintptr_t OFF_NAME = 0x205;
constexpr uintptr_t VIEW_MATRIX_PTR = 0x57DFD0;


int main()
{
    // 1) Prozess finden und öffnen (wie im Aimbot):
    DWORD pid = GetProcessID(L"ac_client.exe"); // Pass the process name as a wide string
    if (!pid)
    {
        std::cerr << "AssaultCube Prozess nicht gefunden\n";
        return 1;
    }
    HANDLE hProc = OpenProcess(PROCESS_VM_READ, FALSE, pid);
    if (!hProc)
    {
        std::cerr << "OpenProcess fehlgeschlagen\n";
        return 1;
    }

    // 2) Basisadressen lesen:
    uintptr_t localBase = 0, listBase = 0;
    int entityCount = 0;
    ReadProcessMemory(hProc, (LPCVOID)LOCAL_PLAYER_PTR, &localBase, sizeof(localBase), nullptr);
    ReadProcessMemory(hProc, (LPCVOID)ENTITY_LIST_PTR, &listBase, sizeof(listBase), nullptr);
    ReadProcessMemory(hProc, (LPCVOID)ENTITY_COUNT_PTR, &entityCount, sizeof(entityCount), nullptr);
    if (!localBase || !listBase)
    {
        std::cerr << "Fehler: Konnte Basisadressen nicht lesen\n";
        return 1;
    }

    // 3) Window/Overlay Setup (vereinfacht):
    HWND hGame = FindWindow(NULL, L"AssaultCube");
    RECT gameRect;
    GetClientRect(hGame, &gameRect);
    /*POINT = pt = {0,0};
    ClientToScreen(hGame, &pt);*/

    int width = gameRect.right;
    int height = gameRect.bottom;
    // Hier: Create overlay window etc. (Aus Platzgründen nicht komplett gezeigt)
    HWND hOverlay = CreateOverlayWindowSameSize(hGame, width, height);
    

    HDC hdc = GetDC(hOverlay);
    // (Einen geeigneten Stift/Brush erstellen:)
    HPEN lineColor = CreatePen(PS_SOLID, 1, RGB(0, 184, 150));
    HPEN boxColor = CreatePen(PS_SOLID, 1, RGB(204, 4, 81));
    HPEN helathBarColor = CreatePen(PS_SOLID, 1, RGB(0, 255, 8));

    HBRUSH greenBrush = CreateSolidBrush(RGB(0, 255, 0));
    HBRUSH redBrush = CreateSolidBrush(RGB(255, 0, 0));

    // 4) Hauptschleife: ständiges Lesen und Zeichnen
    while (true)
    {
        static int oX = 0, oY = 0;          // merken alte Overlay-Pos
        static int oW = width, oH = height; // merken alte Größe

        RECT rc;
        POINT pt{0, 0};

        GetClientRect(hGame, &rc);  // neue Client-Größe
        ClientToScreen(hGame, &pt); // neue obere linke Ecke

        int newW = rc.right - rc.left;
        int newH = rc.bottom - rc.top;

        if (newW != oW || newH != oH || pt.x != oX || pt.y != oY)
        {
            oW = newW;
            oH = newH;
            oX = pt.x;
            oY = pt.y;

            width = oW; // ← wichtig für WorldToScreen
            height = oH;

            MoveWindow(hOverlay, oX, oY, oW, oH, FALSE);
        }


        MSG m;
        while (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&m);
            DispatchMessage(&m);
        }

        // a) Hintergrund löschen (schwarz füllen für Transparenz):
        HBRUSH bg = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RECT fullRect = {0, 0, width, height};
        FillRect(hdc, &fullRect, bg);

        // b) Eigene Team-ID lesen:
        int myTeam = 0;
        ReadProcessMemory(hProc, (LPCVOID)(localBase + OFF_TEAM), &myTeam, sizeof(myTeam), nullptr);

        // c) ViewMatrix lesen:
        float matrix[16];
        ReadProcessMemory(hProc, (LPCVOID)VIEW_MATRIX_PTR, &matrix, sizeof(matrix), nullptr);

        // d) Entity-Liste durchgehen:
        for (int i = 0; i < entityCount; ++i)
        {
            uintptr_t entPtr = 0;
            ReadProcessMemory(hProc, (LPCVOID)(listBase + i * sizeof(uintptr_t)), &entPtr, sizeof(entPtr), nullptr);
            if (!entPtr || entPtr == localBase)
                continue;
            // Team/Health check:
            int entTeam = 0, hp = 0;
            ReadProcessMemory(hProc, (LPCVOID)(entPtr + OFF_TEAM), &entTeam, sizeof(entTeam), nullptr);
            ReadProcessMemory(hProc, (LPCVOID)(entPtr + OFF_HEALTH), &hp, sizeof(hp), nullptr);
            if (entTeam == myTeam || hp <= 0)
                continue;

            

            Vec3 posHead, posFeet;
            ReadProcessMemory(hProc, (LPCVOID)(entPtr + OFF_HEAD_POS), &posHead, sizeof(posHead), nullptr);
            ReadProcessMemory(hProc, (LPCVOID)(entPtr + OFF_FEET_POS), &posFeet, sizeof(posFeet), nullptr);
            Vec2 screenHead, screenFeet;
            if (!WorldToScreen(posHead, screenHead, matrix, width, height))
                continue;
            if (!WorldToScreen(posFeet, screenFeet, matrix, width, height))
                continue;

            // Box Dimensionen:
            float boxHeight = screenFeet.y - screenHead.y;
            float boxWidth = boxHeight / 2.0f;
            float x_center = screenHead.x;
            // Runden auf int für Pixel:
            int top = (int)screenHead.y;
            int bottom = (int)screenFeet.y;
            int left = (int)(x_center - boxWidth / 2);
            int right = (int)(x_center + boxWidth / 2);

            // Box zeichnen (weißer Rahmen):
            SelectObject(hdc, boxColor);
            MoveToEx(hdc, left, top, NULL);
            LineTo(hdc, right, top);
            LineTo(hdc, right, bottom);
            LineTo(hdc, left, bottom);
            LineTo(hdc, left, top);

            // Tracer-Linie zeichnen (rote Linie von Bildschirmmitte-unten zum Fußpunkt):
            SelectObject(hdc, lineColor);
            MoveToEx(hdc, width / 2, height, NULL);
            LineTo(hdc, (int)x_center, bottom);

            // Health-Bar zeichnen (grün/rot neben der Box):
            float healthPerc = hp / 100.0f;
            int barHeight = (int)(boxHeight * healthPerc);
            int barLeft = left - 9;
            int barRight = left - 5;
            int barTop = bottom - barHeight;
            // Rahmen der HP-Bar:
            SelectObject(hdc, helathBarColor);
            MoveToEx(hdc, barLeft, top, NULL);
            LineTo(hdc, barLeft, bottom);
            LineTo(hdc, barRight, bottom);
            LineTo(hdc, barRight, top);
            LineTo(hdc, barLeft, top);
            // Gefüllter Anteil:
            HBRUSH barColor = (healthPerc > 0.3f ? greenBrush : redBrush);
            SelectObject(hdc, barColor);
            RECT barFillRect = {barLeft + 1, barTop, barRight, bottom}; // +1 damit es innerhalb Rahmen ist
            FillRect(hdc, &barFillRect, barColor);


            // (Nach HPEN/HBRUSH-Erstellung einfügen)
            HFONT hFont = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                      CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                      L"Consolas"); // beliebige Monospace-Schrift

            SetBkMode(hdc, TRANSPARENT);           // kein Hintergrund-Rechteck
            SetTextColor(hdc, RGB(245, 0, 94)); 

            char entName[16]{};
            ReadProcessMemory(hProc, (LPCVOID)(entPtr + OFF_NAME), &entName, sizeof(entName), nullptr);
           /* std::cout << "enitity name: " << entName << std::endl;*/
            
            wchar_t nameW[16];
            MultiByteToWideChar(CP_UTF8, 0, entName, -1, nameW, 16);

            int nameX = (int)x_center; // zentriert zur Box
            int nameY = top - 30;      // 15px über Box-Kopf

            // 4) Zeichen (HFONT einmal in den DC auswählen)
            HGDIOBJ oldF = SelectObject(hdc, hFont);

            TextOutW(hdc, nameX - (lstrlenW(nameW) * 7) / 2, // 7px ≈ halbe Zeichenbreite
                     nameY, nameW, lstrlenW(nameW));
            SelectObject(hdc, oldF);          
        }


        // e) kurzen Moment warten und Nachrichten verarbeiten:
        Sleep(16);
        // (Tipp: Hier sollte man idealerweise noch PeekMessage/TranslateMessage/DispatchMessage
        // einbauen, um das Fenster ansprechbar zu halten. Im Worst-Case kann man aber
        // WS_EX_TRANSPARENT nutzen, sodass kein Input nötig ist.)
    }

    // (Aufräumcode: ReleaseDC, zerstöre Fenster etc. - wird hier nie erreicht.)
    return 0;
}