// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_COMMON_PREF_NAMES_H_
#define COMPONENTS_SUPERVISED_USER_CORE_COMMON_PREF_NAMES_H_

#include "extensions/buildflags/buildflags.h"

namespace prefs {

// A bool pref that keeps whether the child status for this profile was already
// successfully checked via ChildAccountService.
inline constexpr char kChildAccountStatusKnown[] = "child_account_status_known";

// Stores the email address associated with the google account of the custodian
// of the supervised user, set when the supervised user is created.
inline constexpr char kSupervisedUserCustodianEmail[] =
    "profile.managed.custodian_email";

// Stores the display name associated with the google account of the custodian
// of the supervised user, updated (if possible) each time the supervised user
// starts a session.
inline constexpr char kSupervisedUserCustodianName[] =
    "profile.managed.custodian_name";

// Stores the obfuscated gaia id associated with the google account of the
// custodian of the supervised user, updated (if possible) each time the
// supervised user starts a session.
inline constexpr char kSupervisedUserCustodianObfuscatedGaiaId[] =
    "profile.managed.custodian_obfuscated_gaia_id";

// Stores the URL of the profile image associated with the google account of the
// custodian of the supervised user.
inline constexpr char kSupervisedUserCustodianProfileImageURL[] =
    "profile.managed.custodian_profile_image_url";

// Stores the URL of the profile associated with the google account of the
// custodian of the supervised user.
inline constexpr char kSupervisedUserCustodianProfileURL[] =
    "profile.managed.custodian_profile_url";

// Stores the email address associated with the google account of the secondary
// custodian of the supervised user, set when the supervised user is created.
inline constexpr char kSupervisedUserSecondCustodianEmail[] =
    "profile.managed.second_custodian_email";

// Stores the display name associated with the google account of the secondary
// custodian of the supervised user, updated (if possible) each time the
// supervised user starts a session.
inline constexpr char kSupervisedUserSecondCustodianName[] =
    "profile.managed.second_custodian_name";

// Stores the obfuscated gaia id associated with the google account of the
// secondary custodian of the supervised user, updated (if possible) each time
// the supervised user starts a session.
inline constexpr char kSupervisedUserSecondCustodianObfuscatedGaiaId[] =
    "profile.managed.second_custodian_obfuscated_gaia_id";

// Stores the URL of the profile image associated with the google account of the
// secondary custodian of the supervised user.
inline constexpr char kSupervisedUserSecondCustodianProfileImageURL[] =
    "profile.managed.second_custodian_profile_image_url";

// Stores the URL of the profile associated with the google account of the
// secondary custodian of the supervised user.
inline constexpr char kSupervisedUserSecondCustodianProfileURL[] =
    "profile.managed.second_custodian_profile_url";

// Whether the supervised user may approve extension permission requests. If
// false, extensions should not be able to request new permissions, and new
// extensions should not be installable.
//
// Only valid if supervised_user::
// IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled() is false.
inline constexpr char kSupervisedUserExtensionsMayRequestPermissions[] =
    "profile.managed.extensions_may_request_permissions";

// Whether the supervised user may approve extension permission requests, under
// the updated supervised user extension handling flow.
// If true extensions can be installed without parental approval, if false
// the parent must grant approvoal on each installation.
//
// Only valid if supervised_user::
// IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled() is true.
inline constexpr char kSkipParentApprovalToInstallExtensions[] =
    "profile.managed.skip_parent_approval_to_install_extensions";

#if BUILDFLAG(ENABLE_EXTENSIONS)
// DictionaryValue that maps extension ids to the approved version of this
// extension for a supervised user. Missing extensions are not approved.
inline constexpr char kSupervisedUserApprovedExtensions[] =
    "profile.managed.approved_extensions";
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// The supervised user ID.
// TODO(b/342097235): this pref is being deprecated.
inline constexpr char kSupervisedUserId[] = "profile.managed_user_id";

// Maps host names to whether the host is manually allowed or blocked.
inline constexpr char kSupervisedUserManualHosts[] =
    "profile.managed.manual_hosts";

// Maps URLs to whether the URL is manually allowed or blocked.
inline constexpr char kSupervisedUserManualURLs[] =
    "profile.managed.manual_urls";

// Integer pref to record the day id (number of days since origin of time) when
// supervised user metrics were last recorded.
inline constexpr char kSupervisedUserMetricsDayId[] =
    "supervised_user.metrics.day_id";

// Stores whether the SafeSites filter is enabled.
inline constexpr char kSupervisedUserSafeSites[] = "profile.managed.safe_sites";

// Stores settings that can be modified both by a supervised user and their
// manager. See SupervisedUserSharedSettingsService for a description of
// the format.
inline constexpr char kSupervisedUserSharedSettings[] =
    "profile.managed.shared_settings";

// An integer pref specifying the fallback behavior for sites outside of content
// packs (see SupervisedUserFilter::FilteringBehavior). One of:
// 0: Allow (does nothing)
// 1: Warn [Deprecated]
// 2: Block
// 3: Invalid
inline constexpr char kDefaultSupervisedUserFilteringBehavior[] =
    "profile.managed.default_filtering_behavior";

// An integer pref that stores the current state of the interstitial banner for
// a supervised user (SupervisedUserFilter::FirstTimeInterstitialBannerState):
// 0: kNeedToShow
// 1: kSetupComplete
// 2: kUnknown
inline constexpr char kFirstTimeInterstitialBannerState[] =
    "profile.managed.banner_state";

#if BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// An integer pref that stores the current state of the local extension
// parent approval migration when the feature
// `kEnableSupervisedUserSkipParentApprovalToInstallExtensions` becomes enabled.
// Its values are drawn from
// `supervised user::LocallyParentApprovedExtensionsMigrationState`.
inline constexpr char kLocallyParentApprovedExtensionsMigrationState[] =
    "profile.managed.locally_parent_approved_extensions_migration_state";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// A dictionary pref that stores the extension Ids that are treated as
// parent-approved on a Desktop device when the feature
// `kEnableSupervisedUserSkipParentApprovalToInstallExtensions` becomes enabled.
// This is only populated on Win/Linux/Mac.
inline constexpr char kSupervisedUserLocallyParentApprovedExtensions[] =
    "profile.managed.locally_parent_approved_extensions";
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// A string pref that stores the family member role of the primary account
// as per kids_management::FamilyRole or
// `supervised_user::kDefaultEmptyFamilyMemberRole` if not in a Family group.
inline constexpr char kFamilyLinkUserMemberRole[] =
    "profile.family_member_role";

}  // namespace prefs

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_PREF_NAMES_H_
