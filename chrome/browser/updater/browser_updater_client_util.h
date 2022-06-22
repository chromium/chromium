// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_UTIL_H_
#define CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_UTIL_H_

#include <string>

#include "chrome/updater/updater_scope.h"

namespace base {
class FilePath;
}

extern const char kUpdaterName[];
extern const char kPrivilegedHelperName[];

// Gets the FilePath to the updater folder (e.g. Chromium/ChromiumUpdater).
base::FilePath GetUpdaterFolderName();

// Gets the FilePath to the updater executable folder.
base::FilePath GetUpdaterExecutablePath();

// Get the current installed version of the browser.
std::string CurrentlyInstalledVersion();

// Returns whether or not the browser can install the updater.
bool CanInstallUpdater();

// System level updater should only be used if the browser is owned by root.
// During promotion, the browser will be changed to be owned by root and wheel.
// A browser must go through promotion before it can utilize the system-level
// updater.
updater::UpdaterScope GetUpdaterScope();

// Updater should be promoted if it meets the following criteria:
//    1) When browser is owned by root and updater is not yet installed.
//    2) When effective user is root and browser is not owned by root.
//    3) When effective user is not the owner of the browser and is an
//    administrator.
bool ShouldPromoteUpdater();

#endif  // CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_UTIL_H_
