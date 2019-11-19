// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_
#define CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_

namespace base {
class CommandLine;
}  // namespace base

#include "base/win/windows_types.h"

namespace updater {

bool RegisterUpdateAppsTask(const base::CommandLine& run_command);
void UnregisterUpdateAppsTask();

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_
