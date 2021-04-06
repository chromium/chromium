// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_

#include <string>

#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_id.h"

class Browser;
class GURL;

namespace content {
class WebContents;
}

namespace web_app {

base::Optional<AppId> GetWebAppForActiveTab(Browser* browser);

bool IsInScope(const GURL& url, const GURL& scope_spec);

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

// Reparents contents to a new app browser when entering the Focus Mode.
Browser* ReparentWebContentsForFocusMode(content::WebContents* contents);

// Set preferences that are unique to app windows.
void SetAppPrefsForWebContents(content::WebContents* web_contents);

// Clear preferences that are unique to app windows.
void ClearAppPrefsForWebContents(content::WebContents* web_contents);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_
