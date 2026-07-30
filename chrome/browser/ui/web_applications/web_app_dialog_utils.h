// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class Browser;
class BrowserWindowInterface;
class Profile;

namespace content {
class Page;
class WebContents;
}

namespace webapps {
enum class WebappInstallSource;
enum class InstallResultCode;
}  // namespace webapps

namespace web_app {

enum class WebAppInstallFlow;

// Returns whether a WebApp installation is allowed for the current page.
bool CanCreateWebApp(Browser* browser);

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
// a promotional banner or omnibox install icon. Returns false without starting
// an install on any early-return path (no WebAppProvider, an install already in
// progress, an install command already running, or no AppBannerManager). If
// this function returns false, |installed_callback| runs synchronously before
// returning. |iph_state| indicates whether or not in-product-help prompted this
// call.
bool CreateWebAppFromManifest(
    content::WebContents* web_contents,
    webapps::WebappInstallSource install_source,
    WebAppInstalledCallback installed_callback,
    PwaInProductHelpState iph_state = PwaInProductHelpState::kNotShown);

// Starts the background install of a WebApp at `install_url`, initiated from a
// `navigator.install` call from within `initiating_web_contents`. This must be
// called from a context where `WebAppProvider` exists and is supported.
// Used for the Web Install API.
void CreateWebAppForBackgroundInstall(
    content::WebContents* initiating_web_contents,
    std::unique_ptr<webapps::MlInstallOperationTracker> tracker,
    const GURL& install_url,
    const std::optional<GURL>& manifest_id,
    const GURL& last_committed_url,
    WebAppInstalledCallback installed_callback);

// Starts the background install of a WebApp using a pre-parsed manifest,
// initiated from a `navigator.install({manifest_url})` call from within
// `initiating_web_contents`. Used for the Web Install API manifest_url flow.
void CreateWebAppForManifestInstall(
    content::WebContents* initiating_web_contents,
    base::WeakPtr<content::Page> initiating_page,
    std::unique_ptr<webapps::MlInstallOperationTracker> tracker,
    blink::mojom::ManifestPtr manifest,
    const GURL& manifest_url,
    const GURL& requesting_page_url,
    WebAppInstalledCallback installed_callback);

// Shows the PWA Install dialog for the active tab in the provided browser.
// Records PWAInstallIcon user metric and closes the PWA install IPH
// if it is showing.
void ShowPwaInstallDialog(BrowserWindowInterface* bwi);

void SetInstalledCallbackForTesting(WebAppInstalledCallback callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_
