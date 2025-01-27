// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"

using signin::GaiaIdHash;

namespace password_manager::features_util {

bool ShouldShowAccountStorageSettingToggle(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service) {
  return internal::IsUserEligibleForAccountStorage(pref_service, sync_service);
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

// Note: See also password_manager_features_util_common.cc for shared
// (cross-platform) and password_manager_features_util_mobile.cc for
// mobile-specific implementations.

}  // namespace password_manager::features_util
