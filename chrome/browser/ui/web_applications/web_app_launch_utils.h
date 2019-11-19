// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_

#include <string>

#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

class Browser;

namespace content {
class WebContents;
}

namespace web_app {

base::Optional<AppId> GetPwaForSecureActiveTab(Browser* browser);

// Reparents the active tab into a new app browser for the web app that has the
// tab's URL in its scope. Does nothing if the tab is not secure or there is no
// applicable web app.
Browser* ReparentWebAppForSecureActiveTab(Browser* browser);

// Reparents |contents| into a new app browser for |app_id|.
Browser* ReparentWebContentsIntoAppBrowser(content::WebContents* contents,
                                           const AppId& app_id);

// Reparents contents to a new app browser when entering the Focus Mode.
Browser* ReparentWebContentsForFocusMode(content::WebContents* contents);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_
