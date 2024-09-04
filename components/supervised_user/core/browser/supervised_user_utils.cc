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

// A templated function to merge multiple values of the same type into either:
// * An empty optional if none of the values are set
// * A non-empty optional if all the set values are equal
// * An optional containing |mixed_value| if there are multiple different
// values.
template <class T>
std::optional<T> GetMergedRecord(const std::vector<std::optional<T>> records,
                                 T mixed_value) {
  std::optional<T> merged_record;
  for (const std::optional<T> record : records) {
    if (!record.has_value()) {
      continue;
    }

    if (merged_record.has_value() && merged_record.value() != record.value()) {
      return mixed_value;
    }
    merged_record = record;
  }
  return merged_record;
}

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
  std::vector<std::optional<WebFilterType>> filter_types;
  for (const FamilyLinkUserLogRecord& record : records) {
    filter_types.push_back(record.GetWebFilterTypeForPrimaryAccount());
  }
  return GetMergedRecord(filter_types, WebFilterType::kMixed);
}

std::optional<ToggleState> GetPermissionsToggleStateForHistogram(
    const std::vector<FamilyLinkUserLogRecord>& records) {
  std::vector<std::optional<ToggleState>> permissions_toggle_states;
  for (const FamilyLinkUserLogRecord& record : records) {
    permissions_toggle_states.push_back(
        record.GetPermissionsToggleStateForPrimaryAccount());
  }
  return GetMergedRecord(permissions_toggle_states, ToggleState::kMixed);
}

std::optional<ToggleState> GetExtensionsToggleStateForHistogram(
    const std::vector<FamilyLinkUserLogRecord>& records) {
  std::vector<std::optional<ToggleState>> extensions_toggle_states;
  for (const FamilyLinkUserLogRecord& record : records) {
    extensions_toggle_states.push_back(
        record.GetExtensionsToggleStateForPrimaryAccount());
  }
  return GetMergedRecord(extensions_toggle_states, ToggleState::kMixed);
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
      NOTREACHED();
  }
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

  std::optional<ToggleState> permissions_toggle_state =
      GetPermissionsToggleStateForHistogram(records);
  if (permissions_toggle_state.has_value()) {
    base::UmaHistogramEnumeration(
        kSitesMayRequestCameraMicLocationHistogramName,
        permissions_toggle_state.value());
    did_emit_histogram = true;
  }

  std::optional<ToggleState> extensions_toggle_state =
      GetExtensionsToggleStateForHistogram(records);
  if (extensions_toggle_state.has_value()) {
    base::UmaHistogramEnumeration(
        kSkipParentApprovalToInstallExtensionsHistogramName,
        extensions_toggle_state.value());
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
