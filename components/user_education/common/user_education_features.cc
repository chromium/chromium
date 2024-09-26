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

inline constexpr char kNewProfileGracePeriodParamName[] =
    "new_profile_grace_period";
inline constexpr base::TimeDelta kDefaultNewProfileGracePeriod = base::Days(7);

inline constexpr char kLowPriorityCooldownParamName[] = "low_priority_cooldown";
inline constexpr base::TimeDelta kDefaultLowPriorityCooldown = base::Days(8);

inline constexpr char kSnoozeDurationParamName[] = "snooze_duration";
inline constexpr base::TimeDelta kDefaultSnoozeDuration = base::Days(7);

inline constexpr char kAbortCooldownParamName[] = "abort_cooldown";
inline constexpr base::TimeDelta kDefaultAbortCooldown = base::Hours(4);

inline constexpr char kMaxSnoozeCountParamName[] = "max_snooze_count";
inline constexpr int kDefaultMaxSnoozeCount = 3;

inline constexpr char kMaxPromoShowCount[] = "max_promo_show_count";
inline constexpr int kDefaultMaxPromoShowCount = 3;

inline constexpr char kNewBadgeShowCount[] = "new_badge_show_count";
inline constexpr int kDefaultNewBadgeShowCount = 10;

inline constexpr char kNewBadgeFeatureUsedCount[] =
    "new_badge_feature_used_count";
inline constexpr int kDefaultNewBadgeFeatureUsedCount = 2;

inline constexpr char kNewBadgeDisplayWindow[] = "new_badge_display_window";
inline constexpr base::TimeDelta kDefaultNewBadgeDisplayWindow = base::Days(60);

}  // namespace

BASE_FEATURE(kUserEducationExperienceVersion2,
             "UserEducationExperienceVersion2",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewBadgeTestFeature,
             "NewBadgeTestFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWhatsNewVersion2,
             "WhatsNewVersion2",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsUserEducationV2() {
  return base::FeatureList::IsEnabled(kUserEducationExperienceVersion2);
}

bool IsWhatsNewV2() {
  return base::FeatureList::IsEnabled(kWhatsNewVersion2);
}

bool IsRateLimitingDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableRateLimitingCommandLine);
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
  if (IsRateLimitingDisabled()) {
    return base::TimeDelta();
  }
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2, kSessionStartGracePeriodParamName,
      kDefaultSessionStartGracePeriod);
}

base::TimeDelta GetLowPriorityCooldown() {
  if (IsRateLimitingDisabled()) {
    return base::TimeDelta();
  }
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2, kLowPriorityCooldownParamName,
      kDefaultLowPriorityCooldown);
}

base::TimeDelta GetNewProfileGracePeriod() {
  if (IsRateLimitingDisabled()) {
    return base::TimeDelta();
  }
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2, kNewProfileGracePeriodParamName,
      kDefaultNewProfileGracePeriod);
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

int GetMaxPromoShowCount() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kUserEducationExperienceVersion2, kMaxPromoShowCount,
      kDefaultMaxPromoShowCount);
}

int GetNewBadgeShowCount() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kUserEducationExperienceVersion2, kNewBadgeShowCount,
      kDefaultNewBadgeShowCount);
}

int GetNewBadgeFeatureUsedCount() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kUserEducationExperienceVersion2, kNewBadgeFeatureUsedCount,
      kDefaultNewBadgeFeatureUsedCount);
}

base::TimeDelta GetNewBadgeDisplayWindow() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserEducationExperienceVersion2, kNewBadgeDisplayWindow,
      kDefaultNewBadgeDisplayWindow);
}

}  // namespace user_education::features
