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
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/navigation_entry.h"

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
    chrome::ShowWebAppInstallDialog(initiator_web_contents,
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
  // Check whether user is allowed to install web app.
  if (!WebAppProvider::Get(browser->profile()) ||
      !AreWebAppsUserInstallable(browser->profile()))
    return false;

  // Check whether we're able to install the current page as an app.
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!IsValidWebAppUrl(web_contents->GetLastCommittedURL()) ||
      web_contents->IsCrashed())
    return false;
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry && entry->GetPageType() == content::PAGE_TYPE_ERROR)
    return false;

  // Check whether the app is externally installed.
  banners::AppBannerManager* app_banner_manager =
      banners::AppBannerManager::FromWebContents(web_contents);

  if (app_banner_manager && app_banner_manager->IsExternallyInstalledWebApp())
    return false;

  return true;
}

bool CanPopOutWebApp(Profile* profile) {
  return AreWebAppsEnabled(profile) && !profile->IsGuestSession() &&
         !profile->IsOffTheRecord();
}

void CreateWebAppFromCurrentWebContents(Browser* browser,
                                        bool force_shortcut_app) {
  DCHECK(CanCreateWebApp(browser));

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  DCHECK(provider);

  WebappInstallSource install_source = InstallableMetrics::GetInstallSource(
      web_contents, force_shortcut_app ? InstallTrigger::CREATE_SHORTCUT
                                       : InstallTrigger::MENU);

  WebAppInstalledCallback callback = base::DoNothing();

  provider->install_manager().InstallWebAppFromManifestWithFallback(
      web_contents, force_shortcut_app, install_source,
      base::BindOnce(WebAppInstallDialogCallback, install_source),
      base::BindOnce(OnWebAppInstalled, std::move(callback)));
}

bool CreateWebAppFromManifest(content::WebContents* web_contents,
                              bool bypass_service_worker_check,
                              WebappInstallSource install_source,
                              WebAppInstalledCallback installed_callback) {
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider)
    return false;

  provider->install_manager().InstallWebAppFromManifest(
      web_contents, bypass_service_worker_check, install_source,
      base::BindOnce(WebAppInstallDialogCallback, install_source),
      base::BindOnce(OnWebAppInstalled, std::move(installed_callback)));
  return true;
}

void SetInstalledCallbackForTesting(WebAppInstalledCallback callback) {
  GetInstalledCallbackForTesting() = std::move(callback);
}

}  // namespace web_app
