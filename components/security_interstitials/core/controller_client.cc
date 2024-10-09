// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/controller_client.h"

#include <utility>

#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/urls.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {

const char kBoxChecked[] = "boxchecked";
const char kDisplayCheckBox[] = "displaycheckbox";
const char kDisplayEnhancedProtectionMessage[] =
    "displayEnhancedProtectionMessage";
const char kOptInLink[] = "optInLink";
const char kEnhancedProtectionMessage[] = "enhancedProtectionMessage";
const char kHelpCenterUrl[] = "https://support.google.com/chrome/";

ControllerClient::ControllerClient(
    std::unique_ptr<MetricsHelper> metrics_helper)
    : metrics_helper_(std::move(metrics_helper)),
      help_center_url_(kHelpCenterUrl) {}

ControllerClient::~ControllerClient() = default;

MetricsHelper* ControllerClient::metrics_helper() const {
  return metrics_helper_.get();
}

void ControllerClient::SetReportingPreference(bool report) {
  DCHECK(GetPrefService());
  GetPrefService()->SetBoolean(GetExtendedReportingPrefName(), report);
  metrics_helper_->RecordUserInteraction(
      report ? MetricsHelper::SET_EXTENDED_REPORTING_ENABLED
             : MetricsHelper::SET_EXTENDED_REPORTING_DISABLED);
}

void ControllerClient::OpenExtendedReportingPrivacyPolicy(
    bool open_links_in_new_tab) {
  metrics_helper_->RecordUserInteraction(MetricsHelper::SHOW_PRIVACY_POLICY);
  GURL privacy_url(kSafeBrowsingPrivacyPolicyUrl);
  privacy_url =
      google_util::AppendGoogleLocaleParam(privacy_url, GetApplicationLocale());
  OpenURL(open_links_in_new_tab, privacy_url);
}

void ControllerClient::OpenExtendedReportingWhitepaper(
    bool open_links_in_new_tab) {
  metrics_helper_->RecordUserInteraction(MetricsHelper::SHOW_WHITEPAPER);
  GURL whitepaper_url(kSafeBrowsingWhitePaperUrl);
  whitepaper_url = google_util::AppendGoogleLocaleParam(whitepaper_url,
                                                        GetApplicationLocale());
  OpenURL(open_links_in_new_tab, whitepaper_url);
}

void ControllerClient::OpenURL(bool open_links_in_new_tab, const GURL& url) {
  if (open_links_in_new_tab) {
    OpenUrlInNewForegroundTab(url);
  } else {
    OpenUrlInCurrentTab(url);
  }
}

bool ControllerClient::HasSeenRecurrentError() {
  return false;
}

GURL ControllerClient::GetBaseHelpCenterUrl() const {
  return help_center_url_;
}

void ControllerClient::SetBaseHelpCenterUrlForTesting(const GURL& test_url) {
  help_center_url_ = test_url;
}

}  // namespace security_interstitials
