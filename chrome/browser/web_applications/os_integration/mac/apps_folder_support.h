// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_APPS_FOLDER_SUPPORT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_APPS_FOLDER_SUPPORT_H_

#include "base/files/file_path.h"

namespace web_app {

// Returns a path to the Chrome Apps folder in ~/Applications. If the folder
// does not already exist it is created. Also makes sure the icon and localized
// name of this folder are set correctly the first time this method is called.
base::FilePath GetChromeAppsFolder();

// Normally GetChromeAppsFolder only updates localized folder name and icon the
// first time it is called. Tests can call this method to cause the name and
// icon logic to trigger again.
void ResetHaveLocalizedAppDirNameForTesting();

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_APPS_FOLDER_SUPPORT_H_
