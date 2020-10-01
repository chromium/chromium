// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cros_healthd/fake_cros_healthd_service.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {
namespace cros_healthd {

FakeCrosHealthdService::FakeCrosHealthdService() = default;
FakeCrosHealthdService::~FakeCrosHealthdService() = default;

void FakeCrosHealthdService::GetProbeService(
    mojom::CrosHealthdProbeServiceRequest service) {
  probe_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthdService::GetDiagnosticsService(
    mojom::CrosHealthdDiagnosticsServiceRequest service) {
  diagnostics_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthdService::GetEventService(
    mojom::CrosHealthdEventServiceRequest service) {
  event_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthdService::SendNetworkHealthService(
    mojo::PendingRemote<chromeos::network_health::mojom::NetworkHealthService>
        remote) {
  network_health_remote_.Bind(std::move(remote));
}

void FakeCrosHealthdService::SendNetworkDiagnosticsRoutines(
    mojo::PendingRemote<
        chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
        network_diagnostics_routines) {
  network_diagnostics_routines_.Bind(std::move(network_diagnostics_routines));
}

void FakeCrosHealthdService::GetAvailableRoutines(
    GetAvailableRoutinesCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), available_routines_),
      callback_delay_);
}

void FakeCrosHealthdService::GetRoutineUpdate(
    int32_t id,
    mojom::DiagnosticRoutineCommandEnum command,
    bool include_output,
    GetRoutineUpdateCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          mojom::RoutineUpdate::New(
              routine_update_response_->progress_percent,
              std::move(routine_update_response_->output),
              std::move(routine_update_response_->routine_update_union))),
      callback_delay_);
}

void FakeCrosHealthdService::RunUrandomRoutine(
    uint32_t length_seconds,
    RunUrandomRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunBatteryCapacityRoutine(
    uint32_t low_mah,
    uint32_t high_mah,
    RunBatteryCapacityRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunBatteryHealthRoutine(
    uint32_t maximum_cycle_count,
    uint32_t percent_battery_wear_allowed,
    RunBatteryHealthRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunSmartctlCheckRoutine(
    RunSmartctlCheckRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunAcPowerRoutine(
    mojom::AcPowerStatusEnum expected_status,
    const base::Optional<std::string>& expected_power_type,
    RunAcPowerRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunCpuCacheRoutine(
    uint32_t length_seconds,
    RunCpuCacheRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunCpuStressRoutine(
    uint32_t length_seconds,
    RunCpuStressRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunFloatingPointAccuracyRoutine(
    uint32_t length_seconds,
    RunFloatingPointAccuracyRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunNvmeWearLevelRoutine(
    uint32_t wear_level_threshold,
    RunNvmeWearLevelRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunNvmeSelfTestRoutine(
    mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
    RunNvmeSelfTestRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunDiskReadRoutine(
    mojom::DiskReadRoutineTypeEnum type,
    uint32_t length_seconds,
    uint32_t file_size_mb,
    RunDiskReadRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunPrimeSearchRoutine(
    uint32_t length_seconds,
    uint64_t max_num,
    RunPrimeSearchRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunBatteryDischargeRoutine(
    uint32_t length_seconds,
    uint32_t maximum_discharge_percent_allowed,
    RunBatteryDischargeRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunBatteryChargeRoutine(
    uint32_t length_seconds,
    uint32_t minimum_charge_percent_required,
    RunBatteryChargeRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunMemoryRoutine(
    RunMemoryRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunLanConnectivityRoutine(
    RunLanConnectivityRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunSignalStrengthRoutine(
    RunSignalStrengthRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::AddBluetoothObserver(
    mojom::CrosHealthdBluetoothObserverPtr observer) {
  bluetooth_observers_.Add(observer.PassInterface());
}

void FakeCrosHealthdService::AddLidObserver(
    mojom::CrosHealthdLidObserverPtr observer) {
  lid_observers_.Add(observer.PassInterface());
}

void FakeCrosHealthdService::AddPowerObserver(
    mojom::CrosHealthdPowerObserverPtr observer) {
  power_observers_.Add(observer.PassInterface());
}

void FakeCrosHealthdService::ProbeTelemetryInfo(
    const std::vector<mojom::ProbeCategoryEnum>& categories,
    ProbeTelemetryInfoCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), telemetry_response_info_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::ProbeProcessInfo(
    const uint32_t process_id,
    ProbeProcessInfoCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), process_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::SetAvailableRoutinesForTesting(
    const std::vector<mojom::DiagnosticRoutineEnum>& available_routines) {
  available_routines_ = available_routines;
}

void FakeCrosHealthdService::SetRunRoutineResponseForTesting(
    mojom::RunRoutineResponsePtr& response) {
  run_routine_response_.Swap(&response);
}

void FakeCrosHealthdService::SetGetRoutineUpdateResponseForTesting(
    mojom::RoutineUpdatePtr& response) {
  routine_update_response_.Swap(&response);
}

void FakeCrosHealthdService::SetProbeTelemetryInfoResponseForTesting(
    mojom::TelemetryInfoPtr& response_info) {
  telemetry_response_info_.Swap(&response_info);
}

void FakeCrosHealthdService::SetProbeProcessInfoResponseForTesting(
    mojom::ProcessResultPtr& result) {
  process_response_.Swap(&result);
}

void FakeCrosHealthdService::SetCallbackDelay(base::TimeDelta delay) {
  callback_delay_ = delay;
}

void FakeCrosHealthdService::EmitAcInsertedEventForTesting() {
  for (auto& observer : power_observers_)
    observer->OnAcInserted();
}

void FakeCrosHealthdService::EmitAdapterAddedEventForTesting() {
  for (auto& observer : bluetooth_observers_)
    observer->OnAdapterAdded();
}

void FakeCrosHealthdService::EmitLidClosedEventForTesting() {
  for (auto& observer : lid_observers_)
    observer->OnLidClosed();
}

void FakeCrosHealthdService::EmitLidOpenedEventForTesting() {
  for (auto& observer : lid_observers_)
    observer->OnLidOpened();
}

void FakeCrosHealthdService::RequestNetworkHealthForTesting(
    chromeos::network_health::mojom::NetworkHealthService::
        GetHealthSnapshotCallback callback) {
  network_health_remote_->GetHealthSnapshot(std::move(callback));
}

void FakeCrosHealthdService::RunLanConnectivityRoutineForTesting(
    chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines::
        LanConnectivityCallback callback) {
  network_diagnostics_routines_->LanConnectivity(std::move(callback));
}

}  // namespace cros_healthd
}  // namespace chromeos
