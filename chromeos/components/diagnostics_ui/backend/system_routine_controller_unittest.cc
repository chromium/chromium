// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/system_routine_controller.h"

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_service.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace diagnostics {
namespace {

namespace healthd = cros_healthd::mojom;

constexpr char kChargePercentKey[] = "chargePercent";
constexpr char kDischargePercentKey[] = "dischargePercent";
constexpr char kResultDetailsKey[] = "resultDetails";

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
    healthd::DiagnosticRoutineStatusEnum status,
    mojo::ScopedHandle output_handle) {
  DCHECK_GE(percent_complete, 0u);
  DCHECK_LE(percent_complete, 100u);

  auto non_interactive_update =
      healthd::NonInteractiveRoutineUpdate::New(status, /*message=*/"");
  auto routine_update_union =
      healthd::RoutineUpdateUnion::NewNoninteractiveUpdate(
          std::move(non_interactive_update));
  auto routine_update = healthd::RoutineUpdate::New();
  routine_update->progress_percent = percent_complete;
  routine_update->output = std::move(output_handle);
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

void VerifyRoutineResult(const mojom::RoutineResultInfo& result_info,
                         mojom::RoutineType expected_routine_type,
                         mojom::PowerRoutineResultPtr expected_result) {
  const mojom::PowerRoutineResultPtr& actual_result =
      result_info.result->get_power_result();

  EXPECT_EQ(expected_result->simple_result, actual_result->simple_result);
  EXPECT_EQ(expected_result->percent_change, actual_result->percent_change);
  EXPECT_EQ(expected_result->time_elapsed_seconds,
            actual_result->time_elapsed_seconds);
  EXPECT_EQ(expected_routine_type, result_info.type);
}

mojom::PowerRoutineResultPtr ConstructPowerRoutineResult(
    mojom::StandardRoutineResult simple_result,
    double percent_change,
    uint32_t time_elapsed_seconds) {
  return mojom::PowerRoutineResult::New(simple_result, percent_change,
                                        time_elapsed_seconds);
}

// Constructs the Power Routine Result json. If `charge_percent` is negative,
// the discharge field will be used.
std::string ConstructPowerRoutineResultJson(double charge_percent,
                                            bool charge) {
  base::Value result_dict(base::Value::Type::DICTIONARY);
  if (charge) {
    result_dict.SetKey(kChargePercentKey, base::Value(charge_percent));

  } else {
    result_dict.SetKey(kDischargePercentKey, base::Value(charge_percent));
  }

  base::Value output_dict(base::Value::Type::DICTIONARY);
  output_dict.SetKey(kResultDetailsKey, std::move(result_dict));

  std::string json;
  const bool serialize_success = base::JSONWriter::Write(output_dict, &json);
  DCHECK(serialize_success);
  return json;
}

void SetAvailableRoutines(
    const std::vector<healthd::DiagnosticRoutineEnum>& routines) {
  cros_healthd::FakeCrosHealthdClient::Get()->SetAvailableRoutinesForTesting(
      routines);
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
  mojo::ScopedHandle CreateMojoHandleForPowerRoutine(double charge_percent,
                                                     bool charge) {
    return CreateMojoHandle(
        ConstructPowerRoutineResultJson(charge_percent, charge));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<SystemRoutineController> system_routine_controller_;

 private:
  mojo::ScopedHandle CreateMojoHandle(const std::string& contents) {
    const bool temp_success = temp_dir_.CreateUniqueTempDir();
    DCHECK(temp_success);

    base::FilePath path;
    base::ScopedFD fd =
        base::CreateAndOpenFdForTemporaryFileInDir(temp_dir_.GetPath(), &path);
    DCHECK(fd.is_valid());
    const bool write_success =
        base::WriteFileDescriptor(fd.get(), contents.data(), contents.size());
    DCHECK(write_success);
    return mojo::WrapPlatformFile(std::move(fd));
  }

  base::ScopedTempDir temp_dir_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
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
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());

  // Before the update interval, the routine status is not processed.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(59));
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
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kFailed,
      mojo::ScopedHandle());

  // Before the update interval, the routine status is not processed.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(59));
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
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kRunning,
      mojo::ScopedHandle());

  // Before the update interval, the routine status is not processed.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(59));
  EXPECT_TRUE(routine_runner.result.is_null());

  // After the update interval, the results from the routine are still not
  // available.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd to signify the routine is completed
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());

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
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kRunning,
      mojo::ScopedHandle());

  // Before the update interval, the routine status is not processed.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(59));
  EXPECT_TRUE(routine_runner.result.is_null());

  // After the update interval, the results from the routine are still not
  // available.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(routine_runner.result.is_null());

  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kRunning,
      mojo::ScopedHandle());

  // After another refresh interval, the routine is still running.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd to signify the routine is completed
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());

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
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(60));
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
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kFailed,
      mojo::ScopedHandle());
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(60));
  EXPECT_FALSE(routine_runner_2.result.is_null());
  VerifyRoutineResult(*routine_runner_2.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestFailed);
}

TEST_F(SystemRoutineControllerTest, PowerRoutineSuccess) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kWaiting);
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/10, healthd::DiagnosticRoutineStatusEnum::kRunning,
      mojo::ScopedHandle());

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kBatteryCharge,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  const uint8_t expected_percent_charge = 2;
  const uint32_t expected_time_elapsed_seconds = 30;

  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      CreateMojoHandleForPowerRoutine(expected_percent_charge,
                                      /*charge=*/true));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(31));

  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(
      *routine_runner.result, mojom::RoutineType::kBatteryCharge,
      ConstructPowerRoutineResult(mojom::StandardRoutineResult::kTestPassed,
                                  expected_percent_charge,
                                  expected_time_elapsed_seconds));
}

TEST_F(SystemRoutineControllerTest, DischargeRoutineSuccess) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kWaiting);
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/10, healthd::DiagnosticRoutineStatusEnum::kRunning,
      mojo::ScopedHandle());

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kBatteryDischarge,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  const uint8_t expected_percent_discharge = 5;
  const uint32_t expected_time_elapsed_seconds = 30;

  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      CreateMojoHandleForPowerRoutine(expected_percent_discharge,
                                      /*charge=*/false));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(31));

  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(
      *routine_runner.result, mojom::RoutineType::kBatteryDischarge,
      ConstructPowerRoutineResult(mojom::StandardRoutineResult::kTestPassed,
                                  expected_percent_discharge,
                                  expected_time_elapsed_seconds));
}

TEST_F(SystemRoutineControllerTest, AvailableRoutines) {
  SetAvailableRoutines({healthd::DiagnosticRoutineEnum::kFloatingPointAccuracy,
                        healthd::DiagnosticRoutineEnum::kMemory,
                        healthd::DiagnosticRoutineEnum::kPrimeSearch,
                        healthd::DiagnosticRoutineEnum::kAcPower,
                        healthd::DiagnosticRoutineEnum::kBatteryCapacity,
                        healthd::DiagnosticRoutineEnum::kBatteryHealth});

  base::RunLoop run_loop;
  system_routine_controller_->GetSupportedRoutines(base::BindLambdaForTesting(
      [&](const std::vector<mojom::RoutineType>& supported_routines) {
        EXPECT_EQ(3u, supported_routines.size());
        EXPECT_FALSE(base::Contains(supported_routines,
                                    mojom::RoutineType::kBatteryCharge));
        EXPECT_FALSE(base::Contains(supported_routines,
                                    mojom::RoutineType::kBatteryDischarge));
        EXPECT_FALSE(
            base::Contains(supported_routines, mojom::RoutineType::kCpuCache));
        EXPECT_FALSE(
            base::Contains(supported_routines, mojom::RoutineType::kCpuStress));
        EXPECT_TRUE(base::Contains(supported_routines,
                                   mojom::RoutineType::kCpuFloatingPoint));
        EXPECT_TRUE(
            base::Contains(supported_routines, mojom::RoutineType::kCpuPrime));
        EXPECT_TRUE(
            base::Contains(supported_routines, mojom::RoutineType::kMemory));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SystemRoutineControllerTest, CancelRoutine) {
  const int32_t expected_id = 1;
  SetRunRoutineResponse(expected_id,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  std::unique_ptr<FakeRoutineRunner> routine_runner =
      std::make_unique<FakeRoutineRunner>();
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner->result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/0, healthd::DiagnosticRoutineStatusEnum::kCancelled,
      mojo::ScopedHandle());

  // Close the routine_runner
  routine_runner.reset();
  base::RunLoop().RunUntilIdle();

  // Verify that CrosHealthd is called with the correct parameters.
  base::Optional<cros_healthd::FakeCrosHealthdService::RoutineUpdateParams>
      update_params =
          cros_healthd::FakeCrosHealthdClient::Get()->GetRoutineUpdateParams();

  ASSERT_TRUE(update_params.has_value());
  EXPECT_EQ(expected_id, update_params->id);
  EXPECT_EQ(healthd::DiagnosticRoutineCommandEnum::kCancel,
            update_params->command);
}

TEST_F(SystemRoutineControllerTest, CancelRoutineDtor) {
  const int32_t expected_id = 2;
  SetRunRoutineResponse(expected_id,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  std::unique_ptr<FakeRoutineRunner> routine_runner =
      std::make_unique<FakeRoutineRunner>();
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner->result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/0, healthd::DiagnosticRoutineStatusEnum::kCancelled,
      mojo::ScopedHandle());

  // Destroy the SystemRoutineController
  system_routine_controller_.reset();
  base::RunLoop().RunUntilIdle();

  // Verify that CrosHealthd is called with the correct parameters.
  base::Optional<cros_healthd::FakeCrosHealthdService::RoutineUpdateParams>
      update_params =
          cros_healthd::FakeCrosHealthdClient::Get()->GetRoutineUpdateParams();

  ASSERT_TRUE(update_params.has_value());
  EXPECT_EQ(expected_id, update_params->id);
  EXPECT_EQ(healthd::DiagnosticRoutineCommandEnum::kCancel,
            update_params->command);
}

TEST_F(SystemRoutineControllerTest, RunRoutineCount0) {
  base::HistogramTester histogram_tester;

  system_routine_controller_.reset();

  histogram_tester.ExpectBucketCount("ChromeOS.DiagnosticsUi.RoutineCount", 0,
                                     1);
}

TEST_F(SystemRoutineControllerTest, RunRoutineCount1) {
  // Run a routine.
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
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());

  // Before the update interval, the routine status is not processed.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(59));
  EXPECT_TRUE(routine_runner.result.is_null());

  // After the update interval, the update is fetched and processed.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestPassed);

  // Destroy the SystemRoutineController and check the emitted result.
  base::HistogramTester histogram_tester;

  system_routine_controller_.reset();

  histogram_tester.ExpectBucketCount("ChromeOS.DiagnosticsUi.RoutineCount", 1,
                                     1);
}

}  // namespace diagnostics
}  // namespace chromeos
