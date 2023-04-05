// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_TEST_TEST_STRINGS_H_
#define CHROME_UPDATER_WIN_TEST_TEST_STRINGS_H_

#include <windows.h>

namespace updater {

// Command line switches.

// The switch to activate the sleeping action for specified delay in seconds
// before killing the process.
extern const char kTestSleepSecondsSwitch[];

// The switch to signal the event with the name given as a switch value.
extern const char kTestEventToSignal[];

// Checks if running at medium integrity, and if so, signals the event given as
// the switch value.
extern const char kTestEventToSignalIfMediumIntegrity[];

// The switch to wait on the event with the name given as a switch value.
extern const char kTestEventToWaitOn[];

// Specifies an exit code that the test process exits with.
extern const char kTestExitCode[];

// Specifies the test name invoking the executable.
extern const char kTestName[];

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_TEST_TEST_STRINGS_H_
