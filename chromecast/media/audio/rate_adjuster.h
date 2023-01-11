// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_RATE_ADJUSTER_H_
#define CHROMECAST_MEDIA_AUDIO_RATE_ADJUSTER_H_

#include <cstdint>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chromecast/base/statistics/weighted_moving_linear_regression.h"

namespace chromecast {
namespace media {

// RateAdjuster handles adjusting a clock rate to minimize errors over time.
// This can be used for A/V sync, for example. The RateAdjuster smooths the
// error samples over time using a moving linear regression. It attempts to
// adjust the clock rate to correct the current absolute error, and the slope
// of the change in error, with the goal of maintaining as close as possible to
// 0 errors over time.
class RateAdjuster {
 public:
  struct Config {
    // The minimum interval between clock rate changes.
    base::TimeDelta rate_change_interval = base::Seconds(1);

    // How long to make the linear regression window for smoothing errors.
    base::TimeDelta linear_regression_window = base::Seconds(10);

    // The maximum current error to ignore, in microseconds.
    int64_t max_ignored_current_error = 0;

    // The maximum current absolute error that should be adjusted for in a
    // single clock rate adjustment.
    double max_current_error_correction = 2.5e-4;

    // The minimum difference between the optimal clock rate and the current
    // clock rate required to actually change the rate.
    double min_rate_change = 0;
  };

  // Called to change the clock rate. Returns the actual new clock rate. The
  // |error_slope| and |current_error| are provided for debugging/logging
  // purposes.
  using RateChangeCallback =
      base::RepeatingCallback<double(double desired_clock_rate,
                                     double error_slope,
                                     double current_error)>;

  RateAdjuster(const Config& config,
               RateChangeCallback change_clock_rate,
               double current_clock_rate);
  ~RateAdjuster();

  // Adds an error sample at the given |timestamp|. Both the |error| and the
  // |timestamp| are in microseconds. The error of some event should be
  // calculated as <actual time of event> - <desired time of event>. Timestamps
  // should increase monotonically. The clock rate will be adjusted
  // synchronously within this method via the callback, if necessary.
  void AddError(int64_t error, int64_t timestamp);

  // Reserves space for |count| error samples, to reduce memory allocation
  // during use.
  void Reserve(int count);

  // Resets to initial state.
  void Reset();

 private:
  const Config config_;
  RateChangeCallback change_clock_rate_;

  WeightedMovingLinearRegression linear_error_;
  bool initialized_ = false;
  int64_t clock_rate_start_timestamp_ = 0;
  int64_t initial_timestamp_ = 0;
  double clock_rate_error_base_ = 0.0;
  double current_clock_rate_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_RATE_ADJUSTER_H_
