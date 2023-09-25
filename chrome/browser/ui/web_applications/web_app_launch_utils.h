// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/common/web_app_id.h"
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

absl::optional<webapps::AppId> GetWebAppForActiveTab(const Browser* browser);

// Clears navigation history prior to user entering app scope.
void PrunePreScopeNavigationHistory(const GURL& scope,
                                    content::WebContents* contents);

// Invokes ReparentWebContentsIntoAppBrowser() for the active tab for the
// web app that has the tab's URL in its scope. Does nothing if there is no web
// app in scope.
Browser* ReparentWebAppForActiveTab(Browser* browser);

// Reparents |contents| into a standalone web app window for |app_id|.
// - If the web app has a launch_handler set to reuse existing windows and there
// are existing web app windows around this will launch the web app into the
// existing window and close |contents|.
// - If the web app is in experimental tabbed mode and has and existing web app
// window, |contents| will be reparented into the existing window.
// - Otherwise a new browser window is created for |contents| to be reparented
// into.
Browser* ReparentWebContentsIntoAppBrowser(content::WebContents* contents,
                                           const webapps::AppId& app_id);

// Tags `contents` with the given app id and marks it as an app. This
// differentiates it from a `WebContents` which happens to be hosting a page
// that is part of an app.
void SetWebContentsActingAsApp(content::WebContents* contents,
                               const webapps::AppId& app_id);

// Marks the web contents as being the pinned home tab of a tabbed web app.
void SetWebContentsIsPinnedHomeTab(content::WebContents* contents);

// Set preferences that are unique to app windows.
void SetAppPrefsForWebContents(content::WebContents* web_contents);

// Clear preferences that are unique to app windows.
void ClearAppPrefsForWebContents(content::WebContents* web_contents);

std::unique_ptr<AppBrowserController> MaybeCreateAppBrowserController(
    Browser* browser);

void MaybeAddPinnedHomeTab(Browser* browser, const std::string& app_id);

Browser* CreateWebApplicationWindow(Profile* profile,
                                    const std::string& app_id,
                                    WindowOpenDisposition disposition,
                                    int32_t restore_id,
                                    bool omit_from_session_restore = false,
                                    bool can_resize = true,
                                    bool can_maximize = true,
                                    bool can_fullscreen = true,
                                    bool is_system_web_app = false,
                                    gfx::Rect initial_bounds = gfx::Rect());

content::WebContents* NavigateWebApplicationWindow(
    Browser* browser,
    const std::string& app_id,
    const GURL& url,
    WindowOpenDisposition disposition);

content::WebContents* NavigateWebAppUsingParams(const std::string& app_id,
                                                NavigateParams& nav_params);

// RecordLaunchMetrics methods report UMA metrics. It shouldn't have other
// side-effects (e.g. updating app launch time).
void RecordLaunchMetrics(const webapps::AppId& app_id,
                         apps::LaunchContainer container,
                         apps::LaunchSource launch_source,
                         const GURL& launch_url,
                         content::WebContents* web_contents);

// Updates statistics about web app launch. For example, app's last launch time
// (populates recently launched app list) and site engagement stats.
void UpdateLaunchStats(content::WebContents* web_contents,
                       const webapps::AppId& app_id,
                       const GURL& launch_url);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_
