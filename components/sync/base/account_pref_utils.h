// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_ACCOUNT_PREF_UTILS_H_
#define COMPONENTS_SYNC_BASE_ACCOUNT_PREF_UTILS_H_

#include <vector>

#include "base/values.h"

namespace signin {
class GaiaIdHash;
}  // namespace signin

class PrefService;

namespace syncer {

// Helpers to ease the use of account-keyed prefs. These prefs are structured as
// follows. Note that the leaf values may be any `base::Value` type, and special
// helpers exist for dictionaries (pref_path2 in the example):
//
// {
//   "pref_path1": {
//     "base64_gaia_id_hash1": "value1",
//     "base64_gaia_id_hash2": "value2"
//   },
//   "pref_path2": {
//     "base64_gaia_id_hash1": {
//       "key1": "value1",
//       "key2": 123
//     },
//     "base64_gaia_id_hash2": {
//       "key1": "value2",
//       "key2": 456
//     }
//   }
// }

// In the account-keyed pref at `pref_path` (which must be a valid registered
// pref), looks up the entry corresponding to `gaia_id_hash` and returns the
// corresponding value. If the `gaia_id_hash` isn't found, returns null.
const base::Value* GetAccountKeyedPrefValue(
    const PrefService* pref_service,
    const char* pref_path,
    const signin::GaiaIdHash& gaia_id_hash);

// In the account-keyed pref at `pref_path` (which must be a valid registered
// pref), sets the value for the given `gaia_id_hash` to `value`.
void SetAccountKeyedPrefValue(PrefService* pref_service,
                              const char* pref_path,
                              const signin::GaiaIdHash& gaia_id_hash,
                              base::Value value);

// In the account-keyed pref at `pref_path` (which must be a valid registered
// pref), clears any value for the given `gaia_id_hash`.
void ClearAccountKeyedPrefValue(PrefService* pref_service,
                                const char* pref_path,
                                const signin::GaiaIdHash& gaia_id_hash);

// In the account-keyed dictionary pref at `pref_path` (which must be a valid
// registered pref), looks up the entry corresponding to `gaia_id_hash`, and
// within that, further looks up the `key` and returns the corresponding value.
// If either the `gaia_id_hash` or the `key` aren't found, returns null.
const base::Value* GetAccountKeyedPrefDictEntry(
    const PrefService* pref_service,
    const char* pref_path,
    const signin::GaiaIdHash& gaia_id_hash,
    const char* key);

// In the account-keyed dictionary pref at `pref_path` (which must be a valid
// registered pref), sets the value under `key` for the given `gaia_id_hash` to
// `value`, creating the entry if required.
void SetAccountKeyedPrefDictEntry(PrefService* pref_service,
                                  const char* pref_path,
                                  const signin::GaiaIdHash& gaia_id_hash,
                                  const char* key,
                                  base::Value value);

// In the account-keyed dictionary pref at `pref_path` (which must be a valid
// registered pref), removes any value under `key` for the given `gaia_id_hash`.
void RemoveAccountKeyedPrefDictEntry(PrefService* pref_service,
                                     const char* pref_path,
                                     const signin::GaiaIdHash& gaia_id_hash,
                                     const char* key);

// For the account-keyed pref at `pref_path` (which must be a valid registered
// pref), drops all entries for accounts that are *not* listed in
// `available_gaia_ids`.
void KeepAccountKeyedPrefValuesOnlyForUsers(
    PrefService* pref_service,
    const char* pref_path,
    const std::vector<signin::GaiaIdHash>& available_gaia_ids);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_ACCOUNT_PREF_UTILS_H_
