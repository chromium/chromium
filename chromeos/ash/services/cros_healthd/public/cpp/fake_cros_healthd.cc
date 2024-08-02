// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"

#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_routine_control.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash::cros_healthd {

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

// Used to track the fake instance, mirrors the instance in the base class.
FakeCrosHealthd* g_instance = nullptr;

}  // namespace

FakeCrosHealthd::FakeCrosHealthd() = default;

FakeCrosHealthd::~FakeCrosHealthd() = default;

// static
void FakeCrosHealthd::Initialize() {
  CHECK(!g_instance);
  g_instance = new FakeCrosHealthd();

  CHECK(mojo_service_manager::IsServiceManagerBound())
      << "Healthd requires mojo service manager.";
  auto* proxy = mojo_service_manager::GetServiceManagerProxy();
  proxy->Register(chromeos::mojo_services::kCrosHealthdDiagnostics,
                  g_instance->diagnostics_provider_.BindNewPipeAndPassRemote());
  proxy->Register(chromeos::mojo_services::kCrosHealthdEvent,
                  g_instance->event_provider_.BindNewPipeAndPassRemote());
  proxy->Register(chromeos::mojo_services::kCrosHealthdProbe,
                  g_instance->probe_provider_.BindNewPipeAndPassRemote());
  proxy->Register(chromeos::mojo_services::kCrosHealthdRoutines,
                  g_instance->routines_provider_.BindNewPipeAndPassRemote());
}

// static
void FakeCrosHealthd::Shutdown() {
  // Make sure that the ServiceConnection is created, so it always use fake
  // to bootstrap. Without this, ServiceConnection could be initialized
  // after FakeCrosHealthd is shutdowned in unit tests and causes weird
  // behavior.
  ServiceConnection::GetInstance();

  CHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;

  // After all the receivers in this class are destructed, flush all the mojo
  // remote in ServiceConnection so it will be disconnected and reset. Without
  // this, the mojo object remain in a unstable state and cause errors.
  ServiceConnection::GetInstance()->FlushForTesting();
}

// static
void FakeCrosHealthd::InitializeInBrowserTest() {
  CHECK(!g_instance);
  g_instance = new FakeCrosHealthd();

  CHECK(mojo_service_manager::IsServiceManagerBound());
  auto* proxy = mojo_service_manager::GetServiceManagerProxy();
  proxy->Register(chromeos::mojo_services::kCrosHealthdDiagnostics,
                  g_instance->diagnostics_provider_.BindNewPipeAndPassRemote());
  proxy->Register(chromeos::mojo_services::kCrosHealthdEvent,
                  g_instance->event_provider_.BindNewPipeAndPassRemote());
  proxy->Register(chromeos::mojo_services::kCrosHealthdProbe,
                  g_instance->probe_provider_.BindNewPipeAndPassRemote());
  proxy->Register(chromeos::mojo_services::kCrosHealthdRoutines,
                  g_instance->routines_provider_.BindNewPipeAndPassRemote());
}

// static
void FakeCrosHealthd::ShutdownInBrowserTest() {
  CHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;
}

// static
FakeCrosHealthd* FakeCrosHealthd::Get() {
  return g_instance;
}

void FakeCrosHealthd::SetAvailableRoutinesForTesting(
    const std::vector<mojom::DiagnosticRoutineEnum>& available_routines) {
  available_routines_ = available_routines;
}

void FakeCrosHealthd::SetRunRoutineResponseForTesting(
    mojom::RunRoutineResponsePtr& response) {
  run_routine_response_.Swap(&response);
}

void FakeCrosHealthd::SetGetRoutineUpdateResponseForTesting(
    mojom::RoutineUpdatePtr& response) {
  routine_update_response_.Swap(&response);
}

void FakeCrosHealthd::SetProbeTelemetryInfoResponseForTesting(
    mojom::TelemetryInfoPtr& response_info) {
  telemetry_response_info_.Swap(&response_info);
}

void FakeCrosHealthd::SetIsEventSupportedResponseForTesting(
    mojom::SupportStatusPtr& result) {
  is_event_supported_response_.Swap(&result);
}

void FakeCrosHealthd::SetIsRoutineArgumentSupportedResponseForTesting(
    mojom::SupportStatusPtr& result) {
  is_routine_argument_supported_response_.Swap(&result);
}

void FakeCrosHealthd::FlushRoutineServiceForTesting() {
  routines_provider_.FlushForTesting();
}

FakeRoutineControl* FakeCrosHealthd::GetRoutineControlForArgumentTag(
    mojom::RoutineArgument::Tag tag) {
  auto it = routine_controllers_.find(tag);
  if (it == routine_controllers_.end()) {
    return nullptr;
  }

  return &it->second;
}

void FakeCrosHealthd::SetProbeProcessInfoResponseForTesting(
    mojom::ProcessResultPtr& result) {
  process_response_.Swap(&result);
}

void FakeCrosHealthd::SetProbeMultipleProcessInfoResponseForTesting(
    mojom::MultipleProcessResultPtr& result) {
  multiple_process_response_.Swap(&result);
}

void FakeCrosHealthd::SetExpectedLastPassedDiagnosticsParametersForTesting(
    base::Value::Dict expected_parameters) {
  expected_passed_parameters_ = std::move(expected_parameters);
}

bool FakeCrosHealthd::DidExpectedDiagnosticsParametersMatch() {
  return expected_passed_parameters_ == actual_passed_parameters_;
}

void FakeCrosHealthd::SetCallbackDelay(base::TimeDelta delay) {
  callback_delay_ = delay;
}

void FakeCrosHealthd::EmitEventForCategory(mojom::EventCategoryEnum category,
                                           mojom::EventInfoPtr info) {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  event_provider_.FlushForTesting();

  auto it = event_observers_.find(category);
  if (it == event_observers_.end()) {
    return;
  }

  for (auto& observer : it->second) {
    observer->OnEvent(info.Clone());
  }
}

mojo::RemoteSet<mojom::EventObserver>* FakeCrosHealthd::GetObserversByCategory(
    mojom::EventCategoryEnum category) {
  auto it = event_observers_.find(category);
  if (it == event_observers_.end()) {
    return nullptr;
  }

  return &it->second;
}

void FakeCrosHealthd::EmitConnectionStateChangedEventForTesting(
    const std::string& network_guid,
    chromeos::network_health::mojom::NetworkState state) {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  event_provider_.FlushForTesting();

  for (auto& observer : network_observers_) {
    observer->OnConnectionStateChanged(network_guid, state);
  }
}

void FakeCrosHealthd::EmitSignalStrengthChangedEventForTesting(
    const std::string& network_guid,
    chromeos::network_health::mojom::UInt32ValuePtr signal_strength) {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  event_provider_.FlushForTesting();

  for (auto& observer : network_observers_) {
    observer->OnSignalStrengthChanged(
        network_guid, chromeos::network_health::mojom::UInt32Value::New(
                          signal_strength->value));
  }
}

std::optional<mojom::DiagnosticRoutineEnum> FakeCrosHealthd::GetLastRunRoutine()
    const {
  return last_run_routine_;
}

std::optional<FakeCrosHealthd::RoutineUpdateParams>
FakeCrosHealthd::GetRoutineUpdateParams() const {
  return routine_update_params_;
}

FakeCrosHealthd::RoutineUpdateParams::RoutineUpdateParams(
    int32_t id,
    mojom::DiagnosticRoutineCommandEnum command,
    bool include_output)
    : id(id), command(command), include_output(include_output) {}

void FakeCrosHealthd::GetAvailableRoutines(
    GetAvailableRoutinesCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), available_routines_),
      callback_delay_);
}

void FakeCrosHealthd::GetRoutineUpdate(
    int32_t id,
    mojom::DiagnosticRoutineCommandEnum command,
    bool include_output,
    GetRoutineUpdateCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("id", id);
  actual_passed_parameters_.Set("command", static_cast<int32_t>(command));
  actual_passed_parameters_.Set("include_output", include_output);

  routine_update_params_ =
      FakeCrosHealthd::RoutineUpdateParams(id, command, include_output);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          mojom::RoutineUpdate::New(
              routine_update_response_->progress_percent,
              CloneScopedHandle(&routine_update_response_->output),
              routine_update_response_->routine_update_union.Clone())),
      callback_delay_);
}

void FakeCrosHealthd::RunUrandomRoutine(mojom::NullableUint32Ptr length_seconds,
                                        RunUrandomRoutineCallback callback) {
  actual_passed_parameters_.clear();
  if (!length_seconds.is_null()) {
    actual_passed_parameters_.Set("length_seconds",
                                  static_cast<int>(length_seconds->value));
  }

  last_run_routine_ = mojom::DiagnosticRoutineEnum::kUrandom;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunBatteryCapacityRoutine(
    RunBatteryCapacityRoutineCallback callback) {
  actual_passed_parameters_.clear();

  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBatteryCapacity;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunBatteryHealthRoutine(
    RunBatteryHealthRoutineCallback callback) {
  actual_passed_parameters_.clear();

  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBatteryHealth;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunSmartctlCheckRoutine(
    mojom::NullableUint32Ptr percentage_used_threshold,
    RunSmartctlCheckRoutineCallback callback) {
  actual_passed_parameters_.clear();
  if (!percentage_used_threshold.is_null()) {
    actual_passed_parameters_.Set(
        "percentage_used_threshold",
        static_cast<int>(percentage_used_threshold->value));
  }

  last_run_routine_ =
      mojom::DiagnosticRoutineEnum::kSmartctlCheckWithPercentageUsed;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunAcPowerRoutine(
    mojom::AcPowerStatusEnum expected_status,
    const std::optional<std::string>& expected_power_type,
    RunAcPowerRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("expected_status",
                                static_cast<int32_t>(expected_status));
  if (expected_power_type.has_value()) {
    actual_passed_parameters_.Set("expected_power_type",
                                  expected_power_type.value());
  }

  last_run_routine_ = mojom::DiagnosticRoutineEnum::kAcPower;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunCpuCacheRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunCpuCacheRoutineCallback callback) {
  actual_passed_parameters_.clear();
  if (!length_seconds.is_null()) {
    actual_passed_parameters_.Set("length_seconds",
                                  static_cast<int>(length_seconds->value));
  }

  last_run_routine_ = mojom::DiagnosticRoutineEnum::kCpuCache;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunCpuStressRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunCpuStressRoutineCallback callback) {
  actual_passed_parameters_.clear();
  if (!length_seconds.is_null()) {
    actual_passed_parameters_.Set("length_seconds",
                                  static_cast<int>(length_seconds->value));
  }

  last_run_routine_ = mojom::DiagnosticRoutineEnum::kCpuStress;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunFloatingPointAccuracyRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunFloatingPointAccuracyRoutineCallback callback) {
  actual_passed_parameters_.clear();
  if (!length_seconds.is_null()) {
    actual_passed_parameters_.Set("length_seconds",
                                  static_cast<int>(length_seconds->value));
  }

  last_run_routine_ = mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::DEPRECATED_RunNvmeWearLevelRoutineWithThreshold(
    uint32_t wear_level_threshold,
    DEPRECATED_RunNvmeWearLevelRoutineWithThresholdCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosHealthd::DEPRECATED_RunNvmeWearLevelRoutine(
    mojom::NullableUint32Ptr wear_level_threshold,
    DEPRECATED_RunNvmeWearLevelRoutineCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosHealthd::RunNvmeSelfTestRoutine(
    mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
    RunNvmeSelfTestRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("nvme_self_test_type",
                                static_cast<int>(nvme_self_test_type));

  last_run_routine_ = mojom::DiagnosticRoutineEnum::kNvmeSelfTest;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunDiskReadRoutine(mojom::DiskReadRoutineTypeEnum type,
                                         uint32_t length_seconds,
                                         uint32_t file_size_mb,
                                         RunDiskReadRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("type", static_cast<int>(type));
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int>(length_seconds));
  actual_passed_parameters_.Set("file_size_mb", static_cast<int>(file_size_mb));

  last_run_routine_ = mojom::DiagnosticRoutineEnum::kDiskRead;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunPrimeSearchRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunPrimeSearchRoutineCallback callback) {
  actual_passed_parameters_.clear();
  if (!length_seconds.is_null()) {
    actual_passed_parameters_.Set("length_seconds",
                                  static_cast<int>(length_seconds->value));
  }

  last_run_routine_ = mojom::DiagnosticRoutineEnum::kPrimeSearch;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunBatteryDischargeRoutine(
    uint32_t length_seconds,
    uint32_t maximum_discharge_percent_allowed,
    RunBatteryDischargeRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int>(length_seconds));
  actual_passed_parameters_.Set(
      "maximum_discharge_percent_allowed",
      static_cast<int>(maximum_discharge_percent_allowed));

  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBatteryDischarge;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunBatteryChargeRoutine(
    uint32_t length_seconds,
    uint32_t minimum_charge_percent_required,
    RunBatteryChargeRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int>(length_seconds));
  actual_passed_parameters_.Set(
      "minimum_charge_percent_required",
      static_cast<int>(minimum_charge_percent_required));

  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBatteryCharge;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunMemoryRoutine(
    std::optional<uint32_t> max_testing_mem_kib,
    RunMemoryRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kMemory;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunLanConnectivityRoutine(
    RunLanConnectivityRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kLanConnectivity;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunSignalStrengthRoutine(
    RunSignalStrengthRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kSignalStrength;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunGatewayCanBePingedRoutine(
    RunGatewayCanBePingedRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kGatewayCanBePinged;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunHasSecureWiFiConnectionRoutine(
    RunHasSecureWiFiConnectionRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kHasSecureWiFiConnection;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunDnsResolverPresentRoutine(
    RunDnsResolverPresentRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kDnsResolverPresent;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunDnsLatencyRoutine(
    RunDnsLatencyRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kDnsLatency;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunDnsResolutionRoutine(
    RunDnsResolutionRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kDnsResolution;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunCaptivePortalRoutine(
    RunCaptivePortalRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kCaptivePortal;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunHttpFirewallRoutine(
    RunHttpFirewallRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kHttpFirewall;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunHttpsFirewallRoutine(
    RunHttpsFirewallRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kHttpsFirewall;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunHttpsLatencyRoutine(
    RunHttpsLatencyRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kHttpsLatency;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunVideoConferencingRoutine(
    const std::optional<std::string>& stun_server_hostname,
    RunVideoConferencingRoutineCallback callback) {
  actual_passed_parameters_.clear();
  if (stun_server_hostname.has_value()) {
    actual_passed_parameters_.Set("stun_server_hostname",
                                  stun_server_hostname.value());
  }
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kVideoConferencing;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunArcHttpRoutine(RunArcHttpRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kArcHttp;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunArcPingRoutine(RunArcPingRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kArcPing;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunArcDnsResolutionRoutine(
    RunArcDnsResolutionRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kArcDnsResolution;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunSensitiveSensorRoutine(
    RunSensitiveSensorRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kSensitiveSensor;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunFingerprintRoutine(
    RunFingerprintRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kFingerprint;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunFingerprintAliveRoutine(
    RunFingerprintAliveRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kFingerprintAlive;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunPrivacyScreenRoutine(
    bool target_state,
    RunPrivacyScreenRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("target_state", target_state);
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kPrivacyScreen;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::DEPRECATED_RunLedLitUpRoutine(
    mojom::DEPRECATED_LedName name,
    mojom::DEPRECATED_LedColor color,
    mojo::PendingRemote<mojom::DEPRECATED_LedLitUpRoutineReplier> replier,
    DEPRECATED_RunLedLitUpRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("name", static_cast<int32_t>(name));
  actual_passed_parameters_.Set("color", static_cast<int32_t>(color));

  last_run_routine_ = mojom::DiagnosticRoutineEnum::DEPRECATED_kLedLitUp;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunEmmcLifetimeRoutine(
    RunEmmcLifetimeRoutineCallback callback) {
  actual_passed_parameters_.clear();
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kEmmcLifetime;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::DEPRECATED_RunAudioSetVolumeRoutine(
    uint64_t node_id,
    uint8_t volume,
    bool mute_on,
    DEPRECATED_RunAudioSetVolumeRoutineCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosHealthd::DEPRECATED_RunAudioSetGainRoutine(
    uint64_t node_id,
    uint8_t gain,
    bool deprecated_mute_on,
    DEPRECATED_RunAudioSetGainRoutineCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosHealthd::RunBluetoothPowerRoutine(
    RunBluetoothPowerRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBluetoothPower;
  std::move(callback).Run(run_routine_response_.Clone());
}
void FakeCrosHealthd::RunBluetoothDiscoveryRoutine(
    RunBluetoothDiscoveryRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBluetoothDiscovery;
  std::move(callback).Run(run_routine_response_.Clone());
}
void FakeCrosHealthd::RunBluetoothScanningRoutine(
    ash::cros_healthd::mojom::NullableUint32Ptr length_seconds,
    RunBluetoothScanningRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBluetoothScanning;
  std::move(callback).Run(run_routine_response_.Clone());
}
void FakeCrosHealthd::RunBluetoothPairingRoutine(
    const std::string& peripheral_id,
    RunBluetoothPairingRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBluetoothPairing;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunPowerButtonRoutine(
    uint32_t timeout_seconds,
    RunPowerButtonRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("timeout_seconds",
                                static_cast<int32_t>(timeout_seconds));

  last_run_routine_ = mojom::DiagnosticRoutineEnum::kPowerButton;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunAudioDriverRoutine(
    RunAudioDriverRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kAudioDriver;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunUfsLifetimeRoutine(
    RunUfsLifetimeRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kUfsLifetime;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunFanRoutine(RunFanRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kFan;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::DEPRECATED_AddBluetoothObserver(
    mojo::PendingRemote<mojom::CrosHealthdBluetoothObserver> observer) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosHealthd::DEPRECATED_AddLidObserver(
    mojo::PendingRemote<mojom::CrosHealthdLidObserver> observer) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosHealthd::DEPRECATED_AddPowerObserver(
    mojo::PendingRemote<mojom::CrosHealthdPowerObserver> observer) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosHealthd::AddNetworkObserver(
    mojo::PendingRemote<chromeos::network_health::mojom::NetworkEventsObserver>
        observer) {
  network_observers_.Add(std::move(observer));
}

void FakeCrosHealthd::DEPRECATED_AddAudioObserver(
    mojo::PendingRemote<mojom::CrosHealthdAudioObserver> observer) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosHealthd::DEPRECATED_AddThunderboltObserver(
    mojo::PendingRemote<mojom::CrosHealthdThunderboltObserver> observer) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosHealthd::DEPRECATED_AddUsbObserver(
    mojo::PendingRemote<mojom::CrosHealthdUsbObserver> observer) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosHealthd::AddEventObserver(
    mojom::EventCategoryEnum category,
    mojo::PendingRemote<mojom::EventObserver> observer) {
  auto it = event_observers_.find(category);
  if (it == event_observers_.end()) {
    it = event_observers_.emplace_hint(it, std::piecewise_construct,
                                       std::forward_as_tuple(category),
                                       std::forward_as_tuple());
  }

  it->second.Add(std::move(observer));
}

void FakeCrosHealthd::IsEventSupported(
    ash::cros_healthd::mojom::EventCategoryEnum category,
    IsEventSupportedCallback callback) {
  std::move(callback).Run(is_event_supported_response_.Clone());
}

void FakeCrosHealthd::ProbeTelemetryInfo(
    const std::vector<mojom::ProbeCategoryEnum>& categories,
    ProbeTelemetryInfoCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), telemetry_response_info_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::ProbeProcessInfo(const uint32_t process_id,
                                       ProbeProcessInfoCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), process_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::ProbeMultipleProcessInfo(
    const std::optional<std::vector<uint32_t>>& process_ids,
    bool ignore_single_process_error,
    ProbeMultipleProcessInfoCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), multiple_process_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::CreateRoutine(
    mojom::RoutineArgumentPtr argument,
    mojo::PendingReceiver<mojom::RoutineControl> pending_receiver,
    mojo::PendingRemote<mojom::RoutineObserver> observer) {
  routine_controllers_.emplace(
      std::piecewise_construct, std::forward_as_tuple(argument->which()),
      std::forward_as_tuple(std::move(pending_receiver), std::move(observer)));
}

void FakeCrosHealthd::IsRoutineArgumentSupported(
    mojom::RoutineArgumentPtr arg,
    IsRoutineArgumentSupportedCallback callback) {
  std::move(callback).Run(is_routine_argument_supported_response_->Clone());
}

}  // namespace ash::cros_healthd
