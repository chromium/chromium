// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/experiment.h"

#include <algorithm>
#include <cmath>

#include "base/check_op.h"

namespace installer {

namespace {

// Returns closest integer of logarithm of |x| with base |b|.
double LogFloor(double x, double b) {
  return std::round(std::log(x) / std::log(b));
}

// Returns the base to use for exponential buckets so that buckets
// 0,1,.. 2^|bits|-1 cover range [0, max_val]. If this function return b
// then Bucket value i will store values from [b^i, b^(i+1)]
double ExpBucketBase(int max_val, int bits) {
  return std::exp(std::log(max_val + 1) / ((1 << bits) - 1));
}

}  // namespace

Experiment::Experiment() = default;
Experiment::Experiment(Experiment&&) = default;
Experiment::Experiment(const Experiment&) = default;
Experiment::~Experiment() = default;

void Experiment::InitializeFromMetrics(const ExperimentMetrics& metrics) {
  *this = Experiment();
  DCHECK(metrics.InInitialState() ||
         metrics.state == ExperimentMetrics::kGroupAssigned);
  metrics_ = metrics;
  state_ = metrics.state;
  group_ = metrics.group;
  if (metrics.state == ExperimentMetrics::kUninitialized) {
    // Reset any value stored in experiment.
    toast_location_ = ExperimentMetrics::kOverTaskbarPin;
    inactive_days_ = 0;
    toast_count_ = 0;
    first_display_time_ = base::Time();
    latest_display_time_ = base::Time();
    user_session_uptime_ = base::TimeDelta();
    action_delay_ = base::TimeDelta();
  }
}

void Experiment::SetState(ExperimentMetrics::State state) {
  DCHECK_NE(ExperimentMetrics::kUninitialized, state);
  state_ = state;
  metrics_.state = state;
}

void Experiment::AssignGroup(int group) {
  DCHECK_GE(group, 0);
  DCHECK_LT(group, ExperimentMetrics::kNumGroups);
  DCHECK(metrics_.InInitialState());

  group_ = group;
  metrics_.group = group;
  SetState(ExperimentMetrics::kGroupAssigned);
}

void Experiment::SetToastLocation(ExperimentMetrics::ToastLocation location) {
  DCHECK(!metrics_.InTerminalState());
  DCHECK(!metrics_.InInitialState());
  toast_location_ = location;
  metrics_.toast_location = location;
}

void Experiment::SetInactiveDays(int days) {
  DCHECK(!metrics_.InTerminalState());
  DCHECK(!metrics_.InInitialState());
  DCHECK_GE(days, 0);
  inactive_days_ = days;
  double log_base = ExpBucketBase(ExperimentMetrics::kMaxLastUsed,
                                  ExperimentMetrics::kLastUsedBucketBits);
  metrics_.last_used_bucket = LogFloor(
      1 + std::min(days, static_cast<int>(ExperimentMetrics::kMaxLastUsed)),
      log_base);
}

void Experiment::SetToastCount(int count) {
  DCHECK(!metrics_.InTerminalState());
  DCHECK(!metrics_.InInitialState());
  toast_count_ = count;
  metrics_.toast_count =
      std::min(count, static_cast<int>(ExperimentMetrics::kMaxToastCount));
}

void Experiment::SetDisplayTime(base::Time time) {
  DCHECK(!metrics_.InTerminalState());
  DCHECK(!metrics_.InInitialState());
  if (metrics_.first_toast_offset_days == 0) {
    // This is the first time toast is shown so add user to today's cohort.
    first_display_time_ = time;
    metrics_.first_toast_offset_days =
        (time - base::Time::UnixEpoch() -
         base::Seconds(ExperimentMetrics::kExperimentStartSeconds))
            .InDays();
    // If display time is outside the experiment range (possible due to
    // invalid local time), then set it to be kMaxFirstToastOffsetDays.
    if (metrics_.first_toast_offset_days < 0) {
      metrics_.first_toast_offset_days =
          ExperimentMetrics::kMaxFirstToastOffsetDays;
    } else {
      metrics_.first_toast_offset_days = std::min(
          metrics_.first_toast_offset_days,
          static_cast<int>(ExperimentMetrics::kMaxFirstToastOffsetDays));
    }
  }
  latest_display_time_ = time;
  metrics_.toast_hour = (time - time.LocalMidnight()).InHours();
  DCHECK_LE(metrics_.toast_hour, 24);
  DCHECK_GE(metrics_.toast_hour, 0);
}

void Experiment::SetUserSessionUptime(base::TimeDelta time_delta) {
  DCHECK(!metrics_.InTerminalState());
  DCHECK(!metrics_.InInitialState());
  user_session_uptime_ = time_delta;
  double log_base = ExpBucketBase(ExperimentMetrics::kMaxSessionLength,
                                  ExperimentMetrics::kSessionLengthBucketBits);
  if (time_delta.InMinutes() < 0 ||
      time_delta.InMinutes() > ExperimentMetrics::kMaxSessionLength) {
    time_delta =
        base::Minutes(ExperimentMetrics::ExperimentMetrics::kMaxSessionLength);
  }
  metrics_.session_length_bucket =
      LogFloor(1 + time_delta.InMinutes(), log_base);
}

void Experiment::SetActionDelay(base::TimeDelta time_delta) {
  DCHECK(!metrics_.InTerminalState());
  DCHECK(!metrics_.InInitialState());
  action_delay_ = time_delta;
  if (time_delta.InSeconds() < 0 ||
      time_delta.InSeconds() > ExperimentMetrics::kMaxActionDelay) {
    time_delta = base::Seconds(ExperimentMetrics::kMaxActionDelay);
  }
  double log_base = ExpBucketBase(ExperimentMetrics::kMaxActionDelay,
                                  ExperimentMetrics::kActionDelayBucketBits);
  metrics_.action_delay_bucket = LogFloor(1 + time_delta.InSeconds(), log_base);
}

}  // namespace installer
