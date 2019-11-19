// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_

#include "base/callback_forward.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

enum class WebappInstallSource;
class Browser;

namespace content {
class WebContents;
}

namespace web_app {

enum class InstallResultCode;

// TODO(loyso): Rework these functions (API). Move all of them into
// WebAppDialogManager.

// Returns true if a WebApp installation is allowed for the current page.
bool CanCreateWebApp(const Browser* browser);

using WebAppInstalledCallback =
    base::OnceCallback<void(const AppId& app_id, InstallResultCode code)>;

// Initiates install of a WebApp for the current page.
void CreateWebAppFromCurrentWebContents(
    Browser* browser,
    bool force_shortcut_app,
    WebAppInstalledCallback installed_callback);

// Starts install of a WebApp for a given |web_contents|, initiated from
// a promotional banner or omnibox install icon.
// Returns false if WebApps are disabled for the profile behind |web_contents|.
bool CreateWebAppFromManifest(content::WebContents* web_contents,
                              WebappInstallSource install_source,
                              WebAppInstalledCallback installed_callback);

void SetInstalledCallbackForTesting(WebAppInstalledCallback callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_
