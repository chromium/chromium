// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/pref_url_list_matcher.h"
#include "components/enterprise/browser/reporting/reporting_features.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_aggregation_utils.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace enterprise_reporting {

namespace {
constexpr char kGeminiInChromeUsageUrl[] = "https://gemini-in-chrome/";
}  // namespace

SaasUsageReportingController::SaasUsageReportingController(
    PrefService* local_state_pref_service,
    PrefService* profile_pref_service,
    std::unique_ptr<PrefURLListMatcher> browser_matcher,
    std::unique_ptr<PrefURLListMatcher> profile_matcher)
    : local_state_pref_service_(CHECK_DEREF(local_state_pref_service)),
      profile_pref_service_(CHECK_DEREF(profile_pref_service)),
      browser_matcher_(std::move(browser_matcher)),
      profile_matcher_(std::move(profile_matcher)) {
  CHECK(browser_matcher_);
  CHECK(profile_matcher_);
}

SaasUsageReportingController::~SaasUsageReportingController() = default;

void SaasUsageReportingController::RecordNavigation(
    const NavigationDataDelegate& delegate) const {
  if (DomainMatches matches = MatchDomains(delegate.GetUrl());
      matches.HasMatch()) {
    delegate.GetEncryptionProtocol(base::BindOnce(
        &SaasUsageReportingController::OnEncryptionProtocolFetched,
        weak_ptr_factory_.GetWeakPtr(), std::move(matches)));
  }
}

void SaasUsageReportingController::RecordGeminiInChromeUsage() const {
  if (!base::FeatureList::IsEnabled(kGeminiInChromeUsageReporting)) {
    return;
  }
  if (DomainMatches matches = MatchDomains(GURL(kGeminiInChromeUsageUrl));
      matches.HasMatch()) {
    OnEncryptionProtocolFetched(std::move(matches), /*encryption_protocol=*/"");
  }
}

void SaasUsageReportingController::OnEncryptionProtocolFetched(
    DomainMatches matches,
    std::string_view encryption_protocol) const {
  if (matches.profile_domain) {
    enterprise_reporting::RecordNavigation(profile_pref_service_.get(),
                                           matches.profile_domain.value(),
                                           encryption_protocol);
  }
  if (matches.browser_domain) {
    enterprise_reporting::RecordNavigation(local_state_pref_service_.get(),
                                           matches.browser_domain.value(),
                                           encryption_protocol);
  }
}

SaasUsageReportingController::DomainMatches
SaasUsageReportingController::MatchDomains(const GURL& url) const {
  return {
      .browser_domain = browser_matcher_->GetMatchedURL(url),
      .profile_domain = profile_matcher_->GetMatchedURL(url),
  };
}

}  // namespace enterprise_reporting
