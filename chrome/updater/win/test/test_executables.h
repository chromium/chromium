// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_TEST_TEST_EXECUTABLES_H_
#define CHROME_UPDATER_WIN_TEST_TEST_EXECUTABLES_H_

#include "base/process/process.h"
#include "chrome/updater/updater_scope.h"

namespace base {
class CommandLine;
}  // namespace base

namespace updater {

// The name of the executable used for tests.
extern const wchar_t kTestProcessExecutableName[];

// Creates a process that will run for a minute, which is long enough to be
// killed by a reasonably fast unit or integration test.
// Populates |command_line| with the used command line if it is not nullptr.
base::Process LongRunningProcess(UpdaterScope scope,
                                 const std::string& test_name,
                                 base::CommandLine* command_line);

// Gets the command line for `kTestServiceExecutableName` in the same directory
// as the current process.
base::CommandLine GetTestProcessCommandLine(UpdaterScope scope,
                                            const std::string& test_name);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_TEST_TEST_EXECUTABLES_H_
