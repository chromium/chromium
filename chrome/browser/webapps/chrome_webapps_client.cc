// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/chrome_webapps_client.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/banners/android/chrome_app_banner_manager_android.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#else
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#endif

namespace webapps {

// static
ChromeWebappsClient* ChromeWebappsClient::GetInstance() {
  static base::NoDestructor<ChromeWebappsClient> instance;
  return instance.get();
}

bool ChromeWebappsClient::IsOriginConsideredSecure(const url::Origin& origin) {
  return origin.scheme() == chrome::kIsolatedAppScheme;
}

security_state::SecurityLevel
ChromeWebappsClient::GetSecurityLevelForWebContents(
    content::WebContents* web_contents) {
  return SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityLevel();
}

infobars::ContentInfoBarManager*
ChromeWebappsClient::GetInfoBarManagerForWebContents(
    content::WebContents* web_contents) {
  return infobars::ContentInfoBarManager::FromWebContents(web_contents);
}

WebappInstallSource ChromeWebappsClient::GetInstallSource(
    content::WebContents* web_contents,
    InstallTrigger trigger) {
  bool is_custom_tab = false;
#if BUILDFLAG(IS_ANDROID)
  auto* delegate = static_cast<android::TabWebContentsDelegateAndroid*>(
      web_contents->GetDelegate());
  is_custom_tab = delegate->IsCustomTab();
#endif

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
  NOTREACHED();
  return WebappInstallSource::COUNT;
}

AppBannerManager* ChromeWebappsClient::GetAppBannerManager(
    content::WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  return ChromeAppBannerManagerAndroid::FromWebContents(web_contents);
#else
  return AppBannerManagerDesktop::FromWebContents(web_contents);
#endif
}

#if BUILDFLAG(IS_ANDROID)
bool ChromeWebappsClient::IsInstallationInProgress(
    content::WebContents* web_contents,
    const GURL& manifest_id) {
  return WebApkInstallService::Get(web_contents->GetBrowserContext())
      ->IsInstallInProgress(manifest_id);
}

bool ChromeWebappsClient::CanShowAppBanners(
    content::WebContents* web_contents) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  return tab && static_cast<android::TabWebContentsDelegateAndroid*>(
                    tab->web_contents()->GetDelegate())
                    ->CanShowAppBanners();
}

void ChromeWebappsClient::OnWebApkInstallInitiatedFromAppMenu(
    content::WebContents* web_contents) {
  DVLOG(2) << "Sending event: IPH used for Installing PWA";
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  tracker->NotifyEvent(feature_engagement::events::kPwaInstallMenuSelected);
}

void ChromeWebappsClient::InstallWebApk(content::WebContents* web_contents,
                                        const AddToHomescreenParams& params) {
  WebApkInstallService::Get(web_contents->GetBrowserContext())
      ->InstallAsync(web_contents, *(params.shortcut_info), params.primary_icon,
                     params.install_source);
}

void ChromeWebappsClient::InstallShortcut(content::WebContents* web_contents,
                                          const AddToHomescreenParams& params) {
  ShortcutHelper::AddToLauncherWithSkBitmap(
      web_contents, *(params.shortcut_info), params.primary_icon,
      params.installable_status);
}
#endif

}  // namespace webapps
