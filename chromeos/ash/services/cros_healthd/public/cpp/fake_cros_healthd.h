// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_CROS_HEALTHD_H_
#define CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_CROS_HEALTHD_H_

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_routine_control.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_exception.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::cros_healthd {
namespace internal {

template <typename MojoInterfaceType>
class ServiceProvider
    : public chromeos::mojo_service_manager::mojom::ServiceProvider {
 public:
  explicit ServiceProvider(MojoInterfaceType* impl) : impl_(impl) {}
  ServiceProvider(const ServiceProvider&) = delete;
  ServiceProvider& operator=(const ServiceProvider&) = delete;
  ~ServiceProvider() override = default;

  // Binds the provider.
  mojo::PendingRemote<chromeos::mojo_service_manager::mojom::ServiceProvider>
  BindNewPipeAndPassRemote() {
    return provider_.BindNewPipeAndPassRemote();
  }

  // Flush the mojo receivers for testing.
  void FlushForTesting() {
    provider_.FlushForTesting();
    service_receiver_set_.FlushForTesting();
  }

 private:
  // chromeos::mojo_service_manager::mojom::ServiceProvider overrides.
  void Request(
      chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
      mojo::ScopedMessagePipeHandle receiver) override {
    service_receiver_set_.Add(
        impl_, mojo::PendingReceiver<MojoInterfaceType>(std::move(receiver)));
  }

  // The provider to receive requests from the service manager.
  mojo::Receiver<chromeos::mojo_service_manager::mojom::ServiceProvider>
      provider_{this};
  // The pointer to the implementation of the mojo interface.
  const raw_ptr<MojoInterfaceType> impl_;
  // The receiver set to keeps the connections from clients to access the mojo
  // service.
  mojo::ReceiverSet<MojoInterfaceType> service_receiver_set_;
};

}  // namespace internal

// This class serves as a fake for all four of cros_healthd's mojo interfaces.
// The factory methods bind to receivers held within FakeCrosHealtdService, and
// all requests on each of the interfaces are fulfilled by
// FakeCrosHealthd.
class FakeCrosHealthd final : public mojom::CrosHealthdDiagnosticsService,
                              public mojom::CrosHealthdEventService,
                              public mojom::CrosHealthdProbeService,
                              public mojom::CrosHealthdRoutinesService {
 public:
  // Stores the params passed to `GetRoutineUpdate`.
  struct RoutineUpdateParams {
    RoutineUpdateParams(int32_t id,
                        mojom::DiagnosticRoutineCommandEnum command,
                        bool include_output);

    int32_t id;
    mojom::DiagnosticRoutineCommandEnum command;
    bool include_output;
  };

  FakeCrosHealthd(const FakeCrosHealthd&) = delete;
  FakeCrosHealthd& operator=(const FakeCrosHealthd&) = delete;

  // Initializes a global instance. This register a fake mojo service for
  // testing. Don't need to call this in browser test because ServiceConnection
  // will initialize this in browser test.
  static void Initialize();

  // Shutdowns the global instance.
  static void Shutdown();

  // Same as above but skip the steps which is not used in browser tests. These
  // also skip mojo flushing to prevent changing the calling sequence when
  // shutting down.
  static void InitializeInBrowserTest();
  static void ShutdownInBrowserTest();

  // Gets the global instance. A `nullptr` could be returned if it is not
  // initialized.
  static FakeCrosHealthd* Get();

  // Set the list of routines that will be used in the response to any
  // GetAvailableRoutines IPCs received.
  void SetAvailableRoutinesForTesting(
      const std::vector<mojom::DiagnosticRoutineEnum>& available_routines);

  // Set the RunRoutine response that will be used in the response to any
  // RunSomeRoutine IPCs received.
  void SetRunRoutineResponseForTesting(mojom::RunRoutineResponsePtr& response);

  // Set the GetRoutineUpdate response that will be used in the response to any
  // GetRoutineUpdate IPCs received.
  void SetGetRoutineUpdateResponseForTesting(mojom::RoutineUpdatePtr& response);

  // Set the TelemetryInfoPtr that will be used in the response to any
  // ProbeTelemetryInfo IPCs received.
  void SetProbeTelemetryInfoResponseForTesting(
      mojom::TelemetryInfoPtr& response_info);

  // Set the ProcessResultPtr that will be used in the response to any
  // ProbeProcessInfo IPCs received.
  void SetProbeProcessInfoResponseForTesting(mojom::ProcessResultPtr& result);

  // Set the MultipleProcessResultPtr that will be used in the response to any
  // ProbeMultipleProcessInfo IPCs received.
  void SetProbeMultipleProcessInfoResponseForTesting(
      mojom::MultipleProcessResultPtr& result);

  // Set the result for a call to `IsEventSupported`.
  void SetIsEventSupportedResponseForTesting(mojom::SupportStatusPtr& result);

  // Set the result for a call to `IsRoutineArgumentSupported`.
  void SetIsRoutineArgumentSupportedResponseForTesting(
      mojom::SupportStatusPtr& result);

  // Flushes the service provider for routines.
  void FlushRoutineServiceForTesting();

  // Gets the `FakeRoutineController` for a certain type of routine. The
  // returned object allows for setting expectations in tests and accessing
  // certain properties that might change during tests. If there is no
  // `FakeRoutineController` registered for a certain type of routine, this
  // returns `nullptr`.
  FakeRoutineControl* GetRoutineControlForArgumentTag(
      mojom::RoutineArgument::Tag tag);

  // Set expectation about the parameter that is passed to a call of
  // a Diagnostics routine (`Run*Routine`) and `GetRoutineUpdate`.
  void SetExpectedLastPassedDiagnosticsParametersForTesting(
      base::Value::Dict expected_parameters);

  // Verifies that the actual passed parameters to Diagnostic
  // routines match the previously set expectations.
  bool DidExpectedDiagnosticsParametersMatch();

  // Adds a delay before the passed callback is called.
  void SetCallbackDelay(base::TimeDelta delay);

  // Calls the `OnEvent` method with `info` on all observers registered for
  // `category`.
  void EmitEventForCategory(mojom::EventCategoryEnum category,
                            mojom::EventInfoPtr info);

  mojo::RemoteSet<mojom::EventObserver>* GetObserversByCategory(
      mojom::EventCategoryEnum category);

  // Calls the network event OnConnectionStateChangedEvent on all registered
  // network observers.
  void EmitConnectionStateChangedEventForTesting(
      const std::string& network_guid,
      chromeos::network_health::mojom::NetworkState state);

  // Calls the network event OnSignalStrengthChangedEvent on all registered
  // network observers.
  void EmitSignalStrengthChangedEventForTesting(
      const std::string& network_guid,
      chromeos::network_health::mojom::UInt32ValuePtr signal_strength);

  // Returns the last created routine by any Run*Routine method.
  std::optional<mojom::DiagnosticRoutineEnum> GetLastRunRoutine() const;

  // Returns the parameters passed for the most recent call to
  // `GetRoutineUpdate`.
  std::optional<RoutineUpdateParams> GetRoutineUpdateParams() const;

 private:
  FakeCrosHealthd();
  ~FakeCrosHealthd() override;

  // CrosHealthdDiagnosticsService overrides:
  void GetAvailableRoutines(GetAvailableRoutinesCallback callback) override;
  void GetRoutineUpdate(int32_t id,
                        mojom::DiagnosticRoutineCommandEnum command,
                        bool include_output,
                        GetRoutineUpdateCallback callback) override;
  void RunUrandomRoutine(mojom::NullableUint32Ptr length_seconds,
                         RunUrandomRoutineCallback callback) override;
  void RunBatteryCapacityRoutine(
      RunBatteryCapacityRoutineCallback callback) override;
  void RunBatteryHealthRoutine(
      RunBatteryHealthRoutineCallback callback) override;
  void RunSmartctlCheckRoutine(
      mojom::NullableUint32Ptr percentage_used_threshold,
      RunSmartctlCheckRoutineCallback callback) override;
  void RunAcPowerRoutine(mojom::AcPowerStatusEnum expected_status,
                         const std::optional<std::string>& expected_power_type,
                         RunAcPowerRoutineCallback callback) override;
  void RunCpuCacheRoutine(mojom::NullableUint32Ptr length_seconds,
                          RunCpuCacheRoutineCallback callback) override;
  void RunCpuStressRoutine(mojom::NullableUint32Ptr length_seconds,
                           RunCpuStressRoutineCallback callback) override;
  void RunFloatingPointAccuracyRoutine(
      mojom::NullableUint32Ptr length_seconds,
      RunFloatingPointAccuracyRoutineCallback callback) override;
  void DEPRECATED_RunNvmeWearLevelRoutineWithThreshold(
      uint32_t wear_level_threshold,
      DEPRECATED_RunNvmeWearLevelRoutineWithThresholdCallback callback)
      override;
  void DEPRECATED_RunNvmeWearLevelRoutine(
      mojom::NullableUint32Ptr wear_level_threshold,
      DEPRECATED_RunNvmeWearLevelRoutineCallback callback) override;
  void RunNvmeSelfTestRoutine(mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
                              RunNvmeSelfTestRoutineCallback callback) override;
  void RunDiskReadRoutine(mojom::DiskReadRoutineTypeEnum type,
                          uint32_t length_seconds,
                          uint32_t file_size_mb,
                          RunDiskReadRoutineCallback callback) override;
  void RunPrimeSearchRoutine(mojom::NullableUint32Ptr length_seconds,
                             RunPrimeSearchRoutineCallback callback) override;
  void RunBatteryDischargeRoutine(
      uint32_t length_seconds,
      uint32_t maximum_discharge_percent_allowed,
      RunBatteryDischargeRoutineCallback callback) override;
  void RunBatteryChargeRoutine(
      uint32_t length_seconds,
      uint32_t minimum_charge_percent_required,
      RunBatteryChargeRoutineCallback callback) override;
  void RunMemoryRoutine(std::optional<uint32_t> max_testing_mem_kib,
                        RunMemoryRoutineCallback callback) override;
  void RunLanConnectivityRoutine(
      RunLanConnectivityRoutineCallback callback) override;
  void RunSignalStrengthRoutine(
      RunSignalStrengthRoutineCallback callback) override;
  void RunGatewayCanBePingedRoutine(
      RunGatewayCanBePingedRoutineCallback callback) override;
  void RunHasSecureWiFiConnectionRoutine(
      RunHasSecureWiFiConnectionRoutineCallback callback) override;
  void RunDnsResolverPresentRoutine(
      RunDnsResolverPresentRoutineCallback callback) override;
  void RunDnsLatencyRoutine(RunDnsLatencyRoutineCallback callback) override;
  void RunDnsResolutionRoutine(
      RunDnsResolutionRoutineCallback callback) override;
  void RunCaptivePortalRoutine(
      RunCaptivePortalRoutineCallback callback) override;
  void RunHttpFirewallRoutine(RunHttpFirewallRoutineCallback callback) override;
  void RunHttpsFirewallRoutine(
      RunHttpsFirewallRoutineCallback callback) override;
  void RunHttpsLatencyRoutine(RunHttpsLatencyRoutineCallback callback) override;
  void RunVideoConferencingRoutine(
      const std::optional<std::string>& stun_server_hostname,
      RunVideoConferencingRoutineCallback callback) override;
  void RunArcHttpRoutine(RunArcHttpRoutineCallback callback) override;
  void RunArcPingRoutine(RunArcPingRoutineCallback callback) override;
  void RunArcDnsResolutionRoutine(
      RunArcDnsResolutionRoutineCallback callback) override;
  void RunSensitiveSensorRoutine(
      RunSensitiveSensorRoutineCallback callback) override;
  void RunFingerprintRoutine(RunFingerprintRoutineCallback callback) override;
  void RunFingerprintAliveRoutine(
      RunFingerprintAliveRoutineCallback callback) override;
  void RunPrivacyScreenRoutine(
      bool target_state,
      RunPrivacyScreenRoutineCallback callback) override;
  void DEPRECATED_RunLedLitUpRoutine(
      mojom::DEPRECATED_LedName name,
      mojom::DEPRECATED_LedColor color,
      mojo::PendingRemote<mojom::DEPRECATED_LedLitUpRoutineReplier> replier,
      DEPRECATED_RunLedLitUpRoutineCallback callback) override;
  void RunEmmcLifetimeRoutine(RunEmmcLifetimeRoutineCallback callback) override;
  void DEPRECATED_RunAudioSetVolumeRoutine(
      uint64_t node_id,
      uint8_t volume,
      bool mute_on,
      DEPRECATED_RunAudioSetVolumeRoutineCallback callback) override;
  void DEPRECATED_RunAudioSetGainRoutine(
      uint64_t node_id,
      uint8_t gain,
      bool deprecated_mute_on,
      DEPRECATED_RunAudioSetGainRoutineCallback callback) override;
  void RunBluetoothPowerRoutine(
      RunBluetoothPowerRoutineCallback callback) override;
  void RunBluetoothDiscoveryRoutine(
      RunBluetoothDiscoveryRoutineCallback callback) override;
  void RunBluetoothScanningRoutine(
      ash::cros_healthd::mojom::NullableUint32Ptr length_seconds,
      RunBluetoothScanningRoutineCallback callback) override;
  void RunBluetoothPairingRoutine(
      const std::string& peripheral_id,
      RunBluetoothPairingRoutineCallback callback) override;
  void RunPowerButtonRoutine(uint32_t timeout_seconds,
                             RunPowerButtonRoutineCallback callback) override;
  void RunAudioDriverRoutine(RunAudioDriverRoutineCallback callback) override;
  void RunUfsLifetimeRoutine(RunUfsLifetimeRoutineCallback callback) override;
  void RunFanRoutine(RunFanRoutineCallback callback) override;

  // CrosHealthdEventService overrides:
  void DEPRECATED_AddBluetoothObserver(
      mojo::PendingRemote<mojom::CrosHealthdBluetoothObserver> observer)
      override;
  void DEPRECATED_AddLidObserver(
      mojo::PendingRemote<mojom::CrosHealthdLidObserver> observer) override;
  void DEPRECATED_AddPowerObserver(
      mojo::PendingRemote<mojom::CrosHealthdPowerObserver> observer) override;
  void AddNetworkObserver(
      mojo::PendingRemote<
          chromeos::network_health::mojom::NetworkEventsObserver> observer)
      override;
  void DEPRECATED_AddAudioObserver(
      mojo::PendingRemote<mojom::CrosHealthdAudioObserver> observer) override;
  void DEPRECATED_AddThunderboltObserver(
      mojo::PendingRemote<mojom::CrosHealthdThunderboltObserver> observer)
      override;
  void DEPRECATED_AddUsbObserver(
      mojo::PendingRemote<mojom::CrosHealthdUsbObserver> observer) override;
  void AddEventObserver(
      ash::cros_healthd::mojom::EventCategoryEnum category,
      mojo::PendingRemote<ash::cros_healthd::mojom::EventObserver> observer)
      override;
  void IsEventSupported(ash::cros_healthd::mojom::EventCategoryEnum category,
                        IsEventSupportedCallback callback) override;

  // CrosHealthdProbeService overrides:
  void ProbeTelemetryInfo(
      const std::vector<mojom::ProbeCategoryEnum>& categories,
      ProbeTelemetryInfoCallback callback) override;
  void ProbeProcessInfo(const uint32_t process_id,
                        ProbeProcessInfoCallback callback) override;
  void ProbeMultipleProcessInfo(
      const std::optional<std::vector<uint32_t>>& process_ids,
      bool ignore_single_process_error,
      ProbeMultipleProcessInfoCallback callback) override;

  // CrosHealthdRoutinesService overrides:
  void CreateRoutine(
      mojom::RoutineArgumentPtr argument,
      mojo::PendingReceiver<mojom::RoutineControl> pending_receiver,
      mojo::PendingRemote<mojom::RoutineObserver> observer) override;
  void IsRoutineArgumentSupported(
      mojom::RoutineArgumentPtr arg,
      IsRoutineArgumentSupportedCallback callback) override;

  // Used as the response to any GetAvailableRoutines IPCs received.
  std::vector<mojom::DiagnosticRoutineEnum> available_routines_;
  // Used to store last created routine by any Run*Routine method.
  std::optional<mojom::DiagnosticRoutineEnum> last_run_routine_;
  // Used as the response to any RunSomeRoutine IPCs received.
  mojom::RunRoutineResponsePtr run_routine_response_{
      mojom::RunRoutineResponse::New()};
  // Used as the response to any GetRoutineUpdate IPCs received.
  mojom::RoutineUpdatePtr routine_update_response_{mojom::RoutineUpdate::New()};
  // Used as the response to any ProbeTelemetryInfo IPCs received.
  mojom::TelemetryInfoPtr telemetry_response_info_{mojom::TelemetryInfo::New()};
  // Used as the response to any IsEventSupported IPCs received.
  mojom::SupportStatusPtr is_event_supported_response_{
      mojom::SupportStatus::NewUnmappedUnionField(0)};
  // Used as the response to any IsRoutineSupported IPCs received.
  mojom::SupportStatusPtr is_routine_argument_supported_response_{
      mojom::SupportStatus::NewUnmappedUnionField(0)};
  // Used as the response to any ProbeProcessInfo IPCs received.
  mojom::ProcessResultPtr process_response_{
      mojom::ProcessResult::NewProcessInfo(mojom::ProcessInfo::New())};
  // Used as the response to any ProbeMultipleProcessInfo IPCs received.
  mojom::MultipleProcessResultPtr multiple_process_response_{
      mojom::MultipleProcessResult::New()};

  // Service providers to provide the services.
  internal::ServiceProvider<mojom::CrosHealthdDiagnosticsService>
      diagnostics_provider_{this};
  internal::ServiceProvider<mojom::CrosHealthdEventService> event_provider_{
      this};
  internal::ServiceProvider<mojom::CrosHealthdProbeService> probe_provider_{
      this};
  internal::ServiceProvider<mojom::CrosHealthdRoutinesService>
      routines_provider_{this};

  // Collection of registered network observers.
  mojo::RemoteSet<chromeos::network_health::mojom::NetworkEventsObserver>
      network_observers_;
  // Collection of registered general observers grouped by category.
  std::map<mojom::EventCategoryEnum, mojo::RemoteSet<mojom::EventObserver>>
      event_observers_;
  std::map<mojom::RoutineArgument::Tag, FakeRoutineControl>
      routine_controllers_;

  // Contains the most recent params passed to `GetRoutineUpdate`, if it has
  // been called.
  std::optional<RoutineUpdateParams> routine_update_params_;

  // Expectation of the passed parameters.
  base::Value::Dict expected_passed_parameters_;
  // Actually passed parameter.
  base::Value::Dict actual_passed_parameters_;

  base::TimeDelta callback_delay_;
};

}  // namespace ash::cros_healthd

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_CROS_HEALTHD_H_
