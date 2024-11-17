// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/account_pref_utils.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/gaia_id_hash.h"

namespace syncer {

const base::Value* GetAccountKeyedPrefValue(
    const PrefService* pref_service,
    const char* pref_path,
    const signin::GaiaIdHash& gaia_id_hash) {
  return pref_service->GetDict(pref_path).Find(gaia_id_hash.ToBase64());
}

void SetAccountKeyedPrefValue(PrefService* pref_service,
                              const char* pref_path,
                              const signin::GaiaIdHash& gaia_id_hash,
                              base::Value value) {
  ScopedDictPrefUpdate update_account_dict(pref_service, pref_path);
  update_account_dict->Set(gaia_id_hash.ToBase64(), std::move(value));
}

void ClearAccountKeyedPrefValue(PrefService* pref_service,
                                const char* pref_path,
                                const signin::GaiaIdHash& gaia_id_hash) {
  ScopedDictPrefUpdate update_account_dict(pref_service, pref_path);
  update_account_dict->Remove(gaia_id_hash.ToBase64());
}

const base::Value* GetAccountKeyedPrefDictEntry(
    const PrefService* pref_service,
    const char* pref_path,
    const signin::GaiaIdHash& gaia_id_hash,
    const char* key) {
  const base::Value::Dict* account_values =
      pref_service->GetDict(pref_path).FindDict(gaia_id_hash.ToBase64());
  if (!account_values) {
    return nullptr;
  }
  return account_values->Find(key);
}

void SetAccountKeyedPrefDictEntry(PrefService* pref_service,
                                  const char* pref_path,
                                  const signin::GaiaIdHash& gaia_id_hash,
                                  const char* key,
                                  base::Value value) {
  ScopedDictPrefUpdate update_account_dict(pref_service, pref_path);
  base::Value::Dict* account_values =
      update_account_dict->EnsureDict(gaia_id_hash.ToBase64());
  account_values->Set(key, std::move(value));
}

void RemoveAccountKeyedPrefDictEntry(PrefService* pref_service,
                                     const char* pref_path,
                                     const signin::GaiaIdHash& gaia_id_hash,
                                     const char* key) {
  ScopedDictPrefUpdate update_account_dict(pref_service, pref_path);
  base::Value::Dict* account_values =
      update_account_dict->FindDict(gaia_id_hash.ToBase64());
  if (account_values) {
    account_values->Remove(key);
  }
}

void KeepAccountKeyedPrefValuesOnlyForUsers(
    PrefService* pref_service,
    const char* pref_path,
    const std::vector<signin::GaiaIdHash>& available_gaia_ids) {
  std::vector<std::string> removed_identities;
  for (std::pair<const std::string&, const base::Value&> account_settings :
       pref_service->GetDict(pref_path)) {
    if (!base::Contains(available_gaia_ids, signin::GaiaIdHash::FromBase64(
                                                account_settings.first))) {
      removed_identities.push_back(account_settings.first);
    }
  }
  if (!removed_identities.empty()) {
    ScopedDictPrefUpdate update_account_dict(pref_service, pref_path);
    for (const auto& account_id : removed_identities) {
      update_account_dict->Remove(account_id);
    }
  }
}

}  // namespace syncer
