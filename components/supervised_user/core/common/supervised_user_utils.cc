// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/common/supervised_user_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
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

absl::optional<LogSegment> SupervisionStatusForUser(
    const signin::IdentityManager* identity_manager) {
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // The user is not signed in to this profile, and is therefore
    // unsupervised.
    return supervised_user::LogSegment::kUnsupervised;
  }

  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  if (!AreParentalSupervisionCapabilitiesKnown(account_info.capabilities)) {
    // The user is signed in, but the parental supervision capabilities are
    // not known.
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

bool EmitLogSegmentHistogram(const std::vector<LogSegment>& log_segments) {
  absl::optional<LogSegment> merged_log_segment;
  for (const LogSegment& log_segment : log_segments) {
    if (merged_log_segment.has_value() &&
        merged_log_segment.value() != log_segment) {
      base::UmaHistogramEnumeration(kFamilyLinkUserLogSegmentHistogramName,
                                    LogSegment::kMixedProfile);
      return true;
    }
    merged_log_segment = log_segment;
  }

  if (merged_log_segment.has_value()) {
    base::UmaHistogramEnumeration(kFamilyLinkUserLogSegmentHistogramName,
                                  merged_log_segment.value());
    return true;
  }

  return false;
}

}  // namespace supervised_user
