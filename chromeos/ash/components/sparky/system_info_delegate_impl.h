// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SPARKY_SYSTEM_INFO_DELEGATE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_SPARKY_SYSTEM_INFO_DELEGATE_IMPL_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/system_info/battery_health.h"
#include "chromeos/ash/components/system_info/cpu_data.h"
#include "chromeos/ash/components/system_info/cpu_usage_data.h"
#include "chromeos/ash/components/system_info/memory_data.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "components/manta/sparky/system_info_delegate.h"

namespace sparky {

// This class extracts System Information requested by the Sparky Provider. It
// is able to return information on the current CPU, memory and battery status.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SPARKY) SystemInfoDelegateImpl
    : public manta::SystemInfoDelegate {
 public:
  SystemInfoDelegateImpl();
  SystemInfoDelegateImpl(const SystemInfoDelegateImpl&) = delete;
  SystemInfoDelegateImpl& operator=(const SystemInfoDelegateImpl&) = delete;

  ~SystemInfoDelegateImpl() override;

  // `manta::SystemInfoDelegate` overrides:
  void ObtainDiagnostics(
      const std::vector<manta::Diagnostics>& diagnostics,
      manta::DiagnosticsDataCallback diagnostics_callback) override;

 private:
  // Called once each of the diagnostics has been collected. Will run the
  // callback once all of the requested information has been collected.
  void OnDiagnosticsUpdated();
  // If there is an error with collected any of the diagnostics information, the
  // callback will be returned with a null pointer.
  void ReturnWithNullptr();
  // Binds the CrOS Healthd Probe Service which is required to request any
  // system information.
  void BindCrosHealthdProbeServiceIfNecessary();
  void OnProbeServiceDisconnect();

  // Functions to update Diagnostics information.
  void UpdateMemoryUsage();
  void UpdateBatteryInfo();
  void UpdateCpuUsage();

  // Functions which are called once the diagnostics information has been
  // collected. It formats the returned response and then calls back
  // OnDiagnosticsUpdated to potentially return back this information.
  void OnMemoryUsageUpdated(
      ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr);
  void OnBatteryInfoUpdated(
      ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr);
  void OnCpuUsageUpdated(ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  bool diagnostics_error_{false};
  manta::DiagnosticsDataCallback diagnostics_callback_;
  std::vector<manta::Diagnostics> diagnostics_requested_;
  std::unique_ptr<manta::BatteryData> battery_data_;
  std::unique_ptr<manta::CpuData> cpu_data_;
  system_info::CpuUsageData previous_cpu_usage_data_{
      system_info::CpuUsageData()};
  std::unique_ptr<manta::MemoryData> memory_data_;
  // A timer is used to request CPU usage info as the first request for CPU data
  // results in a spike in CPU usage which leads to an inflated value.
  std::unique_ptr<base::RepeatingTimer> cpu_usage_timer_;
  // The number of times which CPU usage is calculated before being returned.
  int cpu_refreshes_left_ = 2;
  base::WeakPtrFactory<SystemInfoDelegateImpl> weak_factory_{this};
};

}  // namespace sparky

#endif  // CHROMEOS_ASH_COMPONENTS_SPARKY_SYSTEM_INFO_DELEGATE_IMPL_H_
