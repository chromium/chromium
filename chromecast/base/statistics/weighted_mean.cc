// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/statistics/weighted_mean.h"

#include <cmath>

namespace chromecast {

WeightedMean::WeightedMean() = default;

void WeightedMean::Reset() {
  weighted_mean_ = 0.0;
  variance_sum_ = 0.0;
  sum_weights_ = 0.0;
  sum_squared_weights_ = 0.0;
}

void WeightedMean::AddDelta(double delta, double weight) {
  double old_sum_weights = sum_weights_;
  sum_weights_ += weight;
  // Use std::abs() to handle negative weights (ie, removing a sample).
  sum_squared_weights_ += weight * std::abs(weight);
  if (sum_weights_ == 0) {
    weighted_mean_ = 0;
    variance_sum_ = 0;
  } else {
    double mean_change = delta * weight / sum_weights_;
    weighted_mean_ += mean_change;
    variance_sum_ += old_sum_weights * delta * mean_change;
  }
}

}  // namespace chromecast
