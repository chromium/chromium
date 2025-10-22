// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/user_education_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
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
    base::Seconds(2);

inline constexpr char kPollingInterval[] = "polling_interval";
inline constexpr base::TimeDelta kDefaultPollingInterval =
    base::Milliseconds(500);

inline constexpr char kNtpBrowserPromoTypeOptionName[] = "promo-type";
inline constexpr std::array<base::FeatureParam<NtpBrowserPromoType>::Option, 3U>
    kNtpBrowserPromoTypeOptions({{NtpBrowserPromoType::kNone, "none"},
                                 {NtpBrowserPromoType::kSimple, "simple"},
                                 {NtpBrowserPromoType::kSetupList,
                                  "setuplist"}});

inline constexpr char kNtpBrowserPromoSuppressListName[] = "suppress-list";

inline constexpr char kNtpBrowserPromoMaxTopSpotSessionsName[] =
    "session-rotation";
inline constexpr int kDefaultNtpBrowserPromoMaxTopSpotSessions = 3;

inline constexpr char kNtpBrowserPromoCompletedDurationName[] =
    "completed-duration";
inline constexpr base::TimeDelta kDefaultNtpBrowserPromoCompletedDuration =
    base::Days(7);

inline constexpr char kNtpBrowserPromoClickedHideDurationName[] =
    "clicked-duration";
inline constexpr base::TimeDelta kDefaultNtpBrowserPromoClickedHideDuration =
    base::Days(90);

inline constexpr char kNtpBrowserPromosSnoozedHideDurationName[] =
    "snoozed-duration";
inline constexpr base::TimeDelta kDefaultNtpBrowserPromosSnoozedHideDuration =
    base::Days(7);

inline constexpr char kNtpBrowserPromoSetupListPromoLimitName[] =
    "setup-list-promo-limit";
inline constexpr int kDefaultNtpBrowserPromoSetupListPromoLimit = 10;

inline constexpr char kNtpBrowserPromoSetupListCompletedPromoLimitName[] =
    "setup-list-promo-completed-limit";
inline constexpr int kDefaultNtpBrowserPromoSetupListCompletedPromoLimit = 2;

inline constexpr char kNtpBrowserPromoIndividualPromoLimitName[] =
    "individual-promo-limit";
inline constexpr int kDefaultNtpBrowserPromoIndividualPromoLimit = 1;

}  // namespace

BASE_FEATURE(kUserEducationExperienceVersion2Point5,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewBadgeTestFeature, base::FEATURE_DISABLED_BY_DEFAULT);

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

base::TimeDelta GetNtpSetupListSnoozeTime() {
  return GetNtpBrowserPromosSnoozedHideDuration();
}

BASE_FEATURE(kEnableNtpBrowserPromos, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_ENUM_PARAM(NtpBrowserPromoType,
                        kNtpBrowserPromoType,
                        &kEnableNtpBrowserPromos,
                        kNtpBrowserPromoTypeOptionName,
                        NtpBrowserPromoType::kSimple,
                        kNtpBrowserPromoTypeOptions);

NtpBrowserPromoType GetNtpBrowserPromoType() {
  if (base::FeatureList::IsEnabled(kEnableNtpBrowserPromos)) {
    return kNtpBrowserPromoType.Get();
  }
  return NtpBrowserPromoType::kNone;
}

BASE_FEATURE_PARAM(std::string,
                   kNtpBrowserPromoSuppressList,
                   &kEnableNtpBrowserPromos,
                   kNtpBrowserPromoSuppressListName,
                   "");

BASE_FEATURE_PARAM(int,
                   kNtpBrowserPromoMaxTopSpotSessions,
                   &kEnableNtpBrowserPromos,
                   kNtpBrowserPromoMaxTopSpotSessionsName,
                   kDefaultNtpBrowserPromoMaxTopSpotSessions);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kNtpBrowserPromoCompletedDuration,
                   &kEnableNtpBrowserPromos,
                   kNtpBrowserPromoCompletedDurationName,
                   kDefaultNtpBrowserPromoCompletedDuration);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kNtpBrowserPromoClickedHideDuration,
                   &kEnableNtpBrowserPromos,
                   kNtpBrowserPromoClickedHideDurationName,
                   kDefaultNtpBrowserPromoClickedHideDuration);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kNtpBrowserPromosSnoozedHideDuration,
                   &kEnableNtpBrowserPromos,
                   kNtpBrowserPromosSnoozedHideDurationName,
                   kDefaultNtpBrowserPromosSnoozedHideDuration);

BASE_FEATURE_PARAM(int,
                   kNtpBrowserPromoSetupListPromoLimit,
                   &kEnableNtpBrowserPromos,
                   kNtpBrowserPromoSetupListPromoLimitName,
                   kDefaultNtpBrowserPromoSetupListPromoLimit);

BASE_FEATURE_PARAM(int,
                   kNtpBrowserPromoSetupListCompletedPromoLimit,
                   &kEnableNtpBrowserPromos,
                   kNtpBrowserPromoSetupListCompletedPromoLimitName,
                   kDefaultNtpBrowserPromoSetupListCompletedPromoLimit);

BASE_FEATURE_PARAM(int,
                   kNtpBrowserPromoIndividualPromoLimit,
                   &kEnableNtpBrowserPromos,
                   kNtpBrowserPromoIndividualPromoLimitName,
                   kDefaultNtpBrowserPromoIndividualPromoLimit);

std::vector<std::string> GetNtpBrowserPromoSuppressList() {
  const std::string suppressed_list = kNtpBrowserPromoSuppressList.Get();
  return base::SplitString(suppressed_list, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

int GetNtpBrowserPromoMaxTopSpotSessions() {
  return kNtpBrowserPromoMaxTopSpotSessions.Get();
}

base::TimeDelta GetNtpBrowserPromoCompletedDuration() {
  return kNtpBrowserPromoCompletedDuration.Get();
}

base::TimeDelta GetNtpBrowserPromoClickedHideDuration() {
  return kNtpBrowserPromoClickedHideDuration.Get();
}

base::TimeDelta GetNtpBrowserPromosSnoozedHideDuration() {
  return kNtpBrowserPromosSnoozedHideDuration.Get();
}

int GetNtpBrowserPromoSetupListPromoLimit() {
  return kNtpBrowserPromoSetupListPromoLimit.Get();
}

int GetNtpBrowserPromoSetupListCompletedPromoLimit() {
  return kNtpBrowserPromoSetupListCompletedPromoLimit.Get();
}

int GetNtpBrowserPromoIndividualPromoLimit() {
  return kNtpBrowserPromoIndividualPromoLimit.Get();
}

std::ostream& operator<<(std::ostream& os, NtpBrowserPromoType promo_type) {
  os << kNtpBrowserPromoType.GetName(promo_type);
  return os;
}

}  // namespace user_education::features
