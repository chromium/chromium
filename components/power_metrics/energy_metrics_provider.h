// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_METRICS_ENERGY_METRICS_PROVIDER_H_
#define COMPONENTS_POWER_METRICS_ENERGY_METRICS_PROVIDER_H_

#include <cstdint>
#include <memory>
#include <optional>

namespace power_metrics {

// EnergyMetricsProvider provides system-wide energy consumption metrics.
class EnergyMetricsProvider {
 public:
  struct EnergyMetrics {
    // The absolute energy of the whole processor package.
    uint64_t package_nanojoules;
    // The absolute energy of all of the cores.
    uint64_t cpu_nanojoules;
    // The absolute energy of the uncore (usually the integrated GPU, only
    // available in client CPUs).
    uint64_t gpu_nanojoules;
    // The absolute energy of the DRAM (only available in server CPUs).
    uint64_t dram_nanojoules;
    // The absolute energy of the entire system.
    uint64_t psys_nanojoules;
    // The following metrics are emitted by AMD processors.
    // We don't know what they measure exactly.
    uint64_t vdd_nanojoules;
    uint64_t soc_nanojoules;
    uint64_t socket_nanojoules;
    uint64_t apu_nanojoules;
  };

  // Returns nullptr if no suitable implementation exists.
  static std::unique_ptr<EnergyMetricsProvider> Create();

  EnergyMetricsProvider(const EnergyMetricsProvider&) = delete;
  EnergyMetricsProvider& operator=(const EnergyMetricsProvider&) = delete;

  virtual ~EnergyMetricsProvider();

  // Used to capture energy consumption metrics. It returns nullopt if not
  // available on the current platform.
  virtual std::optional<EnergyMetrics> CaptureMetrics() = 0;

 protected:
  // The constructor is intentionally only exposed to subclasses. Production
  // code must use the Create() factory method.
  EnergyMetricsProvider();
};

}  // namespace power_metrics

#endif  // COMPONENTS_POWER_METRICS_ENERGY_METRICS_PROVIDER_H_
