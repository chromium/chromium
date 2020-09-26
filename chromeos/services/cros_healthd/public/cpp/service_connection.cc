// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace cros_healthd {

namespace {

// Production implementation of ServiceConnection.
class ServiceConnectionImpl : public ServiceConnection {
 public:
  ServiceConnectionImpl();

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
      uint32_t length_seconds,
      mojom::CrosHealthdDiagnosticsService::RunUrandomRoutineCallback callback)
      override;
  void RunBatteryCapacityRoutine(
      uint32_t low_mah,
      uint32_t high_mah,
      mojom::CrosHealthdDiagnosticsService::RunBatteryCapacityRoutineCallback
          callback) override;
  void RunBatteryHealthRoutine(
      uint32_t maximum_cycle_count,
      uint32_t percent_battery_wear_allowed,
      mojom::CrosHealthdDiagnosticsService::RunBatteryHealthRoutineCallback
          callback) override;
  void RunSmartctlCheckRoutine(
      mojom::CrosHealthdDiagnosticsService::RunSmartctlCheckRoutineCallback
          callback) override;
  void RunAcPowerRoutine(
      mojom::AcPowerStatusEnum expected_status,
      const base::Optional<std::string>& expected_power_type,
      mojom::CrosHealthdDiagnosticsService::RunAcPowerRoutineCallback callback)
      override;
  void RunCpuCacheRoutine(
      const base::TimeDelta& exec_duration,
      mojom::CrosHealthdDiagnosticsService::RunCpuCacheRoutineCallback callback)
      override;
  void RunCpuStressRoutine(
      const base::TimeDelta& exec_duration,
      mojom::CrosHealthdDiagnosticsService::RunCpuStressRoutineCallback
          callback) override;
  void RunFloatingPointAccuracyRoutine(
      const base::TimeDelta& exec_duration,
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
      base::TimeDelta& exec_duration,
      uint64_t max_num,
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
  void AddBluetoothObserver(
      mojo::PendingRemote<mojom::CrosHealthdBluetoothObserver> pending_observer)
      override;
  void AddLidObserver(mojo::PendingRemote<mojom::CrosHealthdLidObserver>
                          pending_observer) override;
  void AddPowerObserver(mojo::PendingRemote<mojom::CrosHealthdPowerObserver>
                            pending_observer) override;
  void ProbeTelemetryInfo(
      const std::vector<mojom::ProbeCategoryEnum>& categories_to_test,
      mojom::CrosHealthdProbeService::ProbeTelemetryInfoCallback callback)
      override;
  void ProbeProcessInfo(pid_t process_id,
                        mojom::CrosHealthdProbeService::ProbeProcessInfoCallback
                            callback) override;
  void GetDiagnosticsService(
      mojom::CrosHealthdDiagnosticsServiceRequest service) override;
  void GetProbeService(mojom::CrosHealthdProbeServiceRequest service) override;
  void SetBindNetworkHealthServiceCallback(
      BindNetworkHealthServiceCallback callback) override;
  void SetBindNetworkDiagnosticsRoutinesCallback(
      BindNetworkDiagnosticsRoutinesCallback callback) override;
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

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ServiceConnectionImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceConnectionImpl);
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
    uint32_t length_seconds,
    mojom::CrosHealthdDiagnosticsService::RunUrandomRoutineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunUrandomRoutine(length_seconds,
                                                       std::move(callback));
}

void ServiceConnectionImpl::RunBatteryCapacityRoutine(
    uint32_t low_mah,
    uint32_t high_mah,
    mojom::CrosHealthdDiagnosticsService::RunBatteryCapacityRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunBatteryCapacityRoutine(
      low_mah, high_mah, std::move(callback));
}

void ServiceConnectionImpl::RunBatteryHealthRoutine(
    uint32_t maximum_cycle_count,
    uint32_t percent_battery_wear_allowed,
    mojom::CrosHealthdDiagnosticsService::RunBatteryHealthRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunBatteryHealthRoutine(
      maximum_cycle_count, percent_battery_wear_allowed, std::move(callback));
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
    const base::Optional<std::string>& expected_power_type,
    mojom::CrosHealthdDiagnosticsService::RunAcPowerRoutineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunAcPowerRoutine(
      expected_status, expected_power_type, std::move(callback));
}

void ServiceConnectionImpl::RunCpuCacheRoutine(
    const base::TimeDelta& exec_duration,
    mojom::CrosHealthdDiagnosticsService::RunCpuCacheRoutineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunCpuCacheRoutine(
      exec_duration.InSeconds(), std::move(callback));
}

void ServiceConnectionImpl::RunCpuStressRoutine(
    const base::TimeDelta& exec_duration,
    mojom::CrosHealthdDiagnosticsService::RunCpuStressRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunCpuStressRoutine(
      exec_duration.InSeconds(), std::move(callback));
}

void ServiceConnectionImpl::RunFloatingPointAccuracyRoutine(
    const base::TimeDelta& exec_duration,
    mojom::CrosHealthdDiagnosticsService::
        RunFloatingPointAccuracyRoutineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunFloatingPointAccuracyRoutine(
      exec_duration.InSeconds(), std::move(callback));
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
    base::TimeDelta& exec_duration,
    uint64_t max_num,
    mojom::CrosHealthdDiagnosticsService::RunPrimeSearchRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunPrimeSearchRoutine(
      exec_duration.InSeconds(), max_num, std::move(callback));
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

void ServiceConnectionImpl::AddBluetoothObserver(
    mojo::PendingRemote<mojom::CrosHealthdBluetoothObserver> pending_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdEventServiceIfNeeded();
  mojom::CrosHealthdBluetoothObserverPtr ptr{std::move(pending_observer)};
  cros_healthd_event_service_->AddBluetoothObserver(std::move(ptr));
}

void ServiceConnectionImpl::AddLidObserver(
    mojo::PendingRemote<mojom::CrosHealthdLidObserver> pending_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdEventServiceIfNeeded();
  mojom::CrosHealthdLidObserverPtr ptr{std::move(pending_observer)};
  cros_healthd_event_service_->AddLidObserver(std::move(ptr));
}

void ServiceConnectionImpl::AddPowerObserver(
    mojo::PendingRemote<mojom::CrosHealthdPowerObserver> pending_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdEventServiceIfNeeded();
  mojom::CrosHealthdPowerObserverPtr ptr{std::move(pending_observer)};
  cros_healthd_event_service_->AddPowerObserver(std::move(ptr));
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

void ServiceConnectionImpl::GetDiagnosticsService(
    mojom::CrosHealthdDiagnosticsServiceRequest service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureCrosHealthdServiceFactoryIsBound();
  cros_healthd_service_factory_->GetDiagnosticsService(std::move(service));
}

void ServiceConnectionImpl::SetBindNetworkHealthServiceCallback(
    BindNetworkHealthServiceCallback callback) {
  bind_network_health_callback_ = std::move(callback);
  BindAndSendNetworkHealthService();
}

void ServiceConnectionImpl::SetBindNetworkDiagnosticsRoutinesCallback(
    BindNetworkDiagnosticsRoutinesCallback callback) {
  bind_network_diagnostics_callback_ = std::move(callback);
  BindAndSendNetworkDiagnosticsRoutines();
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
  if (bind_network_health_callback_.is_null())
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureCrosHealthdServiceFactoryIsBound();
  auto remote = bind_network_health_callback_.Run();
  cros_healthd_service_factory_->SendNetworkHealthService(std::move(remote));
}

void ServiceConnectionImpl::BindAndSendNetworkDiagnosticsRoutines() {
  if (bind_network_diagnostics_callback_.is_null())
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureCrosHealthdServiceFactoryIsBound();
  auto remote = bind_network_diagnostics_callback_.Run();
  cros_healthd_service_factory_->SendNetworkDiagnosticsRoutines(
      std::move(remote));
}

void ServiceConnectionImpl::GetProbeService(
    mojom::CrosHealthdProbeServiceRequest service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureCrosHealthdServiceFactoryIsBound();
  cros_healthd_service_factory_->GetProbeService(std::move(service));
}

void ServiceConnectionImpl::EnsureCrosHealthdServiceFactoryIsBound() {
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

  EnsureCrosHealthdServiceFactoryIsBound();
  cros_healthd_service_factory_->GetDiagnosticsService(
      cros_healthd_diagnostics_service_.BindNewPipeAndPassReceiver());
  cros_healthd_diagnostics_service_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnDisconnect, weak_factory_.GetWeakPtr()));
}

void ServiceConnectionImpl::BindCrosHealthdEventServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_event_service_.is_bound())
    return;

  EnsureCrosHealthdServiceFactoryIsBound();
  cros_healthd_service_factory_->GetEventService(
      cros_healthd_event_service_.BindNewPipeAndPassReceiver());
  cros_healthd_event_service_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnDisconnect, weak_factory_.GetWeakPtr()));
}

void ServiceConnectionImpl::BindCrosHealthdProbeServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_probe_service_.is_bound())
    return;

  EnsureCrosHealthdServiceFactoryIsBound();
  cros_healthd_service_factory_->GetProbeService(
      cros_healthd_probe_service_.BindNewPipeAndPassReceiver());
  cros_healthd_probe_service_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnDisconnect, weak_factory_.GetWeakPtr()));
}

ServiceConnectionImpl::ServiceConnectionImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
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

}  // namespace cros_healthd
}  // namespace chromeos
