// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/features/password_manager_features_util.h"

#include <optional>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace password_manager::features_util {

namespace {

bool IsUserEligibleForAccountStorage(const syncer::SyncService* sync_service) {
  if (!sync_service) {
    return false;
  }

  if (sync_service->IsLocalSyncEnabled()) {
    return false;
  }

#if !BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40067058): Delete this when ConsentLevel::kSync is deleted.
  // See ConsentLevel::kSync documentation for details.
  if (sync_service->IsSyncFeatureEnabled()) {
    return false;
  }
#endif

  switch (sync_service->GetTransportState()) {
    case syncer::SyncService::TransportState::DISABLED:
    // Disable in case of an auth error, because then the account store can't
    // upload data. Worse: signing out in this state might not clear the store,
    // and the leftover data might end up in another account that signs in
    // later. See crbug.com/1426774 for a case where this happened with
    // encryption errors.
    //
    // TODO(crbug.com/40262289): Right now the code checks for auth and
    // encryption errors (below) individually. Checking IsTrackingMetadata()
    // would take care of all kinds of errors, future and present.
    case syncer::SyncService::TransportState::PAUSED:
      return false;
    case syncer::SyncService::TransportState::START_DEFERRED:
    case syncer::SyncService::TransportState::INITIALIZING:
    case syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION:
    case syncer::SyncService::TransportState::CONFIGURING:
    case syncer::SyncService::TransportState::ACTIVE:
      break;
  }

  // Disable in case of encryption errors, following the same rationale as auth
  // errors above. The presence of encryption errors can only be checked once
  // the sync engine is initialized. Until then we give it the benefit of the
  // doubt and say account storage is enabled.
  // Note: IsPassphraseRequired() and IsTrustedVaultKeyRequired() seem to be
  // always false until the engine is up, so maybe there's no need to check
  // IsEngineInitialized() here.
  if (sync_service->IsEngineInitialized() &&
      (sync_service->GetUserSettings()->IsPassphraseRequired() ||
       sync_service->GetUserSettings()->IsTrustedVaultKeyRequired())) {
    return false;
  }

  return true;
}

}  // namespace

bool IsAccountStorageEnabled(const syncer::SyncService* sync_service) {
  return IsUserEligibleForAccountStorage(sync_service) &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kPasswords);
}

PasswordAccountStorageUserState ComputePasswordAccountStorageUserState(
    const syncer::SyncService* sync_service) {
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
    return PasswordAccountStorageUserState::kSignedOutUser;
  }

  if (IsAccountStorageEnabled(sync_service)) {
    return PasswordAccountStorageUserState::kSignedInAccountStoreUser;
  }

  return PasswordAccountStorageUserState::kSignedInUser;
}

PasswordAccountStorageUsageLevel ComputePasswordAccountStorageUsageLevel(
    const syncer::SyncService* sync_service) {
  using UserState = PasswordAccountStorageUserState;
  using UsageLevel = PasswordAccountStorageUsageLevel;
  switch (ComputePasswordAccountStorageUserState(sync_service)) {
    case UserState::kSignedOutUser:
    case UserState::kSignedInUser:
      return UsageLevel::kNotUsingAccountStorage;
    case UserState::kSignedInAccountStoreUser:
      return UsageLevel::kUsingAccountStorage;
    case UserState::kSyncUser:
      return UsageLevel::kSyncing;
  }
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
bool ShouldShowAccountStorageSettingToggle(
    const syncer::SyncService* sync_service) {
  // TODO(crbug.com/303613699): Merge IsUserEligibleForAccountStorage() and
  // IsAccountStorageEnabled() after kReplaceSyncPromosWithSignInPromos is
  // launched and cleaned-up.
  return IsUserEligibleForAccountStorage(sync_service) &&
         !base::FeatureList::IsEnabled(
             syncer::kReplaceSyncPromosWithSignInPromos);
}

void MigrateDefaultProfileStorePref(PrefService* pref_service) {
  ScopedDictPrefUpdate new_pref_update(
      pref_service, syncer::prefs::internal::kSelectedTypesPerAccount);
  for (auto [serialized_gaia_id_hash, settings] : pref_service->GetDict(
           prefs::kObsoleteAccountStoragePerAccountSettings)) {
    // `settings` should be a dict but check to avoid a possible startup crash.
    if (!settings.is_dict()) {
      continue;
    }
    if (settings.GetDict().FindInt(kObsoleteAccountStorageDefaultStoreKey) ==
        static_cast<int>(PasswordForm::Store::kProfileStore)) {
      // kObsoleteAccountStoragePerAccountSettings' serialization for the gaia
      // id hash was indeed base64, the same as used by sync. Tests verify it.
      new_pref_update->EnsureDict(serialized_gaia_id_hash)
          ->Set(syncer::prefs::internal::kSyncPasswords, false);
    }
  }
  pref_service->ClearPref(prefs::kObsoleteAccountStoragePerAccountSettings);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace password_manager::features_util
