// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_SYSTEM_WEB_APP_UI_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_SYSTEM_WEB_APP_UI_UTILS_H_

#include <utility>

#include "base/optional.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

// Returns the system app type for the given App ID.
base::Optional<SystemAppType> GetSystemWebAppTypeForAppId(Profile* profile,
                                                          AppId app_id);

// Returns the PWA system App ID for the given system app type.
base::Optional<AppId> GetAppIdForSystemWebApp(Profile* profile,
                                              SystemAppType app_type);

base::Optional<apps::AppLaunchParams> CreateSystemWebAppLaunchParams(
    Profile* profile,
    SystemAppType app_type,
    int64_t display_id);

// Launches a System App to the given URL, reusing any existing window for the
// app. Returns the browser for the System App, or nullptr if launch/focus
// failed. CreateSystemWebAppLaunchParams() can be used to create |params|.
// |did_create| will reflect whether a new window was created if passed.
Browser* LaunchSystemWebApp(
    Profile* profile,
    SystemAppType app_type,
    const GURL& url,
    base::Optional<apps::AppLaunchParams> params = base::nullopt,
    bool* did_create = nullptr);

// Returns a browser that is hosting the given system app type and browser type,
// or nullptr if not found.
Browser* FindSystemWebAppBrowser(
    Profile* profile,
    SystemAppType app_type,
    Browser::Type browser_type = Browser::TYPE_APP);

// Returns true if the |browser| is a system web app.
bool IsSystemWebApp(Browser* browser);

// Returns the SystemAppType that should capture the |url|.
base::Optional<SystemAppType> GetCapturingSystemAppForURL(Profile* profile,
                                                          const GURL& url);

// Returns whether the |browser| hosts the system app |type|.
bool IsBrowserForSystemWebApp(Browser* browser, SystemAppType type);

// Returns the minimum window size for a system web app, or an empty size if
// the app does not specify a minimum size.
gfx::Size GetSystemWebAppMinimumWindowSize(Browser* browser);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_SYSTEM_WEB_APP_UI_UTILS_H_
