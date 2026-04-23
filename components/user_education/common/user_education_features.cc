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

inline constexpr base::TimeDelta kDefaultHighPriorityTimeout =
    base::Seconds(15);

inline constexpr base::TimeDelta kDefaultMediumPriorityTimeout =
    base::Seconds(20);

inline constexpr base::TimeDelta kDefaultLowPriorityTimeout = base::Seconds(30);

inline constexpr base::TimeDelta kDefaultIdleTimeBeforeHeavyweight =
    base::Seconds(2);

inline constexpr base::TimeDelta kDefaultPollingInterval =
    base::Milliseconds(500);

inline constexpr char kNtpBrowserPromoTypeOptionName[] = "promo-type";
inline constexpr std::array<base::FeatureParam<NtpBrowserPromoType>::Option, 2U>
    kNtpBrowserPromoTypeOptions({{NtpBrowserPromoType::kNone, "none"},
                                 {NtpBrowserPromoType::kSimple, "simple"}});

inline constexpr char kNtpBrowserPromoSuppressListName[] = "suppress-list";

inline constexpr char kNtpBrowserPromoMaxSessionsPerTermName[] =
    "session-rotation";

inline constexpr char kNtpBrowserPromoMaxTermsName[] = "max-terms";
inline constexpr int kDefaultNtpBrowserPromoMaxTerms = 3;
inline constexpr int kDefaultNtpBrowserPromoMaxSessionsPerTerm = 2;

inline constexpr char kNtpBrowserPromoClickedHideDurationName[] =
    "clicked-duration";
inline constexpr base::TimeDelta kDefaultNtpBrowserPromoClickedHideDuration =
    base::Days(90);

inline constexpr char kNtpBrowserPromosSnoozedHideDurationName[] =
    "snoozed-duration";
inline constexpr base::TimeDelta kDefaultNtpBrowserPromosSnoozedHideDuration =
    base::Days(7);

std::optional<testing::TimeoutOverrides> g_timeout_overrides;

}  // namespace

BASE_FEATURE(kNewBadgeTestFeature, base::FEATURE_DISABLED_BY_DEFAULT);

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
  return g_timeout_overrides ? g_timeout_overrides->high_priority_timeout
                             : kDefaultHighPriorityTimeout;
}

base::TimeDelta GetMediumPriorityTimeout() {
  return g_timeout_overrides ? g_timeout_overrides->medium_priority_timeout
                             : kDefaultMediumPriorityTimeout;
}

base::TimeDelta GetLowPriorityTimeout() {
  return g_timeout_overrides ? g_timeout_overrides->low_priority_timeout
                             : kDefaultLowPriorityTimeout;
}

base::TimeDelta GetIdleTimeBeforeHeavyweightPromo() {
  return g_timeout_overrides ? g_timeout_overrides->idle_before_heavyweight
                             : kDefaultIdleTimeBeforeHeavyweight;
}

base::TimeDelta GetPromoControllerPollingInterval() {
  return kDefaultPollingInterval;
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
    return NtpBrowserPromoType::kSimple;
  }
  return NtpBrowserPromoType::kNone;
}

BASE_FEATURE_PARAM(std::string,
                   kNtpBrowserPromoSuppressList,
                   &kEnableNtpBrowserPromos,
                   kNtpBrowserPromoSuppressListName,
                   "");

BASE_FEATURE_PARAM(int,
                   kNtpBrowserPromoMaxSessionsPerTerm,
                   &kEnableNtpBrowserPromos,
                   kNtpBrowserPromoMaxSessionsPerTermName,
                   kDefaultNtpBrowserPromoMaxSessionsPerTerm);

BASE_FEATURE_PARAM(int,
                   kNtpBrowserPromoMaxTerms,
                   &kEnableNtpBrowserPromos,
                   kNtpBrowserPromoMaxTermsName,
                   kDefaultNtpBrowserPromoMaxTerms);

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

std::vector<std::string> GetNtpBrowserPromoSuppressList() {
  const std::string suppressed_list = kNtpBrowserPromoSuppressList.Get();
  return base::SplitString(suppressed_list, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

int GetNtpBrowserPromoMaxSessionsPerTerm() {
  return kNtpBrowserPromoMaxSessionsPerTerm.Get();
}

int GetNtpBrowserPromoMaxTerms() {
  return kNtpBrowserPromoMaxTerms.Get();
}

base::TimeDelta GetNtpBrowserPromoClickedHideDuration() {
  return kNtpBrowserPromoClickedHideDuration.Get();
}

std::ostream& operator<<(std::ostream& os, NtpBrowserPromoType promo_type) {
  os << kNtpBrowserPromoType.GetName(promo_type);
  return os;
}

namespace testing {

TimeoutOverrideHandle SetTimeoutOverridesForTest(TimeoutOverrides overrides) {
  return std::make_unique<base::AutoReset<std::optional<TimeoutOverrides>>>(
      &g_timeout_overrides, std::make_optional(overrides));
}

}  // namespace testing

}  // namespace user_education::features
