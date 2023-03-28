// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_UI_UTILS_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_UI_UTILS_H_

#include <vector>

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/files/file_path.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

namespace ash {

// Returns the system app type for the given App ID.
absl::optional<SystemWebAppType> GetSystemWebAppTypeForAppId(
    Profile* profile,
    const web_app::AppId& app_id);

// Returns the PWA system App ID for the given system app type.
absl::optional<web_app::AppId> GetAppIdForSystemWebApp(
    Profile* profile,
    SystemWebAppType app_type);

absl::optional<apps::AppLaunchParams> CreateSystemWebAppLaunchParams(
    Profile* profile,
    SystemWebAppType app_type,
    int64_t display_id);

// Additional parameters to control LaunchSystemAppAsync behaviors.
struct SystemAppLaunchParams {
  SystemAppLaunchParams();
  ~SystemAppLaunchParams();

  // If provided launches System Apps into |url|, instead of its start_url (as
  // specified its WebAppInstallInfo).
  //
  // This is mutually exclusive with non-empty |launch_paths|.
  absl::optional<GURL> url;

  // Where the app is launched from.
  apps::LaunchSource launch_source = apps::LaunchSource::kFromChromeInternal;

  // If non-empty, specifies files passed to Web File Handling. The app needs to
  // specify file handlers in its WebAppInstallInfo.
  //
  // This is mutually exclusive with |url|.
  std::vector<base::FilePath> launch_paths;
};

// Launch the given System Web App |type|, |params| can be used to tweak the
// launch behavior (e.g. launch to app's subpage, specifying launch source for
// metrics). Terminal App should use crostini::LaunchTerminal*.
//
// This function will try to find an appropriate launch profile in these
// circumstances:
//
//   - Incognito profile of a normal session: Launch to original profile, which
//     the incognito profile is created from
//   - Profiles in guest session: Launch to the primary off-the-record profile
//     (the profile used to browser websites in guest sessions)
//   - Other unsuitable profiles (e.g. Sign-in profile): Don't launch, and send
//     a crash report
//
// In tests, remember to use content::TestNavigationObserver to wait the
// navigation.
void LaunchSystemWebAppAsync(
    Profile* profile,
    SystemWebAppType type,
    const SystemAppLaunchParams& params = SystemAppLaunchParams(),
    apps::WindowInfoPtr window_info = nullptr);

// Implementation of LaunchSystemWebApp. Do not use this before discussing your
// use case with the System Web Apps team.
//
// This method returns `nullptr` if the app aborts the launch (e.g. delaying the
// launch after some async operation).
Browser* LaunchSystemWebAppImpl(Profile* profile,
                                SystemWebAppType type,
                                const GURL& url,
                                const apps::AppLaunchParams& params);

// Returns a browser that is dedicated (i.e. has a standalone shelf icon) to
// hosting the given system |app_type| that matches |browser_type| and
// optionally |url|.
//
// If |url| is is not empty, this method only returns the browser if it is
// showing |url| page.
//
// If there are multiple browsers hosting the app, returns the currently active
// (focused) one (if it exists), otherwise the most recently created one.
//
// This method is intended for performing UI manipulations (e.g. changing
// window size and position). Don't retrieve and interact with the WebContents
// in the returned browser, as it might be rendering a different origin.
//
// Consider using the WebUIController to retrieve the WebContents currently
// rendering the app if you want to interact with app's JavaScript environment.
Browser* FindSystemWebAppBrowser(Profile* profile,
                                 SystemWebAppType app_type,
                                 Browser::Type browser_type = Browser::TYPE_APP,
                                 const GURL& url = GURL());

// Returns true if the |browser| is dedicated (see above) to hosting a system
// web app.
bool IsSystemWebApp(Browser* browser);

// Returns whether the |browser| is dedicated (see above) to hosting the system
// app |type|.
bool IsBrowserForSystemWebApp(Browser* browser, SystemWebAppType type);

// Returns the SystemWebAppType that should capture the |url|.
absl::optional<SystemWebAppType> GetCapturingSystemAppForURL(Profile* profile,
                                                             const GURL& url);

// Returns the minimum window size for a system web app, or an empty size if
// the app does not specify a minimum size.
gfx::Size GetSystemWebAppMinimumWindowSize(Browser* browser);

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_UI_UTILS_H_
