// Copyright 2013 The Chromium Authors
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
