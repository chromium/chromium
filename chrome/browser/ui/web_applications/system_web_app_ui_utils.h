// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_SYSTEM_WEB_APP_UI_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_SYSTEM_WEB_APP_UI_UTILS_H_

#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

class WebAppProvider;

// Returns the system app type for the given App ID.
absl::optional<ash::SystemWebAppType> GetSystemWebAppTypeForAppId(
    Profile* profile,
    const AppId& app_id);

// Returns the PWA system App ID for the given system app type.
absl::optional<AppId> GetAppIdForSystemWebApp(Profile* profile,
                                              ash::SystemWebAppType app_type);

absl::optional<apps::AppLaunchParams> CreateSystemWebAppLaunchParams(
    Profile* profile,
    ash::SystemWebAppType app_type,
    int64_t display_id);

// Additional parameters to control LaunchSystemAppAsync behaviors.
struct SystemAppLaunchParams {
  SystemAppLaunchParams();
  ~SystemAppLaunchParams();

  // If provided launches System Apps into |url|, instead of its start_url (as
  // specified its WebAppInstallInfo). Mutually exclusive with non-empty
  // |launch_paths|.
  absl::optional<GURL> url;

  // Where the app is launched from.
  apps::mojom::LaunchSource launch_source =
      apps::mojom::LaunchSource::kFromChromeInternal;

  // If non-empty, specifies files passed to Web File Handling. Apps need to
  // have "FileHandling" origin trial in its SystemAppInfo, and file handlers
  // in its WebAppInstallInfo. Mutually exclusive with |url|.
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
// In tests, remember to call FlushSystemWebAppLaunchesForTesting on the same
// |profile|, or use content::TestNavigationObserver to wait the navigation.
void LaunchSystemWebAppAsync(
    Profile* profile,
    ash::SystemWebAppType type,
    const SystemAppLaunchParams& params = SystemAppLaunchParams(),
    apps::mojom::WindowInfoPtr window_info = nullptr);

// When this method returns, it makes sure all previous LaunchSystemWebAppAsync
// calls on |profile| are processed (i.e. LaunchSystemWebAppImpl finishes
// executing). Useful for testing SWA launch behaviors.
void FlushSystemWebAppLaunchesForTesting(Profile* profile);

// Utility function to set up launch files and launch directory as appropriate.
void SetLaunchFiles(bool should_include_launch_directory,
                    const apps::AppLaunchParams& params,
                    content::WebContents* web_contents,
                    WebAppProvider* provider);

// Implementation of LaunchSystemWebApp. Do not use this before discussing your
// use case with the System Web Apps team.
//
// This method returns `nullptr` if the app aborts the launch (e.g. delaying the
// launch after some async operation).
Browser* LaunchSystemWebAppImpl(Profile* profile,
                                ash::SystemWebAppType type,
                                const GURL& url,
                                const apps::AppLaunchParams& params);

// Returns a browser that is hosting the given system |app_type|,
// |browser_type| and |url| (if not empty) or nullptr if not found.
Browser* FindSystemWebAppBrowser(Profile* profile,
                                 ash::SystemWebAppType app_type,
                                 Browser::Type browser_type = Browser::TYPE_APP,
                                 const GURL& url = GURL());

// Returns true if the |browser| is a system web app.
bool IsSystemWebApp(Browser* browser);

// Returns the ash::SystemWebAppType that should capture the |url|.
absl::optional<ash::SystemWebAppType> GetCapturingSystemAppForURL(
    Profile* profile,
    const GURL& url);

// Returns whether the |browser| hosts the system app |type|.
bool IsBrowserForSystemWebApp(Browser* browser, ash::SystemWebAppType type);

// Returns the minimum window size for a system web app, or an empty size if
// the app does not specify a minimum size.
gfx::Size GetSystemWebAppMinimumWindowSize(Browser* browser);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_SYSTEM_WEB_APP_UI_UTILS_H_
