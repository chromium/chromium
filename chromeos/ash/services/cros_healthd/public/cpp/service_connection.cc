// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"

#include <fcntl.h>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/ash/components/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/components/mojo_service_manager/connection.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/cros_system_api/mojo/service_constants.h"
#include "ui/events/ozone/evdev/event_device_info.h"

#if !defined(USE_REAL_DBUS_CLIENTS)
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#endif

namespace ash::cros_healthd {

namespace {

// Production implementation of ServiceConnection.
class ServiceConnectionImpl : public ServiceConnection {
 public:
  ServiceConnectionImpl();

  ServiceConnectionImpl(const ServiceConnectionImpl&) = delete;
  ServiceConnectionImpl& operator=(const ServiceConnectionImpl&) = delete;

 protected:
  ~ServiceConnectionImpl() override = default;

 private:
  // ServiceConnection overrides:
  void GetAvailableRoutines(
      mojom::CrosHealthdDiagnosticsService::GetAvailableRoutinesCallback
          callback) override;
  void GetRoutineUpdate(
      int32_t id,
      mojom::DiagnosticRoutineCommandEnum command,
      bool include_output,
      mojom::CrosHealthdDiagnosticsService::GetRoutineUpdateCallback callback)
      override;
  void RunUrandomRoutine(
      const absl::optional<base::TimeDelta>& length_seconds,
      mojom::CrosHealthdDiagnosticsService::RunUrandomRoutineCallback callback)
      override;
  void RunBatteryCapacityRoutine(
      mojom::CrosHealthdDiagnosticsService::RunBatteryCapacityRoutineCallback
          callback) override;
  void RunBatteryHealthRoutine(
      mojom::CrosHealthdDiagnosticsService::RunBatteryHealthRoutineCallback
          callback) override;
  void RunSmartctlCheckRoutine(
      mojom::CrosHealthdDiagnosticsService::RunSmartctlCheckRoutineCallback
          callback) override;
  void RunAcPowerRoutine(
      mojom::AcPowerStatusEnum expected_status,
      const absl::optional<std::string>& expected_power_type,
      mojom::CrosHealthdDiagnosticsService::RunAcPowerRoutineCallback callback)
      override;
  void RunCpuCacheRoutine(
      const absl::optional<base::TimeDelta>& exec_duration,
      mojom::CrosHealthdDiagnosticsService::RunCpuCacheRoutineCallback callback)
      override;
  void RunCpuStressRoutine(
      const absl::optional<base::TimeDelta>& exec_duration,
      mojom::CrosHealthdDiagnosticsService::RunCpuStressRoutineCallback
          callback) override;
  void RunFloatingPointAccuracyRoutine(
      const absl::optional<base::TimeDelta>& exec_duration,
      mojom::CrosHealthdDiagnosticsService::
          RunFloatingPointAccuracyRoutineCallback callback) override;
  void RunNvmeWearLevelRoutine(
      uint32_t wear_level_threshold,
      mojom::CrosHealthdDiagnosticsService::RunNvmeWearLevelRoutineCallback
          callback) override;
  void RunNvmeSelfTestRoutine(
      mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
      mojom::CrosHealthdDiagnosticsService::RunNvmeSelfTestRoutineCallback
          callback) override;
  void RunDiskReadRoutine(
      mojom::DiskReadRoutineTypeEnum type,
      base::TimeDelta& exec_duration,
      uint32_t file_size_mb,
      mojom::CrosHealthdDiagnosticsService::RunDiskReadRoutineCallback callback)
      override;
  void RunPrimeSearchRoutine(
      const absl::optional<base::TimeDelta>& exec_duration,
      mojom::CrosHealthdDiagnosticsService::RunPrimeSearchRoutineCallback
          callback) override;
  void RunBatteryDischargeRoutine(
      base::TimeDelta exec_duration,
      uint32_t maximum_discharge_percent_allowed,
      mojom::CrosHealthdDiagnosticsService::RunBatteryDischargeRoutineCallback
          callback) override;
  void RunBatteryChargeRoutine(
      base::TimeDelta exec_duration,
      uint32_t minimum_charge_percent_required,
      mojom::CrosHealthdDiagnosticsService::RunBatteryChargeRoutineCallback
          callback) override;
  void RunMemoryRoutine(
      mojom::CrosHealthdDiagnosticsService::RunMemoryRoutineCallback callback)
      override;
  void RunLanConnectivityRoutine(
      mojom::CrosHealthdDiagnosticsService::RunLanConnectivityRoutineCallback
          callback) override;
  void RunSignalStrengthRoutine(
      mojom::CrosHealthdDiagnosticsService::RunSignalStrengthRoutineCallback
          callback) override;
  void RunGatewayCanBePingedRoutine(
      mojom::CrosHealthdDiagnosticsService::RunGatewayCanBePingedRoutineCallback
          callback) override;
  void RunHasSecureWiFiConnectionRoutine(
      mojom::CrosHealthdDiagnosticsService::
          RunHasSecureWiFiConnectionRoutineCallback callback) override;
  void RunDnsResolverPresentRoutine(
      mojom::CrosHealthdDiagnosticsService::RunDnsResolverPresentRoutineCallback
          callback) override;
  void RunDnsLatencyRoutine(
      mojom::CrosHealthdDiagnosticsService::RunDnsLatencyRoutineCallback
          callback) override;
  void RunDnsResolutionRoutine(
      mojom::CrosHealthdDiagnosticsService::RunDnsResolutionRoutineCallback
          callback) override;
  void RunCaptivePortalRoutine(
      mojom::CrosHealthdDiagnosticsService::RunCaptivePortalRoutineCallback
          callback) override;
  void RunHttpFirewallRoutine(
      mojom::CrosHealthdDiagnosticsService::RunHttpFirewallRoutineCallback
          callback) override;
  void RunHttpsFirewallRoutine(
      mojom::CrosHealthdDiagnosticsService::RunHttpsFirewallRoutineCallback
          callback) override;
  void RunHttpsLatencyRoutine(
      mojom::CrosHealthdDiagnosticsService::RunHttpsLatencyRoutineCallback
          callback) override;
  void RunVideoConferencingRoutine(
      const absl::optional<std::string>& stun_server_hostname,
      mojom::CrosHealthdDiagnosticsService::RunVideoConferencingRoutineCallback
          callback) override;
  void RunArcHttpRoutine(
      mojom::CrosHealthdDiagnosticsService::RunArcHttpRoutineCallback callback)
      override;
  void RunArcPingRoutine(
      mojom::CrosHealthdDiagnosticsService::RunArcPingRoutineCallback callback)
      override;
  void RunArcDnsResolutionRoutine(
      mojom::CrosHealthdDiagnosticsService::RunArcDnsResolutionRoutineCallback
          callback) override;
  void AddBluetoothObserver(
      mojo::PendingRemote<mojom::CrosHealthdBluetoothObserver> pending_observer)
      override;
  void AddLidObserver(mojo::PendingRemote<mojom::CrosHealthdLidObserver>
                          pending_observer) override;
  void AddPowerObserver(mojo::PendingRemote<mojom::CrosHealthdPowerObserver>
                            pending_observer) override;
  void AddNetworkObserver(
      mojo::PendingRemote<
          chromeos::network_health::mojom::NetworkEventsObserver>
          pending_observer) override;
  void AddAudioObserver(mojo::PendingRemote<mojom::CrosHealthdAudioObserver>
                            pending_observer) override;
  void AddThunderboltObserver(
      mojo::PendingRemote<mojom::CrosHealthdThunderboltObserver>
          pending_observer) override;
  void AddUsbObserver(mojo::PendingRemote<mojom::CrosHealthdUsbObserver>
                          pending_observer) override;
  void ProbeTelemetryInfo(
      const std::vector<mojom::ProbeCategoryEnum>& categories_to_test,
      mojom::CrosHealthdProbeService::ProbeTelemetryInfoCallback callback)
      override;
  void ProbeProcessInfo(pid_t process_id,
                        mojom::CrosHealthdProbeService::ProbeProcessInfoCallback
                            callback) override;
  void ProbeMultipleProcessInfo(
      const absl::optional<std::vector<uint32_t>>& process_ids,
      bool ignore_single_process_info,
      mojom::CrosHealthdProbeService::ProbeMultipleProcessInfoCallback callback)
      override;

  void GetDiagnosticsService(
      mojo::PendingReceiver<mojom::CrosHealthdDiagnosticsService> service)
      override;
  void GetProbeService(
      mojo::PendingReceiver<mojom::CrosHealthdProbeService> service) override;
  void SetBindNetworkHealthServiceCallback(
      BindNetworkHealthServiceCallback callback) override;
  void SetBindNetworkDiagnosticsRoutinesCallback(
      BindNetworkDiagnosticsRoutinesCallback callback) override;
  void SendChromiumDataCollector(
      mojo::PendingRemote<internal::mojom::ChromiumDataCollector> remote)
      override;
  std::string FetchTouchpadLibraryName() override;
  void FlushForTesting() override;

  // Uses |bind_network_health_callback_| if set to bind a remote to the
  // NetworkHealthService and send the PendingRemote to the CrosHealthdService.
  void BindAndSendNetworkHealthService();

  // Uses |bind_network_diagnostics_callback_| if set to bind a remote to the
  // NetworkDiagnosticsRoutines interface and send the PendingRemote to
  // cros_healthd.
  void BindAndSendNetworkDiagnosticsRoutines();

  // Binds the factory interface |cros_healthd_service_factory_| to an
  // implementation in the cros_healthd daemon, if it is not already bound. The
  // binding is accomplished via D-Bus bootstrap.
  void EnsureCrosHealthdServiceFactoryIsBound();

  // Uses |cros_healthd_service_factory_| to bind the diagnostics service remote
  // to an implementation in the cros_healethd daemon, if it is not already
  // bound.
  void BindCrosHealthdDiagnosticsServiceIfNeeded();

  // Uses |cros_healthd_service_factory_| to bind the event service remote to an
  // implementation in the cros_healethd daemon, if it is not already bound.
  void BindCrosHealthdEventServiceIfNeeded();

  // Uses |cros_healthd_service_factory_| to bind the probe service remote to an
  // implementation in the cros_healethd daemon, if it is not already bound.
  void BindCrosHealthdProbeServiceIfNeeded();

  // Mojo disconnect handler. Resets |cros_healthd_service_|, which will be
  // reconnected upon next use.
  void OnDisconnect();

  // Response callback for BootstrapMojoConnection.
  void OnBootstrapMojoConnectionResponse(bool success);

  mojo::Remote<mojom::CrosHealthdServiceFactory> cros_healthd_service_factory_;
  mojo::Remote<mojom::CrosHealthdProbeService> cros_healthd_probe_service_;
  mojo::Remote<mojom::CrosHealthdDiagnosticsService>
      cros_healthd_diagnostics_service_;
  mojo::Remote<mojom::CrosHealthdEventService> cros_healthd_event_service_;

  // Repeating callback that binds a mojo::PendingRemote to the
  // NetworkHealthService and returns it.
  BindNetworkHealthServiceCallback bind_network_health_callback_;

  // Repeating callback that binds a mojo::PendingRemote to the
  // NetworkDiagnosticsRoutines interface and returns it.
  BindNetworkDiagnosticsRoutinesCallback bind_network_diagnostics_callback_;

  const bool use_service_manager_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ServiceConnectionImpl> weak_factory_{this};
};

void ServiceConnectionImpl::GetAvailableRoutines(
    mojom::CrosHealthdDiagnosticsService::GetAvailableRoutinesCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->GetAvailableRoutines(std::move(callback));
}

void ServiceConnectionImpl::GetRoutineUpdate(
    int32_t id,
    mojom::DiagnosticRoutineCommandEnum command,
    bool include_output,
    mojom::CrosHealthdDiagnosticsService::GetRoutineUpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->GetRoutineUpdate(
      id, command, include_output, std::move(callback));
}

void ServiceConnectionImpl::RunUrandomRoutine(
    const absl::optional<base::TimeDelta>& length_seconds,
    mojom::CrosHealthdDiagnosticsService::RunUrandomRoutineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  mojom::NullableUint32Ptr routine_parameter;
  if (length_seconds.has_value()) {
    routine_parameter =
        mojom::NullableUint32::New(length_seconds.value().InSeconds());
  }
  cros_healthd_diagnostics_service_->RunUrandomRoutine(
      std::move(routine_parameter), std::move(callback));
}

void ServiceConnectionImpl::RunBatteryCapacityRoutine(
    mojom::CrosHealthdDiagnosticsService::RunBatteryCapacityRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunBatteryCapacityRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::RunBatteryHealthRoutine(
    mojom::CrosHealthdDiagnosticsService::RunBatteryHealthRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunBatteryHealthRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::RunSmartctlCheckRoutine(
    mojom::CrosHealthdDiagnosticsService::RunSmartctlCheckRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunSmartctlCheckRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::RunAcPowerRoutine(
    mojom::AcPowerStatusEnum expected_status,
    const absl::optional<std::string>& expected_power_type,
    mojom::CrosHealthdDiagnosticsService::RunAcPowerRoutineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunAcPowerRoutine(
      expected_status, expected_power_type, std::move(callback));
}

void ServiceConnectionImpl::RunCpuCacheRoutine(
    const absl::optional<base::TimeDelta>& exec_duration,
    mojom::CrosHealthdDiagnosticsService::RunCpuCacheRoutineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  mojom::NullableUint32Ptr routine_duration;
  if (exec_duration.has_value()) {
    routine_duration =
        mojom::NullableUint32::New(exec_duration.value().InSeconds());
  }
  cros_healthd_diagnostics_service_->RunCpuCacheRoutine(
      std::move(routine_duration), std::move(callback));
}

void ServiceConnectionImpl::RunCpuStressRoutine(
    const absl::optional<base::TimeDelta>& exec_duration,
    mojom::CrosHealthdDiagnosticsService::RunCpuStressRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  mojom::NullableUint32Ptr routine_duration;
  if (exec_duration.has_value()) {
    routine_duration =
        mojom::NullableUint32::New(exec_duration.value().InSeconds());
  }
  cros_healthd_diagnostics_service_->RunCpuStressRoutine(
      std::move(routine_duration), std::move(callback));
}

void ServiceConnectionImpl::RunFloatingPointAccuracyRoutine(
    const absl::optional<base::TimeDelta>& exec_duration,
    mojom::CrosHealthdDiagnosticsService::
        RunFloatingPointAccuracyRoutineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  mojom::NullableUint32Ptr routine_duration;
  if (exec_duration.has_value()) {
    routine_duration =
        mojom::NullableUint32::New(exec_duration.value().InSeconds());
  }
  cros_healthd_diagnostics_service_->RunFloatingPointAccuracyRoutine(
      std::move(routine_duration), std::move(callback));
}

void ServiceConnectionImpl::RunNvmeWearLevelRoutine(
    uint32_t wear_level_threshold,
    mojom::CrosHealthdDiagnosticsService::RunNvmeWearLevelRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunNvmeWearLevelRoutine(
      wear_level_threshold, std::move(callback));
}

void ServiceConnectionImpl::RunNvmeSelfTestRoutine(
    mojom::NvmeSelfTestTypeEnum self_test_type,
    mojom::CrosHealthdDiagnosticsService::RunNvmeSelfTestRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunNvmeSelfTestRoutine(
      self_test_type, std::move(callback));
}

void ServiceConnectionImpl::RunDiskReadRoutine(
    mojom::DiskReadRoutineTypeEnum type,
    base::TimeDelta& exec_duration,
    uint32_t file_size_mb,
    mojom::CrosHealthdDiagnosticsService::RunDiskReadRoutineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunDiskReadRoutine(
      type, exec_duration.InSeconds(), file_size_mb, std::move(callback));
}

void ServiceConnectionImpl::RunPrimeSearchRoutine(
    const absl::optional<base::TimeDelta>& exec_duration,
    mojom::CrosHealthdDiagnosticsService::RunPrimeSearchRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  mojom::NullableUint32Ptr routine_duration;
  if (exec_duration.has_value()) {
    routine_duration =
        mojom::NullableUint32::New(exec_duration.value().InSeconds());
  }
  cros_healthd_diagnostics_service_->RunPrimeSearchRoutine(
      std::move(routine_duration), std::move(callback));
}

void ServiceConnectionImpl::RunBatteryDischargeRoutine(
    base::TimeDelta exec_duration,
    uint32_t maximum_discharge_percent_allowed,
    mojom::CrosHealthdDiagnosticsService::RunBatteryDischargeRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunBatteryDischargeRoutine(
      exec_duration.InSeconds(), maximum_discharge_percent_allowed,
      std::move(callback));
}

void ServiceConnectionImpl::RunBatteryChargeRoutine(
    base::TimeDelta exec_duration,
    uint32_t minimum_charge_percent_required,
    mojom::CrosHealthdDiagnosticsService::RunBatteryChargeRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunBatteryChargeRoutine(
      exec_duration.InSeconds(), minimum_charge_percent_required,
      std::move(callback));
}

void ServiceConnectionImpl::RunMemoryRoutine(
    mojom::CrosHealthdDiagnosticsService::RunMemoryRoutineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunMemoryRoutine(std::move(callback));
}

void ServiceConnectionImpl::RunLanConnectivityRoutine(
    mojom::CrosHealthdDiagnosticsService::RunLanConnectivityRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunLanConnectivityRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::RunSignalStrengthRoutine(
    mojom::CrosHealthdDiagnosticsService::RunSignalStrengthRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunSignalStrengthRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::RunGatewayCanBePingedRoutine(
    mojom::CrosHealthdDiagnosticsService::RunGatewayCanBePingedRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunGatewayCanBePingedRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::RunHasSecureWiFiConnectionRoutine(
    mojom::CrosHealthdDiagnosticsService::
        RunHasSecureWiFiConnectionRoutineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunHasSecureWiFiConnectionRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::RunDnsResolverPresentRoutine(
    mojom::CrosHealthdDiagnosticsService::RunDnsResolverPresentRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunDnsResolverPresentRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::RunDnsLatencyRoutine(
    mojom::CrosHealthdDiagnosticsService::RunDnsLatencyRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunDnsLatencyRoutine(std::move(callback));
}

void ServiceConnectionImpl::RunDnsResolutionRoutine(
    mojom::CrosHealthdDiagnosticsService::RunDnsResolutionRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunDnsResolutionRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::RunCaptivePortalRoutine(
    mojom::CrosHealthdDiagnosticsService::RunCaptivePortalRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunCaptivePortalRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::RunHttpFirewallRoutine(
    mojom::CrosHealthdDiagnosticsService::RunHttpFirewallRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunHttpFirewallRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::RunHttpsFirewallRoutine(
    mojom::CrosHealthdDiagnosticsService::RunHttpsFirewallRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunHttpsFirewallRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::RunHttpsLatencyRoutine(
    mojom::CrosHealthdDiagnosticsService::RunHttpsLatencyRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunHttpsLatencyRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::RunArcHttpRoutine(
    mojom::CrosHealthdDiagnosticsService::RunArcHttpRoutineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunArcHttpRoutine(std::move(callback));
}

void ServiceConnectionImpl::RunArcPingRoutine(
    mojom::CrosHealthdDiagnosticsService::RunArcPingRoutineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunArcPingRoutine(std::move(callback));
}

void ServiceConnectionImpl::RunArcDnsResolutionRoutine(
    mojom::CrosHealthdDiagnosticsService::RunArcDnsResolutionRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunArcDnsResolutionRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::RunVideoConferencingRoutine(
    const absl::optional<std::string>& stun_server_hostname,
    mojom::CrosHealthdDiagnosticsService::RunVideoConferencingRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunVideoConferencingRoutine(
      stun_server_hostname, std::move(callback));
}

void ServiceConnectionImpl::AddBluetoothObserver(
    mojo::PendingRemote<mojom::CrosHealthdBluetoothObserver> pending_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdEventServiceIfNeeded();
  cros_healthd_event_service_->AddBluetoothObserver(
      std::move(pending_observer));
}

void ServiceConnectionImpl::AddLidObserver(
    mojo::PendingRemote<mojom::CrosHealthdLidObserver> pending_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdEventServiceIfNeeded();
  cros_healthd_event_service_->AddLidObserver(std::move(pending_observer));
}

void ServiceConnectionImpl::AddPowerObserver(
    mojo::PendingRemote<mojom::CrosHealthdPowerObserver> pending_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdEventServiceIfNeeded();
  cros_healthd_event_service_->AddPowerObserver(std::move(pending_observer));
}

void ServiceConnectionImpl::AddNetworkObserver(
    mojo::PendingRemote<chromeos::network_health::mojom::NetworkEventsObserver>
        pending_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdEventServiceIfNeeded();
  cros_healthd_event_service_->AddNetworkObserver(std::move(pending_observer));
}

void ServiceConnectionImpl::AddAudioObserver(
    mojo::PendingRemote<mojom::CrosHealthdAudioObserver> pending_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdEventServiceIfNeeded();
  cros_healthd_event_service_->AddAudioObserver(std::move(pending_observer));
}

void ServiceConnectionImpl::AddThunderboltObserver(
    mojo::PendingRemote<mojom::CrosHealthdThunderboltObserver>
        pending_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdEventServiceIfNeeded();
  cros_healthd_event_service_->AddThunderboltObserver(
      std::move(pending_observer));
}

void ServiceConnectionImpl::AddUsbObserver(
    mojo::PendingRemote<mojom::CrosHealthdUsbObserver> pending_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdEventServiceIfNeeded();
  cros_healthd_event_service_->AddUsbObserver(std::move(pending_observer));
}

void ServiceConnectionImpl::ProbeTelemetryInfo(
    const std::vector<mojom::ProbeCategoryEnum>& categories_to_test,
    mojom::CrosHealthdProbeService::ProbeTelemetryInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdProbeServiceIfNeeded();
  cros_healthd_probe_service_->ProbeTelemetryInfo(categories_to_test,
                                                  std::move(callback));
}

void ServiceConnectionImpl::ProbeProcessInfo(
    pid_t process_id,
    mojom::CrosHealthdProbeService::ProbeProcessInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(process_id > 0);
  BindCrosHealthdProbeServiceIfNeeded();
  cros_healthd_probe_service_->ProbeProcessInfo(
      static_cast<uint32_t>(process_id), std::move(callback));
}

void ServiceConnectionImpl::ProbeMultipleProcessInfo(
    const absl::optional<std::vector<uint32_t>>& process_ids,
    bool ignore_single_process_info,
    mojom::CrosHealthdProbeService::ProbeMultipleProcessInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdProbeServiceIfNeeded();
  cros_healthd_probe_service_->ProbeMultipleProcessInfo(
      process_ids, ignore_single_process_info, std::move(callback));
}

void ServiceConnectionImpl::GetDiagnosticsService(
    mojo::PendingReceiver<mojom::CrosHealthdDiagnosticsService> service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (use_service_manager_) {
    chromeos::mojo_service_manager::GetServiceManagerProxy()->Request(
        chromeos::mojo_services::kCrosHealthdDiagnostics, absl::nullopt,
        std::move(service).PassPipe());
  } else {
    EnsureCrosHealthdServiceFactoryIsBound();
    cros_healthd_service_factory_->GetDiagnosticsService(std::move(service));
  }
}

void ServiceConnectionImpl::SetBindNetworkHealthServiceCallback(
    BindNetworkHealthServiceCallback callback) {
  // Don't set the interface if service manager is used.
  if (use_service_manager_)
    return;
  bind_network_health_callback_ = std::move(callback);
  BindAndSendNetworkHealthService();
}

void ServiceConnectionImpl::SetBindNetworkDiagnosticsRoutinesCallback(
    BindNetworkDiagnosticsRoutinesCallback callback) {
  // Don't set the interface if service manager is used.
  if (use_service_manager_)
    return;
  bind_network_diagnostics_callback_ = std::move(callback);
  BindAndSendNetworkDiagnosticsRoutines();
}

void ServiceConnectionImpl::SendChromiumDataCollector(
    mojo::PendingRemote<internal::mojom::ChromiumDataCollector> remote) {
  // Don't set the interface if service manager is used.
  if (use_service_manager_)
    return;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureCrosHealthdServiceFactoryIsBound();
  cros_healthd_service_factory_->SendChromiumDataCollector(std::move(remote));
}

// This is a short-term solution for ChromeOS Flex. We should remove this work
// around after cros_healthd team develop a healthier input telemetry approach.
std::string ServiceConnectionImpl::FetchTouchpadLibraryName() {
#if defined(USE_LIBINPUT)
  base::FileEnumerator file_enum(base::FilePath("/dev/input/"), false,
                                 base::FileEnumerator::FileType::FILES);
  for (auto path = file_enum.Next(); !path.empty(); path = file_enum.Next()) {
    base::ScopedFD fd(open(path.value().c_str(), O_RDWR | O_NONBLOCK));
    if (fd.get() < 0) {
      LOG(ERROR) << "Couldn't open device path " << path;
      continue;
    }

    auto devinfo = std::make_unique<ui::EventDeviceInfo>();
    if (!devinfo->Initialize(fd.get(), path)) {
      LOG(ERROR) << "Failed to get device info for " << path;
      continue;
    }

    if (!devinfo->HasTouchpad() ||
        devinfo->device_type() != ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      continue;
    }

    if (devinfo->UseLibinput()) {
      return "libinput";
    }
  }
#endif

#if defined(USE_EVDEV_GESTURES)
  return "gestures";
#else
  return "Default EventConverterEvdev";
#endif
}

void ServiceConnectionImpl::FlushForTesting() {
  if (cros_healthd_service_factory_.is_bound())
    cros_healthd_service_factory_.FlushForTesting();
  if (cros_healthd_probe_service_.is_bound())
    cros_healthd_probe_service_.FlushForTesting();
  if (cros_healthd_diagnostics_service_.is_bound())
    cros_healthd_diagnostics_service_.FlushForTesting();
  if (cros_healthd_event_service_.is_bound())
    cros_healthd_event_service_.FlushForTesting();
}

void ServiceConnectionImpl::BindAndSendNetworkHealthService() {
  DCHECK(!use_service_manager_) << "ServiceFactory is not supported.";
  if (bind_network_health_callback_.is_null())
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureCrosHealthdServiceFactoryIsBound();
  auto remote = bind_network_health_callback_.Run();
  cros_healthd_service_factory_->SendNetworkHealthService(std::move(remote));
}

void ServiceConnectionImpl::BindAndSendNetworkDiagnosticsRoutines() {
  DCHECK(!use_service_manager_) << "ServiceFactory is not supported.";
  if (bind_network_diagnostics_callback_.is_null())
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureCrosHealthdServiceFactoryIsBound();
  auto remote = bind_network_diagnostics_callback_.Run();
  cros_healthd_service_factory_->SendNetworkDiagnosticsRoutines(
      std::move(remote));
}

void ServiceConnectionImpl::GetProbeService(
    mojo::PendingReceiver<mojom::CrosHealthdProbeService> service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (use_service_manager_) {
    chromeos::mojo_service_manager::GetServiceManagerProxy()->Request(
        chromeos::mojo_services::kCrosHealthdProbe, absl::nullopt,
        std::move(service).PassPipe());
  } else {
    EnsureCrosHealthdServiceFactoryIsBound();
    cros_healthd_service_factory_->GetProbeService(std::move(service));
  }
}

void ServiceConnectionImpl::EnsureCrosHealthdServiceFactoryIsBound() {
  DCHECK(!use_service_manager_)
      << "ServiceFactory is not available in service manager.";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_service_factory_.is_bound())
    return;

  auto* client = CrosHealthdClient::Get();
  if (!client)
    return;

  cros_healthd_service_factory_ = client->BootstrapMojoConnection(
      base::BindOnce(&ServiceConnectionImpl::OnBootstrapMojoConnectionResponse,
                     weak_factory_.GetWeakPtr()));

  cros_healthd_service_factory_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnDisconnect, weak_factory_.GetWeakPtr()));
}

void ServiceConnectionImpl::BindCrosHealthdDiagnosticsServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_diagnostics_service_.is_bound())
    return;

  GetDiagnosticsService(
      cros_healthd_diagnostics_service_.BindNewPipeAndPassReceiver());
  cros_healthd_diagnostics_service_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnDisconnect, weak_factory_.GetWeakPtr()));
}

void ServiceConnectionImpl::BindCrosHealthdEventServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_event_service_.is_bound())
    return;

  if (use_service_manager_) {
    chromeos::mojo_service_manager::GetServiceManagerProxy()->Request(
        chromeos::mojo_services::kCrosHealthdEvent, absl::nullopt,
        cros_healthd_event_service_.BindNewPipeAndPassReceiver().PassPipe());
  } else {
    EnsureCrosHealthdServiceFactoryIsBound();
    cros_healthd_service_factory_->GetEventService(
        cros_healthd_event_service_.BindNewPipeAndPassReceiver());
  }
  cros_healthd_event_service_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnDisconnect, weak_factory_.GetWeakPtr()));
}

void ServiceConnectionImpl::BindCrosHealthdProbeServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_probe_service_.is_bound())
    return;

  GetProbeService(cros_healthd_probe_service_.BindNewPipeAndPassReceiver());
  cros_healthd_probe_service_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnDisconnect, weak_factory_.GetWeakPtr()));
}

bool UseServiceManager() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  bool has_service_manager =
      command_line->HasSwitch(ash::switches::kAshUseCrOSMojoServiceManager);
  bool healthd_use_service_manager =
      command_line->HasSwitch(ash::switches::kCrosHealthdUsesServiceManager);
  DCHECK(!healthd_use_service_manager ||
         (healthd_use_service_manager && has_service_manager))
      << "Healthd try to use chromeos mojo service manager but it is not "
         "enabled.";
  return has_service_manager && healthd_use_service_manager;
}

ServiceConnectionImpl::ServiceConnectionImpl()
    : use_service_manager_(UseServiceManager()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
#if !defined(USE_REAL_DBUS_CLIENTS)
  // Creates the fake mojo service if need. This is for browser test to do the
  // initialized.
  // TODO(b/230064284): Remove this after we migrate to mojo service manager.
  if (!FakeCrosHealthd::Get()) {
    CHECK(CrosHealthdClient::Get())
        << "The dbus client is not initialized. This should not happen in "
           "browser tests. In unit tests, use FakeCrosHealthd::Initialize() to "
           "initialize the fake cros healthd service.";
    // Only initialize the fake if fake dbus client is used.
    if (FakeCrosHealthdClient::Get())
      FakeCrosHealthd::Initialize();
  }
#endif  // defined(USE_REAL_DBUS_CLIENTS)
  if (!use_service_manager_)
    EnsureCrosHealthdServiceFactoryIsBound();
}

void ServiceConnectionImpl::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Connection errors are not expected, so log a warning.
  DLOG(WARNING) << "cros_healthd Mojo connection closed.";
  cros_healthd_service_factory_.reset();
  cros_healthd_probe_service_.reset();
  cros_healthd_diagnostics_service_.reset();
  cros_healthd_event_service_.reset();

  // Don't try to reconnect if service manager is used.
  if (use_service_manager_)
    return;

  EnsureCrosHealthdServiceFactoryIsBound();
  // If the cros_healthd_service_factory_ was able to be rebound, resend the
  // Chrome services to the CrosHealthd instance.
  if (cros_healthd_service_factory_.is_bound()) {
    BindAndSendNetworkHealthService();
    BindAndSendNetworkDiagnosticsRoutines();
  }
}

void ServiceConnectionImpl::OnBootstrapMojoConnectionResponse(
    const bool success) {
  DCHECK(!use_service_manager_)
      << "D-Bus is not used if service manager is used.";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    DLOG(WARNING) << "BootstrapMojoConnection D-Bus call failed.";
    cros_healthd_service_factory_.reset();
  }
}

}  // namespace

ServiceConnection* ServiceConnection::GetInstance() {
  static base::NoDestructor<ServiceConnectionImpl> service_connection;
  return service_connection.get();
}

}  // namespace ash::cros_healthd
