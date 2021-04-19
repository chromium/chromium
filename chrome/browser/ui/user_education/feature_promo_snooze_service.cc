// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/feature_promo_snooze_service.h"

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

// Finch parameter to control the snooze duration.
// If this parameter is not specified or is zero, the default duration at the
// client side will be used.
constexpr base::FeatureParam<base::TimeDelta> kOverriddenDuration{
    &feature_engagement::kIPHDesktopSnoozeFeature,
    "x_iph_snooze_overridden_duration", base::TimeDelta::FromHours(0)};

constexpr base::FeatureParam<FeaturePromoSnoozeService::NonClickerPolicy>::
    Option kNonClickerPolicyOptions[] = {
        {FeaturePromoSnoozeService::NonClickerPolicy::kDismiss, "dismiss"},
        {FeaturePromoSnoozeService::NonClickerPolicy::kLongSnooze,
         "long_snooze"}};

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

void FeaturePromoSnoozeService::OnPromoShown(const base::Feature& iph_feature) {
  auto snooze_data = ReadSnoozeData(iph_feature);

  if (!snooze_data)
    snooze_data = SnoozeData();

  snooze_data->last_show_time = base::Time::Now();
  snooze_data->show_count++;

  SaveSnoozeData(iph_feature, *snooze_data);
}

bool FeaturePromoSnoozeService::IsBlocked(const base::Feature& iph_feature) {
  auto snooze_data = ReadSnoozeData(iph_feature);

  if (!snooze_data)
    return false;

  // This IPH has been dismissed by user permanently.
  if (snooze_data->is_dismissed)
    return true;

  // This IPH is shown for the first time.
  if (snooze_data->show_count == 0)
    return false;

  if (snooze_data->snooze_count > 0 &&
      snooze_data->last_snooze_time >= snooze_data->last_show_time) {
    // The IPH was snoozed on last display.

    // Corruption: Snooze time is in the future.
    if (snooze_data->last_snooze_time > base::Time::Now())
      return true;

    // This IPH is snoozed. Test if snooze period has expired.
    return base::Time::Now() <
           snooze_data->last_snooze_time + snooze_data->last_snooze_duration;
  } else {
    // The IPH was neither snoozed or dismissed on last display.
    const base::FeatureParam<FeaturePromoSnoozeService::NonClickerPolicy>
        kNonClickerPolicy{
            &iph_feature, "x_iph_snooze_non_clicker_policy",
            FeaturePromoSnoozeService::NonClickerPolicy::kLongSnooze,
            &kNonClickerPolicyOptions};

    NonClickerPolicy non_clicker_policy = kNonClickerPolicy.Get();

    if (non_clicker_policy == NonClickerPolicy::kDismiss)
      return true;

    return base::Time::Now() <
           snooze_data->last_show_time + base::TimeDelta::FromDays(14);
  }
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
      pref_data->FindBoolPath(path_prefix + kIPHIsDismissedPath);
  base::Optional<base::Time> show_time = util::ValueToTime(
      pref_data->FindPath(path_prefix + kIPHLastShowTimePath));
  base::Optional<base::Time> snooze_time = util::ValueToTime(
      pref_data->FindPath(path_prefix + kIPHLastSnoozeTimePath));
  base::Optional<base::TimeDelta> snooze_duration = util::ValueToTimeDelta(
      pref_data->FindPath(path_prefix + kIPHLastSnoozeDurationPath));
  base::Optional<int> snooze_count =
      pref_data->FindIntPath(path_prefix + kIPHSnoozeCountPath);
  base::Optional<int> show_count =
      pref_data->FindIntPath(path_prefix + kIPHShowCountPath);

  base::Optional<SnoozeData> snooze_data;

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
    show_time = *snooze_time - base::TimeDelta::FromSeconds(1);
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

void FeaturePromoSnoozeService::SaveSnoozeData(
    const base::Feature& iph_feature,
    const FeaturePromoSnoozeService::SnoozeData& snooze_data) {
  std::string path_prefix = std::string(iph_feature.name) + ".";

  DictionaryPrefUpdate update(profile_->GetPrefs(), kIPHSnoozeDataPath);
  base::DictionaryValue* pref_data = update.Get();

  pref_data->SetBoolPath(path_prefix + kIPHIsDismissedPath,
                         snooze_data.is_dismissed);
  pref_data->SetPath(path_prefix + kIPHLastShowTimePath,
                     util::TimeToValue(snooze_data.last_show_time));
  pref_data->SetPath(path_prefix + kIPHLastSnoozeTimePath,
                     util::TimeToValue(snooze_data.last_snooze_time));
  pref_data->SetPath(path_prefix + kIPHLastSnoozeDurationPath,
                     util::TimeDeltaToValue(snooze_data.last_snooze_duration));
  pref_data->SetIntPath(path_prefix + kIPHSnoozeCountPath,
                        snooze_data.snooze_count);
  pref_data->SetIntPath(path_prefix + kIPHShowCountPath,
                        snooze_data.show_count);
}
