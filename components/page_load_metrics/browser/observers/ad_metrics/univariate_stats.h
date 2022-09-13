// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_UNIVARIATE_STATS_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_UNIVARIATE_STATS_H_

namespace page_load_metrics {

// Tracks a variable to be able to calculate its first four moments of
// distribution (i.e. mean, variance, skewness, and kurtosis).
// http://www.itl.nist.gov/div898/handbook/eda/section3/eda35b.htm
class UnivariateStats {
 public:
  struct DistributionMoments {
    double mean = 0;
    double variance = 0;
    double skewness = 0;

    // Default to -3 as we are measuring "excess kurtosis".
    double excess_kurtosis = -3;
  };

  // Update the derived statistics given the new data point.
  void Accumulate(double value, double weight);

  // Calculate the population distribution mean, variance, skewness, and
  // kurtosis of the data observed by `Accumulate()`. If `total_weight_` is too
  // small, return the default `DistributionMoments`; if the variance is too
  // small, only `mean` and `variance` will be set, and `skewness` and
  // `kurtosis` will be their default values.
  DistributionMoments CalculateStats() const;

 private:
  double sum_x_[4] = {};
  double total_weight_ = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_UNIVARIATE_STATS_H_
