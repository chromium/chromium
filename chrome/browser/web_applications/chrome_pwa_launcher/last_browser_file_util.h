// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAST_BROWSER_FILE_UTIL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAST_BROWSER_FILE_UTIL_H_

#include "base/files/file_path.h"

namespace web_app {

// The name of "Last Browser" file, which stores the path of the last Chrome
// executable to use its containing User Data directory.
extern const base::FilePath::StringPieceType kLastBrowserFilename;

// Reads the content of |last_browser_file|, which is assumed to contain a path,
// as a wide string, and returns it as a FilePath. Returns an empty FilePath if
// reading |last_browser_file| fails.
base::FilePath ReadChromePathFromLastBrowserFile(
    const base::FilePath& last_browser_file);

// Writes the current executable path to the Last Browser file in
// |user_data_dir|. This allows Progressive Web Apps in |user_data_dir| to find
// and launch |user_data_dir|'s corresponding chrome.exe, even if it has moved
// (e.g. if a user-level install has been replaced by a system-level install),
// in which case the path will be fixed when the new chrome.exe is launched.
void WriteChromePathToLastBrowserFile(const base::FilePath& user_data_dir);

// Returns the path to the Last Browser file in |web_app_dir|'s
// great-grandparent User Data directory.
base::FilePath GetLastBrowserFileFromWebAppDir(
    const base::FilePath& web_app_dir);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAST_BROWSER_FILE_UTIL_H_
