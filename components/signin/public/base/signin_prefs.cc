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
#include "components/signin/public/base/signin_switches.h"
#include "google_apis/gaia/gaia_id.h"

namespace {
// Name of the main pref dictionary holding the account dictionaries of the
// underlying prefs, with the key as the GaiaIds. The prefs stored through
// this dict here are information not directly related to the account itself,
// only metadata.
constexpr char kSigninAccountPrefs[] = "signin.accounts_metadata_dict";

// Pref used to track the last time the user signed out of Chrome.
constexpr char kChromeLastSignoutTime[] = "kChromeLastSignoutTime";

// Pref used to store the user choice for the Chrome Signin Intercept. It is
// tied to an account, stored as the content of a dictionary mapped by the
// gaia id of the account.
constexpr char kChromeSigninInterceptionUserChoice[] =
    "ChromeSigninInterceptionUserChoice";

// Pref used to track the last time the Chrome Signin bubble was declined. It is
// used to know when to allow future reprompts if the conditions are met. The
// pref will be cleared if the Chrome Signin setting equivalent to showing the
// bubble upon web signin is set to `ChromeSigninUserChoice::kDoNotSignin`, in
// order not to consider the bubble decline interaction anymore.
constexpr char kChromeSigninInterceptionLastBubbleDeclineTime[] =
    "ChromeSigninInterceptionLastBubbleDeclineTime";

// Pref used to track the number of times the Chrome Signin bubble was
// reprompted. It is used to know when the allow future reprompts.
// The pref will be cleared if the Chrome Signin setting equivalent to showing
// the bubble upon web signin is set to `ChromeSigninUserChoice::kDoNotSignin`,
// in order not to consider the bubble decline interaction anymore.
constexpr char kChromeSigninInterceptionRepromptCount[] =
    "ChromeSigninInterceptionRepromptCount";

// Pref used to store the number of dismisses of the Chrome Signin Bubble. It
// is tied to an account, stored as the content of a dictionary mapped by the
// gaia id of the account.
constexpr char kChromeSigninInterceptionDismissCount[] =
    "ChromeSigninInterceptionDismissCount";

// Pref to store the number of times the password bubble signin promo
// has been shown per account.
constexpr char kPasswordSignInPromoShownCount[] =
    "PasswordSignInPromoShownCount";

// Pref to store the number of times the password bubble signin promo
// has been shown per account used for SigninPromoLimitsExperiment.
constexpr char kPasswordSignInPromoShownCountForLimitsExperiment[] =
    "PasswordSignInPromoShownCountForLimitsExperiment";

// Pref to store the number of times the address bubble signin promo
// has been shown per account.
constexpr char kAddressSignInPromoShownCount[] = "AddressSignInPromoShownCount";

// Pref to store the number of times the address bubble signin promo
// has been shown per account used for SigninPromoLimitsExperiment.
constexpr char kAddressSignInPromoShownCountForLimitsExperiment[] =
    "AddressSignInPromoShownCountForLimitsExperiment";

// Pref to store the number of times the bookmark bubble signin promo
// has been shown per account.
constexpr char kBookmarkSignInPromoShownCount[] =
    "BookmarkSignInPromoShownCount";

// Pref to store the number of times the bookmark bubble signin promo
// has been shown per account used for SigninPromoLimitsExperiment.
constexpr char kBookmarkSignInPromoShownCountForLimitsExperiment[] =
    "BookmarkSignInPromoShownCountForLimitsExperiment";

// Pref to store the number of times any autofill bubble signin promo
// has been dismissed per account.
constexpr char kAutofillSignInPromoDismissCount[] =
    "AutofillSignInPromoDismissCount";

// Pref to store the number of times the address bubble signin promo
// has been dismissed per account.
constexpr char kAddressSignInPromoDismissCount[] =
    "AddressSignInPromoDismissCount";

// Pref to store the number of times the bookmark bubble signin promo
// has been dismissed per account used for SigninPromoLimitsExperiment.
constexpr char kBookmarkSignInPromoDismissCount[] =
    "BookmarkSignInPromoDismissCount";

// Pref to store the number of times the password bubble signin promo
// has been dismissed per account.
constexpr char kPasswordSignInPromoDismissCount[] =
    "PasswordSignInPromoDismissCount";

// Registers that the sign in occurred with an explicit user action from the
// bubble that appears after installing an extension. False by default.
constexpr char kExtensionsExplicitBrowserSigninEnabled[] =
    "ExtensionsExplicitBrowserSigninEnabled";

// Registers that the sign in occurred with an explicit user action from the
// bookmark sig in promo. False by default. Note: this pref is only set to true
// when `syncer::kSyncEnableBookmarksInTransportMode` is enabled.
constexpr char kBookmarksExplicitBrowserSigninEnabled[] =
    "BookmarksExplicitBrowserSigninEnabled";

// Sync promo on the avatar button.
//
// Number of times the sync promo was shown in the identity pill (avatar toolbar
// button).
constexpr std::string_view kSyncPromoIdentityPillShownCount =
    "SyncPromoIdentityPillShownCount";
// Number of times the sync promo was used (clicked) in the identity pill
// (avatar toolbar button).
constexpr std::string_view kSyncPromoIdentityPillUsedCount =
    "SyncPromoIdentityPillUsedCount";

// Number of times the Bookmark Batch Upload promo was dismissed.
constexpr std::string_view kBookmarkBatchUploadPromoDismissCount =
    "BookmarkBatchUploadPromoDismissCount";
// The time at which the last Bookmark Batch Upload promo was dismissed.
constexpr std::string_view kBookmarkBatchUploadPromoLastDismissTime =
    "BookmarkBatchUploadPromoLastDismissTime";

constexpr std::string_view kPolicyDisclaimerLastRegistrationFailureTime =
    "PolicyDisclaimerLastRegistrationFailureTime";

// Dictionary pref that contains all the values related to the avatar button
// promo counts.
constexpr std::string_view kAvatarButtonPromoCountDictionary =
    "AvatarButtonPromoCountDictionary";

// DEPRECATED(10/25): Check `SigninPrefs::SigninPrefs()`.
// Testing deprecating pref:
constexpr std::string_view kDeprecatingTestingPref = "DeprecatingTestingPref";
//
// History Sync promo on the avatar button.
//
// Number of times the history sync promo was shown in the identity pill (avata
// toolbar button).
constexpr std::string_view kHistorySyncPromoIdentityPillShownCount =
    "ChromeSigninSyncPromoIdentityPillShownCount";
// Number of times the history sync promo was used (clicked) in the identity
// pill (avatar toolbar button).
constexpr std::string_view kHistorySyncPromoIdentityPillUsedCount =
    "ChromeSigninSyncPromoIdentityPillUsedCount";

}  // namespace

SigninPrefs::SigninPrefs(PrefService& pref_service)
    : pref_service_(pref_service) {}

SigninPrefs::~SigninPrefs() = default;

void SigninPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kSigninAccountPrefs);
  registry->RegisterIntegerPref(prefs::kHistorySyncSuccessiveDeclineCount, 0);
  registry->RegisterInt64Pref(prefs::kHistorySyncLastDeclinedTimestamp, 0);
}

void SigninPrefs::MigrateObsoleteSigninPrefs() {
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // Deprecates prefs within the existing internal account dict.
  for (auto value : scoped_update.Get()) {
    base::DictValue& account_dict = value.second.GetDict();
    account_dict.Remove(kDeprecatingTestingPref);
    account_dict.Remove(kHistorySyncPromoIdentityPillShownCount);
    account_dict.Remove(kHistorySyncPromoIdentityPillUsedCount);
  }
}

bool SigninPrefs::HasAccountPrefs(const GaiaId& gaia_id) const {
  return pref_service_->GetDict(kSigninAccountPrefs)
      .contains(gaia_id.ToString());
}

size_t SigninPrefs::RemoveAllAccountPrefsExcept(
    const base::flat_set<GaiaId>& gaia_ids_to_keep) {
  // Get the list of all accounts that should be removed, not in
  // `gaia_ids_to_keep`. Use `std::string` instead of `GaiaId`  because a
  // reference might loose it's value on removal of items in the next step.
  std::vector<GaiaId> accounts_prefs_to_remove;
  for (const std::pair<const std::string&, const base::Value&> account_prefs :
       pref_service_->GetDict(kSigninAccountPrefs)) {
    GaiaId gaia_id(account_prefs.first);
    if (!base::Contains(gaia_ids_to_keep, gaia_id)) {
      accounts_prefs_to_remove.push_back(std::move(gaia_id));
    }
  }

  // Remove the account prefs that should not be kept.
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  for (const GaiaId& account_prefs_to_remove : accounts_prefs_to_remove) {
    scoped_update->Remove(account_prefs_to_remove.ToString());
  }

  return accounts_prefs_to_remove.size();
}

// static
void SigninPrefs::ObserveSigninPrefsChanges(PrefChangeRegistrar& registrar,
                                            base::RepeatingClosure callback) {
  registrar.Add(kSigninAccountPrefs, callback);
}

void SigninPrefs::SetChromeSigninInterceptionUserChoice(
    const GaiaId& gaia_id,
    ChromeSigninUserChoice user_choice) {
  if (GetChromeSigninInterceptionUserChoice(gaia_id) == user_choice) {
    return;
  }

  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // `EnsureDict` gets or create the dictionary.
  base::Value::Dict* account_dict =
      scoped_update->EnsureDict(gaia_id.ToString());
  // `Set` will add an entry if it doesn't already exists, or if it does, it
  // will overwrite it.
  account_dict->Set(kChromeSigninInterceptionUserChoice,
                    static_cast<int>(user_choice));
}

ChromeSigninUserChoice SigninPrefs::GetChromeSigninInterceptionUserChoice(
    const GaiaId& gaia_id) const {
  const base::Value::Dict* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  // If the account dict does not exist yet; return the default value.
  if (!account_dict) {
    return ChromeSigninUserChoice::kNoChoice;
  }
  // Return the pref value if it exists, otherwise return the default value.
  // No value default to 0 -> `ChromeSigninUserChoice::kNoChoice`.
  return static_cast<ChromeSigninUserChoice>(
      account_dict->FindInt(kChromeSigninInterceptionUserChoice).value_or(0));
}

void SigninPrefs::SetChromeLastSignoutTime(const GaiaId& gaia_id,
                                           base::Time last_signout_time) {
  SetTimePref(last_signout_time, gaia_id, kChromeLastSignoutTime);
}

std::optional<base::Time> SigninPrefs::GetChromeLastSignoutTime(
    const GaiaId& gaia_id) const {
  return GetTimePref(gaia_id, kChromeLastSignoutTime);
}

void SigninPrefs::SetChromeSigninInterceptionLastBubbleDeclineTime(
    const GaiaId& gaia_id,
    base::Time reprompt_time) {
  SetTimePref(reprompt_time, gaia_id,
              kChromeSigninInterceptionLastBubbleDeclineTime);
}

void SigninPrefs::ClearChromeSigninInterceptionLastBubbleDeclineTime(
    const GaiaId& gaia_id) {
  ClearPref(gaia_id, kChromeSigninInterceptionLastBubbleDeclineTime);
}

std::optional<base::Time>
SigninPrefs::GetChromeSigninInterceptionLastBubbleDeclineTime(
    const GaiaId& gaia_id) const {
  return GetTimePref(gaia_id, kChromeSigninInterceptionLastBubbleDeclineTime);
}

int SigninPrefs::IncrementChromeSigninBubbleRepromptCount(
    const GaiaId& gaia_id) {
  return IncrementIntPrefForAccount(gaia_id,
                                    kChromeSigninInterceptionRepromptCount);
}

int SigninPrefs::GetChromeSigninBubbleRepromptCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(gaia_id, kChromeSigninInterceptionRepromptCount);
}

void SigninPrefs::ClearChromeSigninBubbleRepromptCount(const GaiaId& gaia_id) {
  ClearPref(gaia_id, kChromeSigninInterceptionRepromptCount);
}

int SigninPrefs::IncrementChromeSigninInterceptionDismissCount(
    const GaiaId& gaia_id) {
  return IncrementIntPrefForAccount(gaia_id,
                                    kChromeSigninInterceptionDismissCount);
}

int SigninPrefs::GetChromeSigninInterceptionDismissCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(gaia_id, kChromeSigninInterceptionDismissCount);
}

void SigninPrefs::IncrementPasswordSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id,
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? kPasswordSignInPromoShownCountForLimitsExperiment
          : kPasswordSignInPromoShownCount
  );
}

int SigninPrefs::GetPasswordSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id,
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? kPasswordSignInPromoShownCountForLimitsExperiment
          : kPasswordSignInPromoShownCount
  );
}

void SigninPrefs::IncrementAddressSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id,
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? kAddressSignInPromoShownCountForLimitsExperiment
          : kAddressSignInPromoShownCount
  );
}

int SigninPrefs::GetAddressSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id,
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? kAddressSignInPromoShownCountForLimitsExperiment
          : kAddressSignInPromoShownCount
  );
}

void SigninPrefs::IncrementBookmarkSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id,
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? kBookmarkSignInPromoShownCountForLimitsExperiment
          : kBookmarkSignInPromoShownCount);
}

int SigninPrefs::GetBookmarkSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id,
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? kBookmarkSignInPromoShownCountForLimitsExperiment
          : kBookmarkSignInPromoShownCount);
}

void SigninPrefs::IncrementAutofillSigninPromoDismissCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(gaia_id, kAutofillSignInPromoDismissCount);
}

int SigninPrefs::GetAutofillSigninPromoDismissCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(gaia_id, kAutofillSignInPromoDismissCount);
}

void SigninPrefs::IncrementAddressSigninPromoDismissCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(gaia_id, kAddressSignInPromoDismissCount);
}

int SigninPrefs::GetAddressSigninPromoDismissCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(gaia_id, kAddressSignInPromoDismissCount);
}

void SigninPrefs::IncrementBookmarkSigninPromoDismissCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(gaia_id, kBookmarkSignInPromoDismissCount);
}

int SigninPrefs::GetBookmarkSigninPromoDismissCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(gaia_id, kBookmarkSignInPromoDismissCount);
}

void SigninPrefs::IncrementPasswordSigninPromoDismissCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(gaia_id, kPasswordSignInPromoDismissCount);
}

int SigninPrefs::GetPasswordSigninPromoDismissCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(gaia_id, kPasswordSignInPromoDismissCount);
}

void SigninPrefs::SetExtensionsExplicitBrowserSignin(const GaiaId& gaia_id,
                                                     bool enabled) {
  SetBooleanPrefForAccount(gaia_id, kExtensionsExplicitBrowserSigninEnabled,
                           enabled);
}

bool SigninPrefs::GetExtensionsExplicitBrowserSignin(
    const GaiaId& gaia_id) const {
  return GetBooleanPrefForAccount(gaia_id,
                                  kExtensionsExplicitBrowserSigninEnabled);
}

void SigninPrefs::SetBookmarksExplicitBrowserSignin(const GaiaId& gaia_id,
                                                    bool enabled) {
  // The pref can only be set to true if the
  // `switches::kSyncEnableBookmarksInTransportMode` flag is enabled.
  CHECK(!enabled || base::FeatureList::IsEnabled(
                        switches::kSyncEnableBookmarksInTransportMode));
  SetBooleanPrefForAccount(gaia_id, kBookmarksExplicitBrowserSigninEnabled,
                           enabled);
}

bool SigninPrefs::GetBookmarksExplicitBrowserSignin(
    const GaiaId& gaia_id) const {
  return GetBooleanPrefForAccount(gaia_id,
                                  kBookmarksExplicitBrowserSigninEnabled);
}

void SigninPrefs::SetPolicyDisclaimerLastRegistrationFailureTime(
    const GaiaId& gaia_id,
    base::Time last_registration_failure_time) {
  SetTimePref(last_registration_failure_time, gaia_id,
              kPolicyDisclaimerLastRegistrationFailureTime);
}

void SigninPrefs::ClearPolicyDisclaimerLastRegistrationFailureTime(
    const GaiaId& gaia_id) {
  ClearPref(gaia_id, kPolicyDisclaimerLastRegistrationFailureTime);
}

std::optional<base::Time>
SigninPrefs::GetPolicyDisclaimerLastRegistrationFailureTime(
    const GaiaId& gaia_id) const {
  return GetTimePref(gaia_id, kPolicyDisclaimerLastRegistrationFailureTime);
}

void SigninPrefs::IncrementSyncPromoIdentityPillShownCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(gaia_id, kSyncPromoIdentityPillShownCount);
}

int SigninPrefs::GetSyncPromoIdentityPillShownCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(gaia_id, kSyncPromoIdentityPillShownCount);
}

void SigninPrefs::IncrementSyncPromoIdentityPillUsedCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(gaia_id, kSyncPromoIdentityPillUsedCount);
}

int SigninPrefs::GetSyncPromoIdentityPillUsedCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(gaia_id, kSyncPromoIdentityPillUsedCount);
}

int SigninPrefs::GetHistoryPageHistorySyncPromoShownCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(gaia_id,
                              prefs::kHistoryPageHistorySyncPromoShownCount);
}

void SigninPrefs::IncrementHistoryPageHistorySyncPromoShownCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(gaia_id,
                             prefs::kHistoryPageHistorySyncPromoShownCount);
}

std::optional<base::Time>
SigninPrefs::GetHistoryPageHistorySyncPromoLastDismissedTimestamp(
    const GaiaId& gaia_id) const {
  return GetTimePref(gaia_id,
                     prefs::kHistoryPageHistorySyncPromoLastDismissedTimestamp);
}

void SigninPrefs::SetHistoryPageHistorySyncPromoLastDismissedTimestamp(
    const GaiaId& gaia_id,
    base::Time last_dismissed_timestamp) {
  SetTimePref(last_dismissed_timestamp, gaia_id,
              prefs::kHistoryPageHistorySyncPromoLastDismissedTimestamp);
}

bool SigninPrefs::GetHistoryPageHistorySyncPromoShownAfterDismissal(
    const GaiaId& gaia_id) const {
  return GetBooleanPrefForAccount(
      gaia_id, prefs::kHistoryPageHistorySyncPromoShownAfterDismissal);
}

void SigninPrefs::SetHistoryPageHistorySyncPromoShownAfterDismissal(
    const GaiaId& gaia_id) {
  SetBooleanPrefForAccount(
      gaia_id, prefs::kHistoryPageHistorySyncPromoShownAfterDismissal, true);
}

void SigninPrefs::IncrementBookmarkBatchUploadPromoDismissCountWithLastTime(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(gaia_id, kBookmarkBatchUploadPromoDismissCount);
  SetTimePref(base::Time::Now(), gaia_id,
              kBookmarkBatchUploadPromoLastDismissTime);
}

std::pair<int, std::optional<base::Time>>
SigninPrefs::GetBookmarkBatchUploadPromoDismissCountWithLastTime(
    const GaiaId& gaia_id) {
  return {GetIntPrefForAccount(gaia_id, kBookmarkBatchUploadPromoDismissCount),
          GetTimePref(gaia_id, kBookmarkBatchUploadPromoLastDismissTime)};
}

base::DictValue& SigninPrefs::GetOrCreateAvatarButtonPromoCountDictionary(
    const GaiaId& gaia_id) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // `EnsureDict` gets or create the dictionary.
  return *scoped_update->EnsureDict(gaia_id.ToString())
              ->EnsureDict(kAvatarButtonPromoCountDictionary);
}

int SigninPrefs::IncrementIntPrefForAccount(const GaiaId& gaia_id,
                                            std::string_view pref) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);

  // `EnsureDict` gets or create the dictionary.
  base::Value::Dict* account_dict =
      scoped_update->EnsureDict(gaia_id.ToString());
  // Get the current value of the pref.
  int new_value = account_dict->FindInt(pref).value_or(0) + 1;
  // `Set` will add an entry if it doesn't already exists, or if it does, it
  // will overwrite it.
  account_dict->Set(pref, new_value);

  return new_value;
}

int SigninPrefs::GetIntPrefForAccount(const GaiaId& gaia_id,
                                      std::string_view pref) const {
  CHECK(!gaia_id.empty());
  const base::Value::Dict* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  // If the account dict does not exist yet; return the default value.
  if (!account_dict) {
    return 0;
  }

  // Return the pref value if it exists, otherwise return the default value.
  return account_dict->FindInt(pref).value_or(0);
}

void SigninPrefs::SetBooleanPrefForAccount(const GaiaId& gaia_id,
                                           std::string_view pref,
                                           bool enabled) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // `EnsureDict` gets or create the dictionary.
  base::Value::Dict* account_dict =
      scoped_update->EnsureDict(gaia_id.ToString());
  // `Set` will add an entry if it doesn't already exists, or if it does, it
  // will overwrite it.
  account_dict->Set(pref, enabled);
}

bool SigninPrefs::GetBooleanPrefForAccount(const GaiaId& gaia_id,
                                           std::string_view pref) const {
  CHECK(!gaia_id.empty());
  const base::Value::Dict* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  // If the account dict does not exist yet; return the default value.
  if (!account_dict) {
    return false;
  }

  // Return the pref value if it exists, otherwise return the default value.
  return account_dict->FindBool(pref).value_or(false);
}

void SigninPrefs::SetTimePref(base::Time time,
                              const GaiaId& gaia_id,
                              std::string_view pref) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // `EnsureDict` gets or create the dictionary.
  base::Value::Dict* account_dict =
      scoped_update->EnsureDict(gaia_id.ToString());
  // `Set` will add an entry if it doesn't already exists, or if it does, it
  // will overwrite it.
  account_dict->Set(pref, base::TimeToValue(time));
}

std::optional<base::Time> SigninPrefs::GetTimePref(
    const GaiaId& gaia_id,
    std::string_view pref) const {
  CHECK(!gaia_id.empty());
  const base::Value::Dict* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  // If the account dict does not exist yet; return no time.
  if (!account_dict) {
    return std::nullopt;
  }
  // Return the pref value if it exists, otherwise return no time.
  const base::Value* value = account_dict->Find(pref);
  return value ? base::ValueToTime(value) : std::nullopt;
}

void SigninPrefs::ClearPref(const GaiaId& gaia_id, std::string_view pref) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // Do not create an account dictionary if it does not already exist.
  base::Value::Dict* account_dict = scoped_update->FindDict(gaia_id.ToString());
  if (!account_dict) {
    return;
  }

  account_dict->Remove(pref);
}

void SigninPrefs::SetDeprecatedPrefForTesting(const GaiaId& gaia_id) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // `EnsureDict` gets or create the dictionary.
  base::Value::Dict* account_dict =
      scoped_update->EnsureDict(gaia_id.ToString());

  account_dict->Set(kDeprecatingTestingPref, 123);
}

std::optional<int> SigninPrefs::GetDeprecatedPrefForTesting(
    const GaiaId& gaia_id) {
  CHECK(!gaia_id.empty());
  const base::Value::Dict* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  if (!account_dict) {
    return std::nullopt;
  }

  std::optional<int> pref_value =
      account_dict->FindInt(kDeprecatingTestingPref);
  return pref_value.has_value() ? pref_value.value()
                                : std::optional<int>(std::nullopt);
}
