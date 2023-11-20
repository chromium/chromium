// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_FEATURES_H_
#define COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_FEATURES_H_

#include "base/feature_list.h"
#include "base/time/time.h"

namespace user_education::features {

BASE_DECLARE_FEATURE(kUserEducationExperienceVersion2);

inline constexpr char kTimeToIdleParamName[] = "time_to_idle";
inline constexpr char kMinimumValidSessionLengthParamName[] =
    "minimum_valid_session_length";
inline constexpr char kIdleTimeBetweenSessionsParamName[] =
    "idle_time_between_sessions";
inline constexpr char kSessionStartGracePeriod[] = "session_start_grace_period";
inline constexpr char kLowPriorityIphCooldown[] = "low_priority_iph_cooldown";

inline constexpr base::TimeDelta kDefaultTimeToIdle = base::Seconds(30);
inline constexpr base::TimeDelta kDefaultMinimumValidSessionLength =
    base::Minutes(2);
inline constexpr base::TimeDelta kDefaultIdleTimeBetweenSessions =
    base::Hours(4);
inline constexpr base::TimeDelta kDefaultSessionStartGracePeriod =
    base::Minutes(10);
inline constexpr base::TimeDelta kDefaultLowPriorityIphCooldown = base::Days(8);

}  // namespace user_education::features

#endif  // COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_FEATURES_H_
