// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace password_manager::features_util {

namespace internal {

// Returns whether the account-scoped password storage can be enabled in
// principle for the current profile. This is constant for a given profile
// (until browser restart).
bool CanAccountStorageBeEnabled(const PrefService* pref_service,
                                const syncer::SyncService* sync_service) {
  CHECK(pref_service);

  if (!CanCreateAccountStore(pref_service)) {
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
bool IsUserEligibleForAccountStorage(const PrefService* pref_service,
                                     const syncer::SyncService* sync_service) {
  if (!sync_service) {
    return false;
  }

  // TODO(crbug.com/40067058): Delete this when ConsentLevel::kSync is deleted.
  // See ConsentLevel::kSync documentation for details.
  // Eligibility for account storage is controlled by separate flags for syncing
  // and non-syncing users. Enabling the flag is a necessary condition but not
  // sufficient, other checks follow below.
  if (sync_service->IsSyncFeatureEnabled()) {
    if (!base::FeatureList::IsEnabled(
            syncer::kEnablePasswordsAccountStorageForSyncingUsers)) {
      return false;
    }
  } else {
    if (!base::FeatureList::IsEnabled(
            syncer::kEnablePasswordsAccountStorageForNonSyncingUsers)) {
      return false;
    }
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

  // Check last to avoid tests for signed-out users unnecessarily having to
  // register some prefs to avoid a crash.
  return CanAccountStorageBeEnabled(pref_service, sync_service);
}

}  // namespace internal

bool CanCreateAccountStore(const PrefService* pref_service) {
#if BUILDFLAG(IS_ANDROID)
  using password_manager::prefs::UseUpmLocalAndSeparateStoresState;
  switch (
      static_cast<UseUpmLocalAndSeparateStoresState>(pref_service->GetInteger(
          password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores))) {
    case UseUpmLocalAndSeparateStoresState::kOff:
      return false;
    // The decision regarding the presence/absence of the account store happens
    // before the outcome of the migration is known. The decision shouldn't
    // change, many layers cache a pointer to the store and never update it.
    // The solution is to optimistically return true in the "migration pending"
    // state. If the migration later fails, the store will continue to exist
    // until the next restart, but won't actually be used (this is enforced by
    // other predicates).
    case UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending:
    case UseUpmLocalAndSeparateStoresState::kOn:
      return true;
  }
  NOTREACHED();
#else
  return true;
#endif
}

bool IsOptedInForAccountStorage(const PrefService* pref_service,
                                const syncer::SyncService* sync_service) {
  if (!internal::IsUserEligibleForAccountStorage(pref_service, sync_service)) {
    return false;
  }

  if (!sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kPasswords)) {
    return false;
  }

// TODO(crbug.com/40262917): Enable the checks below on Desktop too.
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  // From this point on, we want to check for encryption errors, which we can
  // only do when the engine is initialized. In that meantime, we give it the
  // benefit of the doubt and say the user is opted in.
  if (!sync_service->IsEngineInitialized()) {
    return true;
  }

  // Encryption errors mean the account store can't upload data, which is bad.
  // Worse: in some cases sign-out might not clear the store. If another user
  // signs in later, the leftover data might end up in their account, see
  // crbug.com/1426774.
  // TODO(crbug.com/40262289): Hook this code to IsTrackingMetadata().
  if (sync_service->GetUserSettings()->IsPassphraseRequired() ||
      sync_service->GetUserSettings()->IsTrustedVaultKeyRequired()) {
    return false;
  }
#endif

  return true;
}

bool ShouldShowAccountStorageBubbleUi(const PrefService* pref_service,
                                      const syncer::SyncService* sync_service) {
  // Opted in implies eligible, so that case is covered here too.
  return internal::IsUserEligibleForAccountStorage(pref_service, sync_service);
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

  // TODO(crbug.com/40067058): Delete this when ConsentLevel::kSync is deleted.
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
