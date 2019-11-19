// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_SYSTEM_WEB_APP_UI_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_SYSTEM_WEB_APP_UI_UTILS_H_

#include <utility>

#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace content {
class WebUIDataSource;
}

namespace web_app {

// Returns the system app type for the given App ID.
base::Optional<SystemAppType> GetSystemWebAppTypeForAppId(Profile* profile,
                                                          AppId app_id);

// Returns the PWA system App ID for the given system app type.
base::Optional<AppId> GetAppIdForSystemWebApp(Profile* profile,
                                              SystemAppType app_type);

// Launches a System App to the given URL, reusing any existing window for the
// app. Returns the browser for the System App, or nullptr if launch/focus
// failed. |did_create| will reflect whether a new window was created if passed.
Browser* LaunchSystemWebApp(Profile* profile,
                            SystemAppType app_type,
                            const GURL& url = GURL(),
                            bool* did_create = nullptr);

// Returns a browser that is hosting the given system app type, or nullptr if
// not found.
Browser* FindSystemWebAppBrowser(Profile* profile, SystemAppType app_type);

// Returns true if the |browser| is a system web app.
bool IsSystemWebApp(Browser* browser);

// Returns the minimum window size for a system web app, or an empty size if
// the app does not specify a minimum size.
gfx::Size GetSystemWebAppMinimumWindowSize(Browser* browser);

// Calls |source->SetRequestFilter()| to set up respones to requests for
// "manifest.json" while replacing $i18nRaw{name} in the contents indiciated by
// |manifest_idr| with the name from |name_ids|.
void SetManifestRequestFilter(content::WebUIDataSource* source,
                              int manifest_idr,
                              int name_ids);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_SYSTEM_WEB_APP_UI_UTILS_H_
