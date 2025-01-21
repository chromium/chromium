// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
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

void OptInToAccountStorage(PrefService* pref_service,
                           syncer::SyncService* sync_service) {
  DCHECK(pref_service);
  DCHECK(sync_service);
  CHECK(CanCreateAccountStore(pref_service));

  const GaiaId gaia_id = sync_service->GetAccountInfo().gaia;
  if (gaia_id.empty()) {
    // Maybe the account went away since the opt-in UI was shown. This should be
    // rare, but is ultimately harmless - just do nothing here.
    return;
  }
  if (sync_service->IsSyncFeatureEnabled()) {
    // Same as above, maybe the user enabled sync since the UI was shown. This
    // should be rare, but is ultimately harmless - just do nothing here.
    return;
  }
  syncer::SyncUserSettings* sync_user_settings =
      sync_service->GetUserSettings();
  sync_user_settings->SetSelectedType(syncer::UserSelectableType::kPasswords,
                                      /*is_type_on=*/true);

  // TODO(crbug.com/369341336): Replace this and the opt-out function with
  // direct calls to SyncUserSettings().
}

void OptOutOfAccountStorage(PrefService* pref_service,
                            syncer::SyncService* sync_service) {
  CHECK(pref_service);
  CHECK(sync_service);

  const GaiaId gaia_id = sync_service->GetAccountInfo().gaia;
  if (gaia_id.empty()) {
    // In rare cases, it could happen that the account went away since the
    // opt-out UI was triggered.
    return;
  }

  // Note SyncUserSettings::SetSelectedType() won't clear the gaia id hash
  // but that's not required here.
  syncer::SyncUserSettings* sync_user_settings =
      sync_service->GetUserSettings();
  sync_user_settings->SetSelectedType(syncer::UserSelectableType::kPasswords,
                                      false);
}

void SetDefaultPasswordStore(PrefService* pref_service,
                             const syncer::SyncService* sync_service,
                             PasswordForm::Store default_store) {
  // TODO(crbug.com/369341336): Delete this function.
}

void KeepAccountStorageSettingsOnlyForUsers(
    PrefService* pref_service,
    const std::vector<GaiaId>& gaia_ids) {
  // TODO(crbug.com/369341336): Delete this function.
}

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
