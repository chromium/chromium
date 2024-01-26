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
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/url_constants.h"
#include "components/infobars/content/content_infobar_manager.h"
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

bool WebappsClientDesktop::IsWebAppConsideredFullyInstalled(
    content::BrowserContext* browser_context,
    const GURL& start_url,
    const ManifestId& manifest_id) const {
  CHECK(browser_context);
  return web_app::FindInstalledAppWithUrlInScope(
             Profile::FromBrowserContext(browser_context), start_url)
      .has_value();
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
  return web_app::WebAppProvider::GetForWebApps(profile)
      ->ui_manager()
      .IsInAppWindow(web_contents);
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
  return web_app::WebAppPrefGuardrails::GetForMlInstallPrompt(
             profile->GetPrefs())
      .IsBlockedByGuardrails(web_app::GenerateAppIdFromManifestId(manifest_id));
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

}  // namespace webapps
