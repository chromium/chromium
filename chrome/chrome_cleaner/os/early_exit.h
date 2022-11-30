// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_EARLY_EXIT_H_
#define CHROME_CHROME_CLEANER_OS_EARLY_EXIT_H_

namespace chrome_cleaner {

// Terminates the current process with |exit_code|. This function should always
// be used for early termination, such as on a watchdog timeout or critical
// error, because it handles corner cases correctly. See
// http://crbug.com/603131#c27 for more details.
void EarlyExit(int exit_code);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_EARLY_EXIT_H_
