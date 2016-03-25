// iRacingDeStutterer.cpp : Defines the entry point for the console application.
//

#include <iostream>
#include <windows.h>
#include <TlHelp32.h>
#include <tchar.h>
#include <Shlobj.h>

#include "irsdk_defines.h"

struct handle_wrapper
{
    HANDLE handle;
    handle_wrapper(HANDLE h) : handle(h){}
    ~handle_wrapper() 
    { 
        if (handle != INVALID_HANDLE_VALUE) { ::CloseHandle(handle); }
    }
};

void FixThreadAffinity(DWORD pId)
{
    std::cout << "Fixing iRacing thread affinities." << std::endl;

    handle_wrapper process_handle(::OpenProcess(PROCESS_ALL_ACCESS, FALSE, pId));
    if(process_handle.handle == INVALID_HANDLE_VALUE)
    {
        std::cout << "Error in OpenProcess for iRacing process " << GetLastError() << std::endl;
        return;
    }

    DWORD_PTR processAffinity = 0xFFFF;
    ::SetProcessAffinityMask(process_handle.handle, processAffinity);

    handle_wrapper thread_handle_snapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0));
    if(thread_handle_snapshot.handle == INVALID_HANDLE_VALUE)
    {
        std::cout << "Error getting thread snapshot " << ::GetLastError() << std::endl;
        return;
    }

    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);

    if(!::Thread32First(thread_handle_snapshot.handle, &te32))
    {
        std::cout << "Error Thread32First: " << ::GetLastError() << std::endl;
        return;
    }

    do
    {
        if(te32.th32OwnerProcessID == pId)
        {
            handle_wrapper thread_handle(::OpenThread(THREAD_ALL_ACCESS, FALSE, te32.th32ThreadID));
            if(thread_handle.handle == INVALID_HANDLE_VALUE)
            {
                std::cout << "Error opening iRacing thread " << ::GetLastError() << std::endl;
                return;
            }

            if(!::SetThreadAffinityMask(thread_handle.handle, processAffinity))
            {
                std::cout << "Error setting thread affinity " << ::GetLastError() << std::endl;
            }
        }

    } while(Thread32Next(thread_handle_snapshot.handle, &te32));

}

class DaqRuntime
{
public:

    void process()
    {
        if(irsdk_waitForDataReady(16, 0))
        {
            const irsdk_header* header = irsdk_getHeader();
            if(header)
            {
                checkAffinityFix();
            }

        }
        else if(!irsdk_isConnected())
        {
            reset();
        }
    }

    void reset()
    {
        m_threadsFixed = false;
        m_iRacingProcessId = 0;
    }

    void checkAffinityFix()
    {
        if(!m_threadsFixed)
        {
            if(isForgroundProcess(iRacingProcessId()))
            {
                FixThreadAffinity(iRacingProcessId());

                m_threadsFixed = true;
            }
        }
    }

    DWORD iRacingProcessId()
    {
        if(m_iRacingProcessId != 0)
        {
            return m_iRacingProcessId;
        }

        const std::wstring iRacingProcessName(L"iRacingSim.exe;iRacingSim64.exe;iRacingSimDX11.exe;iRacingSim64DX11.exe");

        PROCESSENTRY32 entry;
        entry.dwSize = sizeof(PROCESSENTRY32);

        handle_wrapper snapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL));

        if(::Process32First(snapshot.handle, &entry) == TRUE)
        {
            do
            {
                if(iRacingProcessName.find(entry.szExeFile) != std::string::npos)
                {
                    m_iRacingProcessId = entry.th32ProcessID;
                    return m_iRacingProcessId;
                }

            } while(::Process32Next(snapshot.handle, &entry) == TRUE);
        }

        return 0;
    }

    bool isForgroundProcess(DWORD processId)
    {
        DWORD fgProcessId = 0;

        if(::GetWindowThreadProcessId(::GetForegroundWindow(), &fgProcessId))
        {
            if(fgProcessId != 0 && fgProcessId == processId)
            {
                return true;
            }
        }

        return false;
    }

private:
    struct irsdk_wrapper
    {
        irsdk_wrapper() { irsdk_startup(); }
        ~irsdk_wrapper() { irsdk_shutdown(); }
    } irsdk;

    DWORD m_iRacingProcessId {0};
    bool m_threadsFixed {false};
};

int main()
{
    DaqRuntime daq;

    std::cout << "iRacingDestutterer by Patrick Moore (patrickwmoore@gmail.com)" << std::endl;
    std::cout << "Press Ctrl-C to exit.\n" << std::endl;

    bool exit = false;
    while(exit == false)
    {
        daq.process();

        Sleep(100);
    }

    return 0;
}

