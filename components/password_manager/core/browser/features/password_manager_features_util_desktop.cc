// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/features/password_manager_features_util.h"

#include <algorithm>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "google_apis/gaia/gaia_auth_util.h"

using signin::GaiaIdHash;

namespace password_manager::features_util {

namespace {

PasswordForm::Store PasswordStoreFromInt(int value) {
  switch (value) {
    case static_cast<int>(PasswordForm::Store::kProfileStore):
      return PasswordForm::Store::kProfileStore;
    case static_cast<int>(PasswordForm::Store::kAccountStore):
      return PasswordForm::Store::kAccountStore;
  }
  return PasswordForm::Store::kNotSet;
}

const char kAccountStorageDefaultStoreKey[] = "default_store";

// Helper class for reading account storage settings for a given account.
class AccountStorageSettingsReader {
 public:
  AccountStorageSettingsReader(const PrefService* prefs,
                               const GaiaIdHash& gaia_id_hash) {
    const base::Value::Dict& global_pref =
        prefs->GetDict(prefs::kAccountStoragePerAccountSettings);
    account_settings_ = global_pref.FindDict(gaia_id_hash.ToBase64());
  }

  PasswordForm::Store GetDefaultStore() const {
    if (!account_settings_) {
      return PasswordForm::Store::kNotSet;
    }
    std::optional<int> value =
        account_settings_->FindInt(kAccountStorageDefaultStoreKey);
    if (!value) {
      return PasswordForm::Store::kNotSet;
    }
    return PasswordStoreFromInt(*value);
  }

 private:
  // May be null, if no settings for this account were saved yet.
  raw_ptr<const base::Value::Dict> account_settings_ = nullptr;
};

// Helper class for updating account storage settings for a given account. Like
// with ScopedDictPrefUpdate, updates are only published once the instance gets
// destroyed.
class ScopedAccountStorageSettingsUpdate {
 public:
  ScopedAccountStorageSettingsUpdate(PrefService* prefs,
                                     const GaiaIdHash& gaia_id_hash)
      : update_(prefs, prefs::kAccountStoragePerAccountSettings),
        account_hash_(gaia_id_hash.ToBase64()) {}

  base::Value::Dict* GetOrCreateAccountSettings() {
    return update_->EnsureDict(account_hash_);
  }

  void SetDefaultStore(PasswordForm::Store default_store) {
    base::Value::Dict* account_settings = GetOrCreateAccountSettings();
    account_settings->Set(kAccountStorageDefaultStoreKey,
                          static_cast<int>(default_store));
  }

  void ClearAllSettings() { update_->Remove(account_hash_); }

 private:
  ScopedDictPrefUpdate update_;
  const std::string account_hash_;
};

}  // namespace

bool ShouldShowAccountStorageOptIn(const syncer::SyncService* sync_service) {
  // Show the opt-in if the user is eligible, but not yet opted in.
  return internal::IsUserEligibleForAccountStorage(sync_service) &&
         !IsOptedInForAccountStorage(sync_service);
}

bool ShouldShowAccountStorageReSignin(const syncer::SyncService* sync_service,
                                      const GURL& current_page_url) {
  // Checks that the sync_service is not null and the feature is enabled.
  // IsUserEligibleForAccountStorage() doesn't fit because it's false for
  // signed-out users.
  if (!internal::CanAccountStorageBeEnabled(sync_service)) {
    return false;  // Opt-in wouldn't work here, so don't show the re-signin.
  }

  // In order to show a re-signin prompt, no user may be logged in.
  if (!sync_service->HasDisableReason(
          syncer::SyncService::DisableReason::DISABLE_REASON_NOT_SIGNED_IN)) {
    return false;
  }

  if (gaia::HasGaiaSchemeHostPort(current_page_url)) {
    return false;
  }

  // Show the opt-in if any known previous user opted into using the account
  // storage before and might want to access it again.
  return sync_service->GetUserSettings()
             ->GetNumberOfAccountsWithPasswordsSelected() > 0;
}

PasswordForm::Store GetDefaultPasswordStore(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service) {
  DCHECK(pref_service);

  if (!internal::IsUserEligibleForAccountStorage(sync_service)) {
    return PasswordForm::Store::kProfileStore;
  }

  std::string gaia_id = sync_service->GetAccountInfo().gaia;
  if (gaia_id.empty()) {
    return PasswordForm::Store::kProfileStore;
  }

  PasswordForm::Store default_store =
      AccountStorageSettingsReader(pref_service,
                                   GaiaIdHash::FromGaiaId(gaia_id))
          .GetDefaultStore();
  // If none of the early-outs above triggered, then we *can* save to the
  // account store in principle (though the user might not have opted in to that
  // yet).
  if (default_store == PasswordForm::Store::kNotSet) {
    // The default store depends on the opt-in state. If the user has not opted
    // in, then saves go to the profile store by default. If the user *has*
    // opted in, then they've chosen to save to the account, so that becomes the
    // default.
    bool save_to_profile_store = !IsOptedInForAccountStorage(sync_service);
    return save_to_profile_store ? PasswordForm::Store::kProfileStore
                                 : PasswordForm::Store::kAccountStore;
  }
  return default_store;
}

bool IsDefaultPasswordStoreSet(const PrefService* pref_service,
                               const syncer::SyncService* sync_service) {
  DCHECK(pref_service);

  if (!sync_service) {
    return false;
  }

  std::string gaia_id = sync_service->GetAccountInfo().gaia;
  if (gaia_id.empty()) {
    return false;
  }

  PasswordForm::Store default_store =
      AccountStorageSettingsReader(pref_service,
                                   GaiaIdHash::FromGaiaId(gaia_id))
          .GetDefaultStore();
  return default_store != PasswordForm::Store::kNotSet;
}

void OptInToAccountStorage(PrefService* pref_service,
                           syncer::SyncService* sync_service) {
  DCHECK(pref_service);
  DCHECK(sync_service);
  DCHECK(
      base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage));

  std::string gaia_id = sync_service->GetAccountInfo().gaia;
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

  // Record the total number of (now) opted-in accounts.
  base::UmaHistogramExactLinear(
      "PasswordManager.AccountStorage.NumOptedInAccountsAfterOptIn",
      sync_user_settings->GetNumberOfAccountsWithPasswordsSelected(), 10);
}

void OptOutOfAccountStorageAndClearSettings(PrefService* pref_service,
                                            syncer::SyncService* sync_service) {
  DCHECK(pref_service);
  DCHECK(sync_service);
  DCHECK(
      base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage));

  std::string gaia_id = sync_service->GetAccountInfo().gaia;
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
  ScopedAccountStorageSettingsUpdate(pref_service,
                                     GaiaIdHash::FromGaiaId(gaia_id))
      .ClearAllSettings();

  // Record the total number of (still) opted-in accounts.
  base::UmaHistogramExactLinear(
      "PasswordManager.AccountStorage.NumOptedInAccountsAfterOptOut",
      sync_user_settings->GetNumberOfAccountsWithPasswordsSelected(), 10);
}

void SetDefaultPasswordStore(PrefService* pref_service,
                             const syncer::SyncService* sync_service,
                             PasswordForm::Store default_store) {
  DCHECK(pref_service);
  DCHECK(sync_service);
  DCHECK(
      base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage));

  std::string gaia_id = sync_service->GetAccountInfo().gaia;
  if (gaia_id.empty()) {
    // Maybe the account went away since the UI was shown. This should be rare,
    // but is ultimately harmless - just do nothing here.
    return;
  }

  ScopedAccountStorageSettingsUpdate(pref_service,
                                     GaiaIdHash::FromGaiaId(gaia_id))
      .SetDefaultStore(default_store);

  base::UmaHistogramEnumeration("PasswordManager.DefaultPasswordStoreSet",
                                default_store);
}

void KeepAccountStorageSettingsOnlyForUsers(
    PrefService* pref_service,
    const std::vector<std::string>& gaia_ids) {
  DCHECK(pref_service);

  // Build a set of hashes of all the Gaia IDs.
  auto hashes_to_keep =
      base::MakeFlatSet<std::string>(gaia_ids, {}, [](const auto& gaia_id) {
        return GaiaIdHash::FromGaiaId(gaia_id).ToBase64();
      });

  // Now remove any settings for account that are *not* in the set of hashes.
  // DictionaryValue doesn't allow removing elements while iterating, so first
  // collect all the keys to remove, then actually remove them in a second pass.
  ScopedDictPrefUpdate update(pref_service,
                              prefs::kAccountStoragePerAccountSettings);
  std::vector<std::string> keys_to_remove;
  for (auto kv : *update) {
    if (!hashes_to_keep.contains(kv.first)) {
      keys_to_remove.push_back(kv.first);
    }
  }
  for (const std::string& key_to_remove : keys_to_remove) {
    update->Remove(key_to_remove);
  }
}

void MigrateOptInPrefToSyncSelectedTypes(PrefService* pref_service) {
  const char kLegacyAccountStorageOptedInKey[] = "opted_in";

  ScopedDictPrefUpdate legacy_pref_update(
      pref_service, prefs::kAccountStoragePerAccountSettings);
  ScopedDictPrefUpdate new_pref_update(
      pref_service, syncer::prefs::internal::kSelectedTypesPerAccount);
  for (auto [serialized_gaia_id_hash, settings] : *legacy_pref_update) {
    // `settings` should be a dict but check to avoid a possible startup crash.
    if (!settings.is_dict()) {
      continue;
    }
    if (settings.GetDict()
            .FindBool(kLegacyAccountStorageOptedInKey)
            .value_or(false)) {
      // Sync doesn't expose an API to set selected types for an arbitrary
      // account, so manipulate the underlying prefs directly. The serialization
      // for the gaia id hash is indeed the same, unit tests verify that by
      // invoking GetSelectedTypes() after the migration.
      new_pref_update->EnsureDict(serialized_gaia_id_hash)
          ->Set(syncer::prefs::internal::kSyncPasswords, true);
    }
    settings.GetDict().Remove(kLegacyAccountStorageOptedInKey);
  }
}

// Note: See also password_manager_features_util_common.cc for shared
// (cross-platform) and password_manager_features_util_mobile.cc for
// mobile-specific implementations.

}  // namespace password_manager::features_util
