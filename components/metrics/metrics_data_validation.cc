// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_data_validation.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"

namespace metrics {
namespace internal {

// Used to assess the reliability of field trial data by injecting different
// levels of effects to pseudo metrics. These pseudo metrics are just mirrors of
// some existing metrics.
BASE_FEATURE(kPseudoMetricsEffectFeature,
             "UMAPseudoMetricsEffect",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The multiplicative factor to apply to all samples. Modified samples will be
// recorded in a pseudo metric alongside with the real metric.
const base::FeatureParam<double> kMultiplicativeFactor{
    &kPseudoMetricsEffectFeature, "multiplicative_factor", 1.0};

// The additive factor to apply to every samples. For time metrics, we'll add
// |additive_factor| milliseconds to samples. Modified samples will be recorded
// in a pseudo metric alongside with the real metric.
const base::FeatureParam<double> kAdditiveFactor{&kPseudoMetricsEffectFeature,
                                                 "additive_factor", 0};

}  // namespace internal

BASE_FEATURE(kNonUniformityValidationFeature,
             "UMANonUniformityLogNormal",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<double> kLogNormalMean{
    &kNonUniformityValidationFeature, "mean", 4.605};
const base::FeatureParam<double> kLogNormalDelta{
    &kNonUniformityValidationFeature, "delta", 0};
const base::FeatureParam<double> kLogNormalStdDev{
    &kNonUniformityValidationFeature, "stdDev", 1.238};

double GetPseudoMetricsSample(double sample) {
  return sample * internal::kMultiplicativeFactor.Get() +
         internal::kAdditiveFactor.Get();
}

base::TimeDelta GetPseudoMetricsSample(base::TimeDelta sample) {
  return sample * internal::kMultiplicativeFactor.Get() +
         base::Milliseconds(internal::kAdditiveFactor.Get());
}

}  // namespace metrics
