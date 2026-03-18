// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_TIME_WEIGHTED_UNIVARIATE_STATS_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_TIME_WEIGHTED_UNIVARIATE_STATS_H_

#include <array>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace page_load_metrics {

// Tracks a variable to calculate its maximum value and the first four moments
// of distribution (i.e. mean, variance, skewness, and kurtosis).
// http://www.itl.nist.gov/div898/handbook/eda/section3/eda35b.htm
//
// This calculates time-weighted statistics: each sample is weighted by the
// amount of time that elapses between when it is recorded and when the next
// sample is added (or the stats are calculated).
//
// Note: For the maximum value, we explicitly return std::nullopt if no value
// is seen. However, for calculating moments, we treat the starting value as 0.
// This inconsistency arises from legacy decisions and should be cleaned up
// (crbug.com/493315764).
class TimeWeightedUnivariateStats {
 public:
  struct DistributionMoments {
    double mean = 0;
    double variance = 0;
    double skewness = 0;

    // Default to -3 as we are measuring "excess kurtosis".
    double excess_kurtosis = -3;
  };

  explicit TimeWeightedUnivariateStats(const base::TickClock* clock = nullptr);
  ~TimeWeightedUnivariateStats() = default;

  // Adds a new sample to the distribution. The sample's ultimate weight will be
  // the time between this being called and the next call to `AddSample` or
  // `CalculateStats`.
  void AddSample(double sample);

  // Pauses the time accumulation. Outstanding time for the current sample is
  // accumulated before pausing.
  void Pause();

  // Resumes the time accumulation.
  void Resume();

  // Calculate the population distribution mean, variance, skewness, and
  // kurtosis of the observed data. This flushes any unaccumulated time
  // for the current value into the stats.
  DistributionMoments CalculateStats();

  std::optional<double> maximum_value() const { return maximum_value_; }

 private:
  // Accumulates the currently held `last_sample_` over the elapsed time since
  // `last_time_`, then advances `last_time_`.
  void AccumulateOutstanding();

  std::array<double, 4> sum_x_ = {};
  double total_weight_ = 0;
  double last_sample_ = 0;
  std::optional<double> maximum_value_;

  raw_ptr<const base::TickClock> clock_;
  base::TimeTicks last_time_;
  bool is_paused_ = false;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_TIME_WEIGHTED_UNIVARIATE_STATS_H_
