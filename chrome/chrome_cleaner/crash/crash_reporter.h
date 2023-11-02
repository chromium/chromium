// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CRASH_CRASH_REPORTER_H_
#define CHROME_CHROME_CLEANER_CRASH_CRASH_REPORTER_H_

#include <string>

// Starts a new instance of this executable running as the crash reporter
// process.
void StartCrashReporter(const std::string version);

// Runs the crash reporter message loop within the current process. On return,
// the current process should exit.
int CrashReporterMain();

// Returns the name of the IPC pipe that is used to communicate with the crash
// reporter process, or an empty string if the current process is not connected
// to a crash reporter process.
std::wstring GetCrashReporterIPCPipeName();

// Uses the crash reporter with the specified |ipc_pipe_name|, instead of
// starting a new crash reporter process.
void UseCrashReporter(const std::wstring& ipc_pipe_name);

#endif  // CHROME_CHROME_CLEANER_CRASH_CRASH_REPORTER_H_
