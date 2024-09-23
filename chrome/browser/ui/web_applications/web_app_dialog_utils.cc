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
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
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
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "content/public/browser/navigation_entry.h"

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/40147906): Enable gn check once it handles conditional
// includes
#include "components/metrics/structured/structured_events.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_client.h"  // nogncheck
#endif

namespace web_app {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
namespace cros_events = metrics::structured::events::v2::cr_os_events;
#endif

void OnWebAppInstallShowInstallDialog(
    WebAppInstallFlow flow,
    webapps::WebappInstallSource install_source,
    PwaInProductHelpState iph_state,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    std::vector<webapps::Screenshot> screenshots,
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceCallback web_app_acceptance_callback) {
  DCHECK(web_app_info);

  switch (flow) {
    case WebAppInstallFlow::kInstallSite:
      web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
#if BUILDFLAG(IS_CHROMEOS)
      if (install_source == webapps::WebappInstallSource::MENU_BROWSER_TAB) {
        webapps::AppId app_id =
            web_app::GenerateAppIdFromManifestId(web_app_info->manifest_id());
        metrics::structured::StructuredMetricsClient::Record(
            cros_events::AppDiscovery_Browser_ClickInstallAppFromMenu()
                .SetAppId(app_id));
      }
#endif
      if (!screenshots.empty()) {
        ShowWebAppDetailedInstallDialog(
            initiator_web_contents, std::move(web_app_info),
            std::move(install_tracker), std::move(web_app_acceptance_callback),
            std::move(screenshots), iph_state);
        return;
      } else if (base::FeatureList::IsEnabled(
                     features::kWebAppUniversalInstall) &&
                 web_app_info->is_diy_app) {
        ShowDiyAppInstallDialog(initiator_web_contents, std::move(web_app_info),
                                std::move(install_tracker),
                                std::move(web_app_acceptance_callback),
                                iph_state);
        return;
      } else {
        ShowSimpleInstallDialogForWebApps(
            initiator_web_contents, std::move(web_app_info),
            std::move(install_tracker), std::move(web_app_acceptance_callback),
            iph_state);
        return;
      }
    case WebAppInstallFlow::kCreateShortcut:
#if BUILDFLAG(IS_CHROMEOS)
    {
      webapps::AppId app_id =
          web_app::GenerateAppIdFromManifestId(web_app_info->manifest_id());
      metrics::structured::StructuredMetricsClient::Record(
          cros_events::AppDiscovery_Browser_CreateShortcut().SetAppId(app_id));
    }
#endif

      ShowCreateShortcutDialog(initiator_web_contents, std::move(web_app_info),
                               std::move(install_tracker),
                               std::move(web_app_acceptance_callback));
      return;
    case WebAppInstallFlow::kUnknown:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
}

WebAppInstalledCallback& GetInstalledCallbackForTesting() {
  static base::NoDestructor<WebAppInstalledCallback> instance;
  return *instance;
}

void OnWebAppInstalled(WebAppInstalledCallback callback,
                       const webapps::AppId& installed_app_id,
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
      web_contents->IsCrashed()) {
    return false;
  }
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

  webapps::MLInstallabilityPromoter* promoter =
      webapps::MLInstallabilityPromoter::FromWebContents(web_contents);
  CHECK(promoter);
  if (promoter->HasCurrentInstall()) {
    return;
  }

  if (provider->command_manager().IsInstallingForWebContents(web_contents)) {
    return;
  }

  webapps::AppBannerManager* app_banner_manager =
      webapps::AppBannerManager::FromWebContents(web_contents);
  if (!app_banner_manager) {
    return;
  }

  std::optional<webapps::WebAppBannerData> data =
      app_banner_manager->GetCurrentWebAppBannerData();

  webapps::WebappInstallSource install_source =
      webapps::InstallableMetrics::GetInstallSource(
          web_contents, flow == WebAppInstallFlow::kCreateShortcut
                            ? webapps::InstallTrigger::CREATE_SHORTCUT
                            : webapps::InstallTrigger::MENU);

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      promoter->RegisterCurrentInstallForWebContents(install_source);

  WebAppInstalledCallback callback = base::DoNothing();

  // Appropriately set the fallback behavior to distinguish installation of DIY
  // apps with the create shortcut flow.
  FallbackBehavior fallback_behavior =
      flow == WebAppInstallFlow::kCreateShortcut
          ? FallbackBehavior::kAllowFallbackDataAlways
          : FallbackBehavior::kUseFallbackInfoWhenNotInstallable;

  provider->scheduler().FetchManifestAndInstall(
      install_source, web_contents->GetWeakPtr(),
      base::BindOnce(OnWebAppInstallShowInstallDialog, flow, install_source,
                     PwaInProductHelpState::kNotShown,
                     std::move(install_tracker),
                     data.has_value() ? std::move(data->screenshots)
                                      : std::vector<webapps::Screenshot>()),
      base::BindOnce(OnWebAppInstalled, std::move(callback)),
      fallback_behavior);
}

bool CreateWebAppFromManifest(content::WebContents* web_contents,
                              webapps::WebappInstallSource install_source,
                              WebAppInstalledCallback installed_callback,
                              PwaInProductHelpState iph_state) {
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider)
    return false;

  webapps::MLInstallabilityPromoter* promoter =
      webapps::MLInstallabilityPromoter::FromWebContents(web_contents);
  if (promoter->HasCurrentInstall()) {
    return false;
  }

  if (provider->command_manager().IsInstallingForWebContents(web_contents)) {
    return false;
  }

  webapps::AppBannerManager* app_banner_manager =
      webapps::AppBannerManager::FromWebContents(web_contents);
  if (!app_banner_manager) {
    return false;
  }

  std::optional<webapps::WebAppBannerData> data =
      app_banner_manager->GetCurrentWebAppBannerData();

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      promoter->RegisterCurrentInstallForWebContents(install_source);

  // If the source is from ML, there may not be a manifest, so allow the command
  // to use the metadata from the page too.
  FallbackBehavior fallback_behavior =
      install_source == webapps::WebappInstallSource::ML_PROMOTION
          ? FallbackBehavior::kUseFallbackInfoWhenNotInstallable
          : FallbackBehavior::kCraftedManifestOnly;

  provider->scheduler().FetchManifestAndInstall(
      install_source, web_contents->GetWeakPtr(),
      base::BindOnce(OnWebAppInstallShowInstallDialog,
                     WebAppInstallFlow::kInstallSite, install_source, iph_state,
                     std::move(install_tracker),
                     data.has_value() ? std::move(data->screenshots)
                                      : std::vector<webapps::Screenshot>()),
      base::BindOnce(OnWebAppInstalled, std::move(installed_callback)),
      fallback_behavior);
  return true;
}

void SetInstalledCallbackForTesting(WebAppInstalledCallback callback) {
  GetInstalledCallbackForTesting() = std::move(callback);
}

}  // namespace web_app
