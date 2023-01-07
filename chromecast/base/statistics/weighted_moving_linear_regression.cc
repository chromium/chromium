// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/statistics/weighted_moving_linear_regression.h"

#include <math.h>
#include <algorithm>

#include "base/check_op.h"
#include "base/logging.h"

namespace chromecast {

WeightedMovingLinearRegression::WeightedMovingLinearRegression(
    int64_t max_x_range)
    : max_x_range_(max_x_range) {
  DCHECK_GE(max_x_range_, 0);
}

WeightedMovingLinearRegression::~WeightedMovingLinearRegression() = default;

void WeightedMovingLinearRegression::Reserve(int count) {
  Sample sample = {0, 0, 0};
  samples_.insert(samples_.end(), count, sample);
  samples_.erase(samples_.end() - count, samples_.end());
}

void WeightedMovingLinearRegression::Reset() {
  x_mean_.Reset();
  y_mean_.Reset();
  covariance_ = 0.0;
  samples_.clear();
  slope_ = 0.0;
  slope_variance_ = 0.0;
  intercept_variance_ = 0.0;

  has_estimate_ = false;
}

void WeightedMovingLinearRegression::AddSample(int64_t x,
                                               int64_t y,
                                               double weight) {
  DCHECK_GE(weight, 0);
  if (!samples_.empty())
    DCHECK_GE(x, samples_.back().x);

  UpdateSet(x, y, weight);
  Sample sample = {x, y, weight};
  samples_.push_back(sample);

  // Remove old samples.
  while (x - samples_.front().x > max_x_range_) {
    const Sample& old_sample = samples_.front();
    UpdateSet(old_sample.x, old_sample.y, -old_sample.weight);
    samples_.pop_front();
  }
  DCHECK(!samples_.empty());

  if (samples_.size() <= 2 || x_mean_.sum_weights() == 0 ||
      x_mean_.variance_sum() == 0) {
    has_estimate_ = false;
    return;
  }

  slope_ = covariance_ / x_mean_.variance_sum();

  double residual_sum_squares =
      (covariance_ * covariance_) / x_mean_.variance_sum();
  double mean_squared_error =
      (y_mean_.variance_sum() - residual_sum_squares) / (samples_.size() - 2);

  slope_variance_ = std::max(0.0, mean_squared_error / x_mean_.variance_sum());
  intercept_variance_ = std::max(
      0.0, (slope_variance_ * x_mean_.variance_sum()) / x_mean_.sum_weights());

  has_estimate_ = true;
}

bool WeightedMovingLinearRegression::EstimateY(int64_t x,
                                               int64_t* y,
                                               double* error) const {
  if (!has_estimate_)
    return false;

  double x_diff = x - x_mean_.weighted_mean();
  double y_estimate = y_mean_.weighted_mean() + (slope_ * x_diff);

  *y = static_cast<int64_t>(round(y_estimate));
  *error = sqrt(intercept_variance_ + (slope_variance_ * x_diff * x_diff));
  return true;
}

bool WeightedMovingLinearRegression::EstimateSlope(double* slope,
                                                   double* error) const {
  if (!has_estimate_)
    return false;

  *slope = slope_;
  *error = sqrt(slope_variance_);
  return true;
}

void WeightedMovingLinearRegression::UpdateSet(int64_t x,
                                               int64_t y,
                                               double weight) {
  double old_y_mean = y_mean_.weighted_mean();
  x_mean_.AddSample(x, weight);
  y_mean_.AddSample(y, weight);
  covariance_ += weight * (x - x_mean_.weighted_mean()) * (y - old_y_mean);
}

void WeightedMovingLinearRegression::DumpSamples() const {
  for (auto sample : samples_) {
    LOG(INFO) << "x, y, weight: " << sample.x << " " << sample.y << " "
              << sample.weight;
  }
}

}  // namespace chromecast
