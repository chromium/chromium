// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_prefs.h"

#include <string_view>

#include "base/json/values_util.h"
#include "base/numerics/clamped_math.h"
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
  IncrementIntPrefForAccount(gaia_id,
                             signin::internal::kPasswordSignInPromoShownCount);
}

int SigninPrefs::GetPasswordSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(gaia_id,
                              signin::internal::kPasswordSignInPromoShownCount);
}

void SigninPrefs::IncrementAddressSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(gaia_id,
                             signin::internal::kAddressSignInPromoShownCount);
}

int SigninPrefs::GetAddressSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(gaia_id,
                              signin::internal::kAddressSignInPromoShownCount);
}

void SigninPrefs::IncrementBookmarkSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(gaia_id,
                             signin::internal::kBookmarkSignInPromoShownCount);
}

int SigninPrefs::GetBookmarkSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(gaia_id,
                              signin::internal::kBookmarkSignInPromoShownCount);
}

void SigninPrefs::IncrementBookmarkManagerSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kBookmarkManagerSignInPromoShownCount);
}

int SigninPrefs::GetBookmarkManagerSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kBookmarkManagerSignInPromoShownCount);
}

void SigninPrefs::IncrementBookmarkManagerSigninPromoDismissCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kBookmarkManagerSignInPromoDismissCount);
}

int SigninPrefs::GetBookmarkManagerSigninPromoDismissCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kBookmarkManagerSignInPromoDismissCount);
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

void SigninPrefs::IncrementAvatarButtonHistorySyncPromoShownCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kAvatarButtonHistorySyncPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonHistorySyncPromoShownCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kAvatarButtonHistorySyncPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::IncrementAvatarButtonHistorySyncPromoUsedCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kAvatarButtonHistorySyncPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonHistorySyncPromoUsedCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kAvatarButtonHistorySyncPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::IncrementAvatarButtonBatchUploadPromoShownCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kAvatarButtonBatchUploadPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonBatchUploadPromoShownCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kAvatarButtonBatchUploadPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::IncrementAvatarButtonBatchUploadPromoUsedCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kAvatarButtonBatchUploadPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonBatchUploadPromoUsedCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kAvatarButtonBatchUploadPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::IncrementAvatarButtonBatchUploadBookmarkPromoShownCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id,
      signin::internal::kAvatarButtonBatchUploadBookmarkPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonBatchUploadBookmarkPromoShownCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id,
      signin::internal::kAvatarButtonBatchUploadBookmarkPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::IncrementAvatarButtonBatchUploadBookmarkPromoUsedCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kAvatarButtonBatchUploadBookmarkPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonBatchUploadBookmarkPromoUsedCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kAvatarButtonBatchUploadBookmarkPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::
    IncrementAvatarButtonBatchUploadWindows10DepreciationPromoShownCount(
        const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id,
      signin::internal::
          kAvatarButtonBatchUploadWindows10DepreciationPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonBatchUploadWindows10DepreciationPromoShownCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id,
      signin::internal::
          kAvatarButtonBatchUploadWindows10DepreciationPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::
    IncrementAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount(
        const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id,
      signin::internal::
          kAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id,
      signin::internal::
          kAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::IncrementAvatarButtonSigninPromoShownCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kAvatarButtonSigninPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonSigninPromoShownCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kAvatarButtonSigninPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::IncrementAvatarButtonSigninPromoUsedCount(
    const GaiaId& gaia_id) {
  IncrementIntPrefForAccount(
      gaia_id, signin::internal::kAvatarButtonSigninPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonSigninPromoUsedCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kAvatarButtonSigninPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::SetAvatarButtonSigninPromoLastShownTime(
    const GaiaId& gaia_id,
    base::Time last_shown_time) {
  SetTimePref(last_shown_time, gaia_id,
              signin::internal::kAvatarButtonSigninPromoLastShownTime,
              signin::internal::kAvatarButtonPromoParents);
}

std::optional<base::Time> SigninPrefs::GetAvatarButtonSigninPromoLastShownTime(
    const GaiaId& gaia_id) const {
  return GetTimePref(gaia_id,
                     signin::internal::kAvatarButtonSigninPromoLastShownTime,
                     signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::SetCrossDeviceHistoryPromoShownCount(const GaiaId& gaia_id,
                                                       int count) {
  SetIntPrefForAccount(gaia_id,
                       signin::internal::kCrossDevicePromoShownCountKey, count,
                       signin::internal::kCrossDeviceHistoryPromoParents);
}

int SigninPrefs::GetCrossDeviceHistoryPromoShownCount(
    const GaiaId& gaia_id) const {
  return GetIntPrefForAccount(
      gaia_id, signin::internal::kCrossDevicePromoShownCountKey,
      signin::internal::kCrossDeviceHistoryPromoParents);
}

void SigninPrefs::SetCrossDeviceHistoryPromoShownAfterDismissal(
    const GaiaId& gaia_id,
    bool shown_after_dismissal) {
  SetBooleanPrefForAccount(
      gaia_id, signin::internal::kCrossDevicePromoShownAfterDismissalKey,
      shown_after_dismissal, signin::internal::kCrossDeviceHistoryPromoParents);
}

bool SigninPrefs::GetCrossDeviceHistoryPromoShownAfterDismissal(
    const GaiaId& gaia_id) const {
  return GetBooleanPrefForAccount(
      gaia_id, signin::internal::kCrossDevicePromoShownAfterDismissalKey,
      signin::internal::kCrossDeviceHistoryPromoParents);
}

void SigninPrefs::SetCrossDeviceHistoryPromoLastDismissedTime(
    const GaiaId& gaia_id,
    base::Time last_dismissed_time) {
  SetTimePref(last_dismissed_time, gaia_id,
              signin::internal::kCrossDevicePromoLastDismissedTimeKey,
              signin::internal::kCrossDeviceHistoryPromoParents);
}

void SigninPrefs::ClearCrossDeviceHistoryPromoLastDismissedTime(
    const GaiaId& gaia_id) {
  ClearPref(gaia_id, signin::internal::kCrossDevicePromoLastDismissedTimeKey,
            signin::internal::kCrossDeviceHistoryPromoParents);
}

std::optional<base::Time>
SigninPrefs::GetCrossDeviceHistoryPromoLastDismissedTime(
    const GaiaId& gaia_id) const {
  return GetTimePref(gaia_id,
                     signin::internal::kCrossDevicePromoLastDismissedTimeKey,
                     signin::internal::kCrossDeviceHistoryPromoParents);
}

int SigninPrefs::IncrementIntPrefForAccount(
    const GaiaId& gaia_id,
    std::string_view pref,
    base::span<const std::string_view> parents) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);

  // `EnsureDict` gets or create the dictionary.
  base::DictValue* dict = scoped_update->EnsureDict(gaia_id.ToString());
  for (std::string_view parent : parents) {
    dict = dict->EnsureDict(parent);
  }
  // Get the current value of the pref.
  int new_value = base::ClampAdd(dict->FindInt(pref).value_or(0), 1);
  // `Set` will add an entry if it doesn't already exists, or if it does, it
  // will overwrite it.
  dict->Set(pref, new_value);

  return new_value;
}

int SigninPrefs::GetIntPrefForAccount(
    const GaiaId& gaia_id,
    std::string_view pref,
    base::span<const std::string_view> parents) const {
  CHECK(!gaia_id.empty());
  const base::DictValue* dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  for (std::string_view parent : parents) {
    if (!dict) {
      return 0;
    }
    dict = dict->FindDict(parent);
  }
  // If the dict does not exist yet; return the default value.
  if (!dict) {
    return 0;
  }

  // Return the pref value if it exists, otherwise return the default value.
  return dict->FindInt(pref).value_or(0);
}

void SigninPrefs::SetIntPrefForAccount(
    const GaiaId& gaia_id,
    std::string_view pref,
    int value,
    base::span<const std::string_view> parents) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  base::DictValue* dict = scoped_update->EnsureDict(gaia_id.ToString());
  for (std::string_view parent : parents) {
    dict = dict->EnsureDict(parent);
  }
  dict->Set(pref, value);
}

void SigninPrefs::SetBooleanPrefForAccount(
    const GaiaId& gaia_id,
    std::string_view pref,
    bool enabled,
    base::span<const std::string_view> parents) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // `EnsureDict` gets or create the dictionary.
  base::DictValue* dict = scoped_update->EnsureDict(gaia_id.ToString());
  for (std::string_view parent : parents) {
    dict = dict->EnsureDict(parent);
  }
  // `Set` will add an entry if it doesn't already exists, or if it does, it
  // will overwrite it.
  dict->Set(pref, enabled);
}

bool SigninPrefs::GetBooleanPrefForAccount(
    const GaiaId& gaia_id,
    std::string_view pref,
    base::span<const std::string_view> parents) const {
  CHECK(!gaia_id.empty());
  const base::DictValue* dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  for (std::string_view parent : parents) {
    if (!dict) {
      return false;
    }
    dict = dict->FindDict(parent);
  }
  // If the dict does not exist yet; return the default value.
  if (!dict) {
    return false;
  }

  // Return the pref value if it exists, otherwise return the default value.
  return dict->FindBool(pref).value_or(false);
}

void SigninPrefs::SetTimePref(base::Time time,
                              const GaiaId& gaia_id,
                              std::string_view pref,
                              base::span<const std::string_view> parents) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // `EnsureDict` gets or create the dictionary.
  base::DictValue* dict = scoped_update->EnsureDict(gaia_id.ToString());
  for (std::string_view parent : parents) {
    dict = dict->EnsureDict(parent);
  }
  // `Set` will add an entry if it doesn't already exists, or if it does, it
  // will overwrite it.
  dict->Set(pref, base::TimeToValue(time));
}

std::optional<base::Time> SigninPrefs::GetTimePref(
    const GaiaId& gaia_id,
    std::string_view pref,
    base::span<const std::string_view> parents) const {
  CHECK(!gaia_id.empty());
  const base::DictValue* dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  for (std::string_view parent : parents) {
    if (!dict) {
      return std::nullopt;
    }
    dict = dict->FindDict(parent);
  }
  // If the dict does not exist yet; return no time.
  if (!dict) {
    return std::nullopt;
  }
  // Return the pref value if it exists, otherwise return no time.
  const base::Value* value = dict->Find(pref);
  return value ? base::ValueToTime(value) : std::nullopt;
}

void SigninPrefs::ClearPref(const GaiaId& gaia_id,
                            std::string_view pref,
                            base::span<const std::string_view> parents) {
  CHECK(!gaia_id.empty());
  // Check whether the target key exists before opening `ScopedDictPrefUpdate`
  // so that clearing an absent key or account does not dirty the pref store or
  // fire `PrefChangeRegistrar` notifications. Only a value controlled by the
  // user pref store is considered, because that is the store
  // `ScopedDictPrefUpdate` mutates; values coming from other stores cannot be
  // removed through it.
  // `PrefService::GetUserPrefValue()` is deliberately not used: it asserts
  // when the persisted value is not a dict, which corrupted on-disk data can
  // trigger. `GetDict()` type-checks every store and falls back to the default
  // (empty) dict instead, so a corrupted value is treated as having nothing to
  // clear.
  if (!pref_service_->FindPreference(kSigninAccountPrefs)->IsUserControlled()) {
    return;
  }
  const base::DictValue* const_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  for (std::string_view parent : parents) {
    if (!const_dict) {
      return;
    }
    const_dict = const_dict->FindDict(parent);
  }
  if (!const_dict || !const_dict->contains(pref)) {
    return;
  }

  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  base::DictValue* dict = scoped_update->FindDict(gaia_id.ToString());
  for (std::string_view parent : parents) {
    CHECK(dict);
    dict = dict->FindDict(parent);
  }
  CHECK(dict);
  dict->Remove(pref);
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
