// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CONTENT_GPU_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CONTENT_GPU_METRICS_PROVIDER_H_

#include "components/metrics/cached_metrics_provider.h"

namespace gpu {
struct GPUInfo;
}

namespace metrics {

class SystemProfileProto;

// GPUMetricsProvider provides GPU-related metrics.
class GPUMetricsProvider : public CachedMetricsProvider {
 public:
  GPUMetricsProvider();

  GPUMetricsProvider(const GPUMetricsProvider&) = delete;
  GPUMetricsProvider& operator=(const GPUMetricsProvider&) = delete;

  ~GPUMetricsProvider() override;

  // MetricsProvider:
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override;

 protected:
  // CachedMetricsProvider:
  void ProvideSystemProfileMetricsFromCache(
      const SystemProfileProto& cached_system_profile,
      SystemProfileProto* system_profile_proto) override;
  void ProvideSystemProfileMetricsNormal(
      SystemProfileProto* system_profile_proto) override;

  // Virtual for testing.
  virtual gpu::GPUInfo GetGPUInfo() const;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CONTENT_GPU_METRICS_PROVIDER_H_
