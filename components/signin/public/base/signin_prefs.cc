// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_prefs.h"

#include <utility>

#include "base/types/pass_key.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs_accessor.h"
#include "components/signin/public/base/signin_prefs_keys.h"
#include "google_apis/gaia/gaia_id.h"

namespace {

// The next unassigned ID for account metrics.
constexpr char kAccountMetricsNextUnassignedId[] =
    "signin.account_metrics_next_unassigned_id";

}  // namespace

SigninPrefs::SigninPrefs(PrefService& pref_service)
    : pref_service_(pref_service),
      accessor_(pref_service, base::PassKey<SigninPrefs>()) {}

SigninPrefs::~SigninPrefs() = default;

void SigninPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  SigninPrefsAccessor::RegisterProfilePrefs(registry,
                                            base::PassKey<SigninPrefs>());
  registry->RegisterIntegerPref(prefs::kHistorySyncSuccessiveDeclineCount, 0);
  registry->RegisterInt64Pref(prefs::kHistorySyncLastDeclinedTimestamp, 0);
  registry->RegisterIntegerPref(kAccountMetricsNextUnassignedId, 0);
}

void SigninPrefs::MigrateObsoleteSigninPrefs() {
  accessor_.MigrateObsoleteAccountPrefs();
}

bool SigninPrefs::HasAccountPrefs(const GaiaId& gaia_id) const {
  return accessor_.HasAccountPrefs(gaia_id);
}

size_t SigninPrefs::RemoveAllAccountPrefsExcept(
    const base::flat_set<GaiaId>& gaia_ids_to_keep) {
  return accessor_.RemoveAllAccountPrefsExcept(gaia_ids_to_keep);
}

// static
void SigninPrefs::ObserveSigninPrefsChanges(PrefChangeRegistrar& registrar,
                                            base::RepeatingClosure callback) {
  SigninPrefsAccessor::ObserveChanges(registrar, std::move(callback),
                                      base::PassKey<SigninPrefs>());
}

void SigninPrefs::SetChromeSigninInterceptionUserChoice(
    const GaiaId& gaia_id,
    ChromeSigninUserChoice user_choice) {
  // TODO(crbug.com/570362906): Audit callers (such as `PeopleHandler` and
  // `ProcessDiceHeaderDelegateImpl`) so they never pass an empty `GaiaId`, and
  // then enforce `CHECK(!gaia_id.empty())` uniformly via `SigninPrefsAccessor`.
  if (gaia_id.empty() ||
      GetChromeSigninInterceptionUserChoice(gaia_id) == user_choice) {
    return;
  }
  accessor_.SetIntPref(gaia_id,
                       signin::internal::kChromeSigninInterceptionUserChoice,
                       static_cast<int>(user_choice));
}

ChromeSigninUserChoice SigninPrefs::GetChromeSigninInterceptionUserChoice(
    const GaiaId& gaia_id) const {
  // TODO(crbug.com/570362906): Disallow empty `gaia_id` once callers are fixed;
  // see `SetChromeSigninInterceptionUserChoice`.
  if (gaia_id.empty()) {
    return ChromeSigninUserChoice::kNoChoice;
  }
  // No value default to 0 -> `ChromeSigninUserChoice::kNoChoice`.
  return static_cast<ChromeSigninUserChoice>(accessor_.GetIntPref(
      gaia_id, signin::internal::kChromeSigninInterceptionUserChoice));
}

void SigninPrefs::SetAccountMetricsId(const GaiaId& gaia_id, int id) {
  accessor_.SetIntPref(gaia_id, signin::internal::kAccountMetricsId, id);
}

std::optional<int> SigninPrefs::GetAccountMetricsId(
    const GaiaId& gaia_id) const {
  return accessor_.MaybeGetIntPref(gaia_id,
                                    signin::internal::kAccountMetricsId);
}

void SigninPrefs::SetAccountMetricsIdCapped(const GaiaId& gaia_id) {
  accessor_.SetBooleanPref(gaia_id,
                            signin::internal::kAccountMetricsIdIsCapped, true);
}

bool SigninPrefs::IsAccountMetricsIdCapped(const GaiaId& gaia_id) const {
  return accessor_.GetBooleanPref(gaia_id,
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
  accessor_.SetTimePref(gaia_id, signin::internal::kChromeLastSignoutTime,
                         last_signout_time);
}

std::optional<base::Time> SigninPrefs::GetChromeLastSignoutTime(
    const GaiaId& gaia_id) const {
  return accessor_.GetTimePref(gaia_id,
                                signin::internal::kChromeLastSignoutTime);
}

void SigninPrefs::SetChromeSigninInterceptionLastBubbleDeclineTime(
    const GaiaId& gaia_id,
    base::Time last_decline_time) {
  accessor_.SetTimePref(
      gaia_id, signin::internal::kChromeSigninInterceptionLastBubbleDeclineTime,
      last_decline_time);
}

void SigninPrefs::ClearChromeSigninInterceptionLastBubbleDeclineTime(
    const GaiaId& gaia_id) {
  accessor_.ClearPref(
      gaia_id,
      signin::internal::kChromeSigninInterceptionLastBubbleDeclineTime);
}

std::optional<base::Time>
SigninPrefs::GetChromeSigninInterceptionLastBubbleDeclineTime(
    const GaiaId& gaia_id) const {
  return accessor_.GetTimePref(
      gaia_id,
      signin::internal::kChromeSigninInterceptionLastBubbleDeclineTime);
}

int SigninPrefs::IncrementChromeSigninBubbleRepromptCount(
    const GaiaId& gaia_id) {
  return accessor_.IncrementIntPref(
      gaia_id, signin::internal::kChromeSigninInterceptionRepromptCount);
}

int SigninPrefs::GetChromeSigninBubbleRepromptCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kChromeSigninInterceptionRepromptCount);
}

void SigninPrefs::ClearChromeSigninBubbleRepromptCount(const GaiaId& gaia_id) {
  accessor_.ClearPref(
      gaia_id, signin::internal::kChromeSigninInterceptionRepromptCount);
}

int SigninPrefs::IncrementChromeSigninInterceptionDismissCount(
    const GaiaId& gaia_id) {
  return accessor_.IncrementIntPref(
      gaia_id, signin::internal::kChromeSigninInterceptionDismissCount);
}

int SigninPrefs::GetChromeSigninInterceptionDismissCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kChromeSigninInterceptionDismissCount);
}

void SigninPrefs::IncrementPasswordSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(gaia_id,
                             signin::internal::kPasswordSignInPromoShownCount);
}

int SigninPrefs::GetPasswordSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(gaia_id,
                              signin::internal::kPasswordSignInPromoShownCount);
}

void SigninPrefs::IncrementAddressSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(gaia_id,
                             signin::internal::kAddressSignInPromoShownCount);
}

int SigninPrefs::GetAddressSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(gaia_id,
                              signin::internal::kAddressSignInPromoShownCount);
}

void SigninPrefs::IncrementBookmarkSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(gaia_id,
                             signin::internal::kBookmarkSignInPromoShownCount);
}

int SigninPrefs::GetBookmarkSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(gaia_id,
                              signin::internal::kBookmarkSignInPromoShownCount);
}

void SigninPrefs::IncrementBookmarkManagerSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id, signin::internal::kBookmarkManagerSignInPromoShownCount);
}

int SigninPrefs::GetBookmarkManagerSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kBookmarkManagerSignInPromoShownCount);
}

void SigninPrefs::IncrementBookmarkManagerSigninPromoDismissCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id, signin::internal::kBookmarkManagerSignInPromoDismissCount);
}

int SigninPrefs::GetBookmarkManagerSigninPromoDismissCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kBookmarkManagerSignInPromoDismissCount);
}

void SigninPrefs::IncrementSearchAIModeSigninPromoImpressionCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id, signin::internal::kSearchAIModeSignInPromoShownCount);
}

int SigninPrefs::GetSearchAIModeSigninPromoImpressionCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kSearchAIModeSignInPromoShownCount);
}

void SigninPrefs::IncrementAutofillSigninPromoDismissCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id, signin::internal::kAutofillSignInPromoDismissCount);
}

int SigninPrefs::GetAutofillSigninPromoDismissCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kAutofillSignInPromoDismissCount);
}

void SigninPrefs::IncrementSearchAIModeSigninPromoDismissCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id, signin::internal::kSearchAIModeSignInPromoDismissCount);
}

int SigninPrefs::GetSearchAIModeSigninPromoDismissCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kSearchAIModeSignInPromoDismissCount);
}

void SigninPrefs::SetExtensionsExplicitBrowserSignin(const GaiaId& gaia_id,
                                                     bool enabled) {
  accessor_.SetBooleanPref(
      gaia_id, signin::internal::kExtensionsExplicitBrowserSigninEnabled,
      enabled);
}

bool SigninPrefs::GetExtensionsExplicitBrowserSignin(
    const GaiaId& gaia_id) const {
  return accessor_.GetBooleanPref(
      gaia_id, signin::internal::kExtensionsExplicitBrowserSigninEnabled);
}

void SigninPrefs::SetBookmarksExplicitBrowserSignin(const GaiaId& gaia_id,
                                                    bool enabled) {
  accessor_.SetBooleanPref(
      gaia_id, signin::internal::kBookmarksExplicitBrowserSigninEnabled,
      enabled);
}

bool SigninPrefs::GetBookmarksExplicitBrowserSignin(
    const GaiaId& gaia_id) const {
  return accessor_.GetBooleanPref(
      gaia_id, signin::internal::kBookmarksExplicitBrowserSigninEnabled);
}

void SigninPrefs::SetPolicyDisclaimerLastRegistrationFailureTime(
    const GaiaId& gaia_id,
    base::Time last_registration_failure_time) {
  accessor_.SetTimePref(
      gaia_id, signin::internal::kPolicyDisclaimerLastRegistrationFailureTime,
      last_registration_failure_time);
}

void SigninPrefs::ClearPolicyDisclaimerLastRegistrationFailureTime(
    const GaiaId& gaia_id) {
  accessor_.ClearPref(
      gaia_id, signin::internal::kPolicyDisclaimerLastRegistrationFailureTime);
}

void SigninPrefs::SetSearchAIModeSigninPromoLastImpressionTime(
    const GaiaId& gaia_id,
    base::Time last_impression_time) {
  accessor_.SetTimePref(
      gaia_id, signin::internal::kSearchAIModeSignInPromoLastImpressionTime,
      last_impression_time);
}

std::optional<base::Time>
SigninPrefs::GetSearchAIModeSigninPromoLastImpressionTime(
    const GaiaId& gaia_id) const {
  return accessor_.GetTimePref(
      gaia_id, signin::internal::kSearchAIModeSignInPromoLastImpressionTime);
}

std::optional<base::Time>
SigninPrefs::GetPolicyDisclaimerLastRegistrationFailureTime(
    const GaiaId& gaia_id) const {
  return accessor_.GetTimePref(
      gaia_id, signin::internal::kPolicyDisclaimerLastRegistrationFailureTime);
}

int SigninPrefs::GetHistoryPageHistorySyncPromoShownCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kHistoryPageHistorySyncPromoShownCount);
}

void SigninPrefs::IncrementHistoryPageHistorySyncPromoShownCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id, signin::internal::kHistoryPageHistorySyncPromoShownCount);
}

std::optional<base::Time>
SigninPrefs::GetHistoryPageHistorySyncPromoLastDismissedTimestamp(
    const GaiaId& gaia_id) const {
  return accessor_.GetTimePref(
      gaia_id,
      signin::internal::kHistoryPageHistorySyncPromoLastDismissedTimestamp);
}

void SigninPrefs::SetHistoryPageHistorySyncPromoLastDismissedTimestamp(
    const GaiaId& gaia_id,
    base::Time last_dismissed_timestamp) {
  accessor_.SetTimePref(
      gaia_id,
      signin::internal::kHistoryPageHistorySyncPromoLastDismissedTimestamp,
      last_dismissed_timestamp);
}

bool SigninPrefs::GetHistoryPageHistorySyncPromoShownAfterDismissal(
    const GaiaId& gaia_id) const {
  return accessor_.GetBooleanPref(
      gaia_id,
      signin::internal::kHistoryPageHistorySyncPromoShownAfterDismissal);
}

void SigninPrefs::SetHistoryPageHistorySyncPromoShownAfterDismissal(
    const GaiaId& gaia_id) {
  accessor_.SetBooleanPref(
      gaia_id,
      signin::internal::kHistoryPageHistorySyncPromoShownAfterDismissal, true);
}

void SigninPrefs::IncrementBookmarkBatchUploadPromoDismissCountWithLastTime(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id, signin::internal::kBookmarkBatchUploadPromoDismissCount);
  accessor_.SetTimePref(
      gaia_id, signin::internal::kBookmarkBatchUploadPromoLastDismissTime,
      base::Time::Now());
}

std::pair<int, std::optional<base::Time>>
SigninPrefs::GetBookmarkBatchUploadPromoDismissCountWithLastTime(
    const GaiaId& gaia_id) {
  return {
      accessor_.GetIntPref(
          gaia_id, signin::internal::kBookmarkBatchUploadPromoDismissCount),
      accessor_.GetTimePref(
          gaia_id, signin::internal::kBookmarkBatchUploadPromoLastDismissTime)};
}

void SigninPrefs::SetBatchUploadLastUploadRemainingLocalDataCount(
    const GaiaId& gaia_id,
    int count) {
  accessor_.SetIntPref(
      gaia_id, signin::internal::kBatchUploadLastUploadRemainingLocalDataCount,
      count);
}

std::optional<int> SigninPrefs::GetBatchUploadLastUploadRemainingLocalDataCount(
    const GaiaId& gaia_id) const {
  return accessor_.MaybeGetIntPref(
      gaia_id, signin::internal::kBatchUploadLastUploadRemainingLocalDataCount);
}

void SigninPrefs::IncrementAvatarButtonHistorySyncPromoShownCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id, signin::internal::kAvatarButtonHistorySyncPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonHistorySyncPromoShownCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kAvatarButtonHistorySyncPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::IncrementAvatarButtonHistorySyncPromoUsedCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id, signin::internal::kAvatarButtonHistorySyncPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonHistorySyncPromoUsedCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kAvatarButtonHistorySyncPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::IncrementAvatarButtonBatchUploadPromoShownCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id, signin::internal::kAvatarButtonBatchUploadPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonBatchUploadPromoShownCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kAvatarButtonBatchUploadPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::IncrementAvatarButtonBatchUploadPromoUsedCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id, signin::internal::kAvatarButtonBatchUploadPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonBatchUploadPromoUsedCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kAvatarButtonBatchUploadPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::IncrementAvatarButtonBatchUploadBookmarkPromoShownCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id,
      signin::internal::kAvatarButtonBatchUploadBookmarkPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonBatchUploadBookmarkPromoShownCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id,
      signin::internal::kAvatarButtonBatchUploadBookmarkPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::IncrementAvatarButtonBatchUploadBookmarkPromoUsedCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id, signin::internal::kAvatarButtonBatchUploadBookmarkPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonBatchUploadBookmarkPromoUsedCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kAvatarButtonBatchUploadBookmarkPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::
    IncrementAvatarButtonBatchUploadWindows10DepreciationPromoShownCount(
        const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id,
      signin::internal::
          kAvatarButtonBatchUploadWindows10DepreciationPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonBatchUploadWindows10DepreciationPromoShownCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id,
      signin::internal::
          kAvatarButtonBatchUploadWindows10DepreciationPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::
    IncrementAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount(
        const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id,
      signin::internal::
          kAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id,
      signin::internal::
          kAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::IncrementAvatarButtonSigninPromoShownCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id, signin::internal::kAvatarButtonSigninPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonSigninPromoShownCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kAvatarButtonSigninPromoShownCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::IncrementAvatarButtonSigninPromoUsedCount(
    const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(
      gaia_id, signin::internal::kAvatarButtonSigninPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

int SigninPrefs::GetAvatarButtonSigninPromoUsedCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kAvatarButtonSigninPromoUsedCount,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::SetAvatarButtonSigninPromoLastShownTime(
    const GaiaId& gaia_id,
    base::Time last_shown_time) {
  accessor_.SetTimePref(
      gaia_id, signin::internal::kAvatarButtonSigninPromoLastShownTime,
      last_shown_time, signin::internal::kAvatarButtonPromoParents);
}

std::optional<base::Time> SigninPrefs::GetAvatarButtonSigninPromoLastShownTime(
    const GaiaId& gaia_id) const {
  return accessor_.GetTimePref(
      gaia_id, signin::internal::kAvatarButtonSigninPromoLastShownTime,
      signin::internal::kAvatarButtonPromoParents);
}

void SigninPrefs::SetCrossDeviceHistoryPromoShownCount(const GaiaId& gaia_id,
                                                       int count) {
  accessor_.SetIntPref(gaia_id,
                        signin::internal::kCrossDevicePromoShownCountKey, count,
                        signin::internal::kCrossDeviceHistoryPromoParents);
}

int SigninPrefs::GetCrossDeviceHistoryPromoShownCount(
    const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(
      gaia_id, signin::internal::kCrossDevicePromoShownCountKey,
      signin::internal::kCrossDeviceHistoryPromoParents);
}

void SigninPrefs::SetCrossDeviceHistoryPromoShownAfterDismissal(
    const GaiaId& gaia_id,
    bool shown_after_dismissal) {
  accessor_.SetBooleanPref(
      gaia_id, signin::internal::kCrossDevicePromoShownAfterDismissalKey,
      shown_after_dismissal, signin::internal::kCrossDeviceHistoryPromoParents);
}

bool SigninPrefs::GetCrossDeviceHistoryPromoShownAfterDismissal(
    const GaiaId& gaia_id) const {
  return accessor_.GetBooleanPref(
      gaia_id, signin::internal::kCrossDevicePromoShownAfterDismissalKey,
      signin::internal::kCrossDeviceHistoryPromoParents);
}

void SigninPrefs::SetCrossDeviceHistoryPromoLastDismissedTime(
    const GaiaId& gaia_id,
    base::Time last_dismissed_time) {
  accessor_.SetTimePref(
      gaia_id, signin::internal::kCrossDevicePromoLastDismissedTimeKey,
      last_dismissed_time, signin::internal::kCrossDeviceHistoryPromoParents);
}

void SigninPrefs::ClearCrossDeviceHistoryPromoLastDismissedTime(
    const GaiaId& gaia_id) {
  accessor_.ClearPref(gaia_id,
                       signin::internal::kCrossDevicePromoLastDismissedTimeKey,
                       signin::internal::kCrossDeviceHistoryPromoParents);
}

std::optional<base::Time>
SigninPrefs::GetCrossDeviceHistoryPromoLastDismissedTime(
    const GaiaId& gaia_id) const {
  return accessor_.GetTimePref(
      gaia_id, signin::internal::kCrossDevicePromoLastDismissedTimeKey,
      signin::internal::kCrossDeviceHistoryPromoParents);
}

void SigninPrefs::SetDeprecatedPrefForTesting(const GaiaId& gaia_id) {
  accessor_.SetDeprecatedPrefForTesting(gaia_id);  // IN-TEST
}

std::optional<int> SigninPrefs::GetDeprecatedPrefForTesting(
    const GaiaId& gaia_id) {
  return accessor_.GetDeprecatedPrefForTesting(gaia_id);  // IN-TEST
}
