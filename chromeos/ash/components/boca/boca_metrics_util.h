// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_METRICS_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_METRICS_UTIL_H_

#include "base/time/time.h"

namespace ash::boca {
inline constexpr char kBocaOnTaskLockedSessionDurationPercentage[] =
    "Ash.Boca.OnTask.LockedSessionDurationPercentage";
inline constexpr char kBocaOnTaskUnlockedSessionDurationPercentage[] =
    "Ash.Boca.OnTask.UnlockedSessionDurationPercentage";

// Records the percentage of the duration that a session was in a particular
// locked or unlocked state.
void RecordOnTaskLockedStateDurationPercentage(
    base::TimeDelta unlocked_state_duration,
    base::TimeDelta locked_state_duration);

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_METRICS_UTIL_H_
