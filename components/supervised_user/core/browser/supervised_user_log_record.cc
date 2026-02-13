// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_log_record.h"

#include <optional>
#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "extensions/buildflags/buildflags.h"

namespace supervised_user {
namespace {

bool AreParentalSupervisionCapabilitiesKnown(
    const AccountCapabilities& capabilities) {
  return capabilities.is_opted_in_to_parental_supervision() !=
             signin::Tribool::kUnknown &&
         capabilities.is_subject_to_parental_controls() !=
             signin::Tribool::kUnknown;
}

bool IsParentFamilyMemberRole(const PrefService& pref_service) {
  // TODO(crbug.com/372607761): Convert string-based pref to enum based on
  // Family Link user state.
  const std::string& family_link_role =
      pref_service.GetString(prefs::kFamilyLinkUserMemberRole);
  return family_link_role == "parent" || family_link_role == "family_manager";
}

std::optional<SupervisedUserLogRecord::Segment> GetSupervisionStatus(
    signin::IdentityManager* identity_manager,
    const PrefService& pref_service,
    const DeviceParentalControls& device_parental_controls) {
  if (!base::FeatureList::IsEnabled(kSupervisedUserEmitLogRecordSeparately) &&
      !IsSubjectToParentalControls(pref_service) &&
      device_parental_controls.IsEnabled()) {
    // This type of supervision is signin-status independent (but only available
    // to non-incognito profiles).
    return SupervisedUserLogRecord::Segment::kSupervisionEnabledLocally;
  }

  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // Unsigned users who are not supervised locally are considered
    // unsupervised.
    return SupervisedUserLogRecord::Segment::kUnsupervised;
  }

  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  if (!AreParentalSupervisionCapabilitiesKnown(account_info.capabilities)) {
    // The user is signed in, but the parental supervision capabilities are
    // not known.
    return std::nullopt;
  }

  auto is_subject_to_parental_controls =
      account_info.capabilities.is_subject_to_parental_controls();
  if (is_subject_to_parental_controls == signin::Tribool::kTrue) {
    auto is_opted_in_to_parental_supervision =
        account_info.capabilities.is_opted_in_to_parental_supervision();
    if (is_opted_in_to_parental_supervision == signin::Tribool::kTrue) {
      return SupervisedUserLogRecord::Segment::
          kSupervisionEnabledByFamilyLinkUser;
    } else {
      // Log as a supervised user that has parental supervision enabled
      // by a policy applied to their account, e.g. Unicorn accounts.
      return SupervisedUserLogRecord::Segment::
          kSupervisionEnabledByFamilyLinkPolicy;
    }
  } else if (account_info.capabilities.can_fetch_family_member_info() ==
             signin::Tribool::kTrue) {
    if (IsParentFamilyMemberRole(pref_service)) {
      return SupervisedUserLogRecord::Segment::kParent;
    }
  }
  // Log as unsupervised user if the account is not subject to parental
  // controls and is not a parent in Family Link.
  return SupervisedUserLogRecord::Segment::kUnsupervised;
}

// Returns true if there is no available supervision status or the account is
// not subject to parental controls.
bool IsUnsupervisedStatus(
    std::optional<SupervisedUserLogRecord::Segment> supervision_status) {
  return !supervision_status.has_value() ||
         supervision_status.value() ==
             SupervisedUserLogRecord::Segment::kUnsupervised ||
         supervision_status.value() ==
             SupervisedUserLogRecord::Segment::kParent;
}

// Returns the web filter type of the primary account user. This function
// collates both off-the-record profiles and regular profiles without local
// supervision into the same empty returned value: in the metrics context the
// difference between these two cases is irrelevant. Locally supervised regular
// users yield kDisabled filter type when they decide to control other features
// than browser content.
std::optional<WebFilterType> GetWebFilterType(
    std::optional<SupervisedUserLogRecord::Segment> supervision_status,
    SupervisedUserUrlFilteringService* url_filtering_service) {
  if (!url_filtering_service || IsUnsupervisedStatus(supervision_status)) {
    return std::nullopt;
  }

  return url_filtering_service->GetWebFilterType();
}

std::optional<ToggleState> GetPermissionsToggleState(
    std::optional<SupervisedUserLogRecord::Segment> supervision_status,
    const PrefService& pref_service,
    const HostContentSettingsMap& content_settings_map) {
#if BUILDFLAG(IS_IOS)
  // The permissions toggle is not supported on iOS.
  return std::nullopt;
#else
  if (IsUnsupervisedStatus(supervision_status)) {
    return std::nullopt;
  }

  // The permissions toggle set multiple content settings. We pick one of them,
  // geolocation, to inspect here to infer the value of the toggle.
  content_settings::ProviderType provider;
  const content_settings::PermissionSettingsInfo* geolocation_info =
      content_settings::PermissionSettingsRegistry::GetInstance()->Get(
          content_settings::GeolocationContentSettingsType());
  bool is_geolocation_blocked_by_default =
      geolocation_info->delegate().IsBlocked(
          content_settings_map.GetDefaultPermissionSetting(
              content_settings::GeolocationContentSettingsType(), &provider));
  // Note: Do not check that the ProviderType is `kSupervisedProvider`. This
  // is true only when the parent has disabled the "Permissions" FL switch.

#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool block_geolocation = is_geolocation_blocked_by_default;
  bool permissions_allowed_pref = pref_service.GetBoolean(
      prefs::kSupervisedUserExtensionsMayRequestPermissions);
  // Cross-check the content setting against the preference that was the former
  // source of truth for a similar metric.
  DCHECK(permissions_allowed_pref != block_geolocation);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return is_geolocation_blocked_by_default ? ToggleState::kDisabled
                                           : ToggleState::kEnabled;
#endif  // BUILDFLAG(IS_IOS)
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
bool SupervisedUserCanSkipExtensionParentApprovals(
    const PrefService& pref_service) {
  return pref_service.GetBoolean(prefs::kSkipParentApprovalToInstallExtensions);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

std::optional<ToggleState> GetExtensionToggleState(
    std::optional<SupervisedUserLogRecord::Segment> supervision_status,
    const PrefService& pref_service) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (IsUnsupervisedStatus(supervision_status)) {
    return std::nullopt;
  }

  return SupervisedUserCanSkipExtensionParentApprovals(pref_service)
             ? ToggleState::kEnabled
             : ToggleState::kDisabled;
#else
  return std::nullopt;
#endif
}

// A templated function to merge multiple values of the same type into either:
// * An empty optional if none of the values are set
// * A non-empty optional if all the set values are equal
// * An optional containing |mixed_value| if there are multiple different
// values.
template <class T>
std::optional<T> GetMergedRecord(const std::vector<std::optional<T>>& records,
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

bool HasSupervisedStatus(
    std::optional<SupervisedUserLogRecord::Segment> segment) {
  if (!segment.has_value()) {
    return false;
  }
  switch (segment.value()) {
    case SupervisedUserLogRecord::Segment::kUnsupervised:
    case SupervisedUserLogRecord::Segment::kParent:
      return false;
    case SupervisedUserLogRecord::Segment::
        kSupervisionEnabledByFamilyLinkPolicy:
    case SupervisedUserLogRecord::Segment::kSupervisionEnabledByFamilyLinkUser:
    case SupervisedUserLogRecord::Segment::kSupervisionEnabledLocally:
      return true;
    case SupervisedUserLogRecord::Segment::kMixedProfile:
      NOTREACHED();
  }
}

std::optional<SupervisedUserLogRecord::Segment> GetLogSegmentForHistogram(
    const std::vector<SupervisedUserLogRecord>& records) {
  bool has_supervised_status = false;
  std::optional<SupervisedUserLogRecord::Segment> merged_log_segment;
  for (const SupervisedUserLogRecord& record : records) {
    std::optional<SupervisedUserLogRecord::Segment> supervision_status =
        record.GetSupervisionStatusForPrimaryAccount();
    has_supervised_status |= HasSupervisedStatus(supervision_status);
    if (merged_log_segment.has_value() &&
        merged_log_segment.value() != supervision_status) {
      if (has_supervised_status) {
        // A supervised user record is only expected to be mixed if there is at
        // least one supervised user.
        return SupervisedUserLogRecord::Segment::kMixedProfile;
      }
      CHECK(merged_log_segment.value() ==
                SupervisedUserLogRecord::Segment::kParent ||
            merged_log_segment.value() ==
                SupervisedUserLogRecord::Segment::kUnsupervised);
      merged_log_segment = SupervisedUserLogRecord::Segment::kParent;
    } else {
      merged_log_segment = supervision_status;
    }
  }
  return merged_log_segment;
}

std::optional<WebFilterType> GetWebFilterForHistogram(
    const std::vector<SupervisedUserLogRecord>& records) {
  std::vector<std::optional<WebFilterType>> filter_types;
  for (const SupervisedUserLogRecord& record : records) {
    filter_types.push_back(record.GetWebFilterTypeForPrimaryAccount());
  }
  return GetMergedRecord(filter_types, WebFilterType::kMixed);
}

std::optional<ToggleState> GetPermissionsToggleStateForHistogram(
    const std::vector<SupervisedUserLogRecord>& records) {
  std::vector<std::optional<ToggleState>> permissions_toggle_states;
  for (const SupervisedUserLogRecord& record : records) {
    permissions_toggle_states.push_back(
        record.GetPermissionsToggleStateForPrimaryAccount());
  }
  return GetMergedRecord(permissions_toggle_states, ToggleState::kMixed);
}

std::optional<ToggleState> GetExtensionsToggleStateForHistogram(
    const std::vector<SupervisedUserLogRecord>& records) {
  std::vector<std::optional<ToggleState>> extensions_toggle_states;
  for (const SupervisedUserLogRecord& record : records) {
    extensions_toggle_states.push_back(
        record.GetExtensionsToggleStateForPrimaryAccount());
  }
  return GetMergedRecord(extensions_toggle_states, ToggleState::kMixed);
}
}  // namespace

SupervisedUserLogRecord SupervisedUserLogRecord::Create(
    signin::IdentityManager* identity_manager,
    const PrefService& pref_service,
    const HostContentSettingsMap& content_settings_map,
    SupervisedUserUrlFilteringService* url_filtering_service,
    const DeviceParentalControls& device_parental_controls) {
  std::optional<SupervisedUserLogRecord::Segment> supervision_status =
      GetSupervisionStatus(identity_manager, pref_service,
                           device_parental_controls);
  return SupervisedUserLogRecord(
      supervision_status,
      GetWebFilterType(supervision_status, url_filtering_service),
      GetPermissionsToggleState(supervision_status, pref_service,
                                content_settings_map),
      GetExtensionToggleState(supervision_status, pref_service));
}

// static
bool SupervisedUserLogRecord::EmitHistograms(
    const std::vector<SupervisedUserLogRecord>& records,
    const DeviceParentalControls& device_parental_controls) {
  bool did_emit_histogram = false;

  if (base::FeatureList::IsEnabled(kSupervisedUserEmitLogRecordSeparately) &&
      device_parental_controls.IsEnabled()) {
    base::UmaHistogramEnumeration(
        kFamilyLinkUserLogSegmentHistogramName,
        SupervisedUserLogRecord::Segment::kSupervisionEnabledLocally);
    did_emit_histogram = true;
  }

  std::optional<SupervisedUserLogRecord::Segment> segment =
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

SupervisedUserLogRecord::SupervisedUserLogRecord(
    std::optional<SupervisedUserLogRecord::Segment> supervision_status,
    std::optional<WebFilterType> web_filter_type,
    std::optional<ToggleState> permissions_toggle_state,
    std::optional<ToggleState> extensions_toggle_state)
    : supervision_status_(supervision_status),
      web_filter_type_(web_filter_type),
      permissions_toggle_state_(permissions_toggle_state),
      extensions_toggle_state_(extensions_toggle_state) {}

std::optional<SupervisedUserLogRecord::Segment>
SupervisedUserLogRecord::GetSupervisionStatusForPrimaryAccount() const {
  return supervision_status_;
}

std::optional<WebFilterType>
SupervisedUserLogRecord::GetWebFilterTypeForPrimaryAccount() const {
  return web_filter_type_;
}

std::optional<ToggleState>
SupervisedUserLogRecord::GetPermissionsToggleStateForPrimaryAccount() const {
  return permissions_toggle_state_;
}

std::optional<ToggleState>
SupervisedUserLogRecord::GetExtensionsToggleStateForPrimaryAccount() const {
  return extensions_toggle_state_;
}

}  // namespace supervised_user
