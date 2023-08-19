// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/common/supervised_user_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/url_matcher/url_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace supervised_user {

namespace {

bool AreParentalSupervisionCapabilitiesKnown(
    const AccountCapabilities& capabilities) {
  return capabilities.is_opted_in_to_parental_supervision() !=
             signin::Tribool::kUnknown &&
         capabilities.is_subject_to_parental_controls() !=
             signin::Tribool::kUnknown;
}

absl::optional<LogSegment> SupervisionStatusOfProfile(
    const AccountInfo& account_info) {
  if (!AreParentalSupervisionCapabilitiesKnown(account_info.capabilities)) {
    return absl::nullopt;
  }
  auto is_subject_to_parental_controls =
      account_info.capabilities.is_subject_to_parental_controls();
  if (is_subject_to_parental_controls == signin::Tribool::kTrue) {
    auto is_opted_in_to_parental_supervision =
        account_info.capabilities.is_opted_in_to_parental_supervision();
    if (is_opted_in_to_parental_supervision == signin::Tribool::kTrue) {
      return LogSegment::kSupervisionEnabledByUser;
    } else {
      // Log as a supervised user that has parental supervision enabled
      // by a policy applied to their account, e.g. Unicorn accounts.
      return LogSegment::kSupervisionEnabledByPolicy;
    }
  } else {
    // Log as unsupervised user if the account is not subject to parental
    // controls.
    return LogSegment::kUnsupervised;
  }
}

}  // namespace
std::string FilteringBehaviorReasonToString(FilteringBehaviorReason reason) {
  switch (reason) {
    case FilteringBehaviorReason::DEFAULT:
      return "Default";
    case FilteringBehaviorReason::ASYNC_CHECKER:
      return "AsyncChecker";
    case FilteringBehaviorReason::MANUAL:
      return "Manual";
    case FilteringBehaviorReason::ALLOWLIST:
      return "Allowlist";
    case FilteringBehaviorReason::NOT_SIGNED_IN:
      return "NotSignedIn";
  }
  return "Unknown";
}

GURL NormalizeUrl(const GURL& url) {
  GURL effective_url = url_matcher::util::GetEmbeddedURL(url);
  if (!effective_url.is_valid()) {
    effective_url = url;
  }
  return url_matcher::util::Normalize(effective_url);
}

bool AreWebFilterPrefsDefault(const PrefService& pref_service) {
  return pref_service
             .FindPreference(prefs::kDefaultSupervisedUserFilteringBehavior)
             ->IsDefaultValue() ||
         pref_service.FindPreference(prefs::kSupervisedUserSafeSites)
             ->IsDefaultValue();
}

bool EmitLogSegmentHistogram(const std::vector<AccountInfo>& primary_accounts) {
  absl::optional<LogSegment> merged_log_segment;
  for (const AccountInfo& account_info : primary_accounts) {
    absl::optional<LogSegment> profile_status =
        SupervisionStatusOfProfile(account_info);
    if (merged_log_segment.has_value() && profile_status.has_value() &&
        merged_log_segment.value() != profile_status.value()) {
      base::UmaHistogramEnumeration(kFamilyLinkUserLogSegmentHistogramName,
                                    LogSegment::kMixedProfile);
      return true;
    }
    merged_log_segment = profile_status;
  }

  if (merged_log_segment.has_value()) {
    base::UmaHistogramEnumeration(kFamilyLinkUserLogSegmentHistogramName,
                                  merged_log_segment.value());
    return true;
  }

  return false;
}

bool IsSubjectToParentalControls(const PrefService* pref_service) {
  return pref_service &&
         pref_service->GetString(prefs::kSupervisedUserId) ==
             kChildAccountSUID &&
         IsChildAccountSupervisionEnabled();
}

}  // namespace supervised_user
