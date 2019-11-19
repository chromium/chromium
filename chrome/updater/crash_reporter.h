// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CRASH_REPORTER_H_
#define CHROME_UPDATER_CRASH_REPORTER_H_

#include <string>

#include "base/strings/string16.h"
#include "build/build_config.h"

namespace updater {

// Starts a new instance of this executable running as the crash reporter
// process.
void StartCrashReporter(const std::string& version);

// Runs the crash reporter message loop within the current process. On return,
// the current process should exit.
int CrashReporterMain();

#if defined(OS_WIN)

// Returns the name of the IPC pipe that is used to communicate with the
// crash reporter process, or an empty string if the current process is
// not connected to a crash reporter process.
base::string16 GetCrashReporterIPCPipeName();

// Uses the crash reporter with the specified |ipc_pipe_name|, instead of
// starting a new crash reporter process.
void UseCrashReporter(const base::string16& ipc_pipe_name);

#endif  // OS_WIN

}  // namespace updater

#endif  // CHROME_UPDATER_CRASH_REPORTER_H_
