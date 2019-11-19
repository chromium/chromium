// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_HANDLER_REGISTRATION_WIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_HANDLER_REGISTRATION_WIN_H_

#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"

#include "base/files/file_path.h"

namespace web_app {

// The name of "Last Browser" file, where UpdateChromeExePath() stores the path
// of the last Chrome executable to use the containing user-data directory.
extern const base::FilePath::StringPieceType kLastBrowserFile;

// Writes the current executable path into the "Last Browser" file in
// |user_data_dir|. This allows Progressive Web Apps in |user_data_dir| to
// find and launch |user_data_dir|'s corresponding chrome.exe, even if it has
// moved (e.g. if a user-level install has been replaced by a system-level
// install), in which case the path will be fixed when the new chrome.exe is
// launched.
void UpdateChromeExePath(const base::FilePath& user_data_dir);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_HANDLER_REGISTRATION_WIN_H_
