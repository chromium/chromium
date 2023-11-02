// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_FALLBACK_CRASH_HANDLING_WIN_H_
#define COMPONENTS_CRASH_CORE_APP_FALLBACK_CRASH_HANDLING_WIN_H_

#include <stdint.h>
#include <string>

#include "base/command_line.h"

namespace crash_reporter {

namespace switches {
extern const char kFallbackCrashHandler[];
// The fallback handler uses the same prefetch ID as the Crashpad handler.
extern const char kPrefetchArgument[];
}

// A somewhat unique exit code for the crashed process. This is mostly
// for testing, as there won't likely be anyone around to record the exit
// code of the Crashpad handler.
extern const uint32_t kFallbackCrashTerminationCode;

// Sets up fallback crash handling for this process.
// Note that this installs an unhandled exception filter, and requires
// that the executable call the "RunAsFallbackCrashHandler" function, when
// it's passed a --type switch with the value |kFallbackCrashHandlerType|.
bool SetupFallbackCrashHandling(const base::CommandLine& command_line);

// Runs the fallback crash handler process, to handle a crash in a process
// that's previously called SetupFallbackCrashHandling.
// The |product_name|, |version| and |channel_name| parameters will be used
// as properties of the crash for the purposes of upload.
int RunAsFallbackCrashHandler(const base::CommandLine& command_line,
                              std::string product_name,
                              std::string version,
                              std::string channel_name);

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_APP_FALLBACK_CRASH_HANDLING_WIN_H_
