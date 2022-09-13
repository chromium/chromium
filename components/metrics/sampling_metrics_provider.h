// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SAMPLING_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_SAMPLING_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace metrics {

// Provides metrics related to sampling of metrics reporting clients. In
// particular, the rate at which clients are sampled.
class SamplingMetricsProvider : public MetricsProvider {
 public:
  // |sampling_rate_per_mille| is the number of clients per 1000 that are in the
  // sample.
  explicit SamplingMetricsProvider(int sampling_rate_per_mille);
  ~SamplingMetricsProvider() override = default;
  SamplingMetricsProvider(const SamplingMetricsProvider&) = delete;
  SamplingMetricsProvider operator=(const SamplingMetricsProvider&) = delete;

 private:
  // MetricsProvider:
  void ProvideStabilityMetrics(
      SystemProfileProto* system_profile_proto) override;

  int sampling_rate_per_mille_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_SAMPLING_METRICS_PROVIDER_H_
