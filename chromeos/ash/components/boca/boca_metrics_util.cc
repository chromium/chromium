// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_metrics_util.h"

#include "base/metrics/histogram_macros.h"

namespace ash::boca {

void RecordOnTaskLockedStateDurationPercentage(
    base::TimeDelta unlocked_state_duration,
    base::TimeDelta locked_state_duration) {
  const base::TimeDelta total_time =
      unlocked_state_duration + locked_state_duration;
  if (total_time.is_zero()) {
    return;
  }
  const double percentage_locked = 100.0 * (locked_state_duration / total_time);
  const double percentage_unlocked = 100.0 - percentage_locked;
  UMA_HISTOGRAM_PERCENTAGE(kBocaOnTaskLockedSessionDurationPercentage,
                           percentage_locked);
  UMA_HISTOGRAM_PERCENTAGE(kBocaOnTaskUnlockedSessionDurationPercentage,
                           percentage_unlocked);
}

void RecordOnTaskNumOfTabsWhenSessionEnded(int num_of_tabs) {
  UMA_HISTOGRAM_COUNTS_100(kBocaOnTaskNumOfTabsWhenSessionEnded, num_of_tabs);
}

void RecordOnTaskMaxNumOfTabsDuringSession(int max_num_of_tabs) {
  UMA_HISTOGRAM_COUNTS_100(kBocaOnTaskMaxNumOfTabsDuringSession,
                           max_num_of_tabs);
}

}  // namespace ash::boca
