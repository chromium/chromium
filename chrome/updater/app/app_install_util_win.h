// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_INSTALL_UTIL_WIN_H_
#define CHROME_UPDATER_APP_APP_INSTALL_UTIL_WIN_H_

#include <windows.h>

#include "chrome/updater/app/app_install_progress.h"

namespace updater {

// Launches the post-install launch command lines for each app in `info`.
bool LaunchCmdLines(const ObserverCompletionInfo& info);

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_INSTALL_UTIL_WIN_H_
