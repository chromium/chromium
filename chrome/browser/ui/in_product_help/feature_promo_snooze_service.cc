// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/in_product_help/feature_promo_snooze_service.h"

#include <ostream>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {
// Snooze data will be saved as a dictionary in the PrefService of a profile.

// PrefService path.
const char kIPHSnoozeDataPath[] = "in_product_help.snoozed_feature";

// Path to the boolean indicates if an IPH is dismissed.
// in_product_help.snoozed_feature.[iph_name].is_dismissed
constexpr char kIPHIsFeatureDismissed[] = "is_dismissed";
// Path to the timestamp an IPH is snoozed.
// in_product_help.snoozed_feature.[iph_name].last_snooze_time
constexpr char kIPHSnoozedFeatureTime[] = "last_snooze_time";
// Path to the duration of snooze.
// in_product_help.snoozed_feature.[iph_name].last_snooze_duration
constexpr char kIPHSnoozedFeatureDuration[] = "last_snooze_duration";
// Path to the count of how many times this IPH has been snoozed.
// in_product_help.snoozed_feature.[iph_name].snooze_count
constexpr char kIPHSnoozedFeatureCount[] = "snooze_count";

// Finch parameter to control the snooze duration.
// If this parameter is not specified or is zero, the default duration at the
// client side will be used.
constexpr base::FeatureParam<base::TimeDelta> kOverriddenDuration{
    &feature_engagement::kIPHDesktopSnoozeFeature,
    "x_iph_snooze_overridden_duration ", base::TimeDelta::FromHours(0)};

// Used in UMA histogram to track if the user snoozes for once or more.
enum class SnoozeType {
  // The user snoozes the IPH for the first time.
  kFirstTimeSnooze = 0,
  // The user snoozes the IPH for the second time or more.
  kRepeatingSnooze = 1,
  kMaxValue = kRepeatingSnooze
};
}  // namespace

const int FeaturePromoSnoozeService::kUmaMaxSnoozeCount = 10;

FeaturePromoSnoozeService::FeaturePromoSnoozeService(Profile* profile)
    : profile_(profile) {}

void FeaturePromoSnoozeService::OnUserSnooze(const base::Feature& iph_feature,
                                             base::TimeDelta snooze_duration) {
  DCHECK(snooze_duration > base::TimeDelta::FromSeconds(0));
  auto snooze_data = ReadSnoozeData(iph_feature);

  if (!snooze_data)
    snooze_data = SnoozeData();

  if (!kOverriddenDuration.Get().is_zero())
    snooze_duration = kOverriddenDuration.Get();

  base::UmaHistogramEnumeration(
      "InProductHelp.Promos.Snooze." + std::string(iph_feature.name),
      snooze_data->snooze_count == 0 ? SnoozeType::kFirstTimeSnooze
                                     : SnoozeType::kRepeatingSnooze);

  snooze_data->last_snooze_time = base::Time::Now();
  snooze_data->last_snooze_duration = snooze_duration;
  snooze_data->snooze_count++;

  SaveSnoozeData(iph_feature, *snooze_data);
}

void FeaturePromoSnoozeService::OnUserDismiss(
    const base::Feature& iph_feature) {
  auto snooze_data = ReadSnoozeData(iph_feature);

  if (!snooze_data)
    snooze_data = SnoozeData();

  snooze_data->is_dismissed = true;

  SaveSnoozeData(iph_feature, *snooze_data);

  // Record count of previous snoozes when the IPH gets dismissed by "Got It"
  // button.
  base::UmaHistogramExactLinear(
      "InProductHelp.Promos.SnoozeCountAtAcknowledge." +
          std::string(iph_feature.name),
      snooze_data->snooze_count, kUmaMaxSnoozeCount);
}

bool FeaturePromoSnoozeService::IsBlocked(const base::Feature& iph_feature) {
  auto snooze_data = ReadSnoozeData(iph_feature);

  if (!snooze_data)
    return false;

  // This IPH has been dismissed by user permanently.
  if (snooze_data->is_dismissed)
    return true;

  // This IPH has neither been dismissed nor snoozed.
  if (snooze_data->snooze_count == 0)
    return false;

  // Corruption: Snooze time is in the future.
  if (snooze_data->last_snooze_time > base::Time::Now())
    return true;

  // This IPH is snoozed. Test if snooze period has expired.
  return base::Time::Now() <
         snooze_data->last_snooze_time + snooze_data->last_snooze_duration;
}

// static
void FeaturePromoSnoozeService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kIPHSnoozeDataPath);
}

void FeaturePromoSnoozeService::Reset(const base::Feature& iph_feature) {
  DictionaryPrefUpdate update(profile_->GetPrefs(), kIPHSnoozeDataPath);
  base::DictionaryValue* pref_data = update.Get();
  pref_data->RemovePath(iph_feature.name);
}

int FeaturePromoSnoozeService::GetSnoozeCount(
    const base::Feature& iph_feature) {
  base::Optional<SnoozeData> snooze_data = ReadSnoozeData(iph_feature);
  return snooze_data ? snooze_data->snooze_count : 0;
}

base::Optional<FeaturePromoSnoozeService::SnoozeData>
FeaturePromoSnoozeService::ReadSnoozeData(const base::Feature& iph_feature) {
  std::string path_prefix = std::string(iph_feature.name) + ".";

  const base::DictionaryValue* pref_data =
      profile_->GetPrefs()->GetDictionary(kIPHSnoozeDataPath);
  base::Optional<bool> is_dismissed =
      pref_data->FindBoolPath(path_prefix + kIPHIsFeatureDismissed);
  base::Optional<base::Time> snooze_time = util::ValueToTime(
      pref_data->FindPath(path_prefix + kIPHSnoozedFeatureTime));
  base::Optional<int> snooze_count =
      pref_data->FindIntPath(path_prefix + kIPHSnoozedFeatureCount);
  base::Optional<base::TimeDelta> snooze_duration = util::ValueToTimeDelta(
      pref_data->FindPath(path_prefix + kIPHSnoozedFeatureDuration));

  base::Optional<SnoozeData> snooze_data;

  if (!is_dismissed)
    return snooze_data;

  if (!snooze_time || !snooze_count || !snooze_duration) {
    // IPH snooze data is corrupt. Clear data of this feature.
    Reset(iph_feature);
    return snooze_data;
  }

  snooze_data = SnoozeData();
  snooze_data->is_dismissed = *is_dismissed;
  snooze_data->last_snooze_time = *snooze_time;
  snooze_data->last_snooze_duration = *snooze_duration;
  snooze_data->snooze_count = *snooze_count;

  return snooze_data;
}

void FeaturePromoSnoozeService::SaveSnoozeData(
    const base::Feature& iph_feature,
    const FeaturePromoSnoozeService::SnoozeData& snooze_data) {
  std::string path_prefix = std::string(iph_feature.name) + ".";

  DictionaryPrefUpdate update(profile_->GetPrefs(), kIPHSnoozeDataPath);
  base::DictionaryValue* pref_data = update.Get();

  pref_data->SetBoolPath(path_prefix + kIPHIsFeatureDismissed,
                         snooze_data.is_dismissed);
  pref_data->SetPath(path_prefix + kIPHSnoozedFeatureTime,
                     util::TimeToValue(snooze_data.last_snooze_time));
  pref_data->SetPath(path_prefix + kIPHSnoozedFeatureDuration,
                     util::TimeDeltaToValue(snooze_data.last_snooze_duration));
  pref_data->SetIntPath(path_prefix + kIPHSnoozedFeatureCount,
                        snooze_data.snooze_count);
}
