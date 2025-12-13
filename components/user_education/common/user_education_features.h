// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_FEATURES_H_
#define COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_FEATURES_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace user_education::features {

// Command-line switch that disables rate limiting in User Education, like the
// new profile grace period and the heavyweight IPH cooldown.
//
// For testing purposes, strongly prefer to inherit from
// `InteractiveFeaturePromoTest`, but this is not possible for unit tests, so
// this is provided as a public constant for convenience.
inline constexpr char kDisableRateLimitingCommandLine[] =
    "disable-user-education-rate-limiting";

BASE_DECLARE_FEATURE(kUserEducationExperienceVersion2Point5);
BASE_DECLARE_FEATURE(kNewBadgeTestFeature);

// Returns whether User Education Version 2.5 policies are enabled.
// This requires User Education Version 2.
extern bool IsUserEducationV25();

// Returns the minimum amount of time a session must last. If this is less than
// `GetIdleTimeBetweenSessions()` then it will have no effect.
extern base::TimeDelta GetMinimumValidSessionLength();

// Returns the minimum amount of time the application must be idle before a new
// session can start.
extern base::TimeDelta GetIdleTimeBetweenSessions();

// Returns the amount of time in which low-priority, heavyweight IPH are
// prevented from showing after a new session starts.
extern base::TimeDelta GetSessionStartGracePeriod();

// Gets the amount of time that must pass after a heavyweight promo before a
// low-priority heavyweight promo can be shown.
extern base::TimeDelta GetLowPriorityCooldown();

// Gets the amount of time after a new profile is created on a device before
// most low-priority user education primitives can be displayed.
extern base::TimeDelta GetNewProfileGracePeriod();

// Gets the minimum amount of time from when an IPH is snoozed until it can be
// shown again. For low-priority IPH, if this is shorter than
// `GetLowPriorityCooldown()` then it will have no additional effect.
extern base::TimeDelta GetSnoozeDuration();

// Gets the minimum amount of time from when a low-priority IPH is aborted due
// to a UI change (i.e. not via user snooze or dismissal) to when it can show
// again.
extern base::TimeDelta GetAbortCooldown();

// Returns the maximum number of times the user can hit "snooze" on an IPH until
// the snooze button no longer appears.
extern int GetMaxSnoozeCount();

// Returns the maximum number of times a low-priority is allowed to show at all
// before it is permanently blocked.
extern int GetMaxPromoShowCount();

// Returns the number of times a "New" Badge is shown before it stops appearing.
extern int GetNewBadgeShowCount();

// Returns the number of times the feature being promoted by a "New" Badge can
// be used before the badge disappears.
extern int GetNewBadgeFeatureUsedCount();

// Returns the amount of time from when a feature being promoted by a "New"
// Badge becomes active that the badge can be displayed to the user. Badges stop
// being displayed after this period.
extern base::TimeDelta GetNewBadgeDisplayWindow();

// Returns timeouts for high, medium, and low-priority promos in the queue for
// User Education 2.5.
extern base::TimeDelta GetHighPriorityTimeout();
extern base::TimeDelta GetMediumPriorityTimeout();
extern base::TimeDelta GetLowPriorityTimeout();

// Returns how long the user must stop sending input before a heavyweight promo
// can be shown.
extern base::TimeDelta GetIdleTimeBeforeHeavyweightPromo();

// Returns the polling interval for the promo controller for User Education 2.5.
extern base::TimeDelta GetPromoControllerPollingInterval();

// Returns how long the NTP Setup List module is snoozed for.
extern base::TimeDelta GetNtpSetupListSnoozeTime();

// Advertises browser features in New Tab Page promos.
BASE_DECLARE_FEATURE(kEnableNtpBrowserPromos);

// Describes the type of NTP promo that can be shown, if any.
enum class NtpBrowserPromoType {
  // Indicates that the flag is disabled.
  kNone,
  // Indicates that a simple (single-promo) option is selected.
  kSimple,
  // Indicates that a full Setup List style is selected.
  kSetupList,
};

// The parameter that specifies which promo option to use.
BASE_DECLARE_FEATURE_PARAM(NtpBrowserPromoType, kNtpBrowserPromoType);

// Returns the current NTP promo type, or kNone if it is not enabled.
extern NtpBrowserPromoType GetNtpBrowserPromoType();

// A list of promo IDs to suppress.
BASE_DECLARE_FEATURE_PARAM(std::string, kNtpBrowserPromoSuppressList);

// The number of sessions a promo may stay in the top spot before being
// rotated out.
BASE_DECLARE_FEATURE_PARAM(int, kNtpBrowserPromoMaxTopSpotSessions);

// How long a promo stays in the "completed" section of the setup list.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kNtpBrowserPromoCompletedDuration);

// How long a promo is hidden after being clicked.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kNtpBrowserPromoClickedHideDuration);

// How long all promos are hidden after being snoozed.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kNtpBrowserPromosSnoozedHideDuration);

// The maximum number of promos to display in setup-list mode.
BASE_DECLARE_FEATURE_PARAM(int, kNtpBrowserPromoSetupListPromoLimit);

// The maximum number of promos to display in individual-promo mode.
BASE_DECLARE_FEATURE_PARAM(int, kNtpBrowserPromoIndividualPromoLimit);

// Accessors for `kEnableNtpBrowserPromos` parameters.
extern std::vector<std::string> GetNtpBrowserPromoSuppressList();
extern int GetNtpBrowserPromoMaxTopSpotSessions();
extern base::TimeDelta GetNtpBrowserPromoCompletedDuration();
extern base::TimeDelta GetNtpBrowserPromoClickedHideDuration();
extern base::TimeDelta GetNtpBrowserPromosSnoozedHideDuration();
extern int GetNtpBrowserPromoSetupListPromoLimit();
extern int GetNtpBrowserPromoSetupListCompletedPromoLimit();
extern int GetNtpBrowserPromoIndividualPromoLimit();

extern std::ostream& operator<<(std::ostream& os,
                                NtpBrowserPromoType promo_type);

}  // namespace user_education::features

#endif  // COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_FEATURES_H_
