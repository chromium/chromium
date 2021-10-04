// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_WEB_APP_STARTUP_UTILS_H_
#define CHROME_BROWSER_UI_STARTUP_WEB_APP_STARTUP_UTILS_H_

#include <vector>

#include "base/callback_forward.h"
#include "chrome/browser/web_applications/web_app_id.h"
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
using ContinueStartupCallback = base::OnceClosure;

// Processes `command_line` to determine if it should be handled as an app
// launch. This function only processes launches that may be aborted, e.g. due
// to a user disallowing the launch --- currently, protocol handler and file
// handler launches. If it's synchronously determined that this is not a handled
// web app launch, the return value will be false. Otherwise, `startup_callback`
// /may/ be asynchronously run to resume normal browser startup. If it is a
// handled web app launch, `finalize_callback` will be run and the return value
// will be true. The profile parameters must all be kept alive while the
// processing is ongoing.
bool MaybeHandleEarlyWebAppLaunch(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    Profile* last_used_profile,
    const std::vector<Profile*>& last_opened_profiles,
    FinalizeWebAppLaunchCallback finalize_callback,
    ContinueStartupCallback startup_callback);

}  // namespace startup
}  // namespace web_app

#endif  //  CHROME_BROWSER_UI_STARTUP_WEB_APP_STARTUP_UTILS_H_
