// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_ACCESSOR_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_ACCESSOR_H_

#include <optional>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "base/values.h"

class GaiaId;
class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;
class SigninPrefs;

// Privileged generic accessor for the single-account metadata dictionary
// (`"signin.accounts_metadata_dict"`) and multi-account metadata list
// (`"signin.multiple_accounts_metadata_list"`).
//
// Every key-based read, write, and clear is `CHECK`-validated
// against `SigninPrefsRegistry` (except `SetDeprecatedPrefForTesting` and
// `GetDeprecatedPrefForTesting`). `GetAccountPrefsDict`,
// `GetMultipleAccountsPrefsList`, and `HasAccountPrefs` inspect the raw
// single-account dictionary or multi-account list directly.
//
// On writes, missing or non-dict intermediate entries along `parents` are
// created/overwritten via `EnsureDict`; on reads, missing or non-dict
// intermediate entries return the default value without mutating prefs.
//
// Access is restricted via `PassKey` and GN `visibility` to `SigninPrefs`
// (which exposes typed methods to standard consumers) and unit tests (via
// `CreatePassKeyForTesting()`).
class SigninPrefsAccessor {
 public:
  using PassKey = base::PassKey<SigninPrefs, SigninPrefsAccessor>;

  static PassKey CreatePassKeyForTesting() {
    return base::PassKey<SigninPrefsAccessor>();
  }

  SigninPrefsAccessor(PrefService& pref_service, PassKey);
  SigninPrefsAccessor(const SigninPrefsAccessor&) = delete;
  SigninPrefsAccessor& operator=(const SigninPrefsAccessor&) = delete;
  ~SigninPrefsAccessor();

  // Registers the profile-level single-account and multi-account metadata
  // prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry, PassKey);

  // Subscribes `callback` to changes in the single-account and multi-account
  // metadata prefs.
  static void ObserveChanges(PrefChangeRegistrar& registrar,
                             base::RepeatingClosure callback,
                             PassKey);

  // Cleans up obsolete single-account preference keys across all accounts in
  // `signin.accounts_metadata_dict`. Corrupted account entries that are not
  // dictionaries are removed.
  void MigrateObsoleteAccountPrefs();

  // Cleans up obsolete or invalid multi-account entries in
  // `signin.multiple_accounts_metadata_list`.
  void MigrateObsoleteMultipleAccountsPrefs();

  // Returns the root dictionary containing all single-account preferences.
  // Note: the returned reference and any pointers into it are invalidated by
  // subsequent pref mutations.
  const base::DictValue& GetAccountPrefsDict() const;

  // Returns the list containing all multi-account preferences.
  // Note: the returned reference and any pointers into it are invalidated by
  // subsequent pref mutations.
  const base::ListValue& GetMultipleAccountsPrefsList() const;

  // Returns true if any single-account preferences are stored for `gaia_id`.
  bool HasAccountPrefs(const GaiaId& gaia_id) const;

  // Removes all single-account dictionaries except those in `gaia_ids_to_keep`,
  // and prunes any multi-account entries referencing removed accounts. Returns
  // the number of removed single-account dictionaries.
  size_t RemoveAllAccountPrefsExcept(
      const base::flat_set<GaiaId>& gaia_ids_to_keep);

  // Increments the integer preference at `(parents, key)` for `gaia_id`,
  // creating intermediate dictionaries if missing. Clamps on overflow.
  int IncrementIntPref(const GaiaId& gaia_id,
                       std::string_view key,
                       base::span<const std::string_view> parents = {});

  // Returns the integer preference at `(parents, key)` for `gaia_id`, or 0 if
  // missing or any intermediate dictionary is absent. Side-effect-free.
  int GetIntPref(const GaiaId& gaia_id,
                 std::string_view key,
                 base::span<const std::string_view> parents = {}) const;

  // Returns the integer preference at `(parents, key)` for `gaia_id`, or
  // `std::nullopt` if missing or any intermediate dictionary is absent.
  // Side-effect-free.
  std::optional<int> MaybeGetIntPref(
      const GaiaId& gaia_id,
      std::string_view key,
      base::span<const std::string_view> parents = {}) const;

  // Sets the integer preference at `(parents, key)` for `gaia_id`, creating
  // intermediate dictionaries if missing.
  void SetIntPref(const GaiaId& gaia_id,
                  std::string_view key,
                  int value,
                  base::span<const std::string_view> parents = {});

  // Sets the boolean preference at `(parents, key)` for `gaia_id`, creating
  // intermediate dictionaries if missing.
  void SetBooleanPref(const GaiaId& gaia_id,
                      std::string_view key,
                      bool value,
                      base::span<const std::string_view> parents = {});

  // Returns the boolean preference at `(parents, key)` for `gaia_id`, or false
  // if missing or any intermediate dictionary is absent. Side-effect-free.
  bool GetBooleanPref(const GaiaId& gaia_id,
                      std::string_view key,
                      base::span<const std::string_view> parents = {}) const;

  // Sets the timestamp preference at `(parents, key)` for `gaia_id`, creating
  // intermediate dictionaries if missing.
  void SetTimePref(const GaiaId& gaia_id,
                   std::string_view key,
                   base::Time value,
                   base::span<const std::string_view> parents = {});

  // Returns the timestamp preference at `(parents, key)` for `gaia_id`, or
  // `std::nullopt` if missing or any intermediate dictionary is absent.
  // Side-effect-free.
  std::optional<base::Time> GetTimePref(
      const GaiaId& gaia_id,
      std::string_view key,
      base::span<const std::string_view> parents = {}) const;

  // Sets a generic leaf preference value at `(parents, key)` for `gaia_id`,
  // creating intermediate dictionaries if missing.
  // `CHECK`-enforces `!gaia_id.empty()` and
  // `SigninPrefsRegistry::IsValidValue` (leaf-only, matching type).
  void SetValue(const GaiaId& gaia_id,
                std::string_view key,
                base::Value value,
                base::span<const std::string_view> parents = {});

  // Removes `(parents, key)` for `gaia_id` without creating missing
  // dictionaries. `CHECK`-enforces `!gaia_id.empty()` and that `(parents, key)`
  // is registered in `SigninPrefsRegistry`. Returns true if the key existed and
  // was removed.
  bool ClearPref(const GaiaId& gaia_id,
                 std::string_view key,
                 base::span<const std::string_view> parents = {});

  // Removes `(parents, key)` across all accounts in `SigninPrefs` in a single
  // pref update without creating missing dictionaries. `CHECK`-enforces that
  // `(parents, key)` is registered in `SigninPrefsRegistry`. Returns the number
  // of accounts from which the key was removed.
  size_t ClearPrefForAllAccounts(
      std::string_view key,
      base::span<const std::string_view> parents = {});

  // Multi-account preference accessors for entries registered with
  // `is_multiple_accounts == true`. Keys each entry by a canonical sorted set
  // of at least two distinct, non-empty `GaiaId`s so input ordering never
  // creates duplicate entries. `CHECK`-enforces valid IDs and registry
  // validation.
  int IncrementMultipleAccountsIntPref(const base::flat_set<GaiaId>& gaia_ids,
                                       std::string_view key);
  int GetMultipleAccountsIntPref(const base::flat_set<GaiaId>& gaia_ids,
                                 std::string_view key) const;
  void SetMultipleAccountsBooleanPref(const base::flat_set<GaiaId>& gaia_ids,
                                      std::string_view key,
                                      bool value);
  bool GetMultipleAccountsBooleanPref(const base::flat_set<GaiaId>& gaia_ids,
                                      std::string_view key) const;

  void SetDeprecatedPrefForTesting(const GaiaId& gaia_id);
  std::optional<int> GetDeprecatedPrefForTesting(const GaiaId& gaia_id) const;

 private:
  const raw_ref<PrefService> pref_service_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_ACCESSOR_H_
