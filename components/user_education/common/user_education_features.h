// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_FEATURES_H_
#define COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_FEATURES_H_

#include "base/feature_list.h"
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

BASE_DECLARE_FEATURE(kUserEducationExperienceVersion2);
BASE_DECLARE_FEATURE(kNewBadgeTestFeature);
BASE_DECLARE_FEATURE(kWhatsNewVersion2);

// Returns whether User Education Version 2 policies are enabled.
extern bool IsUserEducationV2();

// Returns whether What's New Version 2 is enabled.
extern bool IsWhatsNewV2();

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

}  // namespace user_education::features

#endif  // COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_FEATURES_H_
