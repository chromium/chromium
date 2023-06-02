// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_metrics/energy_metrics_provider.h"

#include "build/build_config.h"
#if BUILDFLAG(IS_WIN)
#include "components/power_metrics/energy_metrics_provider_win.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/power_metrics/energy_metrics_provider_linux.h"
#endif  // BUILDFLAG(IS_WIN)

namespace power_metrics {

EnergyMetricsProvider::EnergyMetricsProvider() = default;
EnergyMetricsProvider::~EnergyMetricsProvider() = default;

// static
std::unique_ptr<EnergyMetricsProvider> EnergyMetricsProvider::Create() {
#if BUILDFLAG(IS_WIN)
  return EnergyMetricsProviderWin::Create();
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return EnergyMetricsProviderLinux::Create();
#else
  return nullptr;
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace power_metrics
