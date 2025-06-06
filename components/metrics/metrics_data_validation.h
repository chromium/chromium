// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_DATA_VALIDATION_H_
#define COMPONENTS_METRICS_METRICS_DATA_VALIDATION_H_

#include "base/feature_list.h"
#include "base/time/time.h"

// Features and functions in this file are necessary to set up artificial A / B
// experiments that help us better assess the accuracy and power of our field
// trial data. All code in this file should not have any impact on client's
// experience.
namespace metrics {

// Only used for testing.
namespace internal {
BASE_DECLARE_FEATURE(kPseudoMetricsEffectFeature);
}  // namespace internal

// In order to assess if we're able to accurately detect a statistically
// significant difference in our field trial data, we set up pseudo metrics for
// some of our key metrics. Values of these pseudo metrics are the linear
// transformation (ax + b) of real values (x). The multiplicative factor (a) and
// additive factor (b) are controlled by field trial experiments.
//
// Returns the sample value for a pseudo metric given the |sample| from the real
// metric and the assigned field trial group. The input type is double because
// we don't want to lose precision before applying transformation.
double GetPseudoMetricsSample(double sample);

// Returns the TimeDelta for a pseudo metric given the |sample| from the real
// metric and the assigned field trial group. The unit of the additive factor
// (b) is milliseconds.
base::TimeDelta GetPseudoMetricsSample(base::TimeDelta sample);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_DATA_VALIDATION_H_
