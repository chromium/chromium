// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_METRICS_ENERGY_METRICS_PROVIDER_LINUX_H_
#define COMPONENTS_POWER_METRICS_ENERGY_METRICS_PROVIDER_LINUX_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "components/power_metrics/energy_metrics_provider.h"

namespace power_metrics {

// EnergyMetricsProviderLinux can only work on platforms with Intel Processor
// with RAPL interface. It also requires at least Linux 3.14 and
// /proc/sys/kernel/perf_event_paranoid < 1, which grants permission to read
// perf event.
class EnergyMetricsProviderLinux : public EnergyMetricsProvider {
 public:
  // A PowerEvent corresponds to a metric, which includes the metric type, the
  // scale from ticks to joules and the file descriptor for reading perf data.
  struct PowerEvent {
    std::string metric_type;
    double scale;
    base::ScopedFD fd;

    PowerEvent(std::string metric_type, double scale, base::ScopedFD fd);
    ~PowerEvent();

    PowerEvent(PowerEvent&& other);
    PowerEvent& operator=(PowerEvent&& other);
  };

  // Factory method for production instances.
  static std::unique_ptr<EnergyMetricsProviderLinux> Create();

  EnergyMetricsProviderLinux(const EnergyMetricsProviderLinux&) = delete;
  EnergyMetricsProviderLinux& operator=(const EnergyMetricsProviderLinux&) =
      delete;

  ~EnergyMetricsProviderLinux() override;

  // EnergyMetricsProvider implementation.
  std::optional<EnergyMetrics> CaptureMetrics() override;

 private:
  EnergyMetricsProviderLinux();

  bool Initialize();

  // Used to derive energy consumption data via perf event.
  std::vector<PowerEvent> events_;
  bool is_initialized_ = false;
};

}  // namespace power_metrics

#endif  // COMPONENTS_POWER_METRICS_ENERGY_METRICS_PROVIDER_LINUX_H_
