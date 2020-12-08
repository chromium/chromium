// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_

#include "base/callback_forward.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/components/web_app_id.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}

namespace webapps {
enum class WebappInstallSource;
}

namespace web_app {

enum class InstallResultCode;

// TODO(loyso): Rework these functions (API). Move all of them into
// WebAppDialogManager.

// Returns whether a WebApp installation is allowed for the current page.
bool CanCreateWebApp(const Browser* browser);

// Returns whether the current profile is allowed to pop out a web app into a
// separate window. Does not check whether any particular page can pop out.
bool CanPopOutWebApp(Profile* profile);

using WebAppInstalledCallback =
    base::OnceCallback<void(const AppId& app_id, InstallResultCode code)>;

// Initiates user install of a WebApp for the current page.
// If |force_shortcut_app| is true, the current page will be installed even if
// the site does not meet installability requirements (see
// |AppBannerManager::PerformInstallableCheck|).
void CreateWebAppFromCurrentWebContents(Browser* browser,
                                        bool force_shortcut_app);

// Starts install of a WebApp for a given |web_contents|, initiated from
// a promotional banner or omnibox install icon.
// Returns false if WebApps are disabled for the profile behind |web_contents|.
// |iph_state| indicates whether or not in-product-help prompted this call.
bool CreateWebAppFromManifest(content::WebContents* web_contents,
                              bool bypass_service_worker_check,
                              webapps::WebappInstallSource install_source,
                              WebAppInstalledCallback installed_callback,
                              chrome::PwaInProductHelpState iph_state =
                                  chrome::PwaInProductHelpState::kNotShown);

void SetInstalledCallbackForTesting(WebAppInstalledCallback callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_
