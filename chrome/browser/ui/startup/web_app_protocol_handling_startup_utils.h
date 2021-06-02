// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_WEB_APP_PROTOCOL_HANDLING_STARTUP_UTILS_H_
#define CHROME_BROWSER_UI_STARTUP_WEB_APP_PROTOCOL_HANDLING_STARTUP_UTILS_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Browser;
class Profile;
namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace web_app {
namespace startup {

using FinalizeWebAppLaunchCallback =
    base::OnceCallback<void(Browser* browser,
                            apps::mojom::LaunchContainer container)>;
using StartupLaunchAfterProtocolCallback =
    base::OnceCallback<void(const base::CommandLine& command_line,
                            const base::FilePath& cur_dir,
                            Profile* profile,
                            Profile* last_used_profile,
                            const std::vector<Profile*>& last_opened_profiles)>;

// Launches a web app to handle a URL if `command_line` contains
// --app-id=<app> and at least one URL for which the referenced app is
// is registered in `profile` to handle the URL's protocol. Returns true
// if the command_line contains --app-id=<app> and at least one valid URL
// by checking for blink::IsValidCustomHandlerScheme(). Only one valid URL
// is expected in `command_line` and all other URLs are ignored.
// The URL will be further validated by consulting the os_integration_manager
// and checked for a installed app that is registered in `profile` to handle
// the URL's protocol. `startup_callback` will run when the check fails and
// there is no installed web app registered for `profile` that can handle the
// protocol.`finalize_callback` is used if and only if this function
// returns true.
bool MaybeLaunchProtocolHandlerWebApp(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    Profile* last_used_profile,
    const std::vector<Profile*>& last_opened_profiles,
    FinalizeWebAppLaunchCallback finalize_callback,
    StartupLaunchAfterProtocolCallback startup_callback);

}  // namespace startup

}  // namespace web_app

#endif  //  CHROME_BROWSER_UI_STARTUP_WEB_APP_PROTOCOL_HANDLING_STARTUP_UTILS_H_
