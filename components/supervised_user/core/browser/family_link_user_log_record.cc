// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/family_link_user_log_record.h"

#include <optional>

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
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

std::optional<FamilyLinkUserLogRecord::Segment> GetSupervisionStatus(
    signin::IdentityManager* identity_manager) {
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // The user is not signed in to this profile, and is therefore
    // unsupervised.
    return FamilyLinkUserLogRecord::Segment::kUnsupervised;
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
      return FamilyLinkUserLogRecord::Segment::kSupervisionEnabledByUser;
    } else {
      // Log as a supervised user that has parental supervision enabled
      // by a policy applied to their account, e.g. Unicorn accounts.
      return FamilyLinkUserLogRecord::Segment::kSupervisionEnabledByPolicy;
    }
  } else {
    // Log as unsupervised user if the account is not subject to parental
    // controls.
    return FamilyLinkUserLogRecord::Segment::kUnsupervised;
  }
}

// Returns true if there is no available supervision status or the account is
// not subject to parental controls.
bool IsUnsupervisedStatus(
    std::optional<FamilyLinkUserLogRecord::Segment> supervision_status) {
  return !supervision_status.has_value() ||
         supervision_status.value() ==
             FamilyLinkUserLogRecord::Segment::kUnsupervised;
}

std::optional<WebFilterType> GetWebFilterType(
    std::optional<FamilyLinkUserLogRecord::Segment> supervision_status,
    SupervisedUserURLFilter* supervised_user_filter) {
  if (!supervised_user_filter || IsUnsupervisedStatus(supervision_status)) {
    return std::nullopt;
  }
  return supervised_user_filter->GetWebFilterType();
}

std::optional<ToggleState> GetPermissionsToggleState(
    std::optional<FamilyLinkUserLogRecord::Segment> supervision_status,
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
  auto content_setting = content_settings_map.GetDefaultContentSetting(
      ContentSettingsType::GEOLOCATION, &provider);
  // Note: Do not check that the ProviderType is `kSupervisedProvider`. This
  // is true only when the parent has disabled the "Permissions" FL switch.

#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool block_geolocation =
      content_setting == ContentSetting::CONTENT_SETTING_BLOCK;
  bool permissions_allowed_pref = pref_service.GetBoolean(
      prefs::kSupervisedUserExtensionsMayRequestPermissions);
  // Cross-check the content setting against the preference that was the former
  // source of truth for a similar metric.
  DCHECK(permissions_allowed_pref != block_geolocation);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return content_setting == ContentSetting::CONTENT_SETTING_BLOCK
             ? ToggleState::kDisabled
             : ToggleState::kEnabled;
#endif  // BUILDFLAG(IS_IOS)
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
bool SupervisedUserCanSkipExtensionParentApprovals(
    const PrefService& pref_service) {
  return IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled() &&
         pref_service.GetBoolean(prefs::kSkipParentApprovalToInstallExtensions);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

std::optional<ToggleState> GetExtensionToggleState(
    std::optional<FamilyLinkUserLogRecord::Segment> supervision_status,
    const PrefService& pref_service) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (IsUnsupervisedStatus(supervision_status) ||
      !IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled()) {
    return std::nullopt;
  }

  return SupervisedUserCanSkipExtensionParentApprovals(pref_service)
             ? ToggleState::kEnabled
             : ToggleState::kDisabled;
#else
  return std::nullopt;
#endif
}

}  // namespace

FamilyLinkUserLogRecord FamilyLinkUserLogRecord::Create(
    signin::IdentityManager* identity_manager,
    const PrefService& pref_service,
    const HostContentSettingsMap& content_settings_map,
    SupervisedUserURLFilter* supervised_user_filter) {
  std::optional<FamilyLinkUserLogRecord::Segment> supervision_status =
      GetSupervisionStatus(identity_manager);
  return FamilyLinkUserLogRecord(
      supervision_status,
      GetWebFilterType(supervision_status, supervised_user_filter),
      GetPermissionsToggleState(supervision_status, pref_service,
                                content_settings_map),
      GetExtensionToggleState(supervision_status, pref_service));
}

FamilyLinkUserLogRecord::FamilyLinkUserLogRecord(
    std::optional<FamilyLinkUserLogRecord::Segment> supervision_status,
    std::optional<WebFilterType> web_filter_type,
    std::optional<ToggleState> permissions_toggle_state,
    std::optional<ToggleState> extensions_toggle_state)
    : supervision_status_(supervision_status),
      web_filter_type_(web_filter_type),
      permissions_toggle_state_(permissions_toggle_state),
      extensions_toggle_state_(extensions_toggle_state) {}

std::optional<FamilyLinkUserLogRecord::Segment>
FamilyLinkUserLogRecord::GetSupervisionStatusForPrimaryAccount() const {
  return supervision_status_;
}

std::optional<WebFilterType>
FamilyLinkUserLogRecord::GetWebFilterTypeForPrimaryAccount() const {
  return web_filter_type_;
}

std::optional<ToggleState>
FamilyLinkUserLogRecord::GetPermissionsToggleStateForPrimaryAccount() const {
  return permissions_toggle_state_;
}

std::optional<ToggleState>
FamilyLinkUserLogRecord::GetExtensionsToggleStateForPrimaryAccount() const {
  return extensions_toggle_state_;
}

}  // namespace supervised_user
