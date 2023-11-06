// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "components/webapps/common/web_app_id.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}

namespace webapps {
enum class WebappInstallSource;
enum class InstallResultCode;
}

namespace web_app {

enum class WebAppInstallFlow;

// Returns whether a WebApp installation is allowed for the current page.
bool CanCreateWebApp(const Browser* browser);

// Returns whether the current profile is allowed to pop out a web app into a
// separate window. Does not check whether any particular page can pop out.
bool CanPopOutWebApp(Profile* profile);

using WebAppInstalledCallback =
    base::OnceCallback<void(const webapps::AppId& app_id,
                            webapps::InstallResultCode code)>;

// Initiates user install of a WebApp for the current page.
void CreateWebAppFromCurrentWebContents(Browser* browser,
                                        WebAppInstallFlow flow);

// Starts install of a WebApp for a given |web_contents|, initiated from
// a promotional banner or omnibox install icon.
// Returns false if WebApps are disabled for the profile behind |web_contents|.
// |iph_state| indicates whether or not in-product-help prompted this call.
bool CreateWebAppFromManifest(
    content::WebContents* web_contents,
    webapps::WebappInstallSource install_source,
    WebAppInstalledCallback installed_callback,
    PwaInProductHelpState iph_state = PwaInProductHelpState::kNotShown);

void SetInstalledCallbackForTesting(WebAppInstalledCallback callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_
