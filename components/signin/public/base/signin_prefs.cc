// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_prefs.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
namespace {

// Name of the main pref dictionary holding the account dictionaries of the
// underlying prefs, with the key as the GaiaIds. The prefs stored through this
// dict here are information not directly related to the account itself, only
// metadata.
constexpr char kSigninAccountPrefs[] = "signin.accounts_metadata_dict";

// Dummy value pref; will later be replaced with real values.
constexpr char kSigninAccountDummyValuePref[] = "dummy_value";

}  // namespace

SigninPrefs::SigninPrefs(PrefService& pref_service)
    : pref_service_(pref_service) {}

void SigninPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kSigninAccountPrefs);
}

bool SigninPrefs::HasAccountPrefs(GaiaId gaia_id) const {
  return pref_service_->GetDict(kSigninAccountPrefs).contains(gaia_id);
}

void SigninPrefs::RemoveAllAccountPrefsExcept(
    const base::flat_set<GaiaId>& gaia_ids_to_keep) {
  // Get the list of all accounts that should be removed, not in
  // `gaia_ids_to_keep`. Use `std::string` instead of `GaiaId`  because a
  // reference might loose it's value on removal of items in the next step.
  std::vector<std::string> accounts_prefs_to_remove;
  for (const std::pair<const std::string&, const base::Value&> account_prefs :
       pref_service_->GetDict(kSigninAccountPrefs)) {
    if (!base::Contains(gaia_ids_to_keep, account_prefs.first)) {
      accounts_prefs_to_remove.push_back(account_prefs.first);
    }
  }

  // Remove the account prefs that should not be kept.
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  for (GaiaId account_prefs_to_remove : accounts_prefs_to_remove) {
    scoped_update->Remove(account_prefs_to_remove);
  }
}

void SigninPrefs::SetDummyValue(GaiaId gaia_id, int dummy_value) {
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);

  // `EnsureDict` gets or create the dictionary.
  base::Value::Dict* account_dict = scoped_update->EnsureDict(gaia_id);
  // `Set` will add an entry if it doesn't already exists, or if it does, it
  // will overwrite it.
  account_dict->Set(kSigninAccountDummyValuePref, dummy_value);
}

int SigninPrefs::GetDummyValue(GaiaId gaia_id) const {
  const base::Value::Dict* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id);
  // If the account dict does not exist yet; return the default value.
  if (!account_dict) {
    return 0;
  }

  // Return the pref value if it exists, otherwise return the default value.
  return account_dict->FindInt(kSigninAccountDummyValuePref).value_or(0);
}
