// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_INITIALIZER_H_
#define CHROME_CHROME_CLEANER_OS_INITIALIZER_H_

namespace chrome_cleaner {

// Initializes static variables and state required for various OS utils to
// work.
bool InitializeOSUtils();

// Signals the event handle that was passed on the commandline with
// --init-done-notifier, if it exists.
void NotifyInitializationDone();

// Signals the event handle that was passed on the commandline with
// --init-done-notifier, if it exists. Then waits for the event to be signalled
// again before continuing. This allows a test harness to pause the binary's
// execution, do some extra setup, and resume it.
// Note, this means the event must be AUTOMATIC.
void NotifyInitializationDoneForTesting();

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_INITIALIZER_H_
