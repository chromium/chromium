// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"

#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_education/common/feature_promo_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
// Promo data will be saved as a dictionary in the PrefService of a profile.

// PrefService path. The "snooze" bit is a legacy implementation detail.
const char kIPHPromoDataPath[] = "in_product_help.snoozed_feature";

// Path to the boolean indicates if an IPH was dismissed.
// in_product_help.snoozed_feature.[iph_name].is_dismissed
constexpr char kIPHIsDismissedPath[] = "is_dismissed";
// Path to the enum that indicates how an IPH was dismissed.
// in_product_help.snoozed_feature.[iph_name].last_dismissed_by
constexpr char kIPHLastDismissedByPath[] = "last_dismissed_by";
// Path to the timestamp an IPH was first shown.
// in_product_help.snoozed_feature.[iph_name].first_show_time
constexpr char kIPHFirstShowTimePath[] = "first_show_time";
// Path to the timestamp an IPH was last shown.
// in_product_help.snoozed_feature.[iph_name].last_show_time
constexpr char kIPHLastShowTimePath[] = "last_show_time";
// Path to the timestamp an IPH was last snoozed.
// in_product_help.snoozed_feature.[iph_name].last_snooze_time
constexpr char kIPHLastSnoozeTimePath[] = "last_snooze_time";
// Path to the count of how many times this IPH has been snoozed.
// in_product_help.snoozed_feature.[iph_name].snooze_count
constexpr char kIPHSnoozeCountPath[] = "snooze_count";
// Path to the count of how many times this IPH has been shown.
// in_product_help.snoozed_feature.[iph_name].show_count
constexpr char kIPHShowCountPath[] = "show_count";
// Path to a list of app IDs that the IPH was shown for; applies to app-specific
// IPH only.
constexpr char kIPHShownForAppsPath[] = "shown_for_apps";

// Path to the most recent session start time.
constexpr char kIPHSessionStartPath[] = "in_product_help.session_start_time";
// Path to the most recent active time.
constexpr char kIPHSessionLastActiveTimePath[] =
    "in_product_help.session_last_active_time";

// Path to the time of the most recent heavyweight promo.
constexpr char kIPHPolicyLastHeavyweightPromoPath[] =
    "in_product_help.policy_last_heavyweight_promo_time";

}  // namespace

BrowserFeaturePromoStorageService::BrowserFeaturePromoStorageService(
    Profile* profile)
    : profile_(profile) {}
BrowserFeaturePromoStorageService::~BrowserFeaturePromoStorageService() =
    default;

// static
void BrowserFeaturePromoStorageService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kIPHPromoDataPath);
  registry->RegisterTimePref(kIPHSessionStartPath, base::Time());
  registry->RegisterTimePref(kIPHSessionLastActiveTimePath, base::Time());
  registry->RegisterTimePref(kIPHPolicyLastHeavyweightPromoPath, base::Time());
}

void BrowserFeaturePromoStorageService::Reset(
    const base::Feature& iph_feature) {
  ScopedDictPrefUpdate update(profile_->GetPrefs(), kIPHPromoDataPath);
  update->RemoveByDottedPath(iph_feature.name);
}

absl::optional<user_education::FeaturePromoData>
BrowserFeaturePromoStorageService::ReadPromoData(
    const base::Feature& iph_feature) const {
  const std::string path_prefix = std::string(iph_feature.name) + ".";

  const auto& pref_data = profile_->GetPrefs()->GetDict(kIPHPromoDataPath);
  absl::optional<bool> is_dismissed =
      pref_data.FindBoolByDottedPath(path_prefix + kIPHIsDismissedPath);
  absl::optional<int> last_dismissed_by =
      pref_data.FindIntByDottedPath(path_prefix + kIPHLastDismissedByPath);
  absl::optional<base::Time> first_show_time = base::ValueToTime(
      pref_data.FindByDottedPath(path_prefix + kIPHFirstShowTimePath));
  absl::optional<base::Time> last_show_time = base::ValueToTime(
      pref_data.FindByDottedPath(path_prefix + kIPHLastShowTimePath));
  absl::optional<base::Time> snooze_time = base::ValueToTime(
      pref_data.FindByDottedPath(path_prefix + kIPHLastSnoozeTimePath));
  absl::optional<int> snooze_count =
      pref_data.FindIntByDottedPath(path_prefix + kIPHSnoozeCountPath);
  absl::optional<int> show_count =
      pref_data.FindIntByDottedPath(path_prefix + kIPHShowCountPath);
  const base::Value::List* app_list =
      pref_data.FindListByDottedPath(path_prefix + kIPHShownForAppsPath);

  absl::optional<user_education::FeaturePromoData> promo_data;

  if (!is_dismissed || !snooze_time || !snooze_count) {
    // IPH data is corrupt. Ignore the previous data.
    return promo_data;
  }

  if (!last_show_time || !show_count) {
    // This data was stored by a previous version. Assume previous IPH were
    // snoozed.
    last_show_time = *snooze_time - base::Seconds(1);
    show_count = *snooze_count;
  }

  if (!first_show_time) {
    // This data was stored by a previous version. First show time inherits
    // last show time, or null.
    first_show_time = show_count > 0 ? *last_show_time : base::Time();
  }

  promo_data = user_education::FeaturePromoData();
  promo_data->is_dismissed = *is_dismissed;
  promo_data->first_show_time = *first_show_time;
  promo_data->last_show_time = *last_show_time;
  promo_data->last_snooze_time = *snooze_time;
  promo_data->snooze_count = *snooze_count;
  promo_data->show_count = *show_count;

  // Since `last_dismissed_by` was not previously recorded, default to
  // "canceled" if the data isn't present or is invalid.
  if (!last_dismissed_by || *last_dismissed_by < 0 ||
      *last_dismissed_by >
          static_cast<int>(
              user_education::FeaturePromoClosedReason::kMaxValue)) {
    promo_data->last_dismissed_by =
        user_education::FeaturePromoClosedReason::kCancel;
  } else if (last_dismissed_by) {
    promo_data->last_dismissed_by =
        static_cast<user_education::FeaturePromoClosedReason>(
            *last_dismissed_by);
  }

  if (app_list) {
    for (auto& app : *app_list) {
      if (auto* const app_id = app.GetIfString()) {
        promo_data->shown_for_apps.emplace(*app_id);
      }
    }
  }

  return promo_data;
}

void BrowserFeaturePromoStorageService::SavePromoData(
    const base::Feature& iph_feature,
    const user_education::FeaturePromoData& promo_data) {
  std::string path_prefix = std::string(iph_feature.name) + ".";

  ScopedDictPrefUpdate update(profile_->GetPrefs(), kIPHPromoDataPath);
  auto& pref_data = update.Get();

  pref_data.SetByDottedPath(path_prefix + kIPHIsDismissedPath,
                            promo_data.is_dismissed);
  pref_data.SetByDottedPath(path_prefix + kIPHLastDismissedByPath,
                            static_cast<int>(promo_data.last_dismissed_by));
  pref_data.SetByDottedPath(path_prefix + kIPHFirstShowTimePath,
                            base::TimeToValue(promo_data.first_show_time));
  pref_data.SetByDottedPath(path_prefix + kIPHLastShowTimePath,
                            base::TimeToValue(promo_data.last_show_time));
  pref_data.SetByDottedPath(path_prefix + kIPHLastSnoozeTimePath,
                            base::TimeToValue(promo_data.last_snooze_time));
  pref_data.SetByDottedPath(path_prefix + kIPHSnoozeCountPath,
                            promo_data.snooze_count);
  pref_data.SetByDottedPath(path_prefix + kIPHShowCountPath,
                            promo_data.show_count);

  base::Value::List shown_for_apps;
  for (auto& app_id : promo_data.shown_for_apps) {
    shown_for_apps.Append(app_id);
  }
  pref_data.SetByDottedPath(path_prefix + kIPHShownForAppsPath,
                            std::move(shown_for_apps));
}

void BrowserFeaturePromoStorageService::ResetSession() {
  auto* const prefs = profile_->GetPrefs();
  prefs->ClearPref(kIPHSessionStartPath);
  prefs->ClearPref(kIPHSessionLastActiveTimePath);
}

user_education::FeaturePromoSessionData
BrowserFeaturePromoStorageService::ReadSessionData() const {
  user_education::FeaturePromoSessionData data;
  auto* const prefs = profile_->GetPrefs();
  data.start_time = prefs->GetTime(kIPHSessionStartPath);
  data.most_recent_active_time = prefs->GetTime(kIPHSessionLastActiveTimePath);
  return data;
}

void BrowserFeaturePromoStorageService::SaveSessionData(
    const user_education::FeaturePromoSessionData& session_data) {
  auto* const prefs = profile_->GetPrefs();
  prefs->SetTime(kIPHSessionStartPath, session_data.start_time);
  prefs->SetTime(kIPHSessionLastActiveTimePath,
                 session_data.most_recent_active_time);
}

user_education::FeaturePromoPolicyData
BrowserFeaturePromoStorageService::ReadPolicyData() const {
  user_education::FeaturePromoPolicyData data;
  data.last_heavyweight_promo_time =
      profile_->GetPrefs()->GetTime(kIPHPolicyLastHeavyweightPromoPath);
  return data;
}

void BrowserFeaturePromoStorageService::SavePolicyData(
    const user_education::FeaturePromoPolicyData& policy_data) {
  profile_->GetPrefs()->SetTime(kIPHPolicyLastHeavyweightPromoPath,
                                policy_data.last_heavyweight_promo_time);
}

void BrowserFeaturePromoStorageService::ResetPolicy() {
  profile_->GetPrefs()->ClearPref(kIPHPolicyLastHeavyweightPromoPath);
}
