// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "extensions/common/constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

class Profile;
class Browser;
class GURL;
enum class WindowOpenDisposition;
struct NavigateParams;

namespace content {
class WebContents;
}

namespace web_app {

class AppBrowserController;

absl::optional<AppId> GetWebAppForActiveTab(Browser* browser);

// Clears navigation history prior to user entering app scope.
void PrunePreScopeNavigationHistory(const GURL& scope,
                                    content::WebContents* contents);

// Reparents the active tab into a new app browser for the web app that has the
// tab's URL in its scope. Does nothing if there is no web app in scope.
Browser* ReparentWebAppForActiveTab(Browser* browser);

// Reparents |contents| into an app browser for |app_id|.
// Uses existing app browser if they are in experimental tabbed mode, otherwise
// creates a new browser window.
Browser* ReparentWebContentsIntoAppBrowser(content::WebContents* contents,
                                           const AppId& app_id);

// Tags `contents` with the given app id and marks it as an app. This
// differentiates it from a `WebContents` which happens to be hosting a page
// that is part of an app.
void SetWebContentsActingAsApp(content::WebContents* contents,
                               const AppId& app_id);

// Set preferences that are unique to app windows.
void SetAppPrefsForWebContents(content::WebContents* web_contents);

// Clear preferences that are unique to app windows.
void ClearAppPrefsForWebContents(content::WebContents* web_contents);

std::unique_ptr<AppBrowserController> MaybeCreateAppBrowserController(
    Browser* browser);

Browser* CreateWebApplicationWindow(Profile* profile,
                                    const std::string& app_id,
                                    WindowOpenDisposition disposition,
                                    int32_t restore_id,
                                    bool omit_from_session_restore = false,
                                    bool can_resize = true,
                                    bool can_maximize = true,
                                    gfx::Rect initial_bounds = gfx::Rect());

content::WebContents* NavigateWebApplicationWindow(
    Browser* browser,
    const std::string& app_id,
    const GURL& url,
    WindowOpenDisposition disposition);

content::WebContents* NavigateWebAppUsingParams(const std::string& app_id,
                                                NavigateParams& nav_params);

void RecordAppWindowLaunch(Profile* profile, const std::string& app_id);

void RecordMetrics(const AppId& app_id,
                   apps::LaunchContainer container,
                   extensions::AppLaunchSource launch_source,
                   const GURL& launch_url,
                   content::WebContents* web_contents);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_
