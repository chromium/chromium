// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/features/password_manager_features_util.h"

#include "build/build_config.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace password_manager::features_util {

namespace internal {

// Returns whether the account-scoped password storage can be enabled in
// principle for the current profile. This is constant for a given profile
// (until browser restart).
bool CanAccountStorageBeEnabled(const syncer::SyncService* sync_service) {
  if (!base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage)) {
    return false;
  }

  // |sync_service| is null in incognito mode, or if --disable-sync was
  // specified on the command-line.
  if (!sync_service) {
    return false;
  }

  // The account-scoped password storage does not work with LocalSync aka
  // roaming profiles.
  if (sync_service->IsLocalSyncEnabled()) {
    return false;
  }

  return true;
}

// Whether the currently signed-in user (if any) is eligible for using the
// account-scoped password storage. This is the case if:
// - The account storage can be enabled in principle.
// - Sync-the-feature is NOT enabled (if it is, there's only a single combined
//   storage).
// - Sync-the-transport is enabled (i.e. there's a signed-in user, Sync is not
//   disabled by policy, etc).
// - Desktop-only: There is no custom passphrase (because Sync transport offers
//   no way to enter the passphrase yet).
bool IsUserEligibleForAccountStorage(const syncer::SyncService* sync_service) {
  if (!CanAccountStorageBeEnabled(sync_service)) {
    return false;
  }
  DCHECK(sync_service);
  // TODO(crbug.com/1462978): Delete this when ConsentLevel::kSync is deleted.
  // See ConsentLevel::kSync documentation for details.
  if (sync_service->IsSyncFeatureEnabled()) {
    return false;
  }
  switch (sync_service->GetTransportState()) {
    case syncer::SyncService::TransportState::DISABLED:
    case syncer::SyncService::TransportState::PAUSED:
      return false;
    case syncer::SyncService::TransportState::START_DEFERRED:
    case syncer::SyncService::TransportState::INITIALIZING:
    case syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION:
    case syncer::SyncService::TransportState::CONFIGURING:
    case syncer::SyncService::TransportState::ACTIVE:
      break;
  }
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  if (sync_service->GetUserSettings()->IsUsingExplicitPassphrase()) {
    return false;
  }
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  return true;
}

}  // namespace internal

bool ShouldShowAccountStorageBubbleUi(const PrefService* pref_service,
                                      const syncer::SyncService* sync_service) {
  // Opted in implies eligible, so that case is covered here too.
  return internal::IsUserEligibleForAccountStorage(sync_service);
}

PasswordAccountStorageUserState ComputePasswordAccountStorageUserState(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service) {
  DCHECK(pref_service);
  // The SyncService can be null in incognito, or due to a commandline flag. In
  // those cases, simply consider the user as signed out.
  if (!sync_service) {
    return PasswordAccountStorageUserState::kSignedOutUser;
  }

  // TODO(crbug.com/1462978): Delete this when ConsentLevel::kSync is deleted.
  // See ConsentLevel::kSync documentation for details.
  if (sync_service->IsSyncFeatureEnabled()) {
    return PasswordAccountStorageUserState::kSyncUser;
  }

  if (sync_service->HasDisableReason(
          syncer::SyncService::DisableReason::DISABLE_REASON_NOT_SIGNED_IN)) {
    // Signed out. Check if any account storage opt-in exists.
    return ShouldShowAccountStorageReSignin(pref_service, sync_service, GURL())
               ? PasswordAccountStorageUserState::kSignedOutAccountStoreUser
               : PasswordAccountStorageUserState::kSignedOutUser;
  }

  bool saving_locally = IsDefaultPasswordStoreSet(pref_service, sync_service) &&
                        GetDefaultPasswordStore(pref_service, sync_service) ==
                            PasswordForm::Store::kProfileStore;

  // Signed in. Check for account storage opt-in.
  if (IsOptedInForAccountStorage(pref_service, sync_service)) {
    // Signed in and opted in. Check default storage location.
    return saving_locally
               ? PasswordAccountStorageUserState::
                     kSignedInAccountStoreUserSavingLocally
               : PasswordAccountStorageUserState::kSignedInAccountStoreUser;
  }

  // Signed in but not opted in. Check default storage location.
  return saving_locally
             ? PasswordAccountStorageUserState::kSignedInUserSavingLocally
             : PasswordAccountStorageUserState::kSignedInUser;
}

PasswordAccountStorageUsageLevel ComputePasswordAccountStorageUsageLevel(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service) {
  using UserState = PasswordAccountStorageUserState;
  using UsageLevel = PasswordAccountStorageUsageLevel;
  switch (ComputePasswordAccountStorageUserState(pref_service, sync_service)) {
    case UserState::kSignedOutUser:
    case UserState::kSignedOutAccountStoreUser:
    case UserState::kSignedInUser:
    case UserState::kSignedInUserSavingLocally:
      return UsageLevel::kNotUsingAccountStorage;
    case UserState::kSignedInAccountStoreUser:
    case UserState::kSignedInAccountStoreUserSavingLocally:
      return UsageLevel::kUsingAccountStorage;
    case UserState::kSyncUser:
      return UsageLevel::kSyncing;
  }
}

// Note: See also password_manager_features_util_desktop.cc for desktop-specific
// and password_manager_features_util_mobile.cc for mobile-specific
// implementations.

}  // namespace password_manager::features_util
