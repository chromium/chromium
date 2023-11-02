// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_STATISTICS_WEIGHTED_MEAN_H_
#define CHROMECAST_BASE_STATISTICS_WEIGHTED_MEAN_H_

#include <stdint.h>

namespace chromecast {

// Calculates the weighted mean (and variance) of a set of values. Values can be
// added to or removed from the mean.
class WeightedMean {
 public:
  WeightedMean();

  double weighted_mean() const { return weighted_mean_; }
  // The weighted variance should be calculated as variance_sum()/sum_weights().
  double variance_sum() const { return variance_sum_; }
  double sum_weights() const { return sum_weights_; }
  double sum_squared_weights() const { return sum_squared_weights_; }

  // Adds |value| to the mean if |weight| is positive. Removes |value| from
  // the mean if |weight| is negative. Has no effect if |weight| is 0.
  template <typename T>
  void AddSample(T value, double weight) {
    AddDelta(value - weighted_mean_, weight);
  }

  // Resets to initial state.
  void Reset();

 private:
  void AddDelta(double delta, double weight);

  double weighted_mean_ = 0.0;
  double variance_sum_ = 0.0;
  double sum_weights_ = 0.0;
  double sum_squared_weights_ = 0.0;
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_STATISTICS_WEIGHTED_MEAN_H_
