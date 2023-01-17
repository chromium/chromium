// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_COMMON_NT_FUNCTIONS_H_
#define CHROME_CHROME_CLEANER_ENGINES_COMMON_NT_FUNCTIONS_H_

#include <windows.h>
#include <winternl.h>

// Functions & types used to access non-SDK Nt functions from ntdll.dll.

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

typedef NTSTATUS(WINAPI* NtCreateKeyFunction)(OUT PHANDLE KeyHandle,
                                              IN ACCESS_MASK DesiredAccess,
                                              IN POBJECT_ATTRIBUTES
                                                  ObjectAttributes,
                                              IN ULONG TitleIndex,
                                              IN PUNICODE_STRING Class OPTIONAL,
                                              IN ULONG CreateOptions,
                                              OUT PULONG Disposition OPTIONAL);

typedef NTSTATUS(WINAPI* NtOpenKeyFunction)(OUT PHANDLE KeyHandle,
                                            IN ACCESS_MASK DesiredAccess,
                                            IN POBJECT_ATTRIBUTES
                                                ObjectAttributes);

typedef NTSTATUS(WINAPI* NtDeleteKeyFunction)(IN HANDLE KeyHandle);

typedef NTSTATUS(WINAPI* NtQueryValueKeyFunction)(IN HANDLE KeyHandle,
                                                  IN PUNICODE_STRING ValueName,
                                                  IN KEY_VALUE_INFORMATION_CLASS
                                                      KeyValueInformationClass,
                                                  OUT PVOID KeyValueInformation,
                                                  IN ULONG Length,
                                                  OUT PULONG ResultLength);

typedef NTSTATUS(WINAPI* NtSetValueKeyFunction)(IN HANDLE KeyHandle,
                                                IN PUNICODE_STRING ValueName,
                                                IN ULONG TitleIndex OPTIONAL,
                                                IN ULONG Type,
                                                IN PVOID Data,
                                                IN ULONG DataSize);

// Partial definition only for values not in PROCESS_INFO_CLASS.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wenum-constexpr-conversion"
constexpr auto ProcessCommandLineInformation =
    static_cast<PROCESSINFOCLASS>(60);
#pragma clang diagnostic pop

typedef NTSTATUS(WINAPI* NtQueryInformationProcessFunction)(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL);

#endif  // CHROME_CHROME_CLEANER_ENGINES_COMMON_NT_FUNCTIONS_H_
