// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_APPS_FOLDER_SUPPORT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_APPS_FOLDER_SUPPORT_H_

#include "base/files/file_path.h"

namespace web_app {

// Returns the path to the Chrome Apps folder in `~/Applications` if the folder
// exists, or an empty path otherwise. Never writes to disk. Use this for
// lookup, update, launch, and delete.
base::FilePath GetChromeAppsFolder();

// Returns the path to the Chrome Apps folder in `~/Applications`. Creates
// `~/Applications` and the Chrome Apps folder if they are missing. Returns an
// empty path if `~/Applications` is not writable. If creating the Chrome Apps
// folder fails, this still returns the path so callers can update shims in
// other locations and report the copy error. Use this only on paths that can
// create a new app shim.
base::FilePath GetOrCreateChromeAppsFolder();

// Sets the localized name and icon of `chrome_apps_folder` the first time it is
// called in a process. Does nothing if `chrome_apps_folder` is empty. The first
// successful call creates the folder if it is missing, so pass only a folder
// that exists or that the caller is allowed to create.
void UpdateChromeAppsFolderLocalizationIfNeeded(
    const base::FilePath& chrome_apps_folder);

// Normally the localized folder name and icon are updated only once in each
// process. Tests can call this method to cause the name and icon logic to
// trigger again.
void ResetHaveLocalizedAppDirNameForTesting();

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_APPS_FOLDER_SUPPORT_H_
