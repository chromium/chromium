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

namespace base {
class DictValue;
class Time;
}

class GaiaId;
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
      const GaiaId& gaia_id,
      ChromeSigninUserChoice user_choice);
  ChromeSigninUserChoice GetChromeSigninInterceptionUserChoice(
      const GaiaId& gaia_id) const;

  // Last signout time.
  void SetChromeLastSignoutTime(const GaiaId& gaia_id,
                                base::Time last_signout_time);
  std::optional<base::Time> GetChromeLastSignoutTime(
      const GaiaId& gaia_id) const;

  // This pref is expected to be used with the reprompt logic for the Chrome
  // Signin bubble. The reprompt should only be possible after bubble declines,
  // meaning that this pref and other related prefs will be cleared when the
  // user explicitly sets the setting to not signin to chrome automatically.
  //
  // Last Chrome Signin Bubble Decline time.
  void SetChromeSigninInterceptionLastBubbleDeclineTime(
      const GaiaId& gaia_id,
      base::Time last_repromt_time);
  void ClearChromeSigninInterceptionLastBubbleDeclineTime(
      const GaiaId& gaia_id);
  std::optional<base::Time> GetChromeSigninInterceptionLastBubbleDeclineTime(
      const GaiaId& gaia_id) const;

  // Chrome Signin reprompt count.
  int IncrementChromeSigninBubbleRepromptCount(const GaiaId& gaia_id);
  int GetChromeSigninBubbleRepromptCount(const GaiaId& gaia_id) const;
  void ClearChromeSigninBubbleRepromptCount(const GaiaId& gaia_id);

  int IncrementChromeSigninInterceptionDismissCount(const GaiaId& gaia_id);
  int GetChromeSigninInterceptionDismissCount(const GaiaId& gaia_id) const;

  void IncrementPasswordSigninPromoImpressionCount(const GaiaId& gaia_id);
  int GetPasswordSigninPromoImpressionCount(const GaiaId& gaia_id) const;

  void IncrementAddressSigninPromoImpressionCount(const GaiaId& gaia_id);
  int GetAddressSigninPromoImpressionCount(const GaiaId& gaia_id) const;

  void IncrementBookmarkSigninPromoImpressionCount(const GaiaId& gaia_id);
  int GetBookmarkSigninPromoImpressionCount(const GaiaId& gaia_id) const;

  void IncrementAutofillSigninPromoDismissCount(const GaiaId& gaia_id);
  int GetAutofillSigninPromoDismissCount(const GaiaId& gaia_id) const;

  void IncrementAddressSigninPromoDismissCount(const GaiaId& gaia_id);
  int GetAddressSigninPromoDismissCount(const GaiaId& gaia_id) const;

  void IncrementBookmarkSigninPromoDismissCount(const GaiaId& gaia_id);
  int GetBookmarkSigninPromoDismissCount(const GaiaId& gaia_id) const;

  void IncrementPasswordSigninPromoDismissCount(const GaiaId& gaia_id);
  int GetPasswordSigninPromoDismissCount(const GaiaId& gaia_id) const;

  void SetExtensionsExplicitBrowserSignin(const GaiaId& gaia_id, bool enabled);
  bool GetExtensionsExplicitBrowserSignin(const GaiaId& gaia_id) const;

  void SetBookmarksExplicitBrowserSignin(const GaiaId& gaia_id, bool enabled);
  bool GetBookmarksExplicitBrowserSignin(const GaiaId& gaia_id) const;

  void SetPolicyDisclaimerLastRegistrationFailureTime(
      const GaiaId& gaia_id,
      base::Time last_registration_failure_time);
  void ClearPolicyDisclaimerLastRegistrationFailureTime(const GaiaId& gaia_id);
  std::optional<base::Time> GetPolicyDisclaimerLastRegistrationFailureTime(
      const GaiaId& gaia_id) const;

  // Sync promo on the avatar button.
  void IncrementSyncPromoIdentityPillShownCount(const GaiaId& gaia_id);
  int GetSyncPromoIdentityPillShownCount(const GaiaId& gaia_id) const;
  void IncrementSyncPromoIdentityPillUsedCount(const GaiaId& gaia_id);
  int GetSyncPromoIdentityPillUsedCount(const GaiaId& gaia_id) const;

  // History sync promo on the history page.
  int GetHistoryPageHistorySyncPromoShownCount(const GaiaId& gaia_id) const;
  void IncrementHistoryPageHistorySyncPromoShownCount(const GaiaId& gaia_id);

  std::optional<base::Time>
  GetHistoryPageHistorySyncPromoLastDismissedTimestamp(
      const GaiaId& gaia_id) const;
  void SetHistoryPageHistorySyncPromoLastDismissedTimestamp(
      const GaiaId& gaia_id,
      base::Time last_dismissed_timestamp);

  void SetHistoryPageHistorySyncPromoShownAfterDismissal(const GaiaId& gaia_id);
  bool GetHistoryPageHistorySyncPromoShownAfterDismissal(
      const GaiaId& gaia_id) const;

  // Returns a dictionary of the avatar button promo count for `gaia_id`, if the
  // dictionary didn't exist it will create it.
  // The returned dictionary will not notify observers for underlying pref
  // changes. If this will be required later on, consider returning a
  // `ScopedDictPrefUpdate` instead.
  base::DictValue& GetOrCreateAvatarButtonPromoCountDictionary(
      const GaiaId& gaia_id);

  // Updates the dismiss count of the promo and last time it was dismissed.
  void IncrementBookmarkBatchUploadPromoDismissCountWithLastTime(
      const GaiaId& gaia_id);
  // Returns the number of time the promo was dismissed and the last time it was
  // dismissed.
  std::pair<int, std::optional<base::Time>>
  GetBookmarkBatchUploadPromoDismissCountWithLastTime(const GaiaId& gaia_id);

  // Note: `callback` will be notified on every change in the main dictionary
  // and sub-dictionries (account dictionaries).
  static void ObserveSigninPrefsChanges(PrefChangeRegistrar& registrar,
                                        base::RepeatingClosure callback);

  // Checks if the an account pref with the given `gaia_id` exists.
  bool HasAccountPrefs(const GaiaId& gaia_id) const;

  // Keeps all prefs with the gaia ids given in `gaia_ids_to_keep`.
  // This is done this way since we usually are not aware of the accounts that
  // are not there anymore, so we remove all accounts that should not be kept
  // instead of removing a specific account. Returns the number of accounts
  // that were removed.
  size_t RemoveAllAccountPrefsExcept(
      const base::flat_set<GaiaId>& gaia_ids_to_keep);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  void MigrateObsoleteSigninPrefs();

  void SetDeprecatedPrefForTesting(const GaiaId& gaia_id);
  std::optional<int> GetDeprecatedPrefForTesting(const GaiaId& gaia_id);

 private:
  // Increments any specified `pref` of type int for the given `gaia_id`.
  int IncrementIntPrefForAccount(const GaiaId& gaia_id, std::string_view pref);
  // Gets any specified `pref` of type int for the given `gaia_id`.
  // Returns 0 if the corresponding `pref` doesn't exist for `gaia_id`.
  int GetIntPrefForAccount(const GaiaId& gaia_id, std::string_view pref) const;

  // Sets any specified `pref` of type bool for the given `gaia_id` to
  // `enabled`.
  void SetBooleanPrefForAccount(const GaiaId& gaia_id,
                                std::string_view pref,
                                bool enabled);
  // Gets any specified `pref` of type bool for the given `gaia_id`.
  // Returns false if the corresponding `pref` doesn't exist for `gaia_id`.
  bool GetBooleanPrefForAccount(const GaiaId& gaia_id,
                                std::string_view pref) const;

  // Time pref related, returns by default std::nullopt if the pref is not
  // created yet for the given `gaia_id`.
  void SetTimePref(base::Time time,
                   const GaiaId& gaia_id,
                   std::string_view pref);
  std::optional<base::Time> GetTimePref(const GaiaId& gaia_id,
                                        std::string_view pref) const;

  // Clear any given account pref for the given `gaia_id`.
  void ClearPref(const GaiaId& gaia_id, std::string_view pref);

  const raw_ref<PrefService> pref_service_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_H_
