// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/webapps_client_android.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_install_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/common/url_constants.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/android/app_banner_manager_android.h"
#include "components/webapps/browser/android/webapps_utils.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace webapps {

// static
void WebappsClientAndroid::CreateSingleton() {
  static base::NoDestructor<WebappsClientAndroid> instance;
  instance.get();
}

WebappInstallSource WebappsClientAndroid::GetInstallSource(
    content::WebContents* web_contents,
    InstallTrigger trigger) {
  auto* delegate = static_cast<android::TabWebContentsDelegateAndroid*>(
      web_contents->GetDelegate());
  bool is_custom_tab = delegate->IsCustomTab();

  switch (trigger) {
    case InstallTrigger::AMBIENT_BADGE:
      return is_custom_tab ? WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB
                           : WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB;
    case InstallTrigger::API:
      return is_custom_tab ? WebappInstallSource::API_CUSTOM_TAB
                           : WebappInstallSource::API_BROWSER_TAB;
    case InstallTrigger::AUTOMATIC_PROMPT:
      return is_custom_tab ? WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB
                           : WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB;
    case InstallTrigger::MENU:
      return is_custom_tab ? WebappInstallSource::MENU_CUSTOM_TAB
                           : WebappInstallSource::MENU_BROWSER_TAB;
    // Create shortcut does not exist on Android, so it doesn't apply to custom
    // tab.
    case InstallTrigger::CREATE_SHORTCUT:
      DCHECK(!is_custom_tab);
      return WebappInstallSource::MENU_CREATE_SHORTCUT;
  }
  NOTREACHED_IN_MIGRATION();
  return WebappInstallSource::COUNT;
}

AppBannerManager* WebappsClientAndroid::GetAppBannerManager(
    content::WebContents* web_contents) {
  return AppBannerManagerAndroid::FromWebContents(web_contents);
}

bool WebappsClientAndroid::DoesNewWebAppConflictWithExistingInstallation(
    content::BrowserContext* browsing_context,
    const GURL& start_url,
    const ManifestId& manifest_id) const {
  // Also check if a WebAPK is currently being installed. Installation may take
  // some time, so ensure we don't accidentally allow a new installation whilst
  // one is in flight for the current site.
  return WebappsUtils::IsWebApkInstalled(browsing_context, start_url) ||
         IsInstallationInProgress(browsing_context, manifest_id);
}

bool WebappsClientAndroid::IsInAppBrowsingContext(
    content::WebContents* web_contents) const {
  return false;
}
bool WebappsClientAndroid::IsAppPartiallyInstalledForSiteUrl(
    content::BrowserContext* browsing_context,
    const GURL& site_url) const {
  return false;
}
bool WebappsClientAndroid::IsAppFullyInstalledForSiteUrl(
    content::BrowserContext* browsing_context,
    const GURL& site_url) const {
  return false;
}
bool WebappsClientAndroid::IsUrlControlledBySeenManifest(
    content::BrowserContext* browsing_context,
    const GURL& site_url) const {
  return false;
}

void WebappsClientAndroid::OnManifestSeen(
    content::BrowserContext* browsing_context,
    const blink::mojom::Manifest& manifest) const {}

void WebappsClientAndroid::SaveInstallationDismissedForMl(
    content::BrowserContext* browsing_context,
    const GURL& manifest_id) const {
}
void WebappsClientAndroid::SaveInstallationIgnoredForMl(
    content::BrowserContext* browsing_context,
    const GURL& manifest_id) const {
}
void WebappsClientAndroid::SaveInstallationAcceptedForMl(
    content::BrowserContext* browsing_context,
    const GURL& manifest_id) const {
}
bool WebappsClientAndroid::IsMlPromotionBlockedByHistoryGuardrail(
    content::BrowserContext* browsing_context,
    const GURL& manifest_id) const {
  return false;
}

segmentation_platform::SegmentationPlatformService*
WebappsClientAndroid::GetSegmentationPlatformService(
    content::BrowserContext* browsing_context) const {
  // TODO(crbug.com/40269982): Implement.
  // Note: By returning a non-nullptr, all of the Ml code (after metrics
  // gathering) in `MlInstallabilityPromoter` will execute, including requesting
  // classifiction & eventually calling `OnMlInstallPrediction` above. Make sure
  // that the contract of that class is being followed appropriately, and the ML
  // parts are correct.
  return nullptr;
}

bool WebappsClientAndroid::IsInstallationInProgress(
    content::WebContents* web_contents,
    const GURL& manifest_id) {
  return IsInstallationInProgress(web_contents->GetBrowserContext(),
                                  manifest_id);
}

bool WebappsClientAndroid::CanShowAppBanners(
    const content::WebContents* web_contents) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  return tab && static_cast<android::TabWebContentsDelegateAndroid*>(
                    tab->web_contents()->GetDelegate())
                    ->CanShowAppBanners();
}

void WebappsClientAndroid::OnWebApkInstallInitiatedFromAppMenu(
    content::WebContents* web_contents) {
  DVLOG(2) << "Sending event: IPH used for Installing PWA";
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  tracker->NotifyEvent(feature_engagement::events::kPwaInstallMenuSelected);
}

void WebappsClientAndroid::InstallWebApk(content::WebContents* web_contents,
                                         const AddToHomescreenParams& params) {
  WebApkInstallServiceFactory::GetForBrowserContext(
      web_contents->GetBrowserContext())
      ->InstallAsync(web_contents, *(params.shortcut_info), params.primary_icon,
                     params.install_source);
}

void WebappsClientAndroid::InstallShortcut(
    content::WebContents* web_contents,
    const AddToHomescreenParams& params) {
  ShortcutHelper::AddToLauncherWithSkBitmap(
      web_contents, *(params.shortcut_info), params.primary_icon,
      params.installable_status);
}

bool WebappsClientAndroid::IsInstallationInProgress(
    content::BrowserContext* browser_context,
    const GURL& manifest_id) const {
  return WebApkInstallServiceFactory::GetForBrowserContext(browser_context)
      ->IsInstallInProgress(manifest_id);
}

}  // namespace webapps
