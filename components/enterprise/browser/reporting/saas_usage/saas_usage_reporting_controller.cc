// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_controller.h"

#include <memory>
#include <string_view>

#include "base/check_deref.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/pref_url_list_matcher.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_aggregation_utils.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace enterprise_reporting {

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
  GURL url = delegate.GetUrl();
  std::string encryption_protocol = delegate.GetEncryptionProtocol();
  std::optional<std::string> profile_matched_domain =
      profile_matcher_->GetMatchedURL(url);
  if (profile_matched_domain) {
    enterprise_reporting::RecordNavigation(profile_pref_service_.get(),
                                           profile_matched_domain.value(),
                                           encryption_protocol);
  }

  std::optional<std::string> browser_matched_domain =
      browser_matcher_->GetMatchedURL(url);
  if (browser_matched_domain) {
    enterprise_reporting::RecordNavigation(local_state_pref_service_.get(),
                                           browser_matched_domain.value(),
                                           encryption_protocol);
  }
}

}  // namespace enterprise_reporting
