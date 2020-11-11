// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/system_routine_controller.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/components/diagnostics_ui/backend/cros_healthd_helpers.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"

namespace chromeos {
namespace diagnostics {
namespace {

namespace healthd = cros_healthd::mojom;

constexpr uint32_t kCpuCacheDurationInSeconds = 10;
constexpr uint32_t kCpuFloatingPointDurationInSeconds = 10;
constexpr uint32_t kCpuPrimeDurationInSeconds = 10;
constexpr uint64_t kCpuPrimeMaxNumber = 1000000;
constexpr uint32_t kCpuStressDurationInSeconds = 10;
constexpr uint32_t kExpectedMemoryDurationInSeconds = 1000;
constexpr uint32_t kRoutineResultRefreshIntervalInSeconds = 1;

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

uint32_t GetExpectedRoutineDurationInSeconds(mojom::RoutineType routine_type) {
  switch (routine_type) {
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

}  // namespace

SystemRoutineController::SystemRoutineController() {
  inflight_routine_timer_ = std::make_unique<base::OneShotTimer>();
}

SystemRoutineController::~SystemRoutineController() = default;

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

  inflight_routine_runner_ =
      mojo::Remote<mojom::RoutineRunner>(std::move(runner));
  ExecuteRoutine(type);
}

void SystemRoutineController::ExecuteRoutine(mojom::RoutineType routine_type) {
  BindCrosHealthdDiagnosticsServiceIfNeccessary();

  switch (routine_type) {
    case mojom::RoutineType::kCpuCache:
      diagnostics_service_->RunCpuCacheRoutine(
          kCpuCacheDurationInSeconds,
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         base::Unretained(this), routine_type));
      return;
    case mojom::RoutineType::kCpuFloatingPoint:
      diagnostics_service_->RunFloatingPointAccuracyRoutine(
          kCpuFloatingPointDurationInSeconds,
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         base::Unretained(this), routine_type));
      return;
    case mojom::RoutineType::kCpuPrime:
      diagnostics_service_->RunPrimeSearchRoutine(
          kCpuPrimeDurationInSeconds, kCpuPrimeMaxNumber,
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         base::Unretained(this), routine_type));
      return;
    case mojom::RoutineType::kCpuStress:
      diagnostics_service_->RunCpuStressRoutine(
          kCpuStressDurationInSeconds,
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

  const int32_t id = response_ptr->id;

  // Sleep for the length of the test using a one-shot timer, then start
  // querying again for status.
  ScheduleCheckRoutineStatus(GetExpectedRoutineDurationInSeconds(routine_type),
                             routine_type, id);
}

void SystemRoutineController::CheckRoutineStatus(
    mojom::RoutineType routine_type,
    int32_t id) {
  BindCrosHealthdDiagnosticsServiceIfNeccessary();
  diagnostics_service_->GetRoutineUpdate(
      id, healthd::DiagnosticRoutineCommandEnum::kGetStatus,
      /*include_output=*/false,
      base::BindOnce(&SystemRoutineController::OnRoutineStatusUpdated,
                     base::Unretained(this), routine_type, id));
}

void SystemRoutineController::OnRoutineStatusUpdated(
    mojom::RoutineType routine_type,
    int32_t id,
    healthd::RoutineUpdatePtr update_ptr) {
  const healthd::NonInteractiveRoutineUpdate* update =
      GetNonInteractiveRoutineUpdate(*update_ptr);

  if (!update) {
    DVLOG(2) << "Invalid routine update";
    OnStandardRoutineResult(routine_type,
                            mojom::StandardRoutineResult::kExecutionError);
    return;
  }

  const healthd::DiagnosticRoutineStatusEnum status = update->status;

  // If still running, continue to repoll until it is finished.
  // TODO(baileyberro): Consider adding a timeout mechanism.
  if (status == healthd::DiagnosticRoutineStatusEnum::kRunning) {
    ScheduleCheckRoutineStatus(kRoutineResultRefreshIntervalInSeconds,
                               routine_type, id);
    return;
  }

  // If test passed, report result.
  if (status == healthd::DiagnosticRoutineStatusEnum::kPassed) {
    OnStandardRoutineResult(routine_type, TestStatusToResult(status));
    return;
  }

  // If test failed, report result.
  if (status == healthd::DiagnosticRoutineStatusEnum::kFailed) {
    OnStandardRoutineResult(routine_type, TestStatusToResult(status));
    return;
  }

  // Any other reason, report failure.
  DVLOG(2) << "Routine failed: " << update->status_message;
  OnStandardRoutineResult(routine_type, TestStatusToResult(status));
}

bool SystemRoutineController::IsRoutineRunning() const {
  return inflight_routine_runner_.is_bound();
}

void SystemRoutineController::ScheduleCheckRoutineStatus(
    uint32_t duration_in_seconds,
    mojom::RoutineType routine_type,
    int32_t id) {
  inflight_routine_timer_->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(duration_in_seconds),
      base::BindOnce(&SystemRoutineController::CheckRoutineStatus,
                     base::Unretained(this), routine_type, id));
}

void SystemRoutineController::OnStandardRoutineResult(
    mojom::RoutineType routine_type,
    mojom::StandardRoutineResult result) {
  DCHECK(IsRoutineRunning());
  auto result_info =
      ConstructStandardRoutineResultInfoPtr(routine_type, result);
  inflight_routine_runner_->OnRoutineResult(std::move(result_info));
  inflight_routine_runner_.reset();
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
  inflight_routine_runner_.reset();
  // TODO(baileyberro): Implement routine cancellation.
}

}  // namespace diagnostics
}  // namespace chromeos
