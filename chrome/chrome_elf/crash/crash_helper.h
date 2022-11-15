// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_ELF_CRASH_CRASH_HELPER_H_
#define CHROME_CHROME_ELF_CRASH_CRASH_HELPER_H_

#include <windows.h>

// Keep all crash-related APIs here.  All other chrome_elf code should call here
// for crash support.
namespace elf_crash {

// Init the crash handling system for entire process.
bool InitializeCrashReporting();

// Any late cleanup of the crash handling system.
void ShutdownCrashReporting();

// Permanently disables subsequent calls to
// kernel32!SetUnhandledExceptionFilter(), see comment in .cc for why this is
// needed.
void DisableSetUnhandledExceptionFilter();

// Exception handler for exceptions in chrome_elf which need to be passed on to
// the next handler in the chain. Examples include exceptions in DllMain,
// blocklist interception code, etc.
// Note: the handler takes a minidump.
int GenerateCrashDump(EXCEPTION_POINTERS* exception_pointers);

// Generate a crash dump by calling into crashpad.
void DumpWithoutCrashing();

// Set the metrics client ID in crash keys.
void SetMetricsClientIdImpl(const char* client_id);

}  // namespace elf_crash

#endif  // CHROME_CHROME_ELF_CRASH_CRASH_HELPER_H_
