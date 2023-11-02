// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_STATISTICS_WEIGHTED_MOVING_AVERAGE_H_
#define CHROMECAST_BASE_STATISTICS_WEIGHTED_MOVING_AVERAGE_H_

#include <stdint.h>
#include <deque>

#include "chromecast/base/statistics/weighted_mean.h"

namespace chromecast {

// Calculates the weighted moving average of recent points. The points
// do not need to be evenly distributed on the X axis, but the X coordinate
// is assumed to be generally increasing.
//
// Whenever a new sample is added using AddSample(), old samples whose
// x coordinates are farther than |max_x_range_| from the new sample's
// x coordinate will be removed from the average. Note that |max_x_range_|
// must be non-negative.
class WeightedMovingAverage {
 public:
  explicit WeightedMovingAverage(int64_t max_x_range);

  WeightedMovingAverage(const WeightedMovingAverage&) = delete;
  WeightedMovingAverage& operator=(const WeightedMovingAverage&) = delete;

  ~WeightedMovingAverage();

  int64_t max_x_range() const { return max_x_range_; }
  // Returns the current number of samples that are in the weighted average.
  size_t num_samples() const { return samples_.size(); }

  // Adds an (x, y) sample with the provided weight to the average.
  // |weight| should be non-negative.
  void AddSample(int64_t x, int64_t y, double weight);

  // Gets the current average and standard error.
  // Returns |true| if the average exists, |false| otherwise. If the average
  // does not exist, |average| and |error| are not modified.
  bool Average(int64_t* average, double* error) const;

  // Clears all current samples from the moving average.
  void Clear();

 private:
  struct Sample {
    int64_t x;
    int64_t y;
    double weight;
  };

  const int64_t max_x_range_;
  std::deque<Sample> samples_;
  WeightedMean mean_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_STATISTICS_WEIGHTED_MOVING_AVERAGE_H_
