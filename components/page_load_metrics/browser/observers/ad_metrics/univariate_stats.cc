// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/page_load_metrics/browser/observers/ad_metrics/univariate_stats.h"

#include <cmath>

#include "base/check_op.h"

namespace page_load_metrics {

void UnivariateStats::Accumulate(double value, double weight) {
  DCHECK_GE(weight, 0);

  double x[4] = {};

  double prev = 1;
  for (size_t i = 0; i < 4; ++i) {
    x[i] = prev * value;
    sum_x_[i] += x[i] * weight;
    prev = x[i];
  }

  total_weight_ += weight;
}

UnivariateStats::DistributionMoments UnivariateStats::CalculateStats() const {
  DistributionMoments result;

  if (std::abs(total_weight_) < 1E-7)
    return result;

  double ex[4] = {};
  double mu[4] = {};

  double prev = 1;
  for (size_t i = 0; i < 4; ++i) {
    ex[i] = sum_x_[i] / total_weight_;
    mu[i] = prev * ex[0];
    prev = mu[i];
  }

  double sigma[4] = {};

  sigma[1] = ex[1] - mu[1];
  sigma[0] = std::sqrt(sigma[1]);
  sigma[2] = sigma[1] * sigma[0];
  sigma[3] = sigma[2] * sigma[0];

  result.mean = mu[0];
  result.variance = sigma[1];

  if (std::abs(sigma[3]) < 1E-7)
    return result;

  result.skewness = (ex[2] - 3 * mu[0] * sigma[1] - mu[2]) / sigma[2];
  result.excess_kurtosis =
      (ex[3] - 4 * mu[0] * ex[2] + 6 * mu[1] * ex[1] - 3 * mu[3]) / sigma[3] -
      3;
  return result;
}

}  // namespace page_load_metrics
