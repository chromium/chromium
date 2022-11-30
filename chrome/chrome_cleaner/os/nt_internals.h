// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_NT_INTERNALS_H_
#define CHROME_CHROME_CLEANER_OS_NT_INTERNALS_H_

#define RTL_REGISTRY_SERVICES 1
#define RTL_REGISTRY_CONTROL 2

typedef LONG NTSTATUS;

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#endif

typedef enum _KEY_INFORMATION_CLASS {
  KeyBasicInformation = 0,
  KeyNodeInformation = 1,
  KeyFullInformation = 2,
  KeyNameInformation = 3,
  KeyCachedInformation = 4,
  KeyFlagsInformation = 5,
  KeyVirtualizationInformation = 6,
  KeyHandleTagsInformation = 7,
  MaxKeyInfoClass = 8
} KEY_INFORMATION_CLASS;

typedef struct _KEY_NAME_INFORMATION {
  // The size, in bytes, of the key name string in the Name array.
  ULONG NameLength;
  // An array of wide characters that contains the name of the key.
  WCHAR Name[1];
} KEY_NAME_INFORMATION, *PKEY_NAME_INFORMATION;

// Missing ntdll prototype.
extern "C" {
__declspec(dllimport) DWORD WINAPI
    RtlCheckRegistryKey(ULONG relative_to, PCWSTR path);

__declspec(dllimport) DWORD WINAPI
    NtQueryKey(HANDLE, KEY_INFORMATION_CLASS, PVOID, ULONG, ULONG*);
}

#endif  // CHROME_CHROME_CLEANER_OS_NT_INTERNALS_H_
