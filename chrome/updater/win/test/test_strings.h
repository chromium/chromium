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
inline constexpr char kTestSleepSecondsSwitch[] = "test-sleep-seconds";

// The switch to signal the event with the name given as a switch value.
inline constexpr char kTestEventToSignal[] = "test-event-to-signal";

// Checks if running at medium integrity, and if so, signals the event given as
// the switch value.
inline constexpr char kTestEventToSignalIfMediumIntegrity[] =
    "test-event-to-signal-if-medium-integrity";

// The switch to wait on the event with the name given as a switch value.
inline constexpr char kTestEventToWaitOn[] = "test-event-to-wait-on";

// Specifies an exit code that the test process exits with.
inline constexpr char kTestExitCode[] = "test-exit-code";

// Specifies the test name invoking the executable.
inline constexpr char kTestName[] = "test-name";

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_TEST_TEST_STRINGS_H_
