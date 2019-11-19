// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"

namespace web_app {

namespace {

void WebAppInstallDialogCallback(
    WebappInstallSource install_source,
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    InstallManager::WebAppInstallationAcceptanceCallback
        web_app_acceptance_callback) {
  DCHECK(web_app_info);
  if (for_installable_site == ForInstallableSite::kYes) {
    web_app_info->open_as_window = true;
    chrome::ShowPWAInstallBubble(initiator_web_contents,
                                 std::move(web_app_info),
                                 std::move(web_app_acceptance_callback));
  } else {
    chrome::ShowBookmarkAppDialog(initiator_web_contents,
                                  std::move(web_app_info),
                                  std::move(web_app_acceptance_callback));
  }
}

WebAppInstalledCallback& GetInstalledCallbackForTesting() {
  static base::NoDestructor<WebAppInstalledCallback> instance;
  return *instance;
}

void OnWebAppInstalled(WebAppInstalledCallback callback,
                       const AppId& installed_app_id,
                       InstallResultCode code) {
  if (GetInstalledCallbackForTesting())
    std::move(GetInstalledCallbackForTesting()).Run(installed_app_id, code);

  std::move(callback).Run(installed_app_id, code);
}

}  // namespace

bool CanCreateWebApp(const Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider)
    return false;

  return provider->install_manager().CanInstallWebApp(web_contents);
}

void CreateWebAppFromCurrentWebContents(
    Browser* browser,
    bool force_shortcut_app,
    WebAppInstalledCallback installed_callback) {
  DCHECK(CanCreateWebApp(browser));

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  DCHECK(provider);

  WebappInstallSource install_source =
      InstallableMetrics::GetInstallSource(web_contents, InstallTrigger::MENU);

  provider->install_manager().InstallWebAppFromManifestWithFallback(
      web_contents, force_shortcut_app, install_source,
      base::BindOnce(WebAppInstallDialogCallback, install_source),
      base::BindOnce(OnWebAppInstalled, std::move(installed_callback)));
}

bool CreateWebAppFromManifest(content::WebContents* web_contents,
                              WebappInstallSource install_source,
                              WebAppInstalledCallback installed_callback) {
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider)
    return false;

  provider->install_manager().InstallWebAppFromManifest(
      web_contents, install_source,
      base::BindOnce(WebAppInstallDialogCallback, install_source),
      base::BindOnce(OnWebAppInstalled, std::move(installed_callback)));
  return true;
}

void SetInstalledCallbackForTesting(WebAppInstalledCallback callback) {
  GetInstalledCallbackForTesting() = std::move(callback);
}

}  // namespace web_app
