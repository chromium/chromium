// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/security_interstitial_controller_client.h"

#include <utility>

#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_referral_methods.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"

using content::Referrer;

namespace security_interstitials {

SecurityInterstitialControllerClient::SecurityInterstitialControllerClient(
    content::WebContents* web_contents,
    std::unique_ptr<MetricsHelper> metrics_helper,
    PrefService* prefs,
    const std::string& app_locale,
    const GURL& default_safe_page,
    std::unique_ptr<SettingsPageHelper> settings_page_helper)
    : ControllerClient(std::move(metrics_helper)),
      web_contents_(web_contents),
      prefs_(prefs),
      app_locale_(app_locale),
      default_safe_page_(default_safe_page),
      settings_page_helper_(std::move(settings_page_helper)) {}

SecurityInterstitialControllerClient::~SecurityInterstitialControllerClient() =
    default;

void SecurityInterstitialControllerClient::GoBack() {
  // TODO(crbug.com/40688528): This method is left so class can be non abstract
  // since it is still instantiated in tests. This can be cleaned up by having
  // tests use a subclass.
  NOTREACHED_IN_MIGRATION();
}

bool SecurityInterstitialControllerClient::CanGoBack() {
  return web_contents_->GetController().CanGoBack();
}

void SecurityInterstitialControllerClient::GoBackAfterNavigationCommitted() {
  // If the offending entry has committed, go back or to a safe page without
  // closing the error page. This error page will be closed when the new page
  // commits.
  if (web_contents_->GetController().CanGoBack()) {
    web_contents_->GetController().GoBack();
  } else {
    // For <webview> tags (also known as guests), use about:blank as the
    // default safe page. This is because unlike a normal WebContents, guests
    // cannot load pages like WebUI, including the NTP, which is often used as
    // the default safe page here.
    GURL url_to_load = web_contents_->GetSiteInstance()->IsGuest()
                           ? GURL(url::kAboutBlankURL)
                           : default_safe_page_;
    web_contents_->GetController().LoadURL(url_to_load, content::Referrer(),
                                           ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                           std::string());
  }
}

void SecurityInterstitialControllerClient::Proceed() {
  // TODO(crbug.com/40688528): This method is left so class can be non abstract
  // since it is still instantiated in tests. This can be cleaned up by having
  // tests use a subclass.
  NOTREACHED_IN_MIGRATION();
}

void SecurityInterstitialControllerClient::Reload() {
  web_contents_->GetController().Reload(content::ReloadType::NORMAL, true);
}

void SecurityInterstitialControllerClient::OpenUrlInCurrentTab(
    const GURL& url) {
  content::OpenURLParams params(url, Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params, /*navigation_handle_callback=*/{});
}

void SecurityInterstitialControllerClient::OpenUrlInNewForegroundTab(
    const GURL& url) {
  content::OpenURLParams params(url, Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params, /*navigation_handle_callback=*/{});
}

void SecurityInterstitialControllerClient::OpenEnhancedProtectionSettings() {
#if BUILDFLAG(IS_ANDROID)
  settings_page_helper_->OpenEnhancedProtectionSettings(web_contents_);
#else
  settings_page_helper_->OpenEnhancedProtectionSettingsWithIph(
      web_contents_,
      safe_browsing::SafeBrowsingSettingReferralMethod::kSecurityInterstitial);
#endif
}

const std::string&
SecurityInterstitialControllerClient::GetApplicationLocale() const {
  return app_locale_;
}

PrefService*
SecurityInterstitialControllerClient::GetPrefService() {
  return prefs_;
}

const std::string
SecurityInterstitialControllerClient::GetExtendedReportingPrefName() const {
  return prefs::kSafeBrowsingScoutReportingEnabled;
}

bool SecurityInterstitialControllerClient::CanLaunchDateAndTimeSettings() {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void SecurityInterstitialControllerClient::LaunchDateAndTimeSettings() {
  NOTREACHED_IN_MIGRATION();
}

bool SecurityInterstitialControllerClient::CanGoBackBeforeNavigation() {
  // If checking before navigating to the interstitial, back to safety is
  // possible if the current entry is not the initial NavigationEtry. This
  // preserves old behavior to when we return nullptr instead of the initial
  // entry when no navigation has committed.
  return !web_contents_->GetController()
              .GetLastCommittedEntry()
              ->IsInitialEntry();
}

}  // namespace security_interstitials
