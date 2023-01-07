// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_snooze_service.h"

#include <ostream>

#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace user_education {

namespace {

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

constexpr int FeaturePromoSnoozeService::kUmaMaxSnoozeCount;
constexpr base::TimeDelta FeaturePromoSnoozeService::kDefaultSnoozeDuration;

FeaturePromoSnoozeService::FeaturePromoSnoozeService() = default;
FeaturePromoSnoozeService::~FeaturePromoSnoozeService() = default;

void FeaturePromoSnoozeService::OnUserSnooze(const base::Feature& iph_feature,
                                             base::TimeDelta snooze_duration) {
  DCHECK(snooze_duration > base::Seconds(0));
  auto snooze_data = ReadSnoozeData(iph_feature);

  if (!snooze_data)
    snooze_data = SnoozeData();

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

    return base::Time::Now() < snooze_data->last_show_time + base::Days(14);
  }
}

int FeaturePromoSnoozeService::GetSnoozeCount(
    const base::Feature& iph_feature) {
  absl::optional<SnoozeData> snooze_data = ReadSnoozeData(iph_feature);
  return snooze_data ? snooze_data->snooze_count : 0;
}

}  // namespace user_education
