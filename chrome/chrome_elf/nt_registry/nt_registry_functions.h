// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_ELF_NT_REGISTRY_NT_REGISTRY_FUNCTIONS_H_
#define CHROME_CHROME_ELF_NT_REGISTRY_NT_REGISTRY_FUNCTIONS_H_

#include <windows.h>

#include <winternl.h>

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
