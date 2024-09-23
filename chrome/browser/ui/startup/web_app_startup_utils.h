// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_WEB_APP_STARTUP_UTILS_H_
#define CHROME_BROWSER_UI_STARTUP_WEB_APP_STARTUP_UTILS_H_

#include <optional>

#include "base/functional/callback_internal.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"

class Browser;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace web_app {
namespace startup {

// Various ways web apps can open on Chrome launch.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OpenMode {
  // Launched app by any method other than through the command-line or from a
  // platform shortcut.
  kInWindowOther = 0,
  kInTab = 1,            // Launched as an installed web app in a browser tab.
  kUnknown = 2,          // The requested web app was not installed.
  kInWindowByUrl = 3,    // Launched the app by url with --app switch.
  kInWindowByAppId = 4,  // Launched app by id with --app-id switch.
  kMaxValue = kInWindowByAppId,
};

// Handles a launch for a `command_line` that includes --app-id. If the app id
// is invalid, it will fall back to launching a normal browser window. Will
// return true if the --app-id flag was found, otherwise false.
bool MaybeHandleWebAppLaunch(const base::CommandLine& command_line,
                             const base::FilePath& cur_dir,
                             Profile* profile,
                             chrome::startup::IsFirstRun is_first_run);

// Final handling after a web app has been launched.
void FinalizeWebAppLaunch(std::optional<OpenMode> app_open_mode,
                          const base::CommandLine& command_line,
                          chrome::startup::IsFirstRun is_first_run,
                          Browser* browser,
                          apps::LaunchContainer container);

// `callback` will be run after the next `MaybeHandleWebAppLaunch()` invocation
// finishes executing.
void SetStartupDoneCallbackForTesting(base::OnceClosure callback);

// `callback` will be run after `StartupWebAppCreator::OnBrowserShutdown()`
// finishes. This method is used ONLY for testing purpose.
void SetBrowserShutdownCompleteCallbackForTesting(base::OnceClosure callback);

}  // namespace startup
}  // namespace web_app

#endif  //  CHROME_BROWSER_UI_STARTUP_WEB_APP_STARTUP_UTILS_H_
