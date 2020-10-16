// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_HANDLER_REGISTRATION_WIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_HANDLER_REGISTRATION_WIN_H_

#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "chrome/browser/web_applications/components/web_app_id.h"

namespace web_app {

// Returns the app-specific-launcher filename to be used for |app_name|.
base::FilePath GetAppSpecificLauncherFilename(const base::string16& app_name);

// Returns the Windows ProgId for the web app with the passed |app_id| in
// |profile_path|.
base::string16 GetProgIdForApp(const base::FilePath& profile_path,
                               const AppId& app_id);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_HANDLER_REGISTRATION_WIN_H_
