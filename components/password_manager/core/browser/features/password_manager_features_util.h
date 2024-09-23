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
  // Signed-out user (and no account storage opt-in exists).
  kSignedOutUser = 0,
  // Signed-out user, but an account storage opt-in exists.
  kSignedOutAccountStoreUser = 1,
  // Signed-in non-syncing user, not opted in to the account storage (but may
  // save passwords to the account storage by default).
  kSignedInUser = 2,
  // Signed-in non-syncing user, not opted in to the account storage, and has
  // explicitly chosen to save passwords only on the device.
  kSignedInUserSavingLocally = 3,
  // Signed-in non-syncing user, opted in to the account storage, and saving
  // passwords to the account storage.
  kSignedInAccountStoreUser = 4,
  // Signed-in non-syncing user and opted in to the account storage, but has
  // chosen to save passwords only on the device.
  kSignedInAccountStoreUserSavingLocally = 5,
  // Syncing user.
  kSyncUser = 6,
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

// Whether it makes sense to ask the user to opt-in for account-based
// password storage. This is true if the opt-in doesn't exist yet, but all
// other requirements are met (i.e. there is a signed-in user, Sync-the-feature
// is not enabled, etc).
// |pref_service| must not be null.
// |sync_service| may be null (commonly the case in incognito mode), in which
// case this will simply return false.
// See PasswordFeatureManager::ShouldShowAccountStorageOptIn.
bool ShouldShowAccountStorageOptIn(const PrefService* pref_service,
                                   const syncer::SyncService* sync_service);

// Whether it makes sense to ask the user to signin again to access the
// account-based password storage. This is true if a user on this device
// previously opted into using the account store but is signed-out now.
// |current_page_url| is the current URL, used to suppress the promo on the
// Google signin page (no point in asking the user to sign in while they're
// already doing that). For non-web contexts (e.g. native UIs), it is valid to
// pass an empty GURL.
// See PasswordFeatureManager::ShouldShowAccountStorageReSignin.
bool ShouldShowAccountStorageReSignin(const PrefService* pref_service,
                                      const syncer::SyncService* sync_service,
                                      const GURL& current_page_url);

// Whether it makes sense to ask the user to move a password to their account or
// in which store to save a password (i.e. profile or account store). This
// is true if the user has opted in already, or hasn't opted in but all other
// requirements are met (i.e. there is a signed-in user, Sync-the-feature is not
// enabled, etc). |pref_service| must not be null. |sync_service| may be null
// (commonly the case in incognito mode), in which case this will simply return
// false. See PasswordFeatureManager::ShouldShowPasswordStorePicker.
// TODO(crbug.com/40261471): This predicate is kinda confusing, especially on
// mobile. Consider splitting it in two, "should offer move" and "should offer
// store choice".
bool ShouldShowAccountStorageBubbleUi(const PrefService* pref_service,
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
// current signed-in user (unconsented primary account). Addditionally it sets
// the default password store to kProfileStore.
void OptOutOfAccountStorage(PrefService* pref_service,
                            syncer::SyncService* sync_service);

// Clears the opt-in to using account storage for passwords for the
// current signed-in user (unconsented primary account), as well as all other
// associated settings (e.g. default store choice).
// WARNING: this does not clear the account key from prefs like
// KeepAccountStorageSettingsOnlyForUsers().
// |pref_service| and |sync_service| must not be null.
// See PasswordFeatureManager::OptOutOfAccountStorageAndClearSettings.
void OptOutOfAccountStorageAndClearSettings(PrefService* pref_service,
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
    const std::vector<std::string>& gaia_ids);

// Migrates the old password_manager account storage opt-in pref to
// SyncUserSettings::GetSelectedTypes(), see crbug.com/1484531.
// TODO(crbug.com/40943534): Delete the migration when appropriate, see bug.
void MigrateOptInPrefToSyncSelectedTypes(PrefService* pref_service);

// When the user declines the opt-in offer during saving, the default
// store pref is set to kProfileStore, to signal the opt-in shouldn't be
// offered again (see PasswordFeatureManagerImpl::
// ShouldOfferOptInAndMoveToAccountStoreAfterSavingLocally()).
// This function additionally ensures that the opt-in pref is explicitly
// set to false in that state, which is relevant for GetUserSettings().
// Opt-in offers from other flows are unaffected (e.g. filling).
// See crbug.com/1509865.
void MigrateDeclinedSaveOptInToExplicitOptOut(PrefService* pref_service);

// Whether the user toggle for account storage is shown in settings.
bool ShouldShowAccountStorageSettingToggle(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service);

// Necessary but not sufficient condition to promote account storage opt-in
// for users currently opted-out. In particular for
// ShouldShowAccountStorageOptIn() and ShouldShowAccountStorageReSignin().
// If the promos are disallowed, no reauth is required to enable the feature.
bool AreAccountStorageOptInPromosAllowed();

#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

}  // namespace password_manager::features_util

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_MANAGER_FEATURES_UTIL_H_
