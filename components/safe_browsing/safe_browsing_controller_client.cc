// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/safe_browsing_controller_client.h"

#include "base/feature_list.h"
#include "components/safe_browsing/features.h"
#include "components/security_interstitials/core/metrics_helper.h"

namespace safe_browsing {

SafeBrowsingControllerClient::SafeBrowsingControllerClient(
    content::WebContents* web_contents,
    std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
    PrefService* prefs,
    const std::string& app_locale,
    const GURL& default_safe_page)
    : security_interstitials::SecurityInterstitialControllerClient(
          web_contents,
          std::move(metrics_helper),
          prefs,
          app_locale,
          default_safe_page) {}

SafeBrowsingControllerClient::~SafeBrowsingControllerClient() {}

void SafeBrowsingControllerClient::Proceed() {
  if (!interstitial_page()) {
    DCHECK(
        base::FeatureList::IsEnabled(safe_browsing::kCommittedSBInterstitials));
    // In this case, committed interstitials are enabled, the site has already
    // been added to the whitelist, so reload will proceed.
    Reload();
    return;
  }
  security_interstitials::SecurityInterstitialControllerClient::Proceed();
}

void SafeBrowsingControllerClient::GoBack() {
  if (!interstitial_page()) {
    // In this case, committed interstitials are enabled, so we do a regular
    // back navigation.
    SecurityInterstitialControllerClient::GoBackAfterNavigationCommitted();
    return;
  }

  SecurityInterstitialControllerClient::GoBack();
}

}  // namespace safe_browsing
