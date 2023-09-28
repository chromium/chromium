// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/features/password_manager_features_util.h"

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace password_manager::features_util {

bool IsOptedInForAccountStorage(const PrefService* pref_service,
                                const syncer::SyncService* sync_service) {
  DCHECK(pref_service);

  if (!internal::IsUserEligibleForAccountStorage(sync_service)) {
    return false;
  }

  // On Android and iOS, there is no explicit opt-in - this is handled through
  // Sync's selected data types instead.
  if (!sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kPasswords)) {
    return false;
  }

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
  // TODO(crbug.com/1426774): Hook this code to IsTrackingMetadata().
  if (sync_service->GetUserSettings()->IsPassphraseRequired() ||
      sync_service->GetUserSettings()->IsTrustedVaultKeyRequired()) {
    return false;
  }

  return true;
}

bool ShouldShowAccountStorageOptIn(const PrefService* pref_service,
                                   const syncer::SyncService* sync_service) {
  return false;
}

bool ShouldShowAccountStorageReSignin(const PrefService* pref_service,
                                      const syncer::SyncService* sync_service,
                                      const GURL& current_page_url) {
  // On Android and iOS, there is no re-signin promo.
  return false;
}

PasswordForm::Store GetDefaultPasswordStore(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service) {
  DCHECK(pref_service);

  if (!internal::IsUserEligibleForAccountStorage(sync_service)) {
    return PasswordForm::Store::kProfileStore;
  }

  return IsOptedInForAccountStorage(pref_service, sync_service)
             ? PasswordForm::Store::kAccountStore
             : PasswordForm::Store::kProfileStore;
}

bool IsDefaultPasswordStoreSet(const PrefService* pref_service,
                               const syncer::SyncService* sync_service) {
  // The default store is never explicitly set on Android or iOS.
  return false;
}

// Note: See also password_manager_features_util_common.cc for shared
// (cross-platform) and password_manager_features_util_desktop.cc for
// mobile-specific implementations.

}  // namespace password_manager::features_util
