// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_prefs.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/signin_pref_names.h"

namespace {
// Name of the main pref dictionary holding the account dictionaries of the
// underlying prefs, with the key as the GaiaIds. The prefs stored through
// this dict here are information not directly related to the account itself,
// only metadata.
constexpr char kSigninAccountPrefs[] = "signin.accounts_metadata_dict";

// Pref used to store the user choice for the Chrome Signin Intercept. It is
// tied to an account, stored as the content of a dictionary mapped by the
// gaia id of the account.
constexpr char kChromeSigninInterceptionUserChoice[] =
    "ChromeSigninInterceptionUserChoice";

// Pref used to track the first declined choice of the Chrome Signin intercept.
// It will be used together with the reprompt timing logic to know when to
// trigger Chrome Signin intercept reprompts after declines. It is tied to an
// account, stored as the content of a dictionary mapped by the gaia id of the
// account.
constexpr char kChromeSigninInterceptionFirstDeclinedChoiceTime[] =
    "ChromeSigninInterceptionFirstDeclinedChoiceTime";

// Pref used to store the number of dismisses of the Chrome Signin Bubble. It
// is tied to an account, stored as the content of a dictionary mapped by the
// gaia id of the account.
constexpr char kChromeSigninInterceptionDismissCount[] =
    "ChromeSigninInterceptionDismissCount";

// Pref to store the number of times the password bubble signin promo
// has been shown per account.
constexpr char kPasswordSignInPromoShownCount[] =
    "PasswordSignInPromoShownCount";
// Pref to store the number of times any autofill bubble signin promo
// has been dismissed per account.
constexpr char kAutofillSignInPromoDismissCount[] =
    "AutofillSignInPromoDismissCount";

}  // namespace

SigninPrefs::SigninPrefs(PrefService& pref_service)
    : pref_service_(pref_service) {}

SigninPrefs::~SigninPrefs() = default;

void SigninPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kSigninAccountPrefs);
  registry->RegisterIntegerPref(prefs::kHistorySyncSuccessiveDeclineCount, 0);
  registry->RegisterInt64Pref(prefs::kHistorySyncLastDeclinedTimestamp, 0);
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

// static
void SigninPrefs::ObserveSigninPrefsChanges(PrefChangeRegistrar& registrar,
                                            base::RepeatingClosure callback) {
  registrar.Add(kSigninAccountPrefs, callback);
}

void SigninPrefs::SetChromeSigninInterceptionUserChoice(
    GaiaId gaia_id,
    ChromeSigninUserChoice user_choice) {
  if (GetChromeSigninInterceptionUserChoice(gaia_id) == user_choice) {
    return;
  }

  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // `EnsureDict` gets or create the dictionary.
  base::Value::Dict* account_dict = scoped_update->EnsureDict(gaia_id);
  // `Set` will add an entry if it doesn't already exists, or if it does, it
  // will overwrite it.
  account_dict->Set(kChromeSigninInterceptionUserChoice,
                    static_cast<int>(user_choice));
}

ChromeSigninUserChoice SigninPrefs::GetChromeSigninInterceptionUserChoice(
    GaiaId gaia_id) const {
  const base::Value::Dict* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id);
  // If the account dict does not exist yet; return the default value.
  if (!account_dict) {
    return ChromeSigninUserChoice::kNoChoice;
  }
  // Return the pref value if it exists, otherwise return the default value.
  // No value default to 0 -> `ChromeSigninUserChoice::kNoChoice`.
  return static_cast<ChromeSigninUserChoice>(
      account_dict->FindInt(kChromeSigninInterceptionUserChoice).value_or(0));
}

void SigninPrefs::SetChromeSigninInterceptionFirstDeclinedChoiceTime(
    GaiaId gaia_id,
    base::Time first_declined_time) {
  if (GetChromeSigninInterceptionFirstDeclinedChoiceTime(gaia_id).has_value()) {
    return;
  }

  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // `EnsureDict` gets or create the dictionary.
  base::Value::Dict* account_dict = scoped_update->EnsureDict(gaia_id);
  // `Set` will add an entry if it doesn't already exists, or if it does, it
  // will overwrite it.
  account_dict->Set(kChromeSigninInterceptionFirstDeclinedChoiceTime,
                    base::TimeToValue(first_declined_time));
}

void SigninPrefs::ClearChromeSigninInterceptionFirstDeclinedChoiceTime(
    GaiaId gaia_id) {
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // Do not create a dictionary if this not already exist.
  base::Value::Dict* account_dict = scoped_update->FindDict(gaia_id);
  if (!account_dict) {
    return;
  }

  account_dict->Remove(kChromeSigninInterceptionFirstDeclinedChoiceTime);
}

std::optional<base::Time>
SigninPrefs::GetChromeSigninInterceptionFirstDeclinedChoiceTime(
    GaiaId gaia_id) const {
  const base::Value::Dict* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id);
  // If the account dict does not exist yet; return no time.
  if (!account_dict) {
    return std::nullopt;
  }
  // Return the pref value if it exists, otherwise return no time.
  const base::Value* value =
      account_dict->Find(kChromeSigninInterceptionFirstDeclinedChoiceTime);
  return value ? base::ValueToTime(value) : std::nullopt;
}

int SigninPrefs::IncrementChromeSigninInterceptionDismissCount(GaiaId gaia_id) {
  return IncrementIntPrefForAccount(gaia_id,
                                    kChromeSigninInterceptionDismissCount);
}

int SigninPrefs::GetChromeSigninInterceptionDismissCount(GaiaId gaia_id) const {
  return GetIntPrefForAccount(gaia_id, kChromeSigninInterceptionDismissCount);
}

void SigninPrefs::IncrementPasswordSigninPromoImpressionCount(GaiaId gaia_id) {
  IncrementIntPrefForAccount(gaia_id, kPasswordSignInPromoShownCount);
}

int SigninPrefs::GetPasswordSigninPromoImpressionCount(GaiaId gaia_id) const {
  return GetIntPrefForAccount(gaia_id, kPasswordSignInPromoShownCount);
}

void SigninPrefs::IncrementAutofillSigninPromoDismissCount(GaiaId gaia_id) {
  IncrementIntPrefForAccount(gaia_id, kAutofillSignInPromoDismissCount);
}

int SigninPrefs::GetAutofillSigninPromoDismissCount(GaiaId gaia_id) const {
  return GetIntPrefForAccount(gaia_id, kAutofillSignInPromoDismissCount);
}

int SigninPrefs::IncrementIntPrefForAccount(GaiaId gaia_id,
                                            std::string_view pref) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);

  // `EnsureDict` gets or create the dictionary.
  base::Value::Dict* account_dict = scoped_update->EnsureDict(gaia_id);
  // Get the current value of the pref.
  int new_value = account_dict->FindInt(pref).value_or(0) + 1;
  // `Set` will add an entry if it doesn't already exists, or if it does, it
  // will overwrite it.
  account_dict->Set(pref, new_value);

  return new_value;
}

int SigninPrefs::GetIntPrefForAccount(GaiaId gaia_id,
                                      std::string_view pref) const {
  CHECK(!gaia_id.empty());
  const base::Value::Dict* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id);
  // If the account dict does not exist yet; return the default value.
  if (!account_dict) {
    return 0;
  }

  // Return the pref value if it exists, otherwise return the default value.
  return account_dict->FindInt(pref).value_or(0);
}
