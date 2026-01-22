// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_MANAGER_FEATURES_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_MANAGER_FEATURES_UTIL_H_

#include "build/build_config.h"
#include "components/password_manager/core/browser/password_form.h"

namespace syncer {
class SyncService;
}

namespace password_manager::features_util {

// Represents the state of the user wrt. sign-in and account-scoped storage.
// Used for metrics. Always keep this enum in sync with the corresponding
// histogram_suffixes in histograms.xml!
enum class PasswordAccountStorageUserState {
  // Signed-out user (so account storage is disabled).
  kSignedOutUser,
  // Signed-in non-syncing user, with account storage disabled.
  kSignedInUser,
  // Signed-in non-syncing user, with account storage enabled.
  kSignedInAccountStoreUser,
  // Syncing user.
  kSyncUser,
};

// The usage level of the account-scoped password storage. This is essentially
// a less-detailed version of PasswordAccountStorageUserState, for metrics that
// don't need the fully-detailed breakdown.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PasswordAccountStorageUsageLevel {
  // The user is not using the account-scoped password storage. Either they're
  // not signed in, or account storage was disabled.
  kNotUsingAccountStorage = 0,
  // The user is signed in (but not syncing) and has account storage enabled.
  kUsingAccountStorage = 1,
  // The user has enabled Sync.
  kSyncing = 2,
};

// Whether the Google account storage for passwords is active for the current
// signed-in user. This always returns false for sync-the-feature users and
// signed out users. Account storage can be enabled/disabled via
// syncer::SyncUserSettings::SetSelectedType().
//
// Note that "active" here is largely in line with Sync's definition: account
// storage is enabled and there are no sync errors preventing password sync from
// working. Thus, passwords saved in this state are very likely to be synced to
// the Google account (barring unexpected errors). Sync's definition of
// "active", however, is slightly stricter: During startup, while it's not known
// whether a data type will encounter errors, it's not considered active. This
// method assumes no errors in that case.
//
// Also note that sync-the-feature users might still sync passwords to the
// Google account using the profile store.
//
// |sync_service| may be null (commonly the case in incognito mode), in which
// case this will simply return false.
// See PasswordFeatureManager::IsAccountStorageActive.
bool IsAccountStorageActive(const syncer::SyncService* sync_service);

// See definition of PasswordAccountStorageUserState.
PasswordAccountStorageUserState ComputePasswordAccountStorageUserState(
    const syncer::SyncService* sync_service);

// Returns the "usage level" of the account-scoped password storage. See
// definition of PasswordAccountStorageUsageLevel.
// See PasswordFeatureManager::ComputePasswordAccountStorageUsageLevel.
PasswordAccountStorageUsageLevel ComputePasswordAccountStorageUsageLevel(
    const syncer::SyncService* sync_service);

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

// Whether the user toggle for account storage is shown in settings.
bool ShouldShowAccountStorageSettingToggle(
    const syncer::SyncService* sync_service);

// Password change HaTS product-specific data fields.
//
// Note: Counts and runtime should use bucketing.
inline constexpr char kPasswordChangeSuggestedPasswordsAdoption[] =
    "Has user ever adopted suggested passwords";
inline constexpr char kPasswordChangeBreachedPasswordsCount[] =
    "Number of breached passwords";
inline constexpr char kPasswordChangeSavedPasswordsCount[] =
    "Number of saved passwords";
inline constexpr char kPasswordChangeRuntime[] =
    "Password change feature runtime, in milliseconds";
inline constexpr char kPasswordChangeBlockingChallengeDetected[] =
    "Was there a blocking challenge (e.g. OTP) in the flow";

#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

}  // namespace password_manager::features_util

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_MANAGER_FEATURES_UTIL_H_
