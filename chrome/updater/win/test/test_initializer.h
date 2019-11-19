// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_TEST_TEST_INITIALIZER_H_
#define CHROME_UPDATER_WIN_TEST_TEST_INITIALIZER_H_

namespace updater {

// Signals the event handle that was passed on the command line with
// --init-done-notifier, if it exists. Then waits for the event to be signalled
// again before continuing. This allows a test harness to pause the binary's
// execution, do some extra setup, and resume it.
// Note, this means the event must be AUTOMATIC.
void NotifyInitializationDoneForTesting();

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_TEST_TEST_INITIALIZER_H_
