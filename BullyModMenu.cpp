#include <Windows.h>
#include <iostream>
#include <TlHelp32.h>
#include <string>
#include <thread>
#include <conio.h>

bool godModeEnabled = false; // Toggle god

DWORD GetProcessIdByName(const std::wstring& processName) {
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    if (Process32First(snapshot, &entry)) {
        do {
            if (!_wcsicmp(entry.szExeFile, processName.c_str())) {
                CloseHandle(snapshot);
                return entry.th32ProcessID;
            }
        } while (Process32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return 0;
}

// Open bully.exe
HANDLE OpenProcessById(DWORD pid) {
    return OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, pid);
}

// Read Memory
template <typename T>
T ReadMemory(HANDLE hProcess, uintptr_t address) {
    T buffer;
    SIZE_T bytesRead;
    ReadProcessMemory(hProcess, (LPCVOID)address, &buffer, sizeof(T), &bytesRead);
    return buffer;
}

// Write Memory
template <typename T>
void WriteMemory(HANDLE hProcess, uintptr_t address, T value) {
    SIZE_T bytesWritten;
    WriteProcessMemory(hProcess, (LPVOID)address, &value, sizeof(T), &bytesWritten);
}

// Read pointer
template <typename T>
T ReadPointer(HANDLE hProcess, uintptr_t baseAddress, uintptr_t offset) {
    uintptr_t firstLevelPtr;
    if (!ReadProcessMemory(hProcess, (LPCVOID)baseAddress, &firstLevelPtr, sizeof(uintptr_t), nullptr)) {
        return T();
    }
    return ReadMemory<T>(hProcess, firstLevelPtr + offset);
}

template <typename T>
void WritePointer(HANDLE hProcess, uintptr_t baseAddress, uintptr_t offset, T value) {
    uintptr_t firstLevelPtr;
    if (ReadProcessMemory(hProcess, (LPCVOID)baseAddress, &firstLevelPtr, sizeof(uintptr_t), nullptr)) {
        WriteMemory<T>(hProcess, firstLevelPtr + offset, value);
    }
}

void GodModeThread(HANDLE hProcess, uintptr_t baseAddress, uintptr_t healthOffset, uintptr_t maxHealthOffset) {
    while (true) {
        if (godModeEnabled) {
            float maxHealth = ReadPointer<float>(hProcess, baseAddress, maxHealthOffset);
            WritePointer<float>(hProcess, baseAddress, healthOffset, maxHealth);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}



void DisplayHeader() {
    system("cls");

    std::cout << R"(
 ________  ___  _________  ________  ___  ___  ________    _______  ___  __       
|\_____  \|\  \|\___   ___\\   __  \|\  \|\  \|\   ____\  /  ___  \|\  \|\  \     
 \|___/  /\ \  \|___ \  \_\ \  \|\  \ \  \\\  \ \  \___|_/__/|_/  /\ \  \/  /|_   
     /  / /\ \  \   \ \  \ \ \   _  _\ \  \\\  \ \_____  \__|//  / /\ \   ___  \  
    /  /_/__\ \  \   \ \  \ \ \  \\  \\ \  \\\  \|____|\  \  /  /_/__\ \  \\ \  \ 
   |\________\ \__\   \ \__\ \ \__\\ _\\ \_______\____\_\  \|\________\ \__\\ \__\
    \|_______|\|__|    \|__|  \|__|\|__|\|_______|\_________\\|_______|\|__| \|__|
                                                 \|_________|                     
                                                                                  
                                                                                  
    )" << std::endl;
}


void DisplayMenu() {
    std::cout << "\n [1] Add $100" << std::endl;
    std::cout << " [2] Subtract $100" << std::endl;
    std::cout << " [3] Max Health" << std::endl;
    std::cout << " [4] Remove Health" << std::endl;
    std::cout << " [5] Toggle Godmode [" << (godModeEnabled ? "ON" : "OFF") << "]" << std::endl;
    std::cout << " Select an option: ";
}


void HandleUserInput(HANDLE hProcess, uintptr_t baseAddress, uintptr_t moneyOffset, uintptr_t healthOffset, uintptr_t maxHealthOffset) {
    std::thread godModeThread(GodModeThread, hProcess, baseAddress, healthOffset, maxHealthOffset);
    godModeThread.detach();
    while (true) {
        // Read values
        int moneyRaw = ReadPointer<int>(hProcess, baseAddress, moneyOffset);
        float money = moneyRaw / 100.0f;
        float health = ReadPointer<float>(hProcess, baseAddress, healthOffset);
        float maxHealth = ReadPointer<float>(hProcess, baseAddress, maxHealthOffset);

        DisplayHeader();
        std::cout << " Money:      $" << money << std::endl;
        std::cout << " Health:      " << health << std::endl;
        std::cout << " Max. Health:  " << maxHealth << std::endl;

        DisplayMenu();

        int choice;
        std::cin >> choice;

        switch (choice) {
        case 1:
            //+$100
            moneyRaw += 10000;
            WritePointer<int>(hProcess, baseAddress, moneyOffset, moneyRaw);
            std::cout << "\n Added $100!\n";
            break;

        case 2:
            //-$100
            moneyRaw -= 10000;
            WritePointer<int>(hProcess, baseAddress, moneyOffset, moneyRaw);
            std::cout << "\n Subtracted $100!\n";
            break;

        case 3:
            // Max Health
            WritePointer<float>(hProcess, baseAddress, healthOffset, maxHealth);
            std::cout << "\n Health set to Max!\n";
            break;

        case 4:
            // Remove health
            WritePointer<float>(hProcess, baseAddress, healthOffset, 0.0f);
            std::cout << "\n Health Removed\n";
            break;
        case 5:
            godModeEnabled = !godModeEnabled;
            std::cout << "\n Godmode " << (godModeEnabled ? "Enabled!" : "Disabled!") << "\n";
            break;

        default:
            std::cout << "\n Invalid Option! Try again.\n";
            break;
        }

        // Delay
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

int main() {
    SetConsoleTitleA("BullyModMenu by zitrus2k");

    //Process
    DWORD pid = GetProcessIdByName(L"Bully.exe");
    if (pid == 0) {
        std::cout << "Bully process not found." << std::endl;
        return 0;
    }

    // Open process
    HANDLE hProcess = OpenProcessById(pid);
    if (!hProcess) {
        std::cout << "Failed to open Bully process." << std::endl;
        return 0;
    }

    // Base
    uintptr_t baseAddress = 0x81AEA8;
    uintptr_t moneyOffset = 0x1CA0;
    uintptr_t healthOffset = 0x1CB8;
    uintptr_t maxHealthOffset = 0x1CA4;

    // Get Bully.exe base
    uintptr_t bullyBase = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    MODULEENTRY32 modEntry;
    modEntry.dwSize = sizeof(MODULEENTRY32);
    if (Module32First(hSnapshot, &modEntry)) {
        do {
            if (!_wcsicmp(modEntry.szModule, L"Bully.exe")) {
                bullyBase = (uintptr_t)modEntry.modBaseAddr;
                break;
            }
        } while (Module32Next(hSnapshot, &modEntry));
    }
    CloseHandle(hSnapshot);

    if (bullyBase == 0) {
        std::cout << "Failed to get Bully.exe base address." << std::endl;
        CloseHandle(hProcess);
        return 0;
    }

    uintptr_t finalBaseAddress = bullyBase + baseAddress;

    // Start
    HandleUserInput(hProcess, finalBaseAddress, moneyOffset, healthOffset, maxHealthOffset);

    CloseHandle(hProcess);
    return 0;
}
