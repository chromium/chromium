// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_prefs.h"

#include <string_view>

#include "base/json/values_util.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs_keys.h"
#include "components/signin/public/base/signin_switches.h"
#include "google_apis/gaia/gaia_id.h"

namespace {
// Name of the main pref dictionary holding the account dictionaries of the
// underlying prefs, with the key as the GaiaIds. The prefs stored through
// this dict here are information not directly related to the account itself,
// only metadata.
constexpr char kSigninAccountPrefs[] = "signin.accounts_metadata_dict";

// The next unassigned ID for account metrics.
constexpr char kAccountMetricsNextUnassignedId[] =
    "signin.account_metrics_next_unassigned_id";

}  // namespace

SigninPrefs::SigninPrefs(PrefService& pref_service)
    : pref_service_(pref_service) {}

SigninPrefs::~SigninPrefs() = default;

void SigninPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kSigninAccountPrefs);
  registry->RegisterIntegerPref(prefs::kHistorySyncSuccessiveDeclineCount, 0);
  registry->RegisterInt64Pref(prefs::kHistorySyncLastDeclinedTimestamp, 0);
  registry->RegisterIntegerPref(kAccountMetricsNextUnassignedId, 0);
}

void SigninPrefs::MigrateObsoleteSigninPrefs() {
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // Deprecates prefs within the existing internal account dict.
  for (auto value : scoped_update.Get()) {
    base::DictValue& account_dict = value.second.GetDict();
    for (std::string_view deprecated_pref :
         signin::internal::kDeprecatedSigninPrefs) {
      account_dict.Remove(deprecated_pref);
    }
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
    if (!gaia_ids_to_keep.contains(gaia_id)) {
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
  base::DictValue* account_dict = scoped_update->EnsureDict(gaia_id.ToString());
  // `Set` will add an entry if it doesn't already exists, or if it does, it
  // will overwrite it.
  account_dict->Set(signin::internal::kChromeSigninInterceptionUserChoice,
                    static_cast<int>(user_choice));
}

ChromeSigninUserChoice SigninPrefs::GetChromeSigninInterceptionUserChoice(
    const GaiaId& gaia_id) const {
  const base::DictValue* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  // If the account dict does not exist yet; return the default value.
  if (!account_dict) {
    return ChromeSigninUserChoice::kNoChoice;
  }
  // Return the pref value if it exists, otherwise return the default value.
  // No value default to 0 -> `ChromeSigninUserChoice::kNoChoice`.
  return static_cast<ChromeSigninUserChoice>(
      account_dict
          ->FindInt(signin::internal::kChromeSigninInterceptionUserChoice)
          .value_or(0));
}

void SigninPrefs::SetAccountMetricsId(const GaiaId& gaia_id, int id) {
  SetIntPrefForAccount(gaia_id, signin::internal::kAccountMetricsId, id);
}

std::optional<int> SigninPrefs::GetAccountMetricsId(
    const GaiaId& gaia_id) const {
  CHECK(!gaia_id.empty());
  const base::DictValue* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  if (!account_dict) {
    return std::nullopt;
  }
  return account_dict->FindInt(signin::internal::kAccountMetricsId);
}

void SigninPrefs::SetAccountMetricsIdCapped(const GaiaId& gaia_id) {
  SetBooleanPrefForAccount(gaia_id, signin::internal::kAccountMetricsIdIsCapped,
                           true);
}

bool SigninPrefs::IsAccountMetricsIdCapped(const GaiaId& gaia_id) const {
  return GetBooleanPrefForAccount(gaia_id,
                                  signin::internal::kAccountMetricsIdIsCapped);
}

int SigninPrefs::GetNextAccountMetricsUnassignedId() const {
  return pref_service_->GetInteger(kAccountMetricsNextUnassignedId);
}

void SigninPrefs::SetNextAccountMetricsUnassignedId(int id) {
  pref_service_->SetInteger(kAccountMetricsNextUnassignedId, id);
}

void SigninPrefs::SetChromeLastSignoutTime(const GaiaId& gaia_id,
                                           base::Time last_signout_time) {
  SetTimePref(last_signout_time, gaia_id,
              signin::internal::kChromeLastSignoutTime);
}

std::optional<base::Time> SigninPrefs::GetChromeLastSignoutTime(
    const GaiaId& gaia_id) const {
  return GetTimePref(gaia_id, signin::internal::kChromeLastSignoutTime);
}

void SigninPrefs::SetChromeSigninInterceptionLastBubbleDeclineTime(
    const GaiaId& gaia_id,
    base::Time last_decline_time) {
  SetTimePref(last_decline_time, gaia_id,
              signin::internal::kChromeSigninInterceptionLastBubbleDeclineTime);
}

void SigninPrefs::ClearChromeSigninInterceptionLastBubbleDeclineTime(
    const GaiaId& gaia_id) {
  ClearPref(gaia_id,
            signin::internal::kChromeSigninInterceptionLastBubbleDeclineTime);
}

std::optional<base::Time>
SigninPrefs::GetChromeSigninInterceptionLastBubbleDeclineTime(
    const GaiaId& gaia_id) const {
  return GetTimePref(
      gaia_id,
      signin::internal::kChromeSigninInterceptionLastBubbleDeclineTime);
}

int SigninPrefs::IncrementChromeSigninBubbleRepromptCount(
    const GaiaId& gaia_id) {
  return IncrementIntPrefForAccount(
      gaia_id, signin::internal::kChromeSigninInterceptionRepromptCount);
}

int SigninPrefs::GetChromeSigninBubbleRepromptCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kChromeSigninInterceptionRepromptCount);
}

void SigninPrefs::ClearChromeSigninBubbleRepromptCount(const GaiaId& gaia_id) {
  ClearPref(gaia_id, signin::internal::kChromeSigninInterceptionRepromptCount);
}

int SigninPrefs::IncrementChromeSigninInterceptionDismissCount(
    const GaiaId& gaia_id) {
  return IncrementIntPrefForAccount(
      gaia_id, signin::internal::kChromeSigninInterceptionDismissCount);
}

int SigninPrefs::GetChromeSigninInterceptionDismissCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kChromeSigninInterceptionDismissCount);
}

void SigninPrefs::IncrementPasswordSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id,
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? signin::internal::kPasswordSignInPromoShownCountForLimitsExperiment
          : signin::internal::kPasswordSignInPromoShownCount);
}

int SigninPrefs::GetPasswordSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id,
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? signin::internal::kPasswordSignInPromoShownCountForLimitsExperiment
          : signin::internal::kPasswordSignInPromoShownCount);
}

void SigninPrefs::IncrementAddressSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id,
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? signin::internal::kAddressSignInPromoShownCountForLimitsExperiment
          : signin::internal::kAddressSignInPromoShownCount);
}

int SigninPrefs::GetAddressSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id,
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? signin::internal::kAddressSignInPromoShownCountForLimitsExperiment
          : signin::internal::kAddressSignInPromoShownCount);
}

void SigninPrefs::IncrementBookmarkSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id,
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? signin::internal::kBookmarkSignInPromoShownCountForLimitsExperiment
          : signin::internal::kBookmarkSignInPromoShownCount);
}

int SigninPrefs::GetBookmarkSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id,
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? signin::internal::kBookmarkSignInPromoShownCountForLimitsExperiment
          : signin::internal::kBookmarkSignInPromoShownCount);
}

void SigninPrefs::IncrementSearchAIModeSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kSearchAIModeSignInPromoShownCount);
}

int SigninPrefs::GetSearchAIModeSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kSearchAIModeSignInPromoShownCount);
}

void SigninPrefs::IncrementAutofillSigninPromoDismissCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kAutofillSignInPromoDismissCount);
}

int SigninPrefs::GetAutofillSigninPromoDismissCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kAutofillSignInPromoDismissCount);
}

void SigninPrefs::IncrementAddressSigninPromoDismissCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(gaia_id,
                             signin::internal::kAddressSignInPromoDismissCount);
}

int SigninPrefs::GetAddressSigninPromoDismissCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kAddressSignInPromoDismissCount);
}

void SigninPrefs::IncrementBookmarkSigninPromoDismissCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kBookmarkSignInPromoDismissCount);
}

int SigninPrefs::GetBookmarkSigninPromoDismissCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kBookmarkSignInPromoDismissCount);
}

void SigninPrefs::IncrementPasswordSigninPromoDismissCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kPasswordSignInPromoDismissCount);
}

int SigninPrefs::GetPasswordSigninPromoDismissCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kPasswordSignInPromoDismissCount);
}

void SigninPrefs::IncrementSearchAIModeSigninPromoDismissCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kSearchAIModeSignInPromoDismissCount);
}

int SigninPrefs::GetSearchAIModeSigninPromoDismissCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kSearchAIModeSignInPromoDismissCount);
}

void SigninPrefs::SetExtensionsExplicitBrowserSignin(const GaiaId& gaia_id,
                                                     bool enabled) {
  SetBooleanPrefForAccount(
      gaia_id, signin::internal::kExtensionsExplicitBrowserSigninEnabled,
      enabled);
}

bool SigninPrefs::GetExtensionsExplicitBrowserSignin(
    const GaiaId& gaia_id) const {
  return GetBooleanPrefForAccount(
      gaia_id, signin::internal::kExtensionsExplicitBrowserSigninEnabled);
}

void SigninPrefs::SetBookmarksExplicitBrowserSignin(const GaiaId& gaia_id,
                                                    bool enabled) {
  SetBooleanPrefForAccount(
      gaia_id, signin::internal::kBookmarksExplicitBrowserSigninEnabled,
      enabled);
}

bool SigninPrefs::GetBookmarksExplicitBrowserSignin(
    const GaiaId& gaia_id) const {
  return GetBooleanPrefForAccount(
      gaia_id, signin::internal::kBookmarksExplicitBrowserSigninEnabled);
}

void SigninPrefs::SetPolicyDisclaimerLastRegistrationFailureTime(
    const GaiaId& gaia_id,
    base::Time last_registration_failure_time) {
  SetTimePref(last_registration_failure_time, gaia_id,
              signin::internal::kPolicyDisclaimerLastRegistrationFailureTime);
}

void SigninPrefs::ClearPolicyDisclaimerLastRegistrationFailureTime(
    const GaiaId& gaia_id) {
  ClearPref(gaia_id,
            signin::internal::kPolicyDisclaimerLastRegistrationFailureTime);
}

void SigninPrefs::SetSearchAIModeSigninPromoLastImpressionTime(
    const GaiaId& gaia_id,
    base::Time last_impression_time) {
  SetTimePref(last_impression_time, gaia_id,
              signin::internal::kSearchAIModeSignInPromoLastImpressionTime);
}

std::optional<base::Time>
SigninPrefs::GetSearchAIModeSigninPromoLastImpressionTime(
    const GaiaId& gaia_id) const {
  return GetTimePref(
      gaia_id, signin::internal::kSearchAIModeSignInPromoLastImpressionTime);
}

std::optional<base::Time>
SigninPrefs::GetPolicyDisclaimerLastRegistrationFailureTime(
    const GaiaId& gaia_id) const {
  return GetTimePref(
      gaia_id, signin::internal::kPolicyDisclaimerLastRegistrationFailureTime);
}

int SigninPrefs::GetHistoryPageHistorySyncPromoShownCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kHistoryPageHistorySyncPromoShownCount);
}

void SigninPrefs::IncrementHistoryPageHistorySyncPromoShownCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kHistoryPageHistorySyncPromoShownCount);
}

std::optional<base::Time>
SigninPrefs::GetHistoryPageHistorySyncPromoLastDismissedTimestamp(
    const GaiaId& gaia_id) const {
  return GetTimePref(
      gaia_id,
      signin::internal::kHistoryPageHistorySyncPromoLastDismissedTimestamp);
}

void SigninPrefs::SetHistoryPageHistorySyncPromoLastDismissedTimestamp(
    const GaiaId& gaia_id,
    base::Time last_dismissed_timestamp) {
  SetTimePref(
      last_dismissed_timestamp, gaia_id,
      signin::internal::kHistoryPageHistorySyncPromoLastDismissedTimestamp);
}

bool SigninPrefs::GetHistoryPageHistorySyncPromoShownAfterDismissal(
    const GaiaId& gaia_id) const {
  return GetBooleanPrefForAccount(
      gaia_id,
      signin::internal::kHistoryPageHistorySyncPromoShownAfterDismissal);
}

void SigninPrefs::SetHistoryPageHistorySyncPromoShownAfterDismissal(
    const GaiaId& gaia_id) {
  SetBooleanPrefForAccount(
      gaia_id,
      signin::internal::kHistoryPageHistorySyncPromoShownAfterDismissal, true);
}

void SigninPrefs::IncrementBookmarkBatchUploadPromoDismissCountWithLastTime(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kBookmarkBatchUploadPromoDismissCount);
  SetTimePref(base::Time::Now(), gaia_id,
              signin::internal::kBookmarkBatchUploadPromoLastDismissTime);
}

std::pair<int, std::optional<base::Time>>
SigninPrefs::GetBookmarkBatchUploadPromoDismissCountWithLastTime(
    const GaiaId& gaia_id) {
  return {
      GetIntPrefForAccount(
          gaia_id, signin::internal::kBookmarkBatchUploadPromoDismissCount),
      GetTimePref(gaia_id,
                  signin::internal::kBookmarkBatchUploadPromoLastDismissTime)};
}

void SigninPrefs::SetBatchUploadLastUploadRemainingLocalDataCount(
    const GaiaId& gaia_id,
    int count) {
  SetIntPrefForAccount(
      gaia_id, signin::internal::kBatchUploadLastUploadRemainingLocalDataCount,
      count);
}

std::optional<int> SigninPrefs::GetBatchUploadLastUploadRemainingLocalDataCount(
    const GaiaId& gaia_id) const {
  CHECK(!gaia_id.empty());
  const base::DictValue* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  if (!account_dict) {
    return std::nullopt;
  }
  return account_dict->FindInt(
      signin::internal::kBatchUploadLastUploadRemainingLocalDataCount);
}

base::DictValue& SigninPrefs::GetOrCreateAvatarButtonPromoCountDictionary(
    const GaiaId& gaia_id) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // `EnsureDict` gets or create the dictionary.
  return *scoped_update->EnsureDict(gaia_id.ToString())
              ->EnsureDict(signin::internal::kAvatarButtonPromoCountDictionary);
}

base::DictValue& SigninPrefs::GetOrCreateCrossDevicePromoPrefs(
    const GaiaId& gaia_id) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // `EnsureDict` gets or create the dictionary.
  return *scoped_update->EnsureDict(gaia_id.ToString())
              ->EnsureDict(signin::internal::kCrossDevicePromoPrefs);
}

int SigninPrefs::IncrementIntPrefForAccount(const GaiaId& gaia_id,
                                            std::string_view pref) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);

  // `EnsureDict` gets or create the dictionary.
  base::DictValue* account_dict = scoped_update->EnsureDict(gaia_id.ToString());
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
  const base::DictValue* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  // If the account dict does not exist yet; return the default value.
  if (!account_dict) {
    return 0;
  }

  // Return the pref value if it exists, otherwise return the default value.
  return account_dict->FindInt(pref).value_or(0);
}

void SigninPrefs::SetIntPrefForAccount(const GaiaId& gaia_id,
                                       std::string_view pref,
                                       int value) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  base::DictValue* account_dict = scoped_update->EnsureDict(gaia_id.ToString());
  account_dict->Set(pref, value);
}

void SigninPrefs::SetBooleanPrefForAccount(const GaiaId& gaia_id,
                                           std::string_view pref,
                                           bool enabled) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // `EnsureDict` gets or create the dictionary.
  base::DictValue* account_dict = scoped_update->EnsureDict(gaia_id.ToString());
  // `Set` will add an entry if it doesn't already exists, or if it does, it
  // will overwrite it.
  account_dict->Set(pref, enabled);
}

bool SigninPrefs::GetBooleanPrefForAccount(const GaiaId& gaia_id,
                                           std::string_view pref) const {
  CHECK(!gaia_id.empty());
  const base::DictValue* account_dict =
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
  base::DictValue* account_dict = scoped_update->EnsureDict(gaia_id.ToString());
  // `Set` will add an entry if it doesn't already exists, or if it does, it
  // will overwrite it.
  account_dict->Set(pref, base::TimeToValue(time));
}

std::optional<base::Time> SigninPrefs::GetTimePref(
    const GaiaId& gaia_id,
    std::string_view pref) const {
  CHECK(!gaia_id.empty());
  const base::DictValue* account_dict =
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
  base::DictValue* account_dict = scoped_update->FindDict(gaia_id.ToString());
  if (!account_dict) {
    return;
  }

  account_dict->Remove(pref);
}

void SigninPrefs::SetDeprecatedPrefForTesting(const GaiaId& gaia_id) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // `EnsureDict` gets or create the dictionary.
  base::DictValue* account_dict = scoped_update->EnsureDict(gaia_id.ToString());

  account_dict->Set(signin::internal::kDeprecatedTestingPref, 123);
}

std::optional<int> SigninPrefs::GetDeprecatedPrefForTesting(
    const GaiaId& gaia_id) {
  CHECK(!gaia_id.empty());
  const base::DictValue* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  if (!account_dict) {
    return std::nullopt;
  }

  std::optional<int> pref_value =
      account_dict->FindInt(signin::internal::kDeprecatedTestingPref);
  return pref_value.has_value() ? pref_value.value()
                                : std::optional<int>(std::nullopt);
}
