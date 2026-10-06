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

// Privileged generic accessor for the per-account signin preference dictionary
// (`"signin.accounts_metadata_dict"`).
//
// Every per-account key-based read, write, and clear is `CHECK`-validated
// against `SigninPrefsRegistry` (except `SetDeprecatedPrefForTesting` and
// `GetDeprecatedPrefForTesting`). `GetAccountPrefsDict` and `HasAccountPrefs`
// inspect the raw account dictionary directly.
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

  // Registers the profile-level dictionary holding all account prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry, PassKey);

  // Subscribes `callback` to changes in the account metadata pref dictionary.
  static void ObserveChanges(PrefChangeRegistrar& registrar,
                             base::RepeatingClosure callback,
                             PassKey);

  // Cleans up obsolete per-account preference keys across all accounts.
  // Corrupted account entries that are not dictionaries are removed.
  void MigrateObsoleteAccountPrefs();

  // Returns the root dictionary containing all account preferences.
  // Note: the returned reference and any pointers into it are invalidated by
  // subsequent pref mutations.
  const base::DictValue& GetAccountPrefsDict() const;

  // Returns true if any preferences are stored for `gaia_id`.
  bool HasAccountPrefs(const GaiaId& gaia_id) const;

  // Removes all account dictionaries except those in `gaia_ids_to_keep`.
  // Returns the number of removed account dictionaries.
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

  void SetDeprecatedPrefForTesting(const GaiaId& gaia_id);
  std::optional<int> GetDeprecatedPrefForTesting(const GaiaId& gaia_id) const;

 private:
  const raw_ref<PrefService> pref_service_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_ACCESSOR_H_
