// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_H_

#include <optional>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

class PrefService;
class PrefRegistrySimple;
class PrefChangeRegistrar;

// Value of the user choice for the Chrome Signin bubble effect.
// - `kNoChoice` is the default value, it is applied when the user made no
// explicit choice yet (on the bubble or the settings).
// - The user can made a choice through the Chrome Signin bubble by accepting or
// declining the bubble leading to `kSignin` and `kDoNotSignin` respectively.
// - Dismissing the bubble multiple times will be treated as `kDoNotSignin` as
// long as the user is in `kNoChoice` mode.
// - There is no way to go back to `kNoChoice` after a choice has been taken or
// applied.
// Theses values are persisted to disk through prefs and logs, they should not
// be renumbered or reused.
// LINT.IfChange(ChromeSigninUserChoice)
enum class ChromeSigninUserChoice {
  kNoChoice = 0,
  kAlwaysAsk = 1,
  kSignin = 2,
  kDoNotSignin = 3,

  kMaxValue = kDoNotSignin,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:ChromeSigninUserChoice)

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
  ~SigninPrefs();

  // Pref access:
  // Writing a value will create the account dictionary and the pref value if
  // any of those do not exist yet. It is expected be to used with a valid
  // `gaia_id` for an account that is in Chrome.
  // Reading a value from a pref dictionary or a data pref that do not exist yet
  // will return the default value of that pref or std::nullopt if a default
  // value does apply.

  void SetChromeSigninInterceptionUserChoice(
      GaiaId gaia_id,
      ChromeSigninUserChoice user_choice);
  ChromeSigninUserChoice GetChromeSigninInterceptionUserChoice(
      GaiaId gaia_id) const;

  // Last signout time.
  void SetChromeLastSignoutTime(GaiaId gaia_id, base::Time last_signout_time);
  std::optional<base::Time> GetChromeLastSignoutTime(GaiaId gaia_id) const;

  // This pref is expected to be used with the reprompt logic for the Chrome
  // Signin bubble. The reprompt should only be possible after bubble declines,
  // meaning that this pref and other related prefs will be cleared when the
  // user explicitly sets the setting to not signin to chrome automatically.
  //
  // Last Chrome Signin Bubble Decline time.
  void SetChromeSigninInterceptionLastBubbleDeclineTime(
      GaiaId gaia_id,
      base::Time last_repromt_time);
  void ClearChromeSigninInterceptionLastBubbleDeclineTime(GaiaId gaia_id);
  std::optional<base::Time> GetChromeSigninInterceptionLastBubbleDeclineTime(
      GaiaId gaia_id) const;

  // Chrome Signin reprompt count.
  int IncrementChromeSigninBubbleRepromptCount(GaiaId gaia_id);
  int GetChromeSigninBubbleRepromptCount(GaiaId gaia_id) const;
  void ClearChromeSigninBubbleRepromptCount(GaiaId gaia_id);

  int IncrementChromeSigninInterceptionDismissCount(GaiaId gaia_id);
  int GetChromeSigninInterceptionDismissCount(GaiaId gaia_id) const;

  void IncrementPasswordSigninPromoImpressionCount(GaiaId gaia_id);
  int GetPasswordSigninPromoImpressionCount(GaiaId gaia_id) const;

  void IncrementAutofillSigninPromoDismissCount(GaiaId gaia_id);
  int GetAutofillSigninPromoDismissCount(GaiaId gaia_id) const;

  // Note: `callback` will be notified on every change in the main dictionary
  // and sub-dictionries (account dictionaries).
  static void ObserveSigninPrefsChanges(PrefChangeRegistrar& registrar,
                                        base::RepeatingClosure callback);

  // Checks if the an account pref with the given `gaia_id` exists.
  bool HasAccountPrefs(GaiaId gaia_id) const;

  // Keeps all prefs with the gaia ids given in `gaia_ids_to_keep`.
  // This is done this way since we usually are not aware of the accounts that
  // are not there anymore, so we remove all accounts that should not be kept
  // instead of removing a specific account. Returns the number of accounts
  // that were removed.
  size_t RemoveAllAccountPrefsExcept(
      const base::flat_set<GaiaId>& gaia_ids_to_keep);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // Increments any specified `pref` of type int for the given `gaia_id`.
  int IncrementIntPrefForAccount(GaiaId gaia_id, std::string_view pref);
  // Gets any specified `pref` of type int for the given `gaia_id`.
  // Returns 0 if the corresponding `pref` doesn't exist for `gaia_id`.
  int GetIntPrefForAccount(GaiaId gaia_id, std::string_view pref) const;

  // Time pref related, returns by default std::nullopt if the pref is not
  // created yet for the given `gaia_id`.
  void SetTimePref(base::Time time, GaiaId gaia_id, std::string_view pref);
  std::optional<base::Time> GetTimePref(GaiaId gaia_id,
                                        std::string_view pref) const;

  // Clear any given account pref for the given `gaia_id`.
  void ClearPref(GaiaId gaia_id, std::string_view pref);

  const raw_ref<PrefService> pref_service_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_H_
