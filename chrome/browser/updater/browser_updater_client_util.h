// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_UTIL_H_
#define CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_UTIL_H_

#include <string>

namespace base {
class FilePath;
}

extern const char kUpdaterName[];

// Gets the FilePath to the updater folder (e.g. Chromium/ChromiumUpdater).
base::FilePath GetUpdaterFolderName();

// Get the current installed version of the browser.
std::string CurrentlyInstalledVersion();

#endif  // CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_UTIL_H_
