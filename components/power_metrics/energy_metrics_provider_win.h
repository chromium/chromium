// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_METRICS_ENERGY_METRICS_PROVIDER_WIN_H_
#define COMPONENTS_POWER_METRICS_ENERGY_METRICS_PROVIDER_WIN_H_

#include <memory>
#include <string>
#include <vector>

#include "base/win/scoped_handle.h"
#include "components/power_metrics/energy_metrics_provider.h"

namespace power_metrics {

// EnergyMetricsProviderWin obtains energy metrics via Energy Meter Interface
// (EMI) consisting of a set of IOCTLs, which is only supported in Win11.
class EnergyMetricsProviderWin : public EnergyMetricsProvider {
 public:
  // Factory method for production instances.
  static std::unique_ptr<EnergyMetricsProviderWin> Create();

  EnergyMetricsProviderWin(const EnergyMetricsProviderWin&) = delete;
  EnergyMetricsProviderWin& operator=(const EnergyMetricsProviderWin&) = delete;

  ~EnergyMetricsProviderWin() override;

  // EnergyMetricsProvider implementation.
  std::optional<EnergyMetrics> CaptureMetrics() override;

 private:
  EnergyMetricsProviderWin();

  bool Initialize();

  // Available metric types, derived from EMI metadata.
  std::vector<std::wstring> metric_types_;
  // Used to derive energy consumption data via EMI interface.
  base::win::ScopedHandle handle_;
  bool is_initialized_ = false;
};

}  // namespace power_metrics

#endif  // COMPONENTS_POWER_METRICS_ENERGY_METRICS_PROVIDER_WIN_H_
