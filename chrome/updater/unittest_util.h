// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UNITTEST_UTIL_H_
#define CHROME_UPDATER_UNITTEST_UTIL_H_

#include "base/files/file_path.h"

namespace base {
class TimeDelta;
}

namespace updater::test {

extern const char kChromeAppId[];

// Returns true if a process based on the named executable is running.
bool IsProcessRunning(const base::FilePath::StringType& executable_name);

// Returns true is all processes based on the named executable have exited.
// Otherwise, it returns false if the time delta has expired.
bool WaitForProcessesToExit(const base::FilePath::StringType& executable_name,
                            base::TimeDelta wait);

// Terminates all the processes on the current machine that were launched
// from the given executable name, ending them with the given exit code.
// Returns true if all processes were able to be killed off.
bool KillProcesses(const base::FilePath::StringType& executable_name,
                   int exit_code);

}  // namespace updater::test

#endif  // CHROME_UPDATER_UNITTEST_UTIL_H_
