// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/sampling_metrics_provider.h"

#include "base/metrics/histogram_base.h"
#include "base/metrics/sparse_histogram.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {

SamplingMetricsProvider::SamplingMetricsProvider(int sampling_rate_per_mille)
    : sampling_rate_per_mille_(sampling_rate_per_mille) {}

void SamplingMetricsProvider::ProvideStabilityMetrics(
    SystemProfileProto* system_profile_proto) {
  base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
      "UMA.SamplingRatePerMille",
      base::HistogramBase::kUmaStabilityHistogramFlag);
  histogram->Add(sampling_rate_per_mille_);
}

}  // namespace metrics
