#include "TranspositionTable.h"
#include <windows.h>
#pragma comment(lib, "advapi32.lib")

namespace tt {

    static bool enableLockMemoryPrivilege() {
        HANDLE token;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
            return false;

        LUID luid;
        if (!LookupPrivilegeValue(nullptr, SE_LOCK_MEMORY_NAME, &luid)) {
            CloseHandle(token);
            return false;
        }

        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        bool ok = AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr) && GetLastError() == ERROR_SUCCESS;
        CloseHandle(token);
        return ok;
    }

    void init(size_t mb) {
        printf("\n");
        printf("Initializing transposition table\n");

        U64 bytes = utils::availableMemory();
        printf("Available memory: %llu MB\n", bytes >> 20);

        bool custom = false;
        U64 b;
        if (mb > 0) {
            b = U64(mb) << 20;
            if (b > bytes)  printf("Requested size (%llu MB) is more than available memory\n", mb);
            else            custom = true;
        }

        if (custom) {
            bytes = b;
            printf("Using requested TT size: %llu MB\n", mb);
        }
        else {
            bytes = U64(bytes * 0.9);
            printf("Using 90%% of available memory\n");
        }

        size_t n = bytes / sizeof(Bucket);
        n = std::bit_floor(n);

        BYTES = n * sizeof(Bucket);
        MASK = n - 1;

        printf("Final size: %llu MB\n", BYTES >> 20);

        usingLargePages = false;

        if (enableLockMemoryPrivilege()) {
            SIZE_T largePageMin = GetLargePageMinimum();
            if (largePageMin > 0) {
                SIZE_T allocSize = (BYTES + largePageMin - 1) & ~(largePageMin - 1);
                void* mem = VirtualAlloc(nullptr, allocSize,
                    MEM_LARGE_PAGES | MEM_RESERVE | MEM_COMMIT,
                    PAGE_READWRITE);
                if (mem) {
                    TABLE = static_cast<Bucket*>(mem);
                    usingLargePages = true;
                    printf("Using large pages\n");
                }
            }
        }

        if (!usingLargePages) {
            TABLE = static_cast<Bucket*>(::operator new(BYTES, std::align_val_t(64)));
            printf("Large pages unavailable, using standard allocation\n");
        }

        std::memset(TABLE, 0, BYTES);
        printf("Initialization complete\n\n");
    }

    void free() {
        if (TABLE) {
            if (usingLargePages) VirtualFree(TABLE, 0, MEM_RELEASE);
            else                 ::operator delete(TABLE, std::align_val_t(64));

            TABLE = nullptr;
            MASK = 0;
            BYTES = 0;
        }
        printf("Transposition table cleared\n");
    }
}