// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/system_routine_controller.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace diagnostics {
namespace {

namespace healthd = cros_healthd::mojom;

void SetCrosHealthdRunRoutineResponse(
    healthd::RunRoutineResponsePtr& response) {
  cros_healthd::FakeCrosHealthdClient::Get()->SetRunRoutineResponseForTesting(
      response);
}

void SetRunRoutineResponse(int32_t id,
                           healthd::DiagnosticRoutineStatusEnum status) {
  auto routine_response = healthd::RunRoutineResponse::New(id, status);
  SetCrosHealthdRunRoutineResponse(routine_response);
}

void SetCrosHealthdRoutineUpdateResponse(healthd::RoutineUpdatePtr& response) {
  cros_healthd::FakeCrosHealthdClient::Get()
      ->SetGetRoutineUpdateResponseForTesting(response);
}

void SetNonInteractiveRoutineUpdateResponse(
    uint32_t percent_complete,
    healthd::DiagnosticRoutineStatusEnum status) {
  DCHECK_GE(percent_complete, 0u);
  DCHECK_LE(percent_complete, 100u);

  auto non_interactive_update =
      healthd::NonInteractiveRoutineUpdate::New(status, /*message=*/"");
  auto routine_update_union =
      healthd::RoutineUpdateUnion::NewNoninteractiveUpdate(
          std::move(non_interactive_update));
  auto routine_update = healthd::RoutineUpdate::New();
  routine_update->progress_percent = percent_complete;
  routine_update->routine_update_union = std::move(routine_update_union);

  SetCrosHealthdRoutineUpdateResponse(routine_update);
}

void VerifyRoutineResult(const mojom::RoutineResultInfo& result_info,
                         mojom::RoutineType expected_routine_type,
                         mojom::StandardRoutineResult expected_result) {
  const mojom::StandardRoutineResult actual_result =
      result_info.result->get_simple_result();

  EXPECT_EQ(expected_result, actual_result);
  EXPECT_EQ(expected_routine_type, result_info.type);
}

}  // namespace

struct FakeRoutineRunner : public mojom::RoutineRunner {
  // mojom::RoutineRunner
  void OnRoutineResult(mojom::RoutineResultInfoPtr result_info) override {
    DCHECK(result.is_null()) << "OnRoutineResult should only be called once";

    result = std::move(result_info);
  }

  mojom::RoutineResultInfoPtr result;

  mojo::Receiver<mojom::RoutineRunner> receiver{this};
};

class SystemRoutineControllerTest : public testing::Test {
 public:
  SystemRoutineControllerTest() {
    chromeos::CrosHealthdClient::InitializeFake();
    system_routine_controller_ = std::make_unique<SystemRoutineController>();
  }

  ~SystemRoutineControllerTest() override {
    system_routine_controller_.reset();
    chromeos::CrosHealthdClient::Shutdown();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<SystemRoutineController> system_routine_controller_;
};

TEST_F(SystemRoutineControllerTest, RejectedByCrosHealthd) {
  SetRunRoutineResponse(healthd::kFailedToStartId,
                        healthd::DiagnosticRoutineStatusEnum::kFailedToStart);

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kUnableToRun);
}

TEST_F(SystemRoutineControllerTest, AlreadyInProgress) {
  // Put one routine in progress.
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner_1;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner_1.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner_1.result.is_null());

  FakeRoutineRunner routine_runner_2;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner_2.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the second routine is rejected.
  EXPECT_FALSE(routine_runner_2.result.is_null());
  VerifyRoutineResult(*routine_runner_2.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kUnableToRun);
}

TEST_F(SystemRoutineControllerTest, CpuStressSuccess) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed);

  // Before the update interval, the routine status is not processed.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_TRUE(routine_runner.result.is_null());

  // After the update interval, the update is fetched and processed.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestPassed);
}

TEST_F(SystemRoutineControllerTest, CpuStressFailure) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kFailed);

  // Before the update interval, the routine status is not processed.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_TRUE(routine_runner.result.is_null());

  // After the update interval, the update is fetched and processed.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestFailed);
}

TEST_F(SystemRoutineControllerTest, CpuStressStillRunning) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd to signify the routine is still running.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kRunning);

  // Before the update interval, the routine status is not processed.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_TRUE(routine_runner.result.is_null());

  // After the update interval, the results from the routine are still not
  // available.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd to signify the routine is completed
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed);

  // Fast forward by the refresh interval.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestPassed);
}

TEST_F(SystemRoutineControllerTest, CpuStressStillRunningMultipleIntervals) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd to signify the routine is still running.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kRunning);

  // Before the update interval, the routine status is not processed.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_TRUE(routine_runner.result.is_null());

  // After the update interval, the results from the routine are still not
  // available.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(routine_runner.result.is_null());

  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kRunning);

  // After another refresh interval, the routine is still running.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd to signify the routine is completed
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed);

  // After a second refresh interval, the routine is completed.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestPassed);
}

TEST_F(SystemRoutineControllerTest, TwoConsecutiveRoutines) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner_1;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner_1.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner_1.result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(routine_runner_1.result.is_null());
  VerifyRoutineResult(*routine_runner_1.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestPassed);

  // Run the test again
  SetRunRoutineResponse(/*id=*/2,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner_2;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner_2.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the second routine is not complete.
  EXPECT_TRUE(routine_runner_2.result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kFailed);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(routine_runner_2.result.is_null());
  VerifyRoutineResult(*routine_runner_2.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestFailed);
}

}  // namespace diagnostics
}  // namespace chromeos
