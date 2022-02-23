// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cros_healthd/fake_cros_healthd_service.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace {

// Will destroy `handle` if it's not a valid platform handle.
mojo::ScopedHandle CloneScopedHandle(mojo::ScopedHandle* handle) {
  DCHECK(handle);
  if (!handle->is_valid()) {
    return mojo::ScopedHandle();
  }
  mojo::PlatformHandle platform_handle =
      mojo::UnwrapPlatformHandle(std::move(*handle));
  DCHECK(platform_handle.is_valid());
  *handle = mojo::WrapPlatformHandle(platform_handle.Clone());
  return mojo::WrapPlatformHandle(std::move(platform_handle));
}

}  // namespace

namespace chromeos {
namespace cros_healthd {

FakeCrosHealthdService::RoutineUpdateParams::RoutineUpdateParams(
    int32_t id,
    mojom::DiagnosticRoutineCommandEnum command,
    bool include_output)
    : id(id), command(command), include_output(include_output) {}

FakeCrosHealthdService::FakeCrosHealthdService() = default;
FakeCrosHealthdService::~FakeCrosHealthdService() = default;

void FakeCrosHealthdService::GetProbeService(
    mojo::PendingReceiver<mojom::CrosHealthdProbeService> service) {
  probe_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthdService::GetDiagnosticsService(
    mojo::PendingReceiver<mojom::CrosHealthdDiagnosticsService> service) {
  diagnostics_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthdService::GetEventService(
    mojo::PendingReceiver<mojom::CrosHealthdEventService> service) {
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

void FakeCrosHealthdService::GetSystemService(
    mojo::PendingReceiver<mojom::CrosHealthdSystemService> service) {
  system_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthdService::GetServiceStatus(
    GetServiceStatusCallback callback) {
  auto response = mojom::ServiceStatus::New();
  response->network_health_bound = network_health_remote_.is_bound();
  response->network_diagnostics_bound = network_health_remote_.is_bound();
  std::move(callback).Run(std::move(response));
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
  routine_update_params_ =
      FakeCrosHealthdService::RoutineUpdateParams(id, command, include_output);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          mojom::RoutineUpdate::New(
              routine_update_response_->progress_percent,
              CloneScopedHandle(&routine_update_response_->output),
              routine_update_response_->routine_update_union.Clone())),
      callback_delay_);
}

void FakeCrosHealthdService::RunUrandomRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunUrandomRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kUrandom;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunBatteryCapacityRoutine(
    RunBatteryCapacityRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBatteryCapacity;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunBatteryHealthRoutine(
    RunBatteryHealthRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBatteryHealth;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunSmartctlCheckRoutine(
    RunSmartctlCheckRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kSmartctlCheck;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunAcPowerRoutine(
    mojom::AcPowerStatusEnum expected_status,
    const absl::optional<std::string>& expected_power_type,
    RunAcPowerRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kAcPower;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunCpuCacheRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunCpuCacheRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kCpuCache;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunCpuStressRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunCpuStressRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kCpuStress;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunFloatingPointAccuracyRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunFloatingPointAccuracyRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunNvmeWearLevelRoutine(
    uint32_t wear_level_threshold,
    RunNvmeWearLevelRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kNvmeWearLevel;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunNvmeSelfTestRoutine(
    mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
    RunNvmeSelfTestRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kNvmeSelfTest;
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
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kDiskRead;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunPrimeSearchRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunPrimeSearchRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kPrimeSearch;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunBatteryDischargeRoutine(
    uint32_t length_seconds,
    uint32_t maximum_discharge_percent_allowed,
    RunBatteryDischargeRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBatteryDischarge;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunBatteryChargeRoutine(
    uint32_t length_seconds,
    uint32_t minimum_charge_percent_required,
    RunBatteryChargeRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBatteryCharge;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunMemoryRoutine(
    RunMemoryRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kMemory;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunLanConnectivityRoutine(
    RunLanConnectivityRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kLanConnectivity;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunSignalStrengthRoutine(
    RunSignalStrengthRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kSignalStrength;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunGatewayCanBePingedRoutine(
    RunGatewayCanBePingedRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kGatewayCanBePinged;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunHasSecureWiFiConnectionRoutine(
    RunHasSecureWiFiConnectionRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kHasSecureWiFiConnection;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunDnsResolverPresentRoutine(
    RunDnsResolverPresentRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kDnsResolverPresent;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunDnsLatencyRoutine(
    RunDnsLatencyRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kDnsLatency;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunDnsResolutionRoutine(
    RunDnsResolutionRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kDnsResolution;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunCaptivePortalRoutine(
    RunCaptivePortalRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kCaptivePortal;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunHttpFirewallRoutine(
    RunHttpFirewallRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kHttpFirewall;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunHttpsFirewallRoutine(
    RunHttpsFirewallRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kHttpsFirewall;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunHttpsLatencyRoutine(
    RunHttpsLatencyRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kHttpsLatency;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunVideoConferencingRoutine(
    const absl::optional<std::string>& stun_server_hostname,
    RunVideoConferencingRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kVideoConferencing;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunArcHttpRoutine(
    RunArcHttpRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kArcHttp;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunArcPingRoutine(
    RunArcPingRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kArcPing;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunArcDnsResolutionRoutine(
    RunArcDnsResolutionRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kArcDnsResolution;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::AddBluetoothObserver(
    mojo::PendingRemote<mojom::CrosHealthdBluetoothObserver> observer) {
  bluetooth_observers_.Add(std::move(observer));
}

void FakeCrosHealthdService::AddLidObserver(
    mojo::PendingRemote<mojom::CrosHealthdLidObserver> observer) {
  lid_observers_.Add(std::move(observer));
}

void FakeCrosHealthdService::AddPowerObserver(
    mojo::PendingRemote<mojom::CrosHealthdPowerObserver> observer) {
  power_observers_.Add(std::move(observer));
}

void FakeCrosHealthdService::AddNetworkObserver(
    mojo::PendingRemote<chromeos::network_health::mojom::NetworkEventsObserver>
        observer) {
  network_observers_.Add(std::move(observer));
}

void FakeCrosHealthdService::AddAudioObserver(
    mojo::PendingRemote<mojom::CrosHealthdAudioObserver> observer) {
  audio_observers_.Add(std::move(observer));
}

void FakeCrosHealthdService::AddThunderboltObserver(
    mojo::PendingRemote<mojom::CrosHealthdThunderboltObserver> observer) {
  thunderbolt_observers_.Add(std::move(observer));
}

void FakeCrosHealthdService::AddUsbObserver(
    mojo::PendingRemote<mojom::CrosHealthdUsbObserver> observer) {
  usb_observers_.Add(std::move(observer));
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

void FakeCrosHealthdService::EmitAcRemovedEventForTesting() {
  for (auto& observer : power_observers_)
    observer->OnAcRemoved();
}

void FakeCrosHealthdService::EmitOsSuspendEventForTesting() {
  for (auto& observer : power_observers_)
    observer->OnOsSuspend();
}

void FakeCrosHealthdService::EmitOsResumeEventForTesting() {
  for (auto& observer : power_observers_)
    observer->OnOsResume();
}

void FakeCrosHealthdService::EmitAdapterAddedEventForTesting() {
  for (auto& observer : bluetooth_observers_)
    observer->OnAdapterAdded();
}

void FakeCrosHealthdService::EmitAdapterRemovedEventForTesting() {
  for (auto& observer : bluetooth_observers_)
    observer->OnAdapterRemoved();
}

void FakeCrosHealthdService::EmitAdapterPropertyChangedEventForTesting() {
  for (auto& observer : bluetooth_observers_)
    observer->OnAdapterPropertyChanged();
}

void FakeCrosHealthdService::EmitDeviceAddedEventForTesting() {
  for (auto& observer : bluetooth_observers_)
    observer->OnDeviceAdded();
}

void FakeCrosHealthdService::EmitDeviceRemovedEventForTesting() {
  for (auto& observer : bluetooth_observers_)
    observer->OnDeviceRemoved();
}

void FakeCrosHealthdService::EmitDevicePropertyChangedEventForTesting() {
  for (auto& observer : bluetooth_observers_)
    observer->OnDevicePropertyChanged();
}

void FakeCrosHealthdService::EmitLidClosedEventForTesting() {
  for (auto& observer : lid_observers_)
    observer->OnLidClosed();
}

void FakeCrosHealthdService::EmitLidOpenedEventForTesting() {
  for (auto& observer : lid_observers_)
    observer->OnLidOpened();
}

void FakeCrosHealthdService::EmitAudioUnderrunEventForTesting() {
  for (auto& observer : audio_observers_)
    observer->OnUnderrun();
}

void FakeCrosHealthdService::EmitAudioSevereUnderrunEventForTesting() {
  for (auto& observer : audio_observers_)
    observer->OnSevereUnderrun();
}

void FakeCrosHealthdService::EmitThunderboltAddEventForTesting() {
  for (auto& observer : thunderbolt_observers_)
    observer->OnAdd();
}

void FakeCrosHealthdService::EmitUsbAddEventForTesting() {
  mojom::UsbEventInfo info;
  for (auto& observer : usb_observers_)
    observer->OnAdd(info.Clone());
}

void FakeCrosHealthdService::EmitConnectionStateChangedEventForTesting(
    const std::string& network_guid,
    chromeos::network_health::mojom::NetworkState state) {
  for (auto& observer : network_observers_) {
    observer->OnConnectionStateChanged(network_guid, state);
  }
}

void FakeCrosHealthdService::EmitSignalStrengthChangedEventForTesting(
    const std::string& network_guid,
    chromeos::network_health::mojom::UInt32ValuePtr signal_strength) {
  for (auto& observer : network_observers_) {
    observer->OnSignalStrengthChanged(
        network_guid, chromeos::network_health::mojom::UInt32Value::New(
                          signal_strength->value));
  }
}

void FakeCrosHealthdService::RequestNetworkHealthForTesting(
    chromeos::network_health::mojom::NetworkHealthService::
        GetHealthSnapshotCallback callback) {
  network_health_remote_->GetHealthSnapshot(std::move(callback));
}

void FakeCrosHealthdService::RunLanConnectivityRoutineForTesting(
    chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines::
        RunLanConnectivityCallback callback) {
  network_diagnostics_routines_->RunLanConnectivity(std::move(callback));
}

absl::optional<mojom::DiagnosticRoutineEnum>
FakeCrosHealthdService::GetLastRunRoutine() const {
  return last_run_routine_;
}

absl::optional<FakeCrosHealthdService::RoutineUpdateParams>
FakeCrosHealthdService::GetRoutineUpdateParams() const {
  return routine_update_params_;
}

}  // namespace cros_healthd
}  // namespace chromeos
