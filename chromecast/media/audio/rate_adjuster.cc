// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/rate_adjuster.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/check.h"

namespace chromecast {
namespace media {

namespace {
constexpr auto kMaxRateChangeInterval = base::Minutes(5);
}  // namespace

RateAdjuster::RateAdjuster(const Config& config,
                           RateChangeCallback change_clock_rate,
                           double current_clock_rate)
    : config_(config),
      change_clock_rate_(std::move(change_clock_rate)),
      linear_error_(config_.linear_regression_window.InMicroseconds()),
      current_clock_rate_(current_clock_rate) {
  DCHECK(change_clock_rate_);
}

RateAdjuster::~RateAdjuster() = default;

void RateAdjuster::Reserve(int count) {
  linear_error_.Reserve(count);
}

void RateAdjuster::Reset() {
  linear_error_.Reset();
  initialized_ = false;
  clock_rate_start_timestamp_ = 0;
  initial_timestamp_ = 0;
  clock_rate_error_base_ = 0.0;
}

void RateAdjuster::AddError(int64_t error, int64_t timestamp) {
  if (!initialized_) {
    clock_rate_start_timestamp_ = timestamp;
    clock_rate_error_base_ = 0.0;
    initial_timestamp_ = timestamp;
    initialized_ = true;
  }

  int64_t x = timestamp - initial_timestamp_;

  // Error is positive if actions are happening too late.
  // We perform |current_clock_rate_| seconds of actions per second of actual
  // time. We want to run a linear regression on how the error is changing over
  // time, if we ignore the effects of any previous clock rate changes. To do
  // this, we correct the error value to what it would have been if we had never
  // adjusted the clock rate.
  // In the last N seconds, if the clock rate was 1.0 we would have performed
  // (1.0 - clock_rate) * N more seconds of actions, so the current action would
  // have occurred that much sooner (reducing its error by that amount). We
  // also need to take into account the previous "expected error" (due to clock
  // rate changes) at the point when we last changed the clock rate. The
  // expected error now is the previous expected error, plus the change due to
  // the clock rate of (1.0 - clock_rate) * N seconds.
  int64_t time_at_current_clock_rate = timestamp - clock_rate_start_timestamp_;
  double correction = clock_rate_error_base_ +
                      (1.0 - current_clock_rate_) * time_at_current_clock_rate;
  int64_t corrected_error = error - correction;
  linear_error_.AddSample(x, corrected_error, 1.0);

  if (time_at_current_clock_rate <
      config_.rate_change_interval.InMicroseconds()) {
    // Don't change clock rate too frequently.
    return;
  }

  int64_t offset;
  double slope;
  double e;
  if (!linear_error_.EstimateY(x, &offset, &e) ||
      !linear_error_.EstimateSlope(&slope, &e)) {
    return;
  }

  // Get the smoothed error (linear regression estimate) at the current time,
  // translated back into actual error.
  int64_t smoothed_error = offset + correction;

  // If slope is positive, a clock rate of 1.0 is too slow (actions are
  // occurring progressively later than desired). We wanted to do slope*N
  // seconds actions during N seconds than would have been done at rate 1.0.
  // Therefore the actual clock rate should be (1.0 + slope).
  // However, we also want to correct for any existing offset. We correct so
  // that the error should reduce to 0 by the next rate change interval;
  // however the rate change is capped to prevent very fast slewing.
  double offset_correction =
      static_cast<double>(smoothed_error) /
      (config_.rate_change_interval.InMicroseconds() * 2);
  if (std::abs(smoothed_error) < config_.max_ignored_current_error) {
    // Offset is small enough that we can ignore it, but still correct a little
    // bit to avoid bouncing in and out of the ignored region.
    offset_correction = offset_correction / 4;
  }
  offset_correction =
      std::clamp(offset_correction, -config_.max_current_error_correction,
                 config_.max_current_error_correction);
  double new_rate = (1.0 + slope) + offset_correction;

  // Only change the clock rate if the difference between the desired rate and
  // the current rate is larger than the minimum change.
  if (std::fabs(new_rate - current_clock_rate_) > config_.min_rate_change ||
      time_at_current_clock_rate > kMaxRateChangeInterval.InMicroseconds()) {
    current_clock_rate_ =
        change_clock_rate_.Run(new_rate, slope, smoothed_error);
    clock_rate_start_timestamp_ = timestamp;
    clock_rate_error_base_ = correction;
  }
}

}  // namespace media
}  // namespace chromecast
