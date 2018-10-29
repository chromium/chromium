// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/security_interstitial_controller_client.h"

#include <utility>

#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"

using content::Referrer;

namespace security_interstitials {

SecurityInterstitialControllerClient::SecurityInterstitialControllerClient(
    content::WebContents* web_contents,
    std::unique_ptr<MetricsHelper> metrics_helper,
    PrefService* prefs,
    const std::string& app_locale,
    const GURL& default_safe_page)
    : ControllerClient(std::move(metrics_helper)),
      web_contents_(web_contents),
      interstitial_page_(nullptr),
      prefs_(prefs),
      app_locale_(app_locale),
      default_safe_page_(default_safe_page) {}

SecurityInterstitialControllerClient::~SecurityInterstitialControllerClient() {}

void SecurityInterstitialControllerClient::set_interstitial_page(
    content::InterstitialPage* interstitial_page) {
  interstitial_page_ = interstitial_page;
}

content::InterstitialPage*
SecurityInterstitialControllerClient::interstitial_page() {
  return interstitial_page_;
}

void SecurityInterstitialControllerClient::GoBack() {
  interstitial_page_->DontProceed();
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
    web_contents_->GetController().LoadURL(
        default_safe_page_, content::Referrer(),
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  }
}

void SecurityInterstitialControllerClient::Proceed() {
  interstitial_page_->Proceed();
}

void SecurityInterstitialControllerClient::Reload() {
  web_contents_->GetController().Reload(content::ReloadType::NORMAL, true);
}

void SecurityInterstitialControllerClient::OpenUrlInCurrentTab(
    const GURL& url) {
  content::OpenURLParams params(url, Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params);
}

void SecurityInterstitialControllerClient::OpenUrlInNewForegroundTab(
    const GURL& url) {
  content::OpenURLParams params(url, Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params);
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
  NOTREACHED();
  return false;
}

void SecurityInterstitialControllerClient::LaunchDateAndTimeSettings() {
  NOTREACHED();
}

}  // namespace security_interstitials
