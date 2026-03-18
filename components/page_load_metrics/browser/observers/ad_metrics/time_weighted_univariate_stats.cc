// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/ad_metrics/time_weighted_univariate_stats.h"

#include <array>
#include <cmath>

#include "base/check_op.h"
#include "base/time/default_tick_clock.h"

namespace page_load_metrics {

TimeWeightedUnivariateStats::TimeWeightedUnivariateStats(
    const base::TickClock* clock)
    : clock_(clock ? clock : base::DefaultTickClock::GetInstance()),
      last_time_(clock_->NowTicks()) {}

void TimeWeightedUnivariateStats::AccumulateOutstanding() {
  if (is_paused_) {
    return;
  }

  base::TimeTicks now = clock_->NowTicks();
  base::TimeDelta elapsed = now - last_time_;

  if (elapsed.is_positive()) {
    double weight = elapsed.InMicrosecondsF();
    std::array<double, 4> x = {};
    double prev = 1;

    for (size_t i = 0; i < 4; ++i) {
      x[i] = prev * last_sample_;
      sum_x_[i] += x[i] * weight;
      prev = x[i];
    }
    total_weight_ += weight;
  }

  last_time_ = now;
}

void TimeWeightedUnivariateStats::AddSample(double sample) {
  AccumulateOutstanding();
  last_sample_ = sample;
  maximum_value_ = std::max(maximum_value_.value_or(sample), sample);
}

void TimeWeightedUnivariateStats::Pause() {
  AccumulateOutstanding();
  is_paused_ = true;
}

void TimeWeightedUnivariateStats::Resume() {
  if (!is_paused_) {
    return;
  }
  is_paused_ = false;
  last_time_ = clock_->NowTicks();
}

TimeWeightedUnivariateStats::DistributionMoments
TimeWeightedUnivariateStats::CalculateStats() {
  AccumulateOutstanding();

  DistributionMoments result;

  if (std::abs(total_weight_) < 1E-7) {
    return result;
  }

  std::array<double, 4> ex = {};
  std::array<double, 4> mu = {};

  double prev = 1;
  for (size_t i = 0; i < 4; ++i) {
    ex[i] = sum_x_[i] / total_weight_;
    mu[i] = prev * ex[0];
    prev = mu[i];
  }

  std::array<double, 4> sigma = {};

  sigma[1] = ex[1] - mu[1];
  sigma[0] = std::sqrt(sigma[1]);
  sigma[2] = sigma[1] * sigma[0];
  sigma[3] = sigma[2] * sigma[0];

  result.mean = mu[0];
  result.variance = sigma[1];

  if (std::abs(sigma[3]) < 1E-7) {
    return result;
  }

  result.skewness = (ex[2] - 3 * mu[0] * sigma[1] - mu[2]) / sigma[2];
  result.excess_kurtosis =
      (ex[3] - 4 * mu[0] * ex[2] + 6 * mu[1] * ex[1] - 3 * mu[3]) / sigma[3] -
      3;
  return result;
}

}  // namespace page_load_metrics
