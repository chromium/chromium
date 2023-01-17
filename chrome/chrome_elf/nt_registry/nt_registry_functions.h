// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_ELF_NT_REGISTRY_NT_REGISTRY_FUNCTIONS_H_
#define CHROME_CHROME_ELF_NT_REGISTRY_NT_REGISTRY_FUNCTIONS_H_

#include <windows.h>
#include <winternl.h>

typedef enum _KEY_INFORMATION_CLASS {
  KeyBasicInformation = 0,
  KeyFullInformation = 2
} KEY_INFORMATION_CLASS,
    *PKEY_INFORMATION_CLASS;

typedef enum _KEY_VALUE_INFORMATION_CLASS {
  KeyValueFullInformation = 1
} KEY_VALUE_INFORMATION_CLASS,
    *PKEY_VALUE_INFORMATION_CLASS;

typedef struct _KEY_VALUE_FULL_INFORMATION {
  ULONG TitleIndex;
  ULONG Type;
  ULONG DataOffset;
  ULONG DataLength;
  ULONG NameLength;
  WCHAR Name[1];
} KEY_VALUE_FULL_INFORMATION, *PKEY_VALUE_FULL_INFORMATION;

typedef struct _KEY_BASIC_INFORMATION {
  LARGE_INTEGER LastWriteTime;
  ULONG TitleIndex;
  ULONG NameLength;
  WCHAR Name[1];
} KEY_BASIC_INFORMATION, *PKEY_BASIC_INFORMATION;

typedef struct _KEY_FULL_INFORMATION {
  LARGE_INTEGER LastWriteTime;
  ULONG TitleIndex;
  ULONG ClassOffset;
  ULONG ClassLength;
  ULONG SubKeys;
  ULONG MaxNameLen;
  ULONG MaxClassLen;
  ULONG Values;
  ULONG MaxValueNameLen;
  ULONG MaxValueDataLen;
  WCHAR Class[1];
} KEY_FULL_INFORMATION, *PKEY_FULL_INFORMATION;

extern "C" {
// wdm.h.
NTSTATUS WINAPI NtCreateKey(OUT PHANDLE KeyHandle,
                            IN ACCESS_MASK DesiredAccess,
                            IN POBJECT_ATTRIBUTES ObjectAttributes,
                            IN ULONG TitleIndex,
                            IN PUNICODE_STRING Class OPTIONAL,
                            IN ULONG CreateOptions,
                            OUT PULONG Disposition OPTIONAL);

// wdm.h.
NTSTATUS WINAPI NtOpenKeyEx(OUT PHANDLE KeyHandle,
                            IN ACCESS_MASK DesiredAccess,
                            IN POBJECT_ATTRIBUTES ObjectAttributes,
                            IN DWORD open_options);

// wdm.h.
NTSTATUS WINAPI NtDeleteKey(IN HANDLE KeyHandle);

// wdm.h.
NTSTATUS WINAPI NtQueryKey(IN HANDLE KeyHandle,
                           IN KEY_INFORMATION_CLASS KeyInformationClass,
                           OUT PVOID KeyInformation,
                           IN ULONG Length,
                           OUT PULONG ResultLength);

// wdm.h.
NTSTATUS WINAPI NtEnumerateKey(IN HANDLE KeyHandle,
                               IN ULONG Index,
                               IN KEY_INFORMATION_CLASS KeyInformationClass,
                               OUT PVOID KeyInformation,
                               IN ULONG Length,
                               OUT PULONG ResultLength);

// wdm.h.
NTSTATUS WINAPI
NtQueryValueKey(IN HANDLE KeyHandle,
                IN PUNICODE_STRING ValueName,
                IN KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
                OUT PVOID KeyValueInformation,
                IN ULONG Length,
                OUT PULONG ResultLength);

// wdm.h.
NTSTATUS WINAPI NtSetValueKey(IN HANDLE KeyHandle,
                              IN PUNICODE_STRING ValueName,
                              IN ULONG TitleIndex OPTIONAL,
                              IN ULONG Type,
                              IN PVOID Data,
                              IN ULONG DataSize);

// ntrtl.h.
NTSTATUS WINAPI RtlFormatCurrentUserKeyPath(OUT PUNICODE_STRING RegistryPath);

}  // extern "C"

#endif  // CHROME_CHROME_ELF_NT_REGISTRY_NT_REGISTRY_FUNCTIONS_H_
