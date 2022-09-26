// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/browser_feature_promo_snooze_service.h"

#include <ostream>

#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
// Snooze data will be saved as a dictionary in the PrefService of a profile.

// PrefService path.
const char kIPHSnoozeDataPath[] = "in_product_help.snoozed_feature";

// Path to the boolean indicates if an IPH was dismissed.
// in_product_help.snoozed_feature.[iph_name].is_dismissed
constexpr char kIPHIsDismissedPath[] = "is_dismissed";
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

}  // namespace

BrowserFeaturePromoSnoozeService::BrowserFeaturePromoSnoozeService(
    Profile* profile)
    : profile_(profile) {}
BrowserFeaturePromoSnoozeService::~BrowserFeaturePromoSnoozeService() = default;

// static
void BrowserFeaturePromoSnoozeService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kIPHSnoozeDataPath);
}

void BrowserFeaturePromoSnoozeService::Reset(const base::Feature& iph_feature) {
  ScopedDictPrefUpdate update(profile_->GetPrefs(), kIPHSnoozeDataPath);
  update->RemoveByDottedPath(iph_feature.name);
}

absl::optional<user_education::FeaturePromoSnoozeService::SnoozeData>
BrowserFeaturePromoSnoozeService::ReadSnoozeData(
    const base::Feature& iph_feature) {
  std::string path_prefix = std::string(iph_feature.name) + ".";

  const auto& pref_data = profile_->GetPrefs()->GetDict(kIPHSnoozeDataPath);
  absl::optional<bool> is_dismissed =
      pref_data.FindBoolByDottedPath(path_prefix + kIPHIsDismissedPath);
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

  absl::optional<SnoozeData> snooze_data;

  if (!is_dismissed)
    return snooze_data;

  if (!snooze_time || !snooze_count || !snooze_duration) {
    // IPH snooze data is corrupt. Clear data of this feature.
    Reset(iph_feature);
    return snooze_data;
  }

  if (!show_time || !show_count) {
    // This feature was shipped before without handling
    // non-clickers. Assume previous displays were all snooozed.
    show_time = *snooze_time - base::Seconds(1);
    show_count = *snooze_count;
  }

  snooze_data = SnoozeData();
  snooze_data->is_dismissed = *is_dismissed;
  snooze_data->last_show_time = *show_time;
  snooze_data->last_snooze_time = *snooze_time;
  snooze_data->last_snooze_duration = *snooze_duration;
  snooze_data->snooze_count = *snooze_count;
  snooze_data->show_count = *show_count;

  return snooze_data;
}

void BrowserFeaturePromoSnoozeService::SaveSnoozeData(
    const base::Feature& iph_feature,
    const user_education::FeaturePromoSnoozeService::SnoozeData& snooze_data) {
  std::string path_prefix = std::string(iph_feature.name) + ".";

  ScopedDictPrefUpdate update(profile_->GetPrefs(), kIPHSnoozeDataPath);
  auto& pref_data = update.Get();

  pref_data.SetByDottedPath(path_prefix + kIPHIsDismissedPath,
                            snooze_data.is_dismissed);
  pref_data.SetByDottedPath(path_prefix + kIPHLastShowTimePath,
                            base::TimeToValue(snooze_data.last_show_time));
  pref_data.SetByDottedPath(path_prefix + kIPHLastSnoozeTimePath,
                            base::TimeToValue(snooze_data.last_snooze_time));
  pref_data.SetByDottedPath(
      path_prefix + kIPHLastSnoozeDurationPath,
      base::TimeDeltaToValue(snooze_data.last_snooze_duration));
  pref_data.SetByDottedPath(path_prefix + kIPHSnoozeCountPath,
                            snooze_data.snooze_count);
  pref_data.SetByDottedPath(path_prefix + kIPHShowCountPath,
                            snooze_data.show_count);
}
