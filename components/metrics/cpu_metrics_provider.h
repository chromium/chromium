// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CPU_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CPU_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace metrics {

// CPUMetricsProvider adds CPU Info in the system profile. These include
// CPU vendor information, cpu cores, etc. This doesn't provide CPU usage
// information.
class CPUMetricsProvider : public MetricsProvider {
 public:
  CPUMetricsProvider();

  CPUMetricsProvider(const CPUMetricsProvider&) = delete;
  CPUMetricsProvider& operator=(const CPUMetricsProvider&) = delete;

  ~CPUMetricsProvider() override;

  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CPU_METRICS_PROVIDER_H_
