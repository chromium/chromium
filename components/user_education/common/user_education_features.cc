// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/user_education_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace user_education::features {

namespace {

inline constexpr base::TimeDelta kDefaultMinimumValidSessionLength =
    base::Minutes(2);

inline constexpr base::TimeDelta kDefaultIdleTimeBetweenSessions =
    base::Hours(4);

inline constexpr base::TimeDelta kDefaultSessionStartGracePeriod =
    base::Minutes(10);

inline constexpr base::TimeDelta kDefaultNewProfileGracePeriod = base::Days(7);

inline constexpr base::TimeDelta kDefaultLowPriorityCooldown = base::Days(8);

inline constexpr base::TimeDelta kDefaultSnoozeDuration = base::Days(7);

inline constexpr base::TimeDelta kDefaultAbortCooldown = base::Hours(4);

inline constexpr int kDefaultMaxSnoozeCount = 3;

inline constexpr int kDefaultMaxPromoShowCount = 3;

inline constexpr int kDefaultNewBadgeShowCount = 10;

inline constexpr int kDefaultNewBadgeFeatureUsedCount = 2;

inline constexpr base::TimeDelta kDefaultNewBadgeDisplayWindow = base::Days(60);

inline constexpr char kHighPriorityTimeout[] = "high_priority_timeout";
inline constexpr base::TimeDelta kDefaultHighPriorityTimeout =
    base::Seconds(15);

inline constexpr char kMediumPriorityTimeout[] = "medium_priority_timeout";
inline constexpr base::TimeDelta kDefaultMediumPriorityTimeout =
    base::Seconds(20);

inline constexpr char kLowPriorityTimeout[] = "low_priority_timeout";
inline constexpr base::TimeDelta kDefaultLowPriorityTimeout = base::Seconds(30);

inline constexpr char kIdleTimeBeforeHeavyweight[] = "idle_before_heavyweight";
inline constexpr base::TimeDelta kDefaultIdleTimeBeforeHeavyweight =
    base::Seconds(5);

inline constexpr char kPollingInterval[] = "polling_interval";
inline constexpr base::TimeDelta kDefaultPollingInterval =
    base::Milliseconds(500);

}  // namespace

BASE_FEATURE(kUserEducationExperienceVersion2Point5,
             "UserEducationExperienceVersion2Point5",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewBadgeTestFeature,
             "NewBadgeTestFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUserEducationV25() {
  return base::FeatureList::IsEnabled(kUserEducationExperienceVersion2Point5);
}

bool IsRateLimitingDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableRateLimitingCommandLine);
}

base::TimeDelta GetMinimumValidSessionLength() {
  return kDefaultMinimumValidSessionLength;
}

base::TimeDelta GetIdleTimeBetweenSessions() {
  return kDefaultIdleTimeBetweenSessions;
}

base::TimeDelta GetSessionStartGracePeriod() {
  if (IsRateLimitingDisabled()) {
    return base::TimeDelta();
  }
  return kDefaultSessionStartGracePeriod;
}

base::TimeDelta GetLowPriorityCooldown() {
  if (IsRateLimitingDisabled()) {
    return base::TimeDelta();
  }
  return kDefaultLowPriorityCooldown;
}

base::TimeDelta GetNewProfileGracePeriod() {
  if (IsRateLimitingDisabled()) {
    return base::TimeDelta();
  }
  return kDefaultNewProfileGracePeriod;
}

base::TimeDelta GetSnoozeDuration() {
  return kDefaultSnoozeDuration;
}

base::TimeDelta GetAbortCooldown() {
  return kDefaultAbortCooldown;
}

int GetMaxSnoozeCount() {
  return kDefaultMaxSnoozeCount;
}

int GetMaxPromoShowCount() {
  return kDefaultMaxPromoShowCount;
}

int GetNewBadgeShowCount() {
  return kDefaultNewBadgeShowCount;
}

int GetNewBadgeFeatureUsedCount() {
  return kDefaultNewBadgeFeatureUsedCount;
}

base::TimeDelta GetNewBadgeDisplayWindow() {
  return kDefaultNewBadgeDisplayWindow;
}

base::TimeDelta GetHighPriorityTimeout() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2Point5, kHighPriorityTimeout,
      kDefaultHighPriorityTimeout);
}

base::TimeDelta GetMediumPriorityTimeout() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2Point5, kMediumPriorityTimeout,
      kDefaultMediumPriorityTimeout);
}

base::TimeDelta GetLowPriorityTimeout() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2Point5, kLowPriorityTimeout,
      kDefaultLowPriorityTimeout);
}

base::TimeDelta GetIdleTimeBeforeHeavyweightPromo() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2Point5, kIdleTimeBeforeHeavyweight,
      kDefaultIdleTimeBeforeHeavyweight);
}

base::TimeDelta GetPromoControllerPollingInterval() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2Point5, kPollingInterval,
      kDefaultPollingInterval);
}

}  // namespace user_education::features
