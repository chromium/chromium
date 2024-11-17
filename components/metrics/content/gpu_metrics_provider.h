// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CONTENT_GPU_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CONTENT_GPU_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace metrics {

// GPUMetricsProvider provides GPU-related metrics.
class GPUMetricsProvider : public MetricsProvider {
 public:
  GPUMetricsProvider();

  GPUMetricsProvider(const GPUMetricsProvider&) = delete;
  GPUMetricsProvider& operator=(const GPUMetricsProvider&) = delete;

  ~GPUMetricsProvider() override;

  // MetricsProvider:
  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override;
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CONTENT_GPU_METRICS_PROVIDER_H_
