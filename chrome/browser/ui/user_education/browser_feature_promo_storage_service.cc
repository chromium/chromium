// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/browser_feature_promo_storage_service.h"

#include <ostream>

#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_education/common/feature_promo_storage_service.h"
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
// Path to the timestamp an IPH was last shown.
// in_product_help.snoozed_feature.[iph_name].last_show_time
constexpr char kIPHLastShowTimePath[] = "last_show_time";
// Path to the timestamp an IPH was last snoozed.
// in_product_help.snoozed_feature.[iph_name].last_snooze_time
constexpr char kIPHLastSnoozeTimePath[] = "last_snooze_time";
// Path to the duration of snooze.
// in_product_help.snoozed_feature.[iph_name].last_snooze_duration
constexpr char kIPHLastSnoozeDurationPath[] = "last_snooze_duration";
// Path to the count of how many times this IPH has been snoozed.
// in_product_help.snoozed_feature.[iph_name].snooze_count
constexpr char kIPHSnoozeCountPath[] = "snooze_count";
// Path to the count of how many times this IPH has been shown.
// in_product_help.snoozed_feature.[iph_name].show_count
constexpr char kIPHShowCountPath[] = "show_count";
// Path to a list of app IDs that the IPH was shown for; applies to app-specific
// IPH only.
constexpr char kIPHShownForAppsPath[] = "shown_for_apps";

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
}

void BrowserFeaturePromoStorageService::Reset(
    const base::Feature& iph_feature) {
  ScopedDictPrefUpdate update(profile_->GetPrefs(), kIPHPromoDataPath);
  update->RemoveByDottedPath(iph_feature.name);
}

absl::optional<user_education::FeaturePromoStorageService::PromoData>
BrowserFeaturePromoStorageService::ReadPromoData(
    const base::Feature& iph_feature) const {
  const std::string path_prefix = std::string(iph_feature.name) + ".";

  const auto& pref_data = profile_->GetPrefs()->GetDict(kIPHPromoDataPath);
  absl::optional<bool> is_dismissed =
      pref_data.FindBoolByDottedPath(path_prefix + kIPHIsDismissedPath);
  absl::optional<int> last_dismissed_by =
      pref_data.FindIntByDottedPath(path_prefix + kIPHLastDismissedByPath);
  absl::optional<base::Time> show_time = base::ValueToTime(
      pref_data.FindByDottedPath(path_prefix + kIPHLastShowTimePath));
  absl::optional<base::Time> snooze_time = base::ValueToTime(
      pref_data.FindByDottedPath(path_prefix + kIPHLastSnoozeTimePath));
  absl::optional<base::TimeDelta> snooze_duration = base::ValueToTimeDelta(
      pref_data.FindByDottedPath(path_prefix + kIPHLastSnoozeDurationPath));
  absl::optional<int> snooze_count =
      pref_data.FindIntByDottedPath(path_prefix + kIPHSnoozeCountPath);
  absl::optional<int> show_count =
      pref_data.FindIntByDottedPath(path_prefix + kIPHShowCountPath);
  const base::Value::List* app_list =
      pref_data.FindListByDottedPath(path_prefix + kIPHShownForAppsPath);

  absl::optional<PromoData> promo_data;

  if (!is_dismissed || !snooze_time || !snooze_count || !snooze_duration) {
    // IPH data is corrupt. Ignore the previous data.
    return promo_data;
  }

  if (!show_time || !show_count) {
    // This data was stored by a previous version. Assume previous IPH were
    // snoozed.
    show_time = *snooze_time - base::Seconds(1);
    show_count = *snooze_count;
  }

  promo_data = PromoData();
  promo_data->is_dismissed = *is_dismissed;
  promo_data->last_show_time = *show_time;
  promo_data->last_snooze_time = *snooze_time;
  promo_data->last_snooze_duration = *snooze_duration;
  promo_data->snooze_count = *snooze_count;
  promo_data->show_count = *show_count;

  // Since `last_dismissed_by` was not previously recorded, default to
  // "canceled" if the data isn't present or is invalid.
  if (!last_dismissed_by || *last_dismissed_by < 0 ||
      *last_dismissed_by > CloseReason::kMaxValue) {
    promo_data->last_dismissed_by = CloseReason::kCancel;
  } else if (last_dismissed_by) {
    promo_data->last_dismissed_by =
        static_cast<CloseReason>(*last_dismissed_by);
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
    const user_education::FeaturePromoStorageService::PromoData& promo_data) {
  std::string path_prefix = std::string(iph_feature.name) + ".";

  ScopedDictPrefUpdate update(profile_->GetPrefs(), kIPHPromoDataPath);
  auto& pref_data = update.Get();

  pref_data.SetByDottedPath(path_prefix + kIPHIsDismissedPath,
                            promo_data.is_dismissed);
  pref_data.SetByDottedPath(path_prefix + kIPHLastDismissedByPath,
                            static_cast<int>(promo_data.last_dismissed_by));
  pref_data.SetByDottedPath(path_prefix + kIPHLastShowTimePath,
                            base::TimeToValue(promo_data.last_show_time));
  pref_data.SetByDottedPath(path_prefix + kIPHLastSnoozeTimePath,
                            base::TimeToValue(promo_data.last_snooze_time));
  pref_data.SetByDottedPath(
      path_prefix + kIPHLastSnoozeDurationPath,
      base::TimeDeltaToValue(promo_data.last_snooze_duration));
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
