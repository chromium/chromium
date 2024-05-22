// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace password_manager::features_util {

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

  if (!internal::IsUserEligibleForAccountStorage(pref_service, sync_service)) {
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
