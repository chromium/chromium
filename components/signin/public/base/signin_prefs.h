// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_H_

#include <string_view>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"

class PrefService;
class PrefRegistrySimple;

// Wrapper around `PrefService` to access/update account signin prefs.
// The prefs used here are Chrome specific prefs that are tied to the accounts
// (per account per profile), they do not contain information about the
// accounts, see also `AccountInfo`.
// Creating the pref entries is lazy. It will be first be created when writing a
// value. Reading from an non-existing pref will return the default value of
// that pref.
// Allows managing the prefs lifecycle per account, destroying them when needed.
// This is expected to be done when the account cookies are cleared, except for
// the primary account.
// Account information are stored in a dictionary for which the key is the
// `GaiaId` of the account. The account's `GaiaId` used are expected to be
// accounts known in Chrome (Signed in/out accounts or primary account).
class SigninPrefs {
 public:
  // Used as a key to dictionary pref used for accounts.
  using GaiaId = std::string_view;

  explicit SigninPrefs(PrefService& pref_service);

  // Pref access:
  // Writing a value will create the account dictionary and the pref value if
  // any of those do not exist yet. It is expected be to used with a valid
  // `gaia_id` for an account that is in Chrome.
  // Reading a value from a pref dictionary or a data pref that do not exist yet
  // will return the default value of that pref.

  // Dummy value to show the interface/implementation. To be removed when adding
  // a real value.
  void SetDummyValue(GaiaId gaia_id, int dummy_value);
  int GetDummyValue(GaiaId gaia_id) const;

  // Checks if the an account pref with the given `gaia_id` exists.
  bool HasAccountPrefs(GaiaId gaia_id) const;

  // Keeps all prefs with the gaia ids given in `gaia_ids_to_keep`.
  // This is done this way since we usually are not aware of the accounts that
  // are not there anymore, so we remove all accounts that should not be kept
  // instead of removing a specific account.
  void RemoveAllAccountPrefsExcept(
      const base::flat_set<GaiaId>& gaia_ids_to_keep);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  const raw_ref<PrefService> pref_service_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_H_
