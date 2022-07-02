// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_
#define CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_

#include <sys/types.h>

#include <cstdint>
#include <string>

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "chromeos/ash/services/cros_healthd/private/mojom/cros_healthd_internal.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace cros_healthd {

// Encapsulates a connection to the Chrome OS cros_healthd daemon via its Mojo
// interface.
// Sequencing: Must be used on a single sequence (may be created on another).
class ServiceConnection {
 public:
  static ServiceConnection* GetInstance();

  using BindNetworkHealthServiceCallback =
      base::RepeatingCallback<mojo::PendingRemote<
          chromeos::network_health::mojom::NetworkHealthService>()>;
  using BindNetworkDiagnosticsRoutinesCallback =
      base::RepeatingCallback<mojo::PendingRemote<
          chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>()>;

  // Retrieve a list of available diagnostic routines. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void GetAvailableRoutines(
      mojom::CrosHealthdDiagnosticsService::GetAvailableRoutinesCallback
          callback) = 0;

  // Send a command to an existing routine. Also returns status information
  // for the routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void GetRoutineUpdate(
      int32_t id,
      mojom::DiagnosticRoutineCommandEnum command,
      bool include_output,
      mojom::CrosHealthdDiagnosticsService::GetRoutineUpdateCallback
          callback) = 0;

  // Requests that cros_healthd runs the urandom routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunUrandomRoutine(
      const absl::optional<base::TimeDelta>& length_seconds,
      mojom::CrosHealthdDiagnosticsService::RunUrandomRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the battery capacity routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunBatteryCapacityRoutine(
      mojom::CrosHealthdDiagnosticsService::RunBatteryCapacityRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the battery health routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunBatteryHealthRoutine(
      mojom::CrosHealthdDiagnosticsService::RunBatteryHealthRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the smartcl check routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunSmartctlCheckRoutine(
      mojom::CrosHealthdDiagnosticsService::RunSmartctlCheckRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the AC power routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunAcPowerRoutine(
      mojom::AcPowerStatusEnum expected_status,
      const absl::optional<std::string>& expected_power_type,
      mojom::CrosHealthdDiagnosticsService::RunAcPowerRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the CPU cache routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunCpuCacheRoutine(
      const absl::optional<base::TimeDelta>& exec_duration,
      mojom::CrosHealthdDiagnosticsService::RunCpuCacheRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the CPU stress routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunCpuStressRoutine(
      const absl::optional<base::TimeDelta>& exec_duration,
      mojom::CrosHealthdDiagnosticsService::RunCpuStressRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the floating point accuracy routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunFloatingPointAccuracyRoutine(
      const absl::optional<base::TimeDelta>& exec_duration,
      mojom::CrosHealthdDiagnosticsService::
          RunFloatingPointAccuracyRoutineCallback callback) = 0;

  // Requests that cros_healthd runs the NVMe wear-level routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunNvmeWearLevelRoutine(
      uint32_t wear_level_threshold,
      mojom::CrosHealthdDiagnosticsService::RunNvmeWearLevelRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the NVMe self-test routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunNvmeSelfTestRoutine(
      mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
      mojom::CrosHealthdDiagnosticsService::RunNvmeSelfTestRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the Disk Read routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunDiskReadRoutine(
      mojom::DiskReadRoutineTypeEnum type,
      base::TimeDelta& exec_duration,
      uint32_t file_size_mb,
      mojom::CrosHealthdDiagnosticsService::RunDiskReadRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the prime search routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunPrimeSearchRoutine(
      const absl::optional<base::TimeDelta>& exec_duration,
      mojom::CrosHealthdDiagnosticsService::RunPrimeSearchRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the battery discharge routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunBatteryDischargeRoutine(
      base::TimeDelta exec_duration,
      uint32_t maximum_discharge_percent_allowed,
      mojom::CrosHealthdDiagnosticsService::RunBatteryDischargeRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the battery charge routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunBatteryChargeRoutine(
      base::TimeDelta exec_duration,
      uint32_t minimum_charge_percent_required,
      mojom::CrosHealthdDiagnosticsService::RunBatteryChargeRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the memory routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunMemoryRoutine(
      mojom::CrosHealthdDiagnosticsService::RunMemoryRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the lan connectivity routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunLanConnectivityRoutine(
      mojom::CrosHealthdDiagnosticsService::RunLanConnectivityRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the signal strength routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunSignalStrengthRoutine(
      mojom::CrosHealthdDiagnosticsService::RunSignalStrengthRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the gateway can be pinged routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunGatewayCanBePingedRoutine(
      mojom::CrosHealthdDiagnosticsService::RunGatewayCanBePingedRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the has secure wifi routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunHasSecureWiFiConnectionRoutine(
      mojom::CrosHealthdDiagnosticsService::
          RunHasSecureWiFiConnectionRoutineCallback callback) = 0;

  // Requests that cros_healthd runs DNS resolver present routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunDnsResolverPresentRoutine(
      mojom::CrosHealthdDiagnosticsService::RunDnsResolverPresentRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the DNS latency routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunDnsLatencyRoutine(
      mojom::CrosHealthdDiagnosticsService::RunDnsLatencyRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the DNS resolution routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunDnsResolutionRoutine(
      mojom::CrosHealthdDiagnosticsService::RunDnsResolutionRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the captive portal routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunCaptivePortalRoutine(
      mojom::CrosHealthdDiagnosticsService::RunCaptivePortalRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the HTTP firewall routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunHttpFirewallRoutine(
      mojom::CrosHealthdDiagnosticsService::RunHttpFirewallRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the HTTPS firewall routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunHttpsFirewallRoutine(
      mojom::CrosHealthdDiagnosticsService::RunHttpsFirewallRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the HTTPS latency routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunHttpsLatencyRoutine(
      mojom::CrosHealthdDiagnosticsService::RunHttpsLatencyRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the ARC HTTP routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunArcHttpRoutine(
      mojom::CrosHealthdDiagnosticsService::RunArcHttpRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the ARC PING routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunArcPingRoutine(
      mojom::CrosHealthdDiagnosticsService::RunArcPingRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the ARC DNS resolution routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunArcDnsResolutionRoutine(
      mojom::CrosHealthdDiagnosticsService::RunArcDnsResolutionRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the video conferencing routine. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunVideoConferencingRoutine(
      const absl::optional<std::string>& stun_server_hostname,
      mojom::CrosHealthdDiagnosticsService::RunVideoConferencingRoutineCallback
          callback) = 0;

  // Subscribes to cros_healthd's Bluetooth-related events. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void AddBluetoothObserver(
      mojo::PendingRemote<mojom::CrosHealthdBluetoothObserver>
          pending_observer) = 0;

  // Subscribes to cros_healthd's lid-related events. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void AddLidObserver(
      mojo::PendingRemote<mojom::CrosHealthdLidObserver> pending_observer) = 0;

  // Subscribes to cros_healthd's power-related events. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void AddPowerObserver(
      mojo::PendingRemote<mojom::CrosHealthdPowerObserver>
          pending_observer) = 0;

  // Subscribes to cros_healthd's network-related events. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void AddNetworkObserver(
      mojo::PendingRemote<
          chromeos::network_health::mojom::NetworkEventsObserver>
          pending_observer) = 0;

  // Subscribes to cros_healthd's audio-related events. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void AddAudioObserver(
      mojo::PendingRemote<mojom::CrosHealthdAudioObserver>
          pending_observer) = 0;

  // Subscribes to cros_healthd's Thunderbolt-related events. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void AddThunderboltObserver(
      mojo::PendingRemote<mojom::CrosHealthdThunderboltObserver>
          pending_observer) = 0;

  // Subscribes to cros_healthd's USB-related events. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void AddUsbObserver(
      mojo::PendingRemote<mojom::CrosHealthdUsbObserver> pending_observer) = 0;

  // Gathers pieces of information about the platform. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void ProbeTelemetryInfo(
      const std::vector<mojom::ProbeCategoryEnum>& categories_to_test,
      mojom::CrosHealthdProbeService::ProbeTelemetryInfoCallback callback) = 0;

  // Gathers information about a particular process on the device. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void ProbeProcessInfo(
      pid_t process_id,
      mojom::CrosHealthdProbeService::ProbeProcessInfoCallback callback) = 0;

  // Binds |service| to an implementation of CrosHealthdDiagnosticsService. In
  // production, this implementation is provided by cros_healthd. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void GetDiagnosticsService(
      mojo::PendingReceiver<mojom::CrosHealthdDiagnosticsService> service) = 0;

  // Binds |service| to an implementation of CrosHealthdProbeService. In
  // production, this implementation is provided by cros_healthd. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void GetProbeService(
      mojo::PendingReceiver<mojom::CrosHealthdProbeService> service) = 0;

  // Sets a callback to request binding a PendingRemote to the
  // NetworkHealthService. This callback is invoked once when it is set, and
  // anytime the mojo connection to CrosHealthd is disconnected.
  virtual void SetBindNetworkHealthServiceCallback(
      BindNetworkHealthServiceCallback callback) = 0;

  // Sets a callback to request binding a PendingRemote to the
  // NetworkDiagnosticsRoutines interface. This callback is invoked once when it
  // is set, and anytime the mojo connection to CrosHealthd is disconnected.
  virtual void SetBindNetworkDiagnosticsRoutinesCallback(
      BindNetworkDiagnosticsRoutinesCallback callback) = 0;

  // Sends the ChromiumDataCollector interface to cros_healthd.
  virtual void SendChromiumDataCollector(
      mojo::PendingRemote<
          chromeos::cros_healthd::internal::mojom::ChromiumDataCollector>
          remote) = 0;

  // Fetch touchpad stack driver library name.
  virtual std::string FetchTouchpadLibraryName() = 0;

  // Calls FlushForTesting method on all mojo::Remote objects owned by
  // ServiceConnection. This method can be used for example to gracefully
  // observe destruction of the cros_healthd client.
  virtual void FlushForTesting() = 0;

 protected:
  ServiceConnection() = default;
  virtual ~ServiceConnection() = default;
};

}  // namespace cros_healthd
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
namespace cros_healthd {
using ::chromeos::cros_healthd::ServiceConnection;
}  // namespace cros_healthd
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_
