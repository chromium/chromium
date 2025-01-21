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

class GaiaId;
class PrefService;

namespace password_manager::features_util {

// Represents the state of the user wrt. sign-in and account-scoped storage.
// Used for metrics. Always keep this enum in sync with the corresponding
// histogram_suffixes in histograms.xml!
enum class PasswordAccountStorageUserState {
  // Signed-out user (and no account storage opt-in exists).
  kSignedOutUser,
  // Signed-in non-syncing user, not opted in to the account storage (but may
  // save passwords to the account storage by default).
  kSignedInUser,
  // Signed-in non-syncing user, opted in to the account storage, and saving
  // passwords to the account storage.
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
  // not signed in, or they haven't opted in to the account storage.
  kNotUsingAccountStorage = 0,
  // The user is signed in (but not syncing) and has opted in to the account
  // storage.
  kUsingAccountStorage = 1,
  // The user has enabled Sync.
  kSyncing = 2,
};

// Note on password-account-storage methods on desktop vs mobile:
// On desktop, there is an explicit per-user opt-in, and various associated
// settings (e.g. which store is the default). On mobile, there is no explicit
// opt-in, and no per-user settings.
// As a consequence, all the corresponding setters (opting in/out, setting the
// default store, etc) only exist on desktop. The getters exist on mobile too,
// but have different (usually simpler) implementation.
// Accordingly, the implementation is split up into *_common.cc, *_desktop.cc,
// and *_mobile.cc files.

// Internal helpers, not meant to be used directly:
namespace internal {
bool CanAccountStorageBeEnabled(const PrefService* pref_service,
                                const syncer::SyncService* sync_service);
bool IsUserEligibleForAccountStorage(const PrefService* pref_service,
                                     const syncer::SyncService* sync_service);
}  // namespace internal

// Whether to instantiate a second PasswordStore whose data is account-scoped.
// This doesn't necessarily mean the store is being used, e.g. this predicate
// can return true for a signed-out user. For whether the store can be used,
// see IsOptedInForAccountStorage() instead.
// TODO(b/324038136): Rename IsOptedInForAccountStorage() to
// CanUseAccountStore() - there's no opt-in on mobile platforms anyway. Rename
// CanAccountStorageBeEnabled() and IsUserEligibleForAccountStorage().
bool CanCreateAccountStore(const PrefService* pref_service);

// Whether the current signed-in user (aka unconsented primary account) has
// opted in to use the Google account storage for passwords (as opposed to
// local/profile storage). This always returns false for sync-the-feature users.
// |pref_service| must not be null.
// |sync_service| may be null (commonly the case in incognito mode), in which
// case this will simply return false.
// See PasswordFeatureManager::IsOptedInForAccountStorage.
bool IsOptedInForAccountStorage(const PrefService* pref_service,
                                const syncer::SyncService* sync_service);

// Returns the default storage location for signed-in but non-syncing users
// (i.e. will new passwords be saved to locally or to the account by default).
// Always returns an actual value, never kNotSet.
// |pref_service| must not be null.
// |sync_service| may be null (commonly the case in incognito mode), in which
// case this will return kProfileStore.
// See PasswordFeatureManager::GetDefaultPasswordStore.
PasswordForm::Store GetDefaultPasswordStore(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service);

// Returns whether the default storage location for newly-saved passwords is
// explicitly set, i.e. whether the user has made an explicit choice where to
// save. This can be used to detect "new" users, i.e. those that have never
// interacted with an account-storage-enabled Save flow yet.
// |pref_service| must not be null.
// |sync_service| may be null (commonly the case in incognito mode), in which
// case this will return false.
// See PasswordFeatureManager::IsDefaultPasswordStoreSet.
bool IsDefaultPasswordStoreSet(const PrefService* pref_service,
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

// Sets opt-in to using account storage for passwords for the current
// signed-in user (unconsented primary account).
// |pref_service| and |sync_service| must not be null.
// See PasswordFeatureManager::OptInToAccountStorage.
void OptInToAccountStorage(PrefService* pref_service,
                           syncer::SyncService* sync_service);

// Opts-out from using account storage for passwords for the
// current signed-in user (unconsented primary account). Additionally it sets
// the default password store to kProfileStore.
void OptOutOfAccountStorage(PrefService* pref_service,
                            syncer::SyncService* sync_service);

// Sets the default storage location for signed-in but non-syncing users. This
// store is used for saving new credentials and adding blacking listing entries.
// |pref_service| and |sync_service| must not be null.
// See PasswordFeatureManager::SetDefaultPasswordStore.
void SetDefaultPasswordStore(PrefService* pref_service,
                             const syncer::SyncService* sync_service,
                             PasswordForm::Store default_store);

// Clears all account-storage-related settings for all users *except* the ones
// in the passed-in |gaia_ids|. Most notably, the default password store.
// WARNING: this does not clear the opt-in!
// |pref_service| must not be null.
void KeepAccountStorageSettingsOnlyForUsers(
    PrefService* pref_service,
    const std::vector<GaiaId>& gaia_ids);

// Whether the user toggle for account storage is shown in settings.
bool ShouldShowAccountStorageSettingToggle(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service);

// Users opted into account storage used to have the choice of saving new
// passwords only locally, while keeping existing account passwords available
// for autofill. That was achieved by setting a certain "default store pref" to
// the "profile store". This logic was removed in crbug.com/369341336.
// MigrateDefaultProfileStorePref() migrates users in the legacy state to be
// completely opted out of account storage instead, i.e. so they can't save nor
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
