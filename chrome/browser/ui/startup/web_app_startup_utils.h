// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_WEB_APP_STARTUP_UTILS_H_
#define CHROME_BROWSER_UI_STARTUP_WEB_APP_STARTUP_UTILS_H_

#include "chrome/browser/ui/startup/startup_types.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

enum class LaunchMode;
class Browser;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace web_app {
namespace startup {

// Handles a launch for a `command_line` that includes --app-id. If the app id
// is invalid, it will fall back to launching a normal browser window. Will
// return true if the --app-id flag was found, otherwise false.
bool MaybeHandleWebAppLaunch(const base::CommandLine& command_line,
                             const base::FilePath& cur_dir,
                             Profile* profile,
                             chrome::startup::IsFirstRun is_first_run);

// Final handling after a web app has been launched.
void FinalizeWebAppLaunch(absl::optional<LaunchMode> app_launch_mode,
                          const base::CommandLine& command_line,
                          chrome::startup::IsFirstRun is_first_run,
                          Browser* browser,
                          apps::LaunchContainer container);

// `callback` will be run after the next `MaybeHandleWebAppLaunch()` invocation
// finishes executing.
void SetStartupDoneCallbackForTesting(base::OnceClosure callback);

}  // namespace startup
}  // namespace web_app

#endif  //  CHROME_BROWSER_UI_STARTUP_WEB_APP_STARTUP_UTILS_H_
