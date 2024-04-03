// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_utils.h"

#include <optional>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/family_link_user_log_record.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/url_formatter/url_formatter.h"
#include "components/url_matcher/url_util.h"
#include "url/gurl.h"

namespace supervised_user {

namespace {

std::optional<FamilyLinkUserLogRecord::Segment> GetLogSegmentForHistogram(
    const std::vector<FamilyLinkUserLogRecord>& records) {
  std::optional<FamilyLinkUserLogRecord::Segment> merged_log_segment;
  for (const FamilyLinkUserLogRecord& record : records) {
    std::optional<FamilyLinkUserLogRecord::Segment> supervision_status =
        record.GetSupervisionStatusForPrimaryAccount();
    if (merged_log_segment.has_value() &&
        merged_log_segment.value() != supervision_status) {
      return FamilyLinkUserLogRecord::Segment::kMixedProfile;
    }
    merged_log_segment = supervision_status;
  }
  return merged_log_segment;
}

std::optional<WebFilterType> GetWebFilterForHistogram(
    const std::vector<FamilyLinkUserLogRecord>& records) {
  std::optional<WebFilterType> merged_log_segment;
  for (const FamilyLinkUserLogRecord& record : records) {
    std::optional<WebFilterType> web_filter =
        record.GetWebFilterTypeForPrimaryAccount();
    if (!web_filter.has_value()) {
      continue;
    }

    if (merged_log_segment.has_value() &&
        merged_log_segment.value() != web_filter) {
      return WebFilterType::kMixed;
    }
    merged_log_segment = web_filter;
  }
  return merged_log_segment;
}
}  // namespace

std::string FamilyRoleToString(kidsmanagement::FamilyRole role) {
  switch (role) {
    case kidsmanagement::CHILD:
      return "child";
    case kidsmanagement::MEMBER:
      return "member";
    case kidsmanagement::PARENT:
      return "parent";
    case kidsmanagement::HEAD_OF_HOUSEHOLD:
      return "family_manager";
    default:
      // Keep the previous semantics - other values were not allowed.
      NOTREACHED_NORETURN();
  }
}

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

bool EmitLogRecordHistograms(
    const std::vector<FamilyLinkUserLogRecord>& records) {
  bool did_emit_histogram = false;
  std::optional<FamilyLinkUserLogRecord::Segment> segment =
      GetLogSegmentForHistogram(records);
  if (segment.has_value()) {
    base::UmaHistogramEnumeration(kFamilyLinkUserLogSegmentHistogramName,
                                  segment.value());
    did_emit_histogram = true;
  }
  std::optional<WebFilterType> web_filter = GetWebFilterForHistogram(records);
  if (web_filter.has_value()) {
    base::UmaHistogramEnumeration(
        kFamilyLinkUserLogSegmentWebFilterHistogramName, web_filter.value());
    did_emit_histogram = true;
  }
  return did_emit_histogram;
}

UrlFormatter::UrlFormatter(
    const SupervisedUserURLFilter& supervised_user_url_filter,
    FilteringBehaviorReason filtering_behavior_reason)
    : supervised_user_url_filter_(supervised_user_url_filter),
      filtering_behavior_reason_(filtering_behavior_reason) {}

UrlFormatter::~UrlFormatter() = default;

GURL UrlFormatter::FormatUrl(const GURL& url) const {
  // Strip the trivial subdomain.
  GURL stripped_url(url_formatter::FormatUrl(
      url, url_formatter::kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));

  // If the url is blocked due to an entry in the block list,
  // check if the blocklist entry is a trivial www-subdomain conflict and skip
  // the stripping.
  bool skip_trivial_subdomain_strip =
      filtering_behavior_reason_ == FilteringBehaviorReason::MANUAL &&
      stripped_url.host() != url.host() &&
      supervised_user_url_filter_->IsHostInBlocklist(url.host());

  GURL target_url = skip_trivial_subdomain_strip ? url : stripped_url;

  // TODO(b/322484529): Standardize the url formatting for local approvals
  // across platforms.
#if !BUILDFLAG(IS_CHROMEOS)
  return NormalizeUrl(target_url);
#else
  return target_url;
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

}  // namespace supervised_user
