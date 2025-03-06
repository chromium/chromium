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

class PrefService;

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

// Whether to instantiate a second PasswordStore whose data is account-scoped.
// This doesn't necessarily mean the store is being used, e.g. this predicate
// can return true for a signed-out user. For whether the store can be used,
// see IsAccountStorageEnabled() instead.
// On Android, if the internal backend is not present (i.e. in a public build),
// this method will return true, but the store itself will not be created.
bool CanCreateAccountStore(const PrefService* pref_service);

// Whether the Google account storage for passwords is enabled for the current
// signed-in user. This always returns false for sync-the-feature users and
// signed out users. Account storage can be enabled/disabled via
// syncer::SyncUserSettings::SetSelectedType().
//
// |pref_service| must not be null.
// |sync_service| may be null (commonly the case in incognito mode), in which
// case this will simply return false.
// See PasswordFeatureManager::IsAccountStorageEnabled.
bool IsAccountStorageEnabled(const PrefService* pref_service,
                             const syncer::SyncService* sync_service);

// See definition of PasswordAccountStorageUserState.
PasswordAccountStorageUserState ComputePasswordAccountStorageUserState(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service);

// Returns the "usage level" of the account-scoped password storage. See
// definition of PasswordAccountStorageUsageLevel.
// See PasswordFeatureManager::ComputePasswordAccountStorageUsageLevel.
PasswordAccountStorageUsageLevel ComputePasswordAccountStorageUsageLevel(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service);

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

// Whether the user toggle for account storage is shown in settings.
bool ShouldShowAccountStorageSettingToggle(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service);

// Users with account storage enabled used to have the choice of saving new
// passwords only locally, while keeping existing account passwords available
// for autofill. That was achieved by setting a certain "default store pref" to
// the "profile store". This logic was removed in crbug.com/369341336.
// MigrateDefaultProfileStorePref() migrates users in the legacy state to have
// account storage completely disabled instead, i.e. so they can't save nor
// autofill account passwords. The migration affects both signed-in and
// signed-out users (because account storage settings should survive sign-out).
// kObsoleteAccountStorageDefaultStoreKey was part of the legacy pref's schema
// and is exposed for testing.
inline constexpr char kObsoleteAccountStorageDefaultStoreKey[] =
    "default_store";
void MigrateDefaultProfileStorePref(PrefService* pref_service);

#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

}  // namespace password_manager::features_util

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_MANAGER_FEATURES_UTIL_H_
