// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/user_education_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace user_education::features {

namespace {

inline constexpr char kTimeToIdleParamName[] = "time_to_idle";
inline constexpr base::TimeDelta kDefaultTimeToIdle = base::Seconds(30);

inline constexpr char kMinimumValidSessionLengthParamName[] =
    "minimum_valid_session_length";
inline constexpr base::TimeDelta kDefaultMinimumValidSessionLength =
    base::Minutes(2);

inline constexpr char kIdleTimeBetweenSessionsParamName[] =
    "idle_time_between_sessions";
inline constexpr base::TimeDelta kDefaultIdleTimeBetweenSessions =
    base::Hours(4);

inline constexpr char kSessionStartGracePeriodParamName[] =
    "session_start_grace_period";
inline constexpr base::TimeDelta kDefaultSessionStartGracePeriod =
    base::Minutes(10);

inline constexpr char kLowPriorityCooldownParamName[] = "low_priority_cooldown";
inline constexpr base::TimeDelta kDefaultLowPriorityCooldown = base::Days(8);

inline constexpr char kSnoozeDurationParamName[] = "snooze_duration";
inline constexpr base::TimeDelta kDefaultSnoozeDuration = base::Days(7);

inline constexpr char kAbortCooldownParamName[] = "abort_cooldown";
inline constexpr base::TimeDelta kDefaultAbortCooldown = base::Hours(4);

inline constexpr char kMaxSnoozeCountParamName[] = "max_snooze_count";
inline constexpr int kDefaultMaxSnoozeCount = 3;

}  // namespace

BASE_FEATURE(kUserEducationExperienceVersion2,
             "UserEducationExperienceVersion2",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUserEducationV2() {
  return base::FeatureList::IsEnabled(kUserEducationExperienceVersion2);
}

base::TimeDelta GetTimeToIdle() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2, kTimeToIdleParamName,
      kDefaultTimeToIdle);
}

base::TimeDelta GetMinimumValidSessionLength() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2, kMinimumValidSessionLengthParamName,
      kDefaultMinimumValidSessionLength);
}

base::TimeDelta GetIdleTimeBetweenSessions() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2, kIdleTimeBetweenSessionsParamName,
      kDefaultIdleTimeBetweenSessions);
}

base::TimeDelta GetSessionStartGracePeriod() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2, kSessionStartGracePeriodParamName,
      kDefaultSessionStartGracePeriod);
}

base::TimeDelta GetLowPriorityCooldown() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2, kLowPriorityCooldownParamName,
      kDefaultLowPriorityCooldown);
}

base::TimeDelta GetSnoozeDuration() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2, kSnoozeDurationParamName,
      kDefaultSnoozeDuration);
}

base::TimeDelta GetAbortCooldown() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2, kAbortCooldownParamName,
      kDefaultAbortCooldown);
}

int GetMaxSnoozeCount() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kUserEducationExperienceVersion2, kMaxSnoozeCountParamName,
      kDefaultMaxSnoozeCount);
}

}  // namespace user_education::features
