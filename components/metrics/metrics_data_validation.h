// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_DATA_VALIDATION_H_
#define COMPONENTS_METRICS_METRICS_DATA_VALIDATION_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
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

// Used to assess the reliability of field trial data by sending artificial
// non-uniform data drawn from a log normal distribution.
BASE_DECLARE_FEATURE(kNonUniformityValidationFeature);

// The parameters for the log normal distribution. They refer to the default
// mean, the delta that would be applied to the default mean (the actual mean
// equals mean + log(1 + delta)) and the standard deviation of the distribution
// that's being generated. These parameters are carefully calculated so that
// ~0.01% of data drawn from the distribution would fall in the underflow bucket
// and ~0.01% of data in the overflow bucket. And they also leave us enough
// wiggle room to shift mean using delta in experiments without losing precision
// badly because of data in the overflow bucket.
//
// The way we get these numbers are based on the following calculation:
// u := the lower threshold for the overflow bucket (in this case, 10000).
// l := the upper threshold for the smallest bucket (in this case, 1).
// p := the probability that an observation will fall in the highest bucket (in
//   this case, 0.01%) and also the probability that an observation will fall in
//   the lowest bucket.
//
// mean = (log(u) + log(l)) / 2
// sd = (log(u) - log(l)) / (2 * qnorm(1-p))
//
// At this point, experiments should only control the delta but not mean and
// stdDev. Putting them in feature params so that we can configure them from the
// server side if we want.
extern const base::FeatureParam<double> kLogNormalMean;
extern const base::FeatureParam<double> kLogNormalDelta;
extern const base::FeatureParam<double> kLogNormalStdDev;

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
