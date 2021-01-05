// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/system_routine_controller.h"

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chromeos/components/diagnostics_ui/backend/cros_healthd_helpers.h"
#include "chromeos/components/diagnostics_ui/backend/histogram_util.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"

namespace chromeos {
namespace diagnostics {
namespace {

namespace healthd = cros_healthd::mojom;

constexpr uint32_t kBatteryChargeMinimumPercent = 0;
constexpr uint32_t kBatteryDischargeMaximumPercent = 100;
constexpr uint32_t kBatteryDurationInSeconds = 30;
constexpr uint32_t kCpuCacheDurationInSeconds = 60;
constexpr uint32_t kCpuFloatingPointDurationInSeconds = 60;
constexpr uint32_t kCpuPrimeDurationInSeconds = 60;
constexpr uint32_t kCpuStressDurationInSeconds = 60;
constexpr uint32_t kExpectedMemoryDurationInSeconds = 1000;
constexpr uint32_t kRoutineResultRefreshIntervalInSeconds = 1;

constexpr char kChargePercentKey[] = "chargePercent";
constexpr char kDischargePercentKey[] = "dischargePercent";
constexpr char kResultDetailsKey[] = "resultDetails";

mojom::RoutineResultInfoPtr ConstructStandardRoutineResultInfoPtr(
    mojom::RoutineType type,
    mojom::StandardRoutineResult result) {
  auto routine_result = mojom::RoutineResult::NewSimpleResult(result);
  return mojom::RoutineResultInfo::New(type, std::move(routine_result));
}

// Converts a cros_healthd::mojom::DiagnosticRoutineStatusEnum to a
// mojom::StandardRoutineResult. Should only be called to construct the final
// response. Should not be called for in-progess statuses.
mojom::StandardRoutineResult TestStatusToResult(
    healthd::DiagnosticRoutineStatusEnum status) {
  switch (status) {
    case healthd::DiagnosticRoutineStatusEnum::kPassed:
      return mojom::StandardRoutineResult::kTestPassed;
    case healthd::DiagnosticRoutineStatusEnum::kFailed:
      return mojom::StandardRoutineResult::kTestFailed;
    case healthd::DiagnosticRoutineStatusEnum::kCancelled:
    case healthd::DiagnosticRoutineStatusEnum::kError:
      return mojom::StandardRoutineResult::kExecutionError;
    case healthd::DiagnosticRoutineStatusEnum::kFailedToStart:
    case healthd::DiagnosticRoutineStatusEnum::kUnsupported:
    case healthd::DiagnosticRoutineStatusEnum::kNotRun:
      return mojom::StandardRoutineResult::kUnableToRun;
    case healthd::DiagnosticRoutineStatusEnum::kReady:
    case healthd::DiagnosticRoutineStatusEnum::kRunning:
    case healthd::DiagnosticRoutineStatusEnum::kWaiting:
    case healthd::DiagnosticRoutineStatusEnum::kRemoved:
    case healthd::DiagnosticRoutineStatusEnum::kCancelling:
      NOTREACHED();
      return mojom::StandardRoutineResult::kExecutionError;
  }
}

mojom::RoutineResultInfoPtr ConstructPowerRoutineResultInfoPtr(
    mojom::RoutineType type,
    mojom::StandardRoutineResult result,
    double percent_change,
    uint32_t seconds_elapsed) {
  auto power_result =
      mojom::PowerRoutineResult::New(result, percent_change, seconds_elapsed);
  auto routine_result =
      mojom::RoutineResult::NewPowerResult(std::move(power_result));
  return mojom::RoutineResultInfo::New(type, std::move(routine_result));
}

uint32_t GetExpectedRoutineDurationInSeconds(mojom::RoutineType routine_type) {
  switch (routine_type) {
    case mojom::RoutineType::kBatteryCharge:
      return kBatteryDurationInSeconds;
    case mojom::RoutineType::kBatteryDischarge:
      return kBatteryDurationInSeconds;
    case mojom::RoutineType::kCpuCache:
      return kCpuCacheDurationInSeconds;
    case mojom::RoutineType::kCpuFloatingPoint:
      return kCpuFloatingPointDurationInSeconds;
    case mojom::RoutineType::kCpuPrime:
      return kCpuPrimeDurationInSeconds;
    case mojom::RoutineType::kCpuStress:
      return kCpuCacheDurationInSeconds;
    case mojom::RoutineType::kMemory:
      return kExpectedMemoryDurationInSeconds;
  }
}

bool IsPowerRoutine(mojom::RoutineType routine_type) {
  return routine_type == mojom::RoutineType::kBatteryCharge ||
         routine_type == mojom::RoutineType::kBatteryDischarge;
}

std::string ReadMojoHandleToJsonString(mojo::PlatformHandle handle) {
  base::File file(handle.ReleaseFD());
  std::vector<uint8_t> contents;
  contents.resize(file.GetLength());
  if (!file.ReadAndCheck(0, contents)) {
    return std::string();
  }
  return std::string(contents.begin(), contents.end());
}

bool IsKnownRoutine(healthd::DiagnosticRoutineEnum routine_enum) {
  switch (routine_enum) {
    case healthd::DiagnosticRoutineEnum::kBatteryCharge:
    case healthd::DiagnosticRoutineEnum::kBatteryDischarge:
    case healthd::DiagnosticRoutineEnum::kCpuCache:
    case healthd::DiagnosticRoutineEnum::kCpuStress:
    case healthd::DiagnosticRoutineEnum::kFloatingPointAccuracy:
    case healthd::DiagnosticRoutineEnum::kMemory:
    case healthd::DiagnosticRoutineEnum::kPrimeSearch:
      return true;
    case healthd::DiagnosticRoutineEnum::kAcPower:
    case healthd::DiagnosticRoutineEnum::kBatteryCapacity:
    case healthd::DiagnosticRoutineEnum::kBatteryHealth:
    case healthd::DiagnosticRoutineEnum::kCaptivePortal:
    case healthd::DiagnosticRoutineEnum::kDiskRead:
    case healthd::DiagnosticRoutineEnum::kDnsLatency:
    case healthd::DiagnosticRoutineEnum::kDnsResolution:
    case healthd::DiagnosticRoutineEnum::kDnsResolverPresent:
    case healthd::DiagnosticRoutineEnum::kGatewayCanBePinged:
    case healthd::DiagnosticRoutineEnum::kHasSecureWiFiConnection:
    case healthd::DiagnosticRoutineEnum::kHttpFirewall:
    case healthd::DiagnosticRoutineEnum::kHttpsFirewall:
    case healthd::DiagnosticRoutineEnum::kHttpsLatency:
    case healthd::DiagnosticRoutineEnum::kLanConnectivity:
    case healthd::DiagnosticRoutineEnum::kNvmeSelfTest:
    case healthd::DiagnosticRoutineEnum::kNvmeWearLevel:
    case healthd::DiagnosticRoutineEnum::kSignalStrength:
    case healthd::DiagnosticRoutineEnum::kSmartctlCheck:
    case healthd::DiagnosticRoutineEnum::kUrandom:
      return false;
  }
}

mojom::RoutineType DiagnosticRoutineEnumToRoutineType(
    healthd::DiagnosticRoutineEnum routine_enum) {
  switch (routine_enum) {
    case healthd::DiagnosticRoutineEnum::kBatteryCharge:
      return mojom::RoutineType::kBatteryCharge;
    case healthd::DiagnosticRoutineEnum::kBatteryDischarge:
      return mojom::RoutineType::kBatteryDischarge;
    case healthd::DiagnosticRoutineEnum::kCpuCache:
      return mojom::RoutineType::kCpuCache;
    case healthd::DiagnosticRoutineEnum::kCpuStress:
      return mojom::RoutineType::kCpuStress;
    case healthd::DiagnosticRoutineEnum::kFloatingPointAccuracy:
      return mojom::RoutineType::kCpuFloatingPoint;
    case healthd::DiagnosticRoutineEnum::kMemory:
      return mojom::RoutineType::kMemory;
    case healthd::DiagnosticRoutineEnum::kPrimeSearch:
      return mojom::RoutineType::kCpuPrime;
    case healthd::DiagnosticRoutineEnum::kAcPower:
    case healthd::DiagnosticRoutineEnum::kBatteryCapacity:
    case healthd::DiagnosticRoutineEnum::kBatteryHealth:
    case healthd::DiagnosticRoutineEnum::kCaptivePortal:
    case healthd::DiagnosticRoutineEnum::kDiskRead:
    case healthd::DiagnosticRoutineEnum::kDnsLatency:
    case healthd::DiagnosticRoutineEnum::kDnsResolution:
    case healthd::DiagnosticRoutineEnum::kDnsResolverPresent:
    case healthd::DiagnosticRoutineEnum::kGatewayCanBePinged:
    case healthd::DiagnosticRoutineEnum::kHasSecureWiFiConnection:
    case healthd::DiagnosticRoutineEnum::kHttpFirewall:
    case healthd::DiagnosticRoutineEnum::kHttpsFirewall:
    case healthd::DiagnosticRoutineEnum::kHttpsLatency:
    case healthd::DiagnosticRoutineEnum::kLanConnectivity:
    case healthd::DiagnosticRoutineEnum::kNvmeSelfTest:
    case healthd::DiagnosticRoutineEnum::kNvmeWearLevel:
    case healthd::DiagnosticRoutineEnum::kSignalStrength:
    case healthd::DiagnosticRoutineEnum::kSmartctlCheck:
    case healthd::DiagnosticRoutineEnum::kUrandom:
      NOTREACHED() << "DiagnosticRoutineEnumToRoutineType called with "
                      "unsupported routine.";
      return mojom::RoutineType::kBatteryCharge;
  }
}

}  // namespace

SystemRoutineController::SystemRoutineController() {
  inflight_routine_timer_ = std::make_unique<base::OneShotTimer>();
}

SystemRoutineController::~SystemRoutineController() {
  if (inflight_routine_runner_) {
    // Since SystemRoutineController is torn down at the same time as the
    // frontend, there's no guarantee that the disconnect handler will be
    // called. If there's a routine inflight, cancel it but do not pass a
    // callback.
    BindCrosHealthdDiagnosticsServiceIfNeccessary();
    diagnostics_service_->GetRoutineUpdate(
        inflight_routine_id_, healthd::DiagnosticRoutineCommandEnum::kCancel,
        /*should_include_output=*/false, base::DoNothing());
  }

  // Emit the total number of routines run.
  metrics::EmitRoutineRunCount(routine_count_);
}

void SystemRoutineController::RunRoutine(
    mojom::RoutineType type,
    mojo::PendingRemote<mojom::RoutineRunner> runner) {
  if (IsRoutineRunning()) {
    // If a routine is already running, alert the caller that we were unable
    // to start the routine.
    mojo::Remote<mojom::RoutineRunner> routine_runner(std::move(runner));
    auto result = ConstructStandardRoutineResultInfoPtr(
        type, mojom::StandardRoutineResult::kUnableToRun);
    routine_runner->OnRoutineResult(std::move(result));
    return;
  }

  ++routine_count_;

  inflight_routine_runner_ =
      mojo::Remote<mojom::RoutineRunner>(std::move(runner));
  inflight_routine_runner_.set_disconnect_handler(base::BindOnce(
      &SystemRoutineController::OnInflightRoutineRunnerDisconnected,
      base::Unretained(this)));
  ExecuteRoutine(type);
}

void SystemRoutineController::GetSupportedRoutines(
    GetSupportedRoutinesCallback callback) {
  BindCrosHealthdDiagnosticsServiceIfNeccessary();
  diagnostics_service_->GetAvailableRoutines(
      base::BindOnce(&SystemRoutineController::OnAvailableRoutinesFetched,
                     base::Unretained(this), std::move(callback)));
}

void SystemRoutineController::BindInterface(
    mojo::PendingReceiver<mojom::SystemRoutineController> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void SystemRoutineController::OnAvailableRoutinesFetched(
    GetSupportedRoutinesCallback callback,
    const std::vector<healthd::DiagnosticRoutineEnum>& available_routines) {
  std::vector<mojom::RoutineType> supported_routines;
  for (const auto& routine : available_routines) {
    if (IsKnownRoutine(routine)) {
      supported_routines.push_back(DiagnosticRoutineEnumToRoutineType(routine));
    }
  }
  std::move(callback).Run(supported_routines);
}

void SystemRoutineController::ExecuteRoutine(mojom::RoutineType routine_type) {
  BindCrosHealthdDiagnosticsServiceIfNeccessary();

  switch (routine_type) {
    case mojom::RoutineType::kBatteryCharge:
      diagnostics_service_->RunBatteryChargeRoutine(
          kBatteryDurationInSeconds, kBatteryChargeMinimumPercent,
          base::BindOnce(&SystemRoutineController::OnPowerRoutineStarted,
                         base::Unretained(this), routine_type));

      return;
    case mojom::RoutineType::kBatteryDischarge:
      diagnostics_service_->RunBatteryDischargeRoutine(
          kBatteryDurationInSeconds, kBatteryDischargeMaximumPercent,
          base::BindOnce(&SystemRoutineController::OnPowerRoutineStarted,
                         base::Unretained(this), routine_type));

      return;
    case mojom::RoutineType::kCpuCache:
      diagnostics_service_->RunCpuCacheRoutine(
          healthd::NullableUint32::New(kCpuCacheDurationInSeconds),
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         base::Unretained(this), routine_type));
      return;
    case mojom::RoutineType::kCpuFloatingPoint:
      diagnostics_service_->RunFloatingPointAccuracyRoutine(
          healthd::NullableUint32::New(kCpuFloatingPointDurationInSeconds),
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         base::Unretained(this), routine_type));
      return;
    case mojom::RoutineType::kCpuPrime:
      diagnostics_service_->RunPrimeSearchRoutine(
          healthd::NullableUint32::New(kCpuPrimeDurationInSeconds),
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         base::Unretained(this), routine_type));
      return;
    case mojom::RoutineType::kCpuStress:
      diagnostics_service_->RunCpuStressRoutine(
          healthd::NullableUint32::New(kCpuStressDurationInSeconds),
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         base::Unretained(this), routine_type));
      return;
    case mojom::RoutineType::kMemory:
      diagnostics_service_->RunMemoryRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         base::Unretained(this), routine_type));
      return;
  }
}

void SystemRoutineController::OnRoutineStarted(
    mojom::RoutineType routine_type,
    healthd::RunRoutineResponsePtr response_ptr) {
  DCHECK(!IsPowerRoutine(routine_type));
  // Check for error conditions.
  // TODO(baileyberro): Handle additional statuses.
  if (response_ptr->status ==
          healthd::DiagnosticRoutineStatusEnum::kFailedToStart ||
      response_ptr->id == healthd::kFailedToStartId) {
    OnStandardRoutineResult(mojom::RoutineType::kCpuStress,
                            TestStatusToResult(response_ptr->status));
    return;
  }
  DCHECK_EQ(healthd::DiagnosticRoutineStatusEnum::kRunning,
            response_ptr->status);

  DCHECK_EQ(kInvalidRoutineId, inflight_routine_id_);
  inflight_routine_id_ = response_ptr->id;

  // Sleep for the length of the test using a one-shot timer, then start
  // querying again for status.
  ScheduleCheckRoutineStatus(GetExpectedRoutineDurationInSeconds(routine_type),
                             routine_type);
}

void SystemRoutineController::OnPowerRoutineStarted(
    mojom::RoutineType routine_type,
    healthd::RunRoutineResponsePtr response_ptr) {
  DCHECK(IsPowerRoutine(routine_type));
  // TODO(baileyberro): Handle additional statuses.
  if (response_ptr->status != healthd::DiagnosticRoutineStatusEnum::kWaiting) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    return;
  }

  DCHECK_EQ(kInvalidRoutineId, inflight_routine_id_);
  inflight_routine_id_ = response_ptr->id;

  ContinuePowerRoutine(routine_type);
}

void SystemRoutineController::ContinuePowerRoutine(
    mojom::RoutineType routine_type) {
  DCHECK(IsPowerRoutine(routine_type));

  BindCrosHealthdDiagnosticsServiceIfNeccessary();
  diagnostics_service_->GetRoutineUpdate(
      inflight_routine_id_, healthd::DiagnosticRoutineCommandEnum::kContinue,
      /*should_include_output=*/true,
      base::BindOnce(&SystemRoutineController::OnPowerRoutineContinued,
                     base::Unretained(this), routine_type));
}

void SystemRoutineController::OnPowerRoutineContinued(
    mojom::RoutineType routine_type,
    healthd::RoutineUpdatePtr update_ptr) {
  DCHECK(IsPowerRoutine(routine_type));

  const healthd::NonInteractiveRoutineUpdate* update =
      GetNonInteractiveRoutineUpdate(*update_ptr);

  if (!update ||
      update->status != healthd::DiagnosticRoutineStatusEnum::kRunning) {
    DVLOG(2) << "Failed to resume power routine.";
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    return;
  }

  ScheduleCheckRoutineStatus(GetExpectedRoutineDurationInSeconds(routine_type),
                             routine_type);
}

void SystemRoutineController::CheckRoutineStatus(
    mojom::RoutineType routine_type) {
  DCHECK_NE(kInvalidRoutineId, inflight_routine_id_);
  BindCrosHealthdDiagnosticsServiceIfNeccessary();
  const bool should_include_output = IsPowerRoutine(routine_type);
  diagnostics_service_->GetRoutineUpdate(
      inflight_routine_id_, healthd::DiagnosticRoutineCommandEnum::kGetStatus,
      should_include_output,
      base::BindOnce(&SystemRoutineController::OnRoutineStatusUpdated,
                     base::Unretained(this), routine_type));
}

void SystemRoutineController::OnRoutineStatusUpdated(
    mojom::RoutineType routine_type,
    healthd::RoutineUpdatePtr update_ptr) {
  if (IsPowerRoutine(routine_type)) {
    HandlePowerRoutineStatusUpdate(routine_type, std::move(update_ptr));
    return;
  }

  const healthd::NonInteractiveRoutineUpdate* update =
      GetNonInteractiveRoutineUpdate(*update_ptr);

  if (!update) {
    DVLOG(2) << "Invalid routine update";
    OnStandardRoutineResult(routine_type,
                            mojom::StandardRoutineResult::kExecutionError);
    return;
  }

  const healthd::DiagnosticRoutineStatusEnum status = update->status;

  switch (status) {
    case healthd::DiagnosticRoutineStatusEnum::kRunning:
      // If still running, continue to repoll until it is finished.
      // TODO(baileyberro): Consider adding a timeout mechanism.
      ScheduleCheckRoutineStatus(kRoutineResultRefreshIntervalInSeconds,
                                 routine_type);
      return;
    case healthd::DiagnosticRoutineStatusEnum::kPassed:
    case healthd::DiagnosticRoutineStatusEnum::kFailed:
      OnStandardRoutineResult(routine_type, TestStatusToResult(status));
      return;
    case healthd::DiagnosticRoutineStatusEnum::kCancelled:
    case healthd::DiagnosticRoutineStatusEnum::kError:
    case healthd::DiagnosticRoutineStatusEnum::kFailedToStart:
    case healthd::DiagnosticRoutineStatusEnum::kUnsupported:
    case healthd::DiagnosticRoutineStatusEnum::kReady:
    case healthd::DiagnosticRoutineStatusEnum::kWaiting:
    case healthd::DiagnosticRoutineStatusEnum::kRemoved:
    case healthd::DiagnosticRoutineStatusEnum::kCancelling:
    case healthd::DiagnosticRoutineStatusEnum::kNotRun:
      // Any other reason, report failure.
      DVLOG(2) << "Routine failed: " << update->status_message;
      OnStandardRoutineResult(routine_type, TestStatusToResult(status));
      return;
  }
}

void SystemRoutineController::HandlePowerRoutineStatusUpdate(
    mojom ::RoutineType routine_type,
    cros_healthd::mojom::RoutineUpdatePtr update_ptr) {
  DCHECK(IsPowerRoutine(routine_type));

  const healthd::NonInteractiveRoutineUpdate* update =
      GetNonInteractiveRoutineUpdate(*update_ptr);

  if (!update) {
    DVLOG(2) << "Invalid routine update";
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    return;
  }

  const healthd::DiagnosticRoutineStatusEnum status = update->status;

  // If still running, continue to repoll until it is finished.
  // TODO(baileyberro): Consider adding a timeout mechanism.
  if (status == healthd::DiagnosticRoutineStatusEnum::kRunning) {
    ScheduleCheckRoutineStatus(kRoutineResultRefreshIntervalInSeconds,
                               routine_type);
    return;
  }

  // If test passed, report result.
  if (status == healthd::DiagnosticRoutineStatusEnum::kPassed) {
    ParsePowerRoutineResult(routine_type,
                            mojom::StandardRoutineResult::kTestPassed,
                            std::move(update_ptr->output));
    return;
  }

  // If test failed, report result.
  if (status == healthd::DiagnosticRoutineStatusEnum::kFailed) {
    ParsePowerRoutineResult(routine_type,
                            mojom::StandardRoutineResult::kTestFailed,
                            std::move(update_ptr->output));
    return;
  }

  // Any other reason, report failure.
  DVLOG(2) << "Routine failed: " << update->status_message;
  OnPowerRoutineResult(routine_type,
                       mojom::StandardRoutineResult::kExecutionError,
                       /*percent_change=*/0, /*seconds_elapsed=*/0);
}

bool SystemRoutineController::IsRoutineRunning() const {
  return inflight_routine_runner_.is_bound();
}

void SystemRoutineController::ScheduleCheckRoutineStatus(
    uint32_t duration_in_seconds,
    mojom::RoutineType routine_type) {
  inflight_routine_timer_->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(duration_in_seconds),
      base::BindOnce(&SystemRoutineController::CheckRoutineStatus,
                     base::Unretained(this), routine_type));
}

void SystemRoutineController::ParsePowerRoutineResult(
    mojom::RoutineType routine_type,
    mojom::StandardRoutineResult result,
    mojo::ScopedHandle output_handle) {
  if (!output_handle.is_valid()) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    return;
  }

  mojo::PlatformHandle platform_handle =
      mojo::UnwrapPlatformHandle(std::move(output_handle));
  if (!platform_handle.is_valid()) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ReadMojoHandleToJsonString, std::move(platform_handle)),
      base::BindOnce(&SystemRoutineController::OnPowerRoutineResultFetched,
                     weak_factory_.GetWeakPtr(), routine_type));
}

void SystemRoutineController::OnPowerRoutineResultFetched(
    mojom::RoutineType routine_type,
    const std::string& file_contents) {
  if (file_contents.empty()) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    DVLOG(2) << "Empty Power Routine Result File.";
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      file_contents,
      base::BindOnce(&SystemRoutineController::OnPowerRoutineJsonParsed,
                     weak_factory_.GetWeakPtr(), routine_type));
  return;
}

void SystemRoutineController::OnPowerRoutineJsonParsed(
    mojom::RoutineType routine_type,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    DVLOG(2) << "JSON parsing failed: " << *result.error;
    return;
  }

  const base::Value& parsed_json = *result.value;

  if (parsed_json.type() != base::Value::Type::DICTIONARY) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    DVLOG(2) << "Malformed Routine Result File.";
    return;
  }

  const base::Value* result_details_dict =
      parsed_json.FindDictKey(kResultDetailsKey);
  if (!result_details_dict) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    DVLOG(2) << "Malformed Routine Result File.";
    return;
  }

  base::Optional<double> charge_percent_opt =
      routine_type == mojom::RoutineType::kBatteryCharge
          ? result_details_dict->FindDoubleKey(kChargePercentKey)
          : result_details_dict->FindDoubleKey(kDischargePercentKey);
  if (!charge_percent_opt.has_value()) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    DVLOG(2) << "Malformed Routine Result File.";
    return;
  }

  OnPowerRoutineResult(routine_type, mojom::StandardRoutineResult::kTestPassed,
                       *charge_percent_opt, kBatteryDurationInSeconds);
}

void SystemRoutineController::OnStandardRoutineResult(
    mojom::RoutineType routine_type,
    mojom::StandardRoutineResult result) {
  DCHECK(IsRoutineRunning());
  auto result_info =
      ConstructStandardRoutineResultInfoPtr(routine_type, result);
  SendRoutineResult(std::move(result_info));
}

void SystemRoutineController::OnPowerRoutineResult(
    mojom::RoutineType routine_type,
    mojom::StandardRoutineResult result,
    double percent_change,
    uint32_t seconds_elapsed) {
  DCHECK(IsRoutineRunning());
  auto result_info = ConstructPowerRoutineResultInfoPtr(
      routine_type, result, percent_change, seconds_elapsed);
  SendRoutineResult(std::move(result_info));
}

void SystemRoutineController::SendRoutineResult(
    mojom::RoutineResultInfoPtr result_info) {
  inflight_routine_runner_->OnRoutineResult(std::move(result_info));
  inflight_routine_runner_.reset();
  inflight_routine_id_ = kInvalidRoutineId;
}

void SystemRoutineController::BindCrosHealthdDiagnosticsServiceIfNeccessary() {
  if (!diagnostics_service_ || !diagnostics_service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->GetDiagnosticsService(
        diagnostics_service_.BindNewPipeAndPassReceiver());
    diagnostics_service_.set_disconnect_handler(base::BindOnce(
        &SystemRoutineController::OnDiagnosticsServiceDisconnected,
        base::Unretained(this)));
  }
}

void SystemRoutineController::OnDiagnosticsServiceDisconnected() {
  diagnostics_service_.reset();
}

void SystemRoutineController::OnInflightRoutineRunnerDisconnected() {
  // Reset `inflight_routine_runner_` since the other side of the pipe is
  // already disconnected.
  inflight_routine_runner_.reset();

  // Make a best effort attempt to remove the routine.
  BindCrosHealthdDiagnosticsServiceIfNeccessary();
  diagnostics_service_->GetRoutineUpdate(
      inflight_routine_id_, healthd::DiagnosticRoutineCommandEnum::kCancel,
      /*should_include_output=*/false,
      base::BindOnce(&SystemRoutineController::OnRoutineCancelAttempted,
                     base::Unretained(this)));
}

void SystemRoutineController::OnRoutineCancelAttempted(
    healthd::RoutineUpdatePtr update_ptr) {
  const healthd::NonInteractiveRoutineUpdate* update =
      GetNonInteractiveRoutineUpdate(*update_ptr);

  if (!update ||
      update->status != healthd::DiagnosticRoutineStatusEnum::kCancelled) {
    DVLOG(2) << "Failed to cancel routine.";
    return;
  }
}

}  // namespace diagnostics
}  // namespace chromeos
