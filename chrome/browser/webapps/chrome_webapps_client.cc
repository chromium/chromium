// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/chrome_webapps_client.h"

#include "base/logging.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/webapps/installable/installable_metrics.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#endif

namespace webapps {

// static
ChromeWebappsClient* ChromeWebappsClient::GetInstance() {
  static base::NoDestructor<ChromeWebappsClient> instance;
  return instance.get();
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
  return InfoBarService::FromWebContents(web_contents);
}

WebappInstallSource ChromeWebappsClient::GetInstallSource(
    content::WebContents* web_contents,
    InstallTrigger trigger) {
  bool is_custom_tab = false;
#if defined(OS_ANDROID)
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

#if defined(OS_ANDROID)
bool ChromeWebappsClient::IsInstallationInProgress(
    content::WebContents* web_contents,
    const GURL& manifest_url) {
  return WebApkInstallService::Get(web_contents->GetBrowserContext())
      ->IsInstallInProgress(manifest_url);
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
#endif

}  // namespace webapps
