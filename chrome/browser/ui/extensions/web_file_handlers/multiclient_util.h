// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_WEB_FILE_HANDLERS_MULTICLIENT_UTIL_H_
#define CHROME_BROWSER_UI_EXTENSIONS_WEB_FILE_HANDLERS_MULTICLIENT_UTIL_H_

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"

class Profile;

namespace extensions {

// Launch an application if `launch_type` is `multiple_clients`. First find a
// matching handler for the intent. Return a non-empty vector if `launch_type`
// equals `multiple-clients`.
std::vector<apps::AppLaunchParams>
GetLaunchParamsIfLaunchTypeEqualsMultipleClients(
    const WebFileHandler& handler,
    const apps::AppLaunchParams& params,
    Profile* profile,
    const Extension& extension);

// Launch type is defined in the manifest. It's `single-client` by default,
// which makes all files available in the single tab. `multiple-client` opens a
// new tab for each file.
std::vector<apps::AppLaunchParams> CheckForMultiClientLaunchSupport(
    const Extension* extension,
    Profile* profile,
    const extensions::WebFileHandlersInfo& handlers,
    const apps::AppLaunchParams& params);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_WEB_FILE_HANDLERS_MULTICLIENT_UTIL_H_
