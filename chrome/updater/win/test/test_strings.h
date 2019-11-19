// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_TEST_TEST_STRINGS_H_
#define CHROME_UPDATER_WIN_TEST_TEST_STRINGS_H_

#include <windows.h>

namespace updater {

// Command line switches.

// The switch to activate the sleeping action for specified delay in minutes
// before killing the process.
extern const char kTestSleepMinutesSwitch[];

// The switch to signal the event with the name given as a switch value.
extern const char kTestEventToSignal[];

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_TEST_TEST_STRINGS_H_
