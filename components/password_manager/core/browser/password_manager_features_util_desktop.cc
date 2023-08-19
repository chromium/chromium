// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_features_util.h"

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
#include "components/sync/service/sync_service.h"
#include "google_apis/gaia/gaia_auth_util.h"

using password_manager::metrics_util::PasswordAccountStorageUsageLevel;
using password_manager::metrics_util::PasswordAccountStorageUserState;
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

const char kAccountStorageOptedInKey[] = "opted_in";
const char kAccountStorageDefaultStoreKey[] = "default_store";
const char kMoveToAccountStoreOfferedCountKey[] =
    "move_to_account_store_refused_count";

// Returns the total number of accounts for which an opt-in to the account
// storage exists. Used for metrics.
int GetNumberOfOptedInAccounts(const PrefService* pref_service) {
  const base::Value::Dict& global_pref =
      pref_service->GetDict(prefs::kAccountStoragePerAccountSettings);
  int count = 0;
  for (auto entry : global_pref) {
    if (entry.second.GetDict()
            .FindBool(kAccountStorageOptedInKey)
            .value_or(false)) {
      ++count;
    }
  }
  return count;
}

// Helper class for reading account storage settings for a given account.
class AccountStorageSettingsReader {
 public:
  AccountStorageSettingsReader(const PrefService* prefs,
                               const GaiaIdHash& gaia_id_hash) {
    const base::Value::Dict& global_pref =
        prefs->GetDict(prefs::kAccountStoragePerAccountSettings);
    account_settings_ = global_pref.FindDict(gaia_id_hash.ToBase64());
  }

  bool IsOptedIn() {
    if (!account_settings_) {
      return false;
    }
    return account_settings_->FindBool(kAccountStorageOptedInKey)
        .value_or(false);
  }

  PasswordForm::Store GetDefaultStore() const {
    if (!account_settings_) {
      return PasswordForm::Store::kNotSet;
    }
    absl::optional<int> value =
        account_settings_->FindInt(kAccountStorageDefaultStoreKey);
    if (!value) {
      return PasswordForm::Store::kNotSet;
    }
    return PasswordStoreFromInt(*value);
  }

  int GetMoveOfferedToNonOptedInUserCount() const {
    if (!account_settings_) {
      return 0;
    }
    return account_settings_->FindInt(kMoveToAccountStoreOfferedCountKey)
        .value_or(0);
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

  void SetOptedIn() {
    base::Value::Dict* account_settings = GetOrCreateAccountSettings();
    // The count of refusals is only tracked when the user is not opted-in.
    account_settings->Remove(kMoveToAccountStoreOfferedCountKey);
    account_settings->Set(kAccountStorageOptedInKey, true);
  }

  void SetDefaultStore(PasswordForm::Store default_store) {
    base::Value::Dict* account_settings = GetOrCreateAccountSettings();
    account_settings->Set(kAccountStorageDefaultStoreKey,
                          static_cast<int>(default_store));
  }

  void RecordMoveOfferedToNonOptedInUser() {
    base::Value::Dict* account_settings = GetOrCreateAccountSettings();
    int count = account_settings->FindInt(kMoveToAccountStoreOfferedCountKey)
                    .value_or(0);
    account_settings->Set(kMoveToAccountStoreOfferedCountKey, ++count);
  }

  void ClearAllSettings() { update_->Remove(account_hash_); }

 private:
  ScopedDictPrefUpdate update_;
  const std::string account_hash_;
};

}  // namespace

bool IsOptedInForAccountStorage(const PrefService* pref_service,
                                const syncer::SyncService* sync_service) {
  DCHECK(pref_service);

  // If the account storage can't be enabled (e.g. because the feature flag was
  // turned off), then don't consider the user opted in, even if the pref is
  // set.
  // Note: IsUserEligibleForAccountStorage() is not appropriate here, because
  // a) Sync-the-feature users are not considered eligible, but might have
  //    opted in before turning on Sync, and
  // b) eligibility requires IsEngineInitialized() (i.e. will be false for a
  //    few seconds after browser startup).
  if (!internal::CanAccountStorageBeEnabled(sync_service)) {
    return false;
  }

  // If there's no signed-in account, there can be no opt-in.
  std::string gaia_id = sync_service->GetAccountInfo().gaia;
  if (gaia_id.empty()) {
    return false;
  }

  return AccountStorageSettingsReader(pref_service,
                                      GaiaIdHash::FromGaiaId(gaia_id))
      .IsOptedIn();
}

bool ShouldShowAccountStorageOptIn(const PrefService* pref_service,
                                   const syncer::SyncService* sync_service) {
  DCHECK(pref_service);

  // Show the opt-in if the user is eligible, but not yet opted in.
  return internal::IsUserEligibleForAccountStorage(sync_service) &&
         !IsOptedInForAccountStorage(pref_service, sync_service);
}

bool ShouldShowAccountStorageReSignin(const PrefService* pref_service,
                                      const syncer::SyncService* sync_service,
                                      const GURL& current_page_url) {
  DCHECK(pref_service);

  // Checks that the sync_service is not null and the feature is enabled.
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
  return base::ranges::any_of(
      pref_service->GetDict(prefs::kAccountStoragePerAccountSettings),
      [](const std::pair<std::string, const base::Value&>& p) {
        return p.second.GetDict()
            .FindBool(kAccountStorageOptedInKey)
            .value_or(false);
      });
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
    bool save_to_profile_store =
        !IsOptedInForAccountStorage(pref_service, sync_service);
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
                           const syncer::SyncService* sync_service) {
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
  ScopedAccountStorageSettingsUpdate(pref_service,
                                     GaiaIdHash::FromGaiaId(gaia_id))
      .SetOptedIn();

  // Record the total number of (now) opted-in accounts.
  base::UmaHistogramExactLinear(
      "PasswordManager.AccountStorage.NumOptedInAccountsAfterOptIn",
      GetNumberOfOptedInAccounts(pref_service), 10);
}

void OptOutOfAccountStorageAndClearSettings(
    PrefService* pref_service,
    const syncer::SyncService* sync_service) {
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

  OptOutOfAccountStorageAndClearSettingsForAccount(pref_service, gaia_id);
}

void OptOutOfAccountStorageAndClearSettingsForAccount(
    PrefService* pref_service,
    const std::string& gaia_id) {
  ScopedAccountStorageSettingsUpdate(pref_service,
                                     GaiaIdHash::FromGaiaId(gaia_id))
      .ClearAllSettings();

  // Record the total number of (still) opted-in accounts.
  base::UmaHistogramExactLinear(
      "PasswordManager.AccountStorage.NumOptedInAccountsAfterOptOut",
      GetNumberOfOptedInAccounts(pref_service), 10);
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

void ClearAccountStorageSettingsForAllUsers(PrefService* pref_service) {
  DCHECK(pref_service);
  pref_service->ClearPref(prefs::kAccountStoragePerAccountSettings);
}

void RecordMoveOfferedToNonOptedInUser(
    PrefService* pref_service,
    const syncer::SyncService* sync_service) {
  DCHECK(pref_service);
  DCHECK(sync_service);
  std::string gaia_id = sync_service->GetAccountInfo().gaia;
  DCHECK(!gaia_id.empty());
  DCHECK(!AccountStorageSettingsReader(pref_service,
                                       GaiaIdHash::FromGaiaId(gaia_id))
              .IsOptedIn());
  ScopedAccountStorageSettingsUpdate(pref_service,
                                     GaiaIdHash::FromGaiaId(gaia_id))
      .RecordMoveOfferedToNonOptedInUser();
}

int GetMoveOfferedToNonOptedInUserCount(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service) {
  DCHECK(pref_service);
  DCHECK(sync_service);
  std::string gaia_id = sync_service->GetAccountInfo().gaia;
  DCHECK(!gaia_id.empty());
  AccountStorageSettingsReader reader(pref_service,
                                      GaiaIdHash::FromGaiaId(gaia_id));
  DCHECK(!reader.IsOptedIn());
  return reader.GetMoveOfferedToNonOptedInUserCount();
}

// Note: See also password_manager_features_util_common.cc for shared
// (cross-platform) and password_manager_features_util_mobile.cc for
// mobile-specific implementations.

}  // namespace password_manager::features_util
