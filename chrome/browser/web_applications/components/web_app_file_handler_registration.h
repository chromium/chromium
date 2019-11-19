// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_HANDLER_REGISTRATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_HANDLER_REGISTRATION_H_

#include <set>
#include <string>

#include "chrome/browser/web_applications/components/web_app_helpers.h"

class Profile;

namespace web_app {

// Returns true if Chrome supports WebApp File Handling on this OS.
bool OsSupportsWebAppFileHandling();

// Do OS-specific registration to handle opening files with the specified
// |file_extensions| and |mime_types| with the PWA with the specified |app_id|.
// This may also involve creating a shim app to launch Chrome from.
void RegisterFileHandlersForWebApp(const AppId& app_id,
                                   const std::string& app_name,
                                   Profile* profile,
                                   const std::set<std::string>& file_extensions,
                                   const std::set<std::string>& mime_types);

// Undo the file extensions registration for the PWA with specified |app_id|.
// If a shim app was required, also removes the shim app.
void UnregisterFileHandlersForWebApp(const AppId& app_id, Profile* profile);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_HANDLER_REGISTRATION_H_
