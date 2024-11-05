// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/webapps_client_desktop.h"

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/browser/web_applications/visited_manifest_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/url_constants.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace webapps {

// static
void WebappsClientDesktop::CreateSingleton() {
  static base::NoDestructor<WebappsClientDesktop> instance;
  instance.get();
}

WebappInstallSource WebappsClientDesktop::GetInstallSource(
    content::WebContents* web_contents,
    InstallTrigger trigger) {
  switch (trigger) {
    case InstallTrigger::AMBIENT_BADGE:
      return WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB;
    case InstallTrigger::API:
      return WebappInstallSource::API_BROWSER_TAB;
    case InstallTrigger::AUTOMATIC_PROMPT:
      return WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB;
    case InstallTrigger::MENU:
      return WebappInstallSource::MENU_BROWSER_TAB;
    case InstallTrigger::CREATE_SHORTCUT:
      return WebappInstallSource::MENU_CREATE_SHORTCUT;
  }
}

AppBannerManager* WebappsClientDesktop::GetAppBannerManager(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  return AppBannerManagerDesktop::FromWebContents(web_contents);
}

bool WebappsClientDesktop::DoesNewWebAppConflictWithExistingInstallation(
    content::BrowserContext* browser_context,
    const GURL& start_url,
    const ManifestId& manifest_id) const {
  CHECK(browser_context);

  // We prompt the user to re-install if the site wants to be in a
  // standalone window but the user has opted for opening in browser tab. This
  // is to support the situation where a site is not a PWA, users have installed
  // it via Create Shortcut action, the site becomes a standalone PWA later and
  // we want to prompt them to "install" the new PWA experience.
  // TODO(crbug.com/40180519): Showing an install button when it's already
  // installed is confusing. Perhaps different UX would be best.

  Profile* profile = Profile::FromBrowserContext(browser_context);
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);

  // We can install if it's not installed, or this is crafted app and already
  // installed but opens in a tab.
  std::optional<web_app::mojom::UserDisplayMode> user_display_mode =
      provider->registrar_unsafe().GetAppUserDisplayMode(
          web_app::GenerateAppIdFromManifestId(manifest_id));
  if (user_display_mode == web_app::mojom::UserDisplayMode::kBrowser) {
    return false;
  }

  // We cannot install if we are in scope of an installed crafted app, no matter
  // the user display type.
  std::optional<AppId> non_diy_app_id =
      provider->registrar_unsafe().FindInstalledAppWithUrlInScope(
          start_url,
          /*window_only=*/false, /*exclude_diy_apps=*/true);
  if (non_diy_app_id) {
    return true;
  }
  // Otherwise there is no app installed here, or there is a DIY app that
  // controls this URL but that's fine.
  return false;
}

bool WebappsClientDesktop::IsInAppBrowsingContext(
    content::WebContents* web_contents) const {
  CHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider) {
    return false;
  }
  return provider->ui_manager().IsInAppWindow(web_contents);
}

bool WebappsClientDesktop::IsAppPartiallyInstalledForSiteUrl(
    content::BrowserContext* browser_context,
    const GURL& site_url) const {
  CHECK(browser_context);
  return web_app::IsNonLocallyInstalledAppWithUrlInScope(
      Profile::FromBrowserContext(browser_context), site_url);
}

bool WebappsClientDesktop::IsAppFullyInstalledForSiteUrl(
    content::BrowserContext* browser_context,
    const GURL& site_url) const {
  CHECK(browser_context);
  return web_app::FindInstalledAppWithUrlInScope(
             Profile::FromBrowserContext(browser_context), site_url)
      .has_value();
}

bool WebappsClientDesktop::IsUrlControlledBySeenManifest(
    content::BrowserContext* browsing_context,
    const GURL& site_url) const {
  auto* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(browsing_context));
  return provider && provider->is_registry_ready()
             ? provider->visited_manifest_manager()
                   .IsUrlControlledBySeenManifest(site_url)
             : false;
}

void WebappsClientDesktop::OnManifestSeen(
    content::BrowserContext* browsing_context,
    const blink::mojom::Manifest& manifest) const {
  auto* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(browsing_context));
  if (provider && provider->is_registry_ready()) {
    provider->visited_manifest_manager().OnManifestSeen(manifest);
  }
}

void WebappsClientDesktop::SaveInstallationDismissedForMl(
    content::BrowserContext* browser_context,
    const GURL& manifest_id) const {
  CHECK(browser_context);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  CHECK(profile);
  web_app::WebAppPrefGuardrails::GetForMlInstallPrompt(profile->GetPrefs())
      .RecordDismiss(web_app::GenerateAppIdFromManifestId(manifest_id),
                     base::Time::Now());
}

void WebappsClientDesktop::SaveInstallationIgnoredForMl(
    content::BrowserContext* browser_context,
    const GURL& manifest_id) const {
  CHECK(browser_context);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  CHECK(profile);
  web_app::WebAppPrefGuardrails::GetForMlInstallPrompt(profile->GetPrefs())
      .RecordIgnore(web_app::GenerateAppIdFromManifestId(manifest_id),
                    base::Time::Now());
}

void WebappsClientDesktop::SaveInstallationAcceptedForMl(
    content::BrowserContext* browser_context,
    const GURL& manifest_id) const {
  CHECK(browser_context);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  CHECK(profile);
  web_app::WebAppPrefGuardrails::GetForMlInstallPrompt(profile->GetPrefs())
      .RecordAccept(web_app::GenerateAppIdFromManifestId(manifest_id));
}

bool WebappsClientDesktop::IsMlPromotionBlockedByHistoryGuardrail(
    content::BrowserContext* browser_context,
    const GURL& manifest_id) const {
  CHECK(browser_context);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  CHECK(profile);

  if (!manifest_id.is_empty() &&
      web_app::WebAppPrefGuardrails::GetForMlInstallPrompt(profile->GetPrefs())
          .IsBlockedByGuardrails(
              web_app::GenerateAppIdFromManifestId(manifest_id))) {
    return true;
  }

  // Do not copy this. This is a temporary hack to help not bother users before
  // the ML triggering is moved to triggering IPH for a new install entry point.
  // crbug.com/356401517
  UserEducationService* const user_education_service =
      UserEducationServiceFactory::GetForBrowserContext(profile);
  user_education::FeaturePromoResult synthetic_result =
      user_education_service->feature_promo_session_policy().CanShowPromo(
          {.weight =
               user_education::FeaturePromoSessionPolicy::PromoWeight::kHeavy,
           .priority =
               user_education::FeaturePromoSessionPolicy::PromoPriority::kLow},
          /*currently_showing=*/std::nullopt);
  return synthetic_result.failure() ==
         user_education::FeaturePromoResult::kBlockedByGracePeriod;
}

segmentation_platform::SegmentationPlatformService*
WebappsClientDesktop::GetSegmentationPlatformService(
    content::BrowserContext* browser_context) const {
  if (segmentation_platform_for_testing()) {  // IN-TEST
    CHECK_IS_TEST();
    return segmentation_platform_for_testing();  // IN-TEST
  }
  CHECK(browser_context);
  return segmentation_platform::SegmentationPlatformServiceFactory::
      GetForProfile(Profile::FromBrowserContext(browser_context));
}

std::optional<webapps::AppId> WebappsClientDesktop::GetAppIdForWebContents(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  web_app::WebAppTabHelper* helper =
      web_app::WebAppTabHelper::FromWebContents(web_contents);
  if (!helper) {
    return std::nullopt;
  }
  return helper->app_id();
}

}  // namespace webapps
