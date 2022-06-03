// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Public interface for enabling Breakpad on Linux systems.

#ifndef COMPONENTS_CRASH_CORE_APP_BREAKPAD_LINUX_H_
#define COMPONENTS_CRASH_CORE_APP_BREAKPAD_LINUX_H_

#include <signal.h>
#include <string>

#include "build/build_config.h"

namespace breakpad {

// Turns on the crash reporter in any process.
extern void InitCrashReporter(const std::string& process_type);

// Sets the product/distribution channel crash key.
void SetChannelCrashKey(const std::string& channel);

#if defined(OS_ANDROID)
extern void InitCrashKeysForTesting();

struct SanitizationInfo {
  bool should_sanitize_dumps = false;
  bool skip_dump_if_principal_mapping_not_referenced = false;
  uintptr_t address_within_principal_mapping = 0;
};

// Turns on the crash reporter in any process.
extern void InitCrashReporter(const std::string& process_type,
                              const SanitizationInfo& sanitization_info);

const char kWebViewSingleProcessType[] = "webview";
const char kBrowserProcessType[] = "browser";

// Enables the crash reporter in child processes.
extern void InitNonBrowserCrashReporterForAndroid(
    const std::string& process_type);
// Enables the crash reporter in child processes.
extern void InitNonBrowserCrashReporterForAndroid(
    const std::string& process_type,
    const SanitizationInfo& sanitization_info);

// Enables *micro*dump only. Can be called from any process.
extern void InitMicrodumpCrashHandlerIfNecessary(
    const std::string& process_type,
    const SanitizationInfo& sanitization_info);

extern void AddGpuFingerprintToMicrodumpCrashHandler(
    const std::string& gpu_fingerprint);

// Calling SuppressDumpGeneration causes subsequent crashes to not
// generate dumps. Calling base::debug::DumpWithoutCrashing will still
// generate a dump.
extern void SuppressDumpGeneration();
#endif  // defined(OS_ANDROID)

// Checks if crash reporting is enabled. Note that this is not the same as
// being opted into metrics reporting (and crash reporting), which controls
// whether InitCrashReporter() is called.
bool IsCrashReporterEnabled();

// Generates a minidump on demand for this process, writing it to |dump_fd|.
void GenerateMinidumpOnDemandForAndroid(int dump_fd);

// Install a handler that gets a change to handle faults before Breakpad does
// any processing. This is used by V8 for trap-based bounds checks.
void SetFirstChanceExceptionHandler(bool (*handler)(int, siginfo_t*, void*));
}  // namespace breakpad

#endif  // COMPONENTS_CRASH_CORE_APP_BREAKPAD_LINUX_H_
