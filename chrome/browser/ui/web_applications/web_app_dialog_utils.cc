// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/navigation_entry.h"

namespace web_app {

namespace {

void OnWebAppInstallShowInstallDialog(
    WebAppInstallFlow flow,
    webapps::WebappInstallSource install_source,
    chrome::PwaInProductHelpState iph_state,
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceCallback web_app_acceptance_callback) {
  DCHECK(web_app_info);

  switch (flow) {
    case WebAppInstallFlow::kInstallSite:
      web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
      if (base::FeatureList::IsEnabled(
              webapps::features::kDesktopPWAsDetailedInstallDialog) &&
          webapps::AppBannerManager::FromWebContents(initiator_web_contents)
              ->screenshots()
              .size()) {
        chrome::ShowWebAppDetailedInstallDialog(
            initiator_web_contents, std::move(web_app_info),
            std::move(web_app_acceptance_callback),
            webapps::AppBannerManager::FromWebContents(initiator_web_contents)
                ->screenshots(),
            iph_state);
        return;
      } else {
        chrome::ShowPWAInstallBubble(
            initiator_web_contents, std::move(web_app_info),
            std::move(web_app_acceptance_callback), iph_state);
        return;
      }
    case WebAppInstallFlow::kCreateShortcut:
      chrome::ShowWebAppInstallDialog(initiator_web_contents,
                                      std::move(web_app_info),
                                      std::move(web_app_acceptance_callback));
      return;
    case WebAppInstallFlow::kUnknown:
      NOTREACHED();
  }
  NOTREACHED();
}

WebAppInstalledCallback& GetInstalledCallbackForTesting() {
  static base::NoDestructor<WebAppInstalledCallback> instance;
  return *instance;
}

void OnWebAppInstalled(WebAppInstalledCallback callback,
                       const AppId& installed_app_id,
                       webapps::InstallResultCode code) {
  if (GetInstalledCallbackForTesting())
    std::move(GetInstalledCallbackForTesting()).Run(installed_app_id, code);

  std::move(callback).Run(installed_app_id, code);
}

}  // namespace

bool CanCreateWebApp(const Browser* browser) {
  // Check whether user is allowed to install web app.
  if (!WebAppProvider::GetForWebApps(browser->profile()) ||
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

  return true;
}

bool CanPopOutWebApp(Profile* profile) {
  return AreWebAppsEnabled(profile) && !profile->IsGuestSession() &&
         !profile->IsOffTheRecord();
}

void CreateWebAppFromCurrentWebContents(Browser* browser,
                                        WebAppInstallFlow flow) {
  DCHECK(CanCreateWebApp(browser));

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  DCHECK(provider);

  if (provider->command_manager().IsInstallingForWebContents(web_contents)) {
    return;
  }

  webapps::WebappInstallSource install_source =
      webapps::InstallableMetrics::GetInstallSource(
          web_contents, flow == WebAppInstallFlow::kCreateShortcut
                            ? webapps::InstallTrigger::CREATE_SHORTCUT
                            : webapps::InstallTrigger::MENU);

  WebAppInstalledCallback callback = base::DoNothing();

  provider->scheduler().FetchManifestAndInstall(
      install_source, web_contents->GetWeakPtr(),
      /*bypass_service_worker_check=*/false,
      base::BindOnce(OnWebAppInstallShowInstallDialog, flow, install_source,
                     chrome::PwaInProductHelpState::kNotShown),
      base::BindOnce(OnWebAppInstalled, std::move(callback)),
      /*use_fallback=*/true);
}

bool CreateWebAppFromManifest(content::WebContents* web_contents,
                              bool bypass_service_worker_check,
                              webapps::WebappInstallSource install_source,
                              WebAppInstalledCallback installed_callback,
                              chrome::PwaInProductHelpState iph_state) {
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider)
    return false;

  if (provider->command_manager().IsInstallingForWebContents(web_contents)) {
    return false;
  }

  provider->scheduler().FetchManifestAndInstall(
      install_source, web_contents->GetWeakPtr(), bypass_service_worker_check,
      base::BindOnce(OnWebAppInstallShowInstallDialog,
                     WebAppInstallFlow::kInstallSite, install_source,
                     iph_state),
      base::BindOnce(OnWebAppInstalled, std::move(installed_callback)),
      /*use_fallback=*/false);
  return true;
}

void SetInstalledCallbackForTesting(WebAppInstalledCallback callback) {
  GetInstalledCallbackForTesting() = std::move(callback);
}

}  // namespace web_app
