// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_prefs_utils.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

namespace {

const base::Value::Dict* GetWebAppDictionary(const PrefService* pref_service,
                                             const webapps::AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::Dict& web_apps_prefs =
      pref_service->GetDict(prefs::kWebAppsPreferences);

  return web_apps_prefs.FindDict(app_id);
}

base::Value::Dict& UpdateWebAppDictionary(
    ScopedDictPrefUpdate& web_apps_prefs_update,
    const webapps::AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return *web_apps_prefs_update->EnsureDict(app_id);
}

// Returns whether the time occurred within X days.
bool TimeOccurredWithinDaysInternal(absl::optional<base::Time> time, int days) {
  return time && (base::Time::Now() - time.value()).InDays() < days;
}

bool AreAllMLPromosBlocked(PrefService* pref_service) {
  const base::Value::Dict& dict =
      pref_service->GetDict(prefs::kWebAppsAppAgnosticMlState);
  return dict.contains(kAllMLPromosBlockedTime);
}

// Return true or false based on whether the kAllMLPromosBlockedDate should be
// deleted based on the difference between today's date and
// kTotalDaysToStoreMLGuardrails.
bool ShouldResetMLPromosBlockData(PrefService* pref_service) {
  // If all ML promotion has not been blocked yet, i.e. the date when it was
  // blocked was not set, then return early.
  if (!AreAllMLPromosBlocked(pref_service)) {
    return false;
  }

  const base::Value::Dict& dict =
      pref_service->GetDict(prefs::kWebAppsAppAgnosticMlState);
  const base::Value* value = dict.FindByDottedPath(kAllMLPromosBlockedTime);
  if (!value) {
    return false;
  }

  absl::optional<base::Time> last_time_ml_promo_blocked =
      base::ValueToTime(value);

  // We only want to clear the ML guardrails if
  // kMaxDaysForMLPromotionGuardrailStorage is crossed.
  return !TimeOccurredWithinDaysInternal(
      last_time_ml_promo_blocked,
      webapps::features::kMaxDaysForMLPromotionGuardrailStorage.Get());
}

}  // namespace

bool TimeOccurredWithinDays(absl::optional<base::Time> time, int days) {
  return TimeOccurredWithinDaysInternal(time, days);
}

absl::optional<int> GetIntWebAppPref(const PrefService* pref_service,
                                     const webapps::AppId& app_id,
                                     base::StringPiece path) {
  const base::Value::Dict* web_app_prefs =
      GetWebAppDictionary(pref_service, app_id);
  if (web_app_prefs) {
    return web_app_prefs->FindIntByDottedPath(path);
  }
  return absl::nullopt;
}

void UpdateIntWebAppPref(PrefService* pref_service,
                         const webapps::AppId& app_id,
                         base::StringPiece path,
                         int value) {
  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsPreferences);

  base::Value::Dict& web_app_prefs = UpdateWebAppDictionary(update, app_id);
  web_app_prefs.SetByDottedPath(path, value);
}

absl::optional<base::Time> GetTimeWebAppPref(const PrefService* pref_service,
                                             const webapps::AppId& app_id,
                                             base::StringPiece path) {
  if (const auto* web_app_prefs = GetWebAppDictionary(pref_service, app_id)) {
    if (auto* value = web_app_prefs->FindByDottedPath(path)) {
      return base::ValueToTime(value);
    }
  }

  return absl::nullopt;
}

void UpdateTimeWebAppPref(PrefService* pref_service,
                          const webapps::AppId& app_id,
                          base::StringPiece path,
                          base::Time value) {
  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsPreferences);

  auto& web_app_prefs = UpdateWebAppDictionary(update, app_id);
  web_app_prefs.SetByDottedPath(path, base::TimeToValue(value));
}

void RemoveWebAppPref(PrefService* pref_service,
                      const webapps::AppId& app_id,
                      base::StringPiece path) {
  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsPreferences);

  base::Value::Dict& web_app_prefs = UpdateWebAppDictionary(update, app_id);
  web_app_prefs.RemoveByDottedPath(path);
}

// The stored preferences look like:
//   "web_app_ids": {
//     "<app_id_1>": {
//       "was_external_app_uninstalled_by_user": true,
//       "IPH_num_of_consecutive_ignore": 2,
//       A string-flavored base::value representing the int64_t number of
//       microseconds since the Windows epoch, using base::TimeToValue().
//       "IPH_last_ignore_time": "13249617864945580",
//       A string-flavored base::value representing the int64_t number of
//       microseconds since the Windows epoch, using base::TimeToValue().
//       "ML_last_time_install_ignored": "13249617864945580",
//       A string-flavored base::value representing the int64_t number of
//       microseconds since the Windows epoch, using base::TimeToValue().
//       "ML_last_time_install_dismissed": "13249617864945580",
//       "ML_num_of_consecutive_not_accepted": 2,
//       "error_loaded_policy_app_migrated": true
//     },
//   },
//   "app_agnostic_ml_state": {
//       A string-flavored base::value representing the int64_t number of
//       microseconds since the Windows epoch, using base::TimeToValue().
//       "ML_last_time_install_ignored": "13249617864945580",
//       A string-flavored base::value representing the int64_t number of
//       microseconds since the Windows epoch, using base::TimeToValue().
//       "ML_last_time_install_dismissed": "13249617864945580",
//       "ML_num_of_consecutive_not_accepted": 2,
//       "ML_all_promos_blocked_date": "13249617864945580",
//   },
//   "app_agnostic_iph_state": {
//     "IPH_num_of_consecutive_ignore": 3,
//     A string-flavored base::Value representing int64_t number of microseconds
//     since the Windows epoch, using base::TimeToValue().
//     "IPH_last_ignore_time": "13249617864945500",
//   },
//   isolation_state is managed by isolation_prefs_utils
//   "isolation_state": {
//     "<origin>": {
//       "storage_isolation_key": "abc123",
//     },
//   }
//

void WebAppPrefsUtilsRegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(::prefs::kWebAppsPreferences);
  registry->RegisterDictionaryPref(::prefs::kWebAppsAppAgnosticIphState);
  registry->RegisterDictionaryPref(::prefs::kWebAppsAppAgnosticMlState);
  registry->RegisterBooleanPref(::prefs::kShouldGarbageCollectStoragePartitions,
                                false);
  registry->RegisterBooleanPref(
      ::prefs::kErrorLoadedPolicyAppMigrationCompleted, false);
}

const char kLastTimeMlInstallIgnored[] = "ML_last_time_install_ignored";
const char kLastTimeMlInstallDismissed[] = "ML_last_time_install_dismissed";
const char kConsecutiveMlInstallNotAcceptedCount[] =
    "ML_num_of_consecutive_not_accepted";
const char kAllMLPromosBlockedTime[] = "ML_all_promos_blocked_date";
const char kMLPromotionGuardrailBlockReason[] = "ML_guardrail_blocked";

const int kMuteMlInstallAfterConsecutiveAppSpecificNotAcceptedCount = 3;
const int kMuteMlInstallAfterIgnoreForDays = 2;
const int kMuteMlInstallAfterDismissForDays = 14;

const int kMuteMlInstallAfterConsecutiveAppAgnosticNotAcceptedCount = 5;
const int kMuteMlInstallAfterAnyIgnoreForDays = 1;
const int kMuteMlInstallAfterAnyDismissForDays = 7;

void RecordMlInstallIgnored(PrefService* pref_service,
                            const webapps::AppId& app_id,
                            base::Time time) {
  CHECK(pref_service);

  absl::optional<int> ignored_count = GetIntWebAppPref(
      pref_service, app_id, kConsecutiveMlInstallNotAcceptedCount);
  int new_count = base::saturated_cast<int>(1 + ignored_count.value_or(0));

  UpdateIntWebAppPref(pref_service, app_id,
                      kConsecutiveMlInstallNotAcceptedCount, new_count);
  UpdateTimeWebAppPref(pref_service, app_id, kLastTimeMlInstallIgnored, time);

  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsAppAgnosticMlState);
  int global_count =
      update->FindInt(kConsecutiveMlInstallNotAcceptedCount).value_or(0);
  update->Set(kConsecutiveMlInstallNotAcceptedCount,
              base::saturated_cast<int>(global_count + 1));
  update->Set(kLastTimeMlInstallIgnored, base::TimeToValue(time));
}

void RecordMlInstallDismissed(PrefService* pref_service,
                              const webapps::AppId& app_id,
                              base::Time time) {
  CHECK(pref_service);

  absl::optional<int> ignored_count = GetIntWebAppPref(
      pref_service, app_id, kConsecutiveMlInstallNotAcceptedCount);
  int new_count = base::saturated_cast<int>(1 + ignored_count.value_or(0));

  UpdateIntWebAppPref(pref_service, app_id,
                      kConsecutiveMlInstallNotAcceptedCount, new_count);
  UpdateTimeWebAppPref(pref_service, app_id, kLastTimeMlInstallDismissed, time);

  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsAppAgnosticMlState);
  int global_count =
      update->FindInt(kConsecutiveMlInstallNotAcceptedCount).value_or(0);
  update->Set(kConsecutiveMlInstallNotAcceptedCount,
              base::saturated_cast<int>(global_count + 1));
  update->Set(kLastTimeMlInstallDismissed, base::TimeToValue(time));
}

void RecordMlInstallAccepted(PrefService* pref_service,
                             const webapps::AppId& app_id,
                             base::Time time) {
  // The ignored count is meant to track consecutive occurrences of the user
  // ignoring ML install, to help determine when ML install should be muted.
  // Therefore resetting ignored count on successful install.
  UpdateIntWebAppPref(pref_service, app_id,
                      kConsecutiveMlInstallNotAcceptedCount, 0);

  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsAppAgnosticMlState);
  update->Set(kConsecutiveMlInstallNotAcceptedCount, 0);

  // If an ML install is accepted, remove the kAllMLPromosBlockedTime entry as a
  // "reset".
  update->Remove(kAllMLPromosBlockedTime);
}

bool IsMlPromotionBlockedByHistoryGuardrail(PrefService* pref_service,
                                            const webapps::AppId& app_id) {
  // Reset kAllMLPromosBlockedTime and consective ML install not accepted count
  // data if needed before going through guardrail logic.
  if (ShouldResetMLPromosBlockData(pref_service)) {
    ResetAllMLPromosBlockedDateAndGuardrails(pref_service, app_id);
  }

  // Do not show Ml install if the user ignored the last N+ promos for this app.
  int app_ignored_count =
      GetIntWebAppPref(pref_service, app_id,
                       kConsecutiveMlInstallNotAcceptedCount)
          .value_or(0);
  if (app_ignored_count >=
      kMuteMlInstallAfterConsecutiveAppSpecificNotAcceptedCount) {
    ScopedDictPrefUpdate update(pref_service,
                                prefs::kWebAppsAppAgnosticMlState);
    update->Set(kMLPromotionGuardrailBlockReason,
                "app_specific_not_accept_count_exceeded");
    return true;
  }
  // Do not show Ml install if the user ignored a promo for this app within N
  // days.
  auto app_last_ignore =
      GetTimeWebAppPref(pref_service, app_id, kLastTimeMlInstallIgnored);
  if (TimeOccurredWithinDays(app_last_ignore,
                             kMuteMlInstallAfterIgnoreForDays)) {
    ScopedDictPrefUpdate update(pref_service,
                                prefs::kWebAppsAppAgnosticMlState);
    update->Set(kMLPromotionGuardrailBlockReason,
                "app_specific_ml_install_ignore_days_hit");
    return true;
  }
  // Do not show Ml install if the user dismissed a promo for this app within N
  // days.
  auto app_last_dismissed =
      GetTimeWebAppPref(pref_service, app_id, kLastTimeMlInstallDismissed);
  if (TimeOccurredWithinDays(app_last_dismissed,
                             kMuteMlInstallAfterDismissForDays)) {
    ScopedDictPrefUpdate update(pref_service,
                                prefs::kWebAppsAppAgnosticMlState);
    update->Set(kMLPromotionGuardrailBlockReason,
                "app_specific_ml_install_dismiss_days_hit");
    return true;
  }

  const base::Value::Dict& dict =
      pref_service->GetDict(prefs::kWebAppsAppAgnosticMlState);

  // Do not show Ml install if the user ignored the last N+ promos for any app.
  // Also set the date for the ML promotion being permanently blocked so that it
  // can be tracked.
  int global_ignored_count =
      dict.FindInt(kConsecutiveMlInstallNotAcceptedCount).value_or(0);
  if (global_ignored_count >=
      kMuteMlInstallAfterConsecutiveAppAgnosticNotAcceptedCount) {
    ScopedDictPrefUpdate update(pref_service,
                                prefs::kWebAppsAppAgnosticMlState);

    // Only set the date that ML promos were blocked if the field does not
    // exist. This prevents a situation where the date gets set everytime this
    // guardrail condition is hit, leading to an user being permanently blocked
    // from ever seeing the install dialog triggered by ML.
    if (!AreAllMLPromosBlocked(pref_service)) {
      update->Set(kAllMLPromosBlockedTime,
                  base::TimeToValue(base::Time::Now()));
    }
    update->Set(kMLPromotionGuardrailBlockReason,
                "app_agnostic_not_accept_count_exceeded");
    return true;
  }

  // Do not show Ml install if the user ignored a promo for any app within N
  // days.
  auto global_last_ignore_time =
      base::ValueToTime(dict.Find(kLastTimeMlInstallIgnored));
  if (TimeOccurredWithinDays(global_last_ignore_time,
                             kMuteMlInstallAfterAnyIgnoreForDays)) {
    ScopedDictPrefUpdate update(pref_service,
                                prefs::kWebAppsAppAgnosticMlState);
    update->Set(kMLPromotionGuardrailBlockReason,
                "app_agnostic_ml_install_ignore_days_hit");
    return true;
  }
  // Do not show Ml install if the user dismissed a promo for any app within N
  // days.
  auto global_last_dismiss =
      base::ValueToTime(dict.Find(kLastTimeMlInstallDismissed));
  if (TimeOccurredWithinDays(global_last_dismiss,
                             kMuteMlInstallAfterAnyDismissForDays)) {
    ScopedDictPrefUpdate update(pref_service,
                                prefs::kWebAppsAppAgnosticMlState);
    update->Set(kMLPromotionGuardrailBlockReason,
                "app_agnostic_ml_install_dismiss_days_hit");
    return true;
  }
  return false;
}

// Resets kAllMLPromosBlockedDate and kConsecutiveMlInstallNotAcceptedCount on
// the app specific and app agnostic levels.
void ResetAllMLPromosBlockedDateAndGuardrails(PrefService* pref_service,
                                              const webapps::AppId& app_id) {
  // First reset the app agnostic data.
  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsAppAgnosticMlState);
  update->Remove(kAllMLPromosBlockedTime);
  update->Remove(kMLPromotionGuardrailBlockReason);
  update->Set(kConsecutiveMlInstallNotAcceptedCount, 0);

  // Next, reset the app specific data.
  UpdateIntWebAppPref(pref_service, app_id,
                      kConsecutiveMlInstallNotAcceptedCount, 0);
}

}  // namespace web_app
