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
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/web_apps/progress_delay.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"
#include "chrome/browser/ui/web_applications/pwa_install_page_action.h"
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
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/browser_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "components/webapps/browser/web_app_url_config.h"
#include "content/public/browser/navigation_entry.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/os_integration/mac/icon_utils.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/40147906): Enable gn check once it handles conditional
// includes
#include "components/metrics/structured/structured_events.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_client.h"  // nogncheck
#endif

namespace web_app {

namespace {

constexpr base::TimeDelta kProgressDelay = base::Seconds(2);
constexpr int kProgressDelaySteps = 100;

#if BUILDFLAG(IS_CHROMEOS)
namespace cros_events = metrics::structured::events::v2::cr_os_events;
#endif

// Helper function to show the web app installation dialog. This is called
// once the folder icon has been resolved (asynchronously on Mac, or
// immediately on other platforms).
void ShowWebAppInstallFlowDialog(
    base::WeakPtr<content::WebContents> initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    WebAppInstallationAcceptanceCallback web_app_acceptance_callback,
    PwaInProductHelpState iph_state,
    base::WeakPtr<WebAppScreenshotFetcher> screenshot_fetcher,
    bool show_initiating_origin,
    InstallDialogType install_type,
    InstallOsType os_type,
    std::optional<ui::ImageModel> folder_image_model,
    std::optional<std::u16string> folder_label) {
  if (!initiator_web_contents) {
    return;
  }
  auto progress_delay =
      std::make_unique<ProgressDelay>(kProgressDelay, kProgressDelaySteps);
  WebAppInstallFlowDialogDelegate::Show(
      initiator_web_contents.get(), std::move(web_app_info),
      std::move(install_tracker), std::move(web_app_acceptance_callback),
      iph_state, std::move(screenshot_fetcher), show_initiating_origin,
      install_type, os_type, std::move(progress_delay), folder_image_model,
      folder_label);
}

void OnWebAppInstallShowInstallDialog(
    WebAppInstallFlow flow,
    webapps::WebappInstallSource install_source,
    PwaInProductHelpState iph_state,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    bool show_initiating_origin,
    base::WeakPtr<WebAppScreenshotFetcher> screenshot_fetcher,
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceCallback web_app_acceptance_callback) {
  DCHECK(web_app_info);
  InstallOsType os_type = InstallOsType::kOther;
#if BUILDFLAG(IS_CHROMEOS)
  os_type = InstallOsType::kCros;
#endif
#if BUILDFLAG(IS_MAC)
  os_type = InstallOsType::kMac;
#endif
#if BUILDFLAG(IS_WIN)
  os_type = InstallOsType::kWin;
#endif

  switch (flow) {
    case WebAppInstallFlow::kInstallSite: {
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
      if (base::FeatureList::IsEnabled(features::kWebAppInstallDialog)) {
        InstallDialogType install_type = kSimple;
        if (screenshot_fetcher) {
          install_type = kDetailed;
        } else if (web_app_info->is_diy_app) {
          install_type = kDiy;
        }

        // Bind the standard dialog showing function. This callback accepts the
        // folder image and label, which may be computed asynchronously on Mac.
        auto show_dialog = base::BindOnce(
            &ShowWebAppInstallFlowDialog, initiator_web_contents->GetWeakPtr(),
            std::move(web_app_info), std::move(install_tracker),
            std::move(web_app_acceptance_callback), iph_state,
            screenshot_fetcher, show_initiating_origin, install_type, os_type);

#if BUILDFLAG(IS_MAC)
        // On macOS, generating the folder icon involves heavy image
        // manipulation (overlaying the Chrome Apps launcher icon onto the
        // system folder icon). To avoid blocking the UI thread, we fetch this
        // icon asynchronously on the thread pool.
        auto show_dialog_with_image = base::BindOnce(
            [](base::OnceCallback<void(std::optional<ui::ImageModel>,
                                       std::optional<std::u16string>)>
                   show_dialog_callback,
               gfx::Image folder_image) {
              std::move(show_dialog_callback)
                  .Run(ui::ImageModel::FromImage(folder_image),
                       shell_integration::GetAppShortcutsSubdirName());
            },
            std::move(show_dialog));
        GetMacAppsFolderImageAsync(kLargeImageSize,
                                   std::move(show_dialog_with_image));
#else
        std::move(show_dialog).Run(std::nullopt, std::nullopt);
#endif
        return;
      }

      // The UI methods below expect a callback that takes only 2 arguments, but
      // WebAppInstallationAcceptanceCallback now takes 3 arguments. We use this
      // adapter to pass a dummy result callback.
      auto launch_app_on_install_success =
          AdaptToLaunchOnInstallSuccess(std::move(web_app_acceptance_callback));

      if (screenshot_fetcher) {
        ShowWebAppDetailedInstallDialog(
            initiator_web_contents, std::move(web_app_info),
            std::move(install_tracker),
            std::move(launch_app_on_install_success), screenshot_fetcher,
            iph_state);
        return;
      } else if (web_app_info->is_diy_app) {
        ShowDiyAppInstallDialog(initiator_web_contents, std::move(web_app_info),
                                std::move(install_tracker),
                                std::move(launch_app_on_install_success),
                                iph_state);
        return;
      } else {
        ShowSimpleInstallDialogForWebApps(
            initiator_web_contents, std::move(web_app_info),
            std::move(install_tracker),
            std::move(launch_app_on_install_success), iph_state,
            show_initiating_origin);
        return;
      }
    }
#if BUILDFLAG(IS_CHROMEOS)
    case WebAppInstallFlow::kCreateShortcut: {
      webapps::AppId app_id =
          web_app::GenerateAppIdFromManifestId(web_app_info->manifest_id());
      metrics::structured::StructuredMetricsClient::Record(
          cros_events::AppDiscovery_Browser_CreateShortcut().SetAppId(app_id));

      auto launch_app_on_install_success =
          AdaptToLaunchOnInstallSuccess(std::move(web_app_acceptance_callback));

      ShowCreateShortcutDialog(initiator_web_contents, std::move(web_app_info),
                               std::move(install_tracker),
                               std::move(launch_app_on_install_success));
      return;
    }
#endif
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
                       const webapps::AppId& installed_app_id,
                       webapps::InstallResultCode code) {
  if (GetInstalledCallbackForTesting()) {
    std::move(GetInstalledCallbackForTesting()).Run(installed_app_id, code);
  }

  std::move(callback).Run(installed_app_id, code);
}

}  // namespace

bool CanCreateWebApp(Browser* browser) {
  // Check whether user is allowed to install web app.
  if (!WebAppProvider::GetForWebApps(browser->GetProfile()) ||
      !AreWebAppsUserInstallable(browser->GetProfile())) {
    return false;
  }

  // Check whether we're able to install the current page as an app.
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!webapps::IsUrlEligibleForWebApp(web_contents->GetLastCommittedURL()) ||
      web_contents->IsCrashed()) {
    return false;
  }
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry && entry->GetPageType() == content::PAGE_TYPE_ERROR) {
    return false;
  }

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
          web_contents,
#if BUILDFLAG(IS_CHROMEOS)
          flow == WebAppInstallFlow::kCreateShortcut
              ? webapps::InstallTrigger::CREATE_SHORTCUT
              :
#endif
              webapps::InstallTrigger::MENU);

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      promoter->RegisterCurrentInstallForWebContents(install_source);

  WebAppInstalledCallback callback = base::DoNothing();

  // Appropriately set the fallback behavior to distinguish installation of DIY
  // apps with the create shortcut flow.
  FallbackBehavior fallback_behavior =
#if BUILDFLAG(IS_CHROMEOS)
      flow == WebAppInstallFlow::kCreateShortcut
          ? FallbackBehavior::kAllowFallbackDataAlways
          :
#endif
          FallbackBehavior::kUseFallbackInfoWhenNotInstallable;

  provider->scheduler().FetchManifestAndInstall(
      install_source, web_contents->GetWeakPtr(),
      base::BindOnce(OnWebAppInstallShowInstallDialog, flow, install_source,
                     PwaInProductHelpState::kNotShown,
                     std::move(install_tracker),
                     /*show_initiating_origin=*/false),
      base::BindOnce(OnWebAppInstalled, std::move(callback)),
      fallback_behavior);
}

bool CreateWebAppFromManifest(content::WebContents* web_contents,
                              webapps::WebappInstallSource install_source,
                              WebAppInstalledCallback installed_callback,
                              PwaInProductHelpState iph_state) {
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider) {
    return false;
  }

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
                     /*show_initiating_origin=*/false),
      base::BindOnce(OnWebAppInstalled, std::move(installed_callback)),
      fallback_behavior);
  return true;
}

void CreateWebAppForBackgroundInstall(
    content::WebContents* initiating_web_contents,
    std::unique_ptr<webapps::MlInstallOperationTracker> tracker,
    const GURL& install_url,
    const std::optional<GURL>& manifest_id,
    const GURL& last_committed_url,
    WebAppInstalledCallback installed_callback) {
  auto* provider = WebAppProvider::GetForWebContents(initiating_web_contents);
  CHECK(provider);

  provider->scheduler().InstallAppFromUrl(
      install_url, manifest_id, initiating_web_contents->GetWeakPtr(),
      last_committed_url,
      base::BindOnce(&OnWebAppInstallShowInstallDialog,
                     WebAppInstallFlow::kInstallSite,
                     webapps::WebappInstallSource::WEB_INSTALL,
                     PwaInProductHelpState::kNotShown, std::move(tracker),
                     /*show_initiating_origin=*/true),
      std::move(installed_callback));
}

void CreateWebAppForManifestInstall(
    content::WebContents* initiating_web_contents,
    base::WeakPtr<content::Page> initiating_page,
    std::unique_ptr<webapps::MlInstallOperationTracker> tracker,
    blink::mojom::ManifestPtr manifest,
    const GURL& manifest_url,
    const GURL& requesting_page_url,
    WebAppInstalledCallback installed_callback) {
  auto* provider = WebAppProvider::GetForWebContents(initiating_web_contents);
  CHECK(provider);

  provider->scheduler().InstallAppFromManifest(
      std::move(manifest), manifest_url, initiating_web_contents->GetWeakPtr(),
      std::move(initiating_page), requesting_page_url,
      base::BindOnce(&OnWebAppInstallShowInstallDialog,
                     WebAppInstallFlow::kInstallSite,
                     webapps::WebappInstallSource::WEB_INSTALL,
                     PwaInProductHelpState::kNotShown, std::move(tracker),
                     /*show_initiating_origin=*/true),
      std::move(installed_callback));
}

void ShowPwaInstallDialog(BrowserWindowInterface* bwi) {
  CHECK(bwi);

  base::RecordAction(base::UserMetricsAction("PWAInstallIcon"));

  content::WebContents* const web_contents =
      bwi->GetTabStripModel()->GetActiveWebContents();
  CHECK(web_contents);

  PwaInstallPageActionController* const pwa_install_controller =
      bwi->GetActiveTabInterface()
          ->GetTabFeatures()
          ->pwa_install_page_action_controller();
  pwa_install_controller->SetIsExecuting(true);

  // Close PWA install IPH if it is showing.
  PwaInProductHelpState iph_state = PwaInProductHelpState::kNotShown;
  const bool install_icon_clicked_after_iph_shown =
      BrowserUserEducationInterface::From(bwi)->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHDesktopPwaInstallFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  if (install_icon_clicked_after_iph_shown) {
    iph_state = PwaInProductHelpState::kShown;
  }

#if BUILDFLAG(IS_CHROMEOS)
  metrics::structured::StructuredMetricsClient::Record(
      metrics::structured::events::v2::cr_os_events::
          AppDiscovery_Browser_OmniboxInstallIconClicked()
              .SetIPHShown(install_icon_clicked_after_iph_shown));
#endif

  CreateWebAppFromManifest(web_contents,
                           webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
                           base::DoNothing(), iph_state);
  pwa_install_controller->SetIsExecuting(false);
}

void SetInstalledCallbackForTesting(WebAppInstalledCallback callback) {
  GetInstalledCallbackForTesting() = std::move(callback);
}

}  // namespace web_app
