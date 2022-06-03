// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/statistics/weighted_moving_average.h"

#include <math.h>

#include "base/check_op.h"

namespace chromecast {

WeightedMovingAverage::WeightedMovingAverage(int64_t max_x_range)
    : max_x_range_(max_x_range) {
  DCHECK_GE(max_x_range_, 0);
}

WeightedMovingAverage::~WeightedMovingAverage() {}

void WeightedMovingAverage::AddSample(int64_t x, int64_t y, double weight) {
  DCHECK_GE(weight, 0);
  if (!samples_.empty())
    DCHECK_GE(x, samples_.back().x);

  Sample sample = {x, y, weight};
  samples_.push_back(sample);
  mean_.AddSample(y, weight);

  // Remove old samples.
  while (x - samples_.front().x > max_x_range_) {
    const Sample& old_sample = samples_.front();
    mean_.AddSample(old_sample.y, -old_sample.weight);
    samples_.pop_front();
  }
  DCHECK(!samples_.empty());
}

bool WeightedMovingAverage::Average(int64_t* average, double* error) const {
  if (samples_.empty() || mean_.sum_weights() == 0)
    return false;

  *average = static_cast<int64_t>(round(mean_.weighted_mean()));

  const double effective_sample_size =
      mean_.sum_weights() * mean_.sum_weights() / mean_.sum_squared_weights();
  const double variance = mean_.variance_sum() / mean_.sum_weights();
  *error = sqrt(variance / effective_sample_size);
  return true;
}

void WeightedMovingAverage::Clear() {
  samples_.clear();
  mean_ = WeightedMean();
}

}  // namespace chromecast
