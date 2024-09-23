// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/diagnostics/diagnostics_service_ash.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class DiagnosticsServiceAshTest : public testing::Test {
 public:
  // testing::Test:
  void SetUp() override { cros_healthd::FakeCrosHealthd::Initialize(); }
  void TearDown() override { cros_healthd::FakeCrosHealthd::Shutdown(); }

  crosapi::mojom::DiagnosticsServiceProxy* diagnostics_service() {
    return remote_diagnostics_service_.get();
  }

 protected:
  void SetSuccessfulRoutineResponse() {
    auto response = cros_healthd::mojom::RunRoutineResponse::New();
    response->status = cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady;
    response->id = 42;
    cros_healthd::FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(
        response);
  }

  void ValidateResponse(
      const crosapi::mojom::DiagnosticsRunRoutineResponsePtr& result,
      cros_healthd::mojom::DiagnosticRoutineEnum expected_routine) {
    EXPECT_EQ(result->id, 42);
    EXPECT_EQ(result->status,
              crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady);
    EXPECT_EQ(cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
              expected_routine);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;

  mojo::Remote<crosapi::mojom::DiagnosticsService> remote_diagnostics_service_;
  std::unique_ptr<crosapi::mojom::DiagnosticsService> diagnostics_service_{
      DiagnosticsServiceAsh::Factory::Create(
          remote_diagnostics_service_.BindNewPipeAndPassReceiver())};
};

TEST_F(DiagnosticsServiceAshTest, GetAvailableRoutinesSuccess) {
  // Configure FakeCrosHealthd.
  cros_healthd::FakeCrosHealthd::Get()->SetAvailableRoutinesForTesting({
      cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower,
      cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity,
      cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge,
      cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge,
      cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth,
      cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache,
      cros_healthd::mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy,
      cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch,
      cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress,
      cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
      cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolution,
      cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolverPresent,
      cros_healthd::mojom::DiagnosticRoutineEnum::kLanConnectivity,
      cros_healthd::mojom::DiagnosticRoutineEnum::kMemory,
      cros_healthd::mojom::DiagnosticRoutineEnum::kSignalStrength,
      cros_healthd::mojom::DiagnosticRoutineEnum::kGatewayCanBePinged,
      cros_healthd::mojom::DiagnosticRoutineEnum::kSmartctlCheck,
      cros_healthd::mojom::DiagnosticRoutineEnum::kSensitiveSensor,
      cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest,
      cros_healthd::mojom::DiagnosticRoutineEnum::kFingerprintAlive,
      cros_healthd::mojom::DiagnosticRoutineEnum::
          kSmartctlCheckWithPercentageUsed,
  });

  base::test::TestFuture<
      const std::vector<crosapi::mojom::DiagnosticsRoutineEnum>&>
      future;
  diagnostics_service()->GetAvailableRoutines(future.GetCallback());

  ASSERT_TRUE(future.Wait());

  EXPECT_THAT(
      future.Get(),
      testing::ElementsAre(
          crosapi::mojom::DiagnosticsRoutineEnum::kAcPower,
          crosapi::mojom::DiagnosticsRoutineEnum::kBatteryCapacity,
          crosapi::mojom::DiagnosticsRoutineEnum::kBatteryCharge,
          crosapi::mojom::DiagnosticsRoutineEnum::kBatteryDischarge,
          crosapi::mojom::DiagnosticsRoutineEnum::kBatteryHealth,
          crosapi::mojom::DiagnosticsRoutineEnum::kCpuCache,
          crosapi::mojom::DiagnosticsRoutineEnum::kFloatingPointAccuracy,
          crosapi::mojom::DiagnosticsRoutineEnum::kPrimeSearch,
          crosapi::mojom::DiagnosticsRoutineEnum::kCpuStress,
          crosapi::mojom::DiagnosticsRoutineEnum::kDiskRead,
          crosapi::mojom::DiagnosticsRoutineEnum::kDnsResolution,
          crosapi::mojom::DiagnosticsRoutineEnum::kDnsResolverPresent,
          crosapi::mojom::DiagnosticsRoutineEnum::kLanConnectivity,
          crosapi::mojom::DiagnosticsRoutineEnum::kMemory,
          crosapi::mojom::DiagnosticsRoutineEnum::kSignalStrength,
          crosapi::mojom::DiagnosticsRoutineEnum::kGatewayCanBePinged,
          crosapi::mojom::DiagnosticsRoutineEnum::kSmartctlCheck,
          crosapi::mojom::DiagnosticsRoutineEnum::kSensitiveSensor,
          crosapi::mojom::DiagnosticsRoutineEnum::kNvmeSelfTest,
          crosapi::mojom::DiagnosticsRoutineEnum::kFingerprintAlive,
          crosapi::mojom::DiagnosticsRoutineEnum::
              kSmartctlCheckWithPercentageUsed));
}

TEST_F(DiagnosticsServiceAshTest, GetRoutineUpdateSuccess) {
  constexpr char kStatusMessage[] = "Routine ran by Google.";
  constexpr uint32_t kProgress = 87;

  // Configure FakeCrosHealthd.
  {
    auto non_interactive_routine_update =
        cros_healthd::mojom::NonInteractiveRoutineUpdate::New();
    non_interactive_routine_update->status =
        cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady;
    non_interactive_routine_update->status_message = kStatusMessage;

    auto routine_update_union =
        cros_healthd::mojom::RoutineUpdateUnion::NewNoninteractiveUpdate(
            std::move(non_interactive_routine_update));

    auto response = cros_healthd::mojom::RoutineUpdate::New();
    response->progress_percent = kProgress;
    response->routine_update_union = std::move(routine_update_union);

    cros_healthd::FakeCrosHealthd::Get()->SetGetRoutineUpdateResponseForTesting(
        response);

    base::Value::Dict expected_passed_parameters;
    expected_passed_parameters.Set("id", 123456);
    expected_passed_parameters.Set(
        "command",
        static_cast<int32_t>(
            crosapi::mojom::DiagnosticsRoutineCommandEnum::kGetStatus));
    expected_passed_parameters.Set("include_output", true);
    cros_healthd::FakeCrosHealthd::Get()
        ->SetExpectedLastPassedDiagnosticsParametersForTesting(
            std::move(expected_passed_parameters));
  }

  base::test::TestFuture<crosapi::mojom::DiagnosticsRoutineUpdatePtr> future;
  diagnostics_service()->GetRoutineUpdate(
      123456, crosapi::mojom::DiagnosticsRoutineCommandEnum::kGetStatus, true,
      future.GetCallback());

  ASSERT_TRUE(future.Wait());

  const auto& result = future.Get();
  EXPECT_EQ(result->progress_percent, kProgress);
  ASSERT_TRUE(result->routine_update_union);
  ASSERT_TRUE(result->routine_update_union->is_noninteractive_update());

  const auto& update_result =
      result->routine_update_union->get_noninteractive_update();
  EXPECT_EQ(update_result->status_message, kStatusMessage);
  EXPECT_EQ(update_result->status,
            crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady);
  EXPECT_TRUE(cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

TEST_F(DiagnosticsServiceAshTest, RunAcPowerRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();
  base::Value::Dict expected_passed_parameters;
  expected_passed_parameters.Set(
      "expected_status",
      static_cast<int32_t>(
          crosapi::mojom::DiagnosticsAcPowerStatusEnum::kConnected));
  cros_healthd::FakeCrosHealthd::Get()
      ->SetExpectedLastPassedDiagnosticsParametersForTesting(
          std::move(expected_passed_parameters));

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunAcPowerRoutine(
      crosapi::mojom::DiagnosticsAcPowerStatusEnum::kConnected, std::nullopt,
      future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower);
  EXPECT_TRUE(cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

TEST_F(DiagnosticsServiceAshTest, RunAudioDriverRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunAudioDriverRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kAudioDriver);
}

TEST_F(DiagnosticsServiceAshTest, RunBatteryCapacityRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunBatteryCapacityRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(
      result, cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity);
}

TEST_F(DiagnosticsServiceAshTest, RunBatteryChargeRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();
  base::Value::Dict expected_passed_parameters;
  expected_passed_parameters.Set("length_seconds", 423);
  expected_passed_parameters.Set("minimum_charge_percent_required", 123);
  cros_healthd::FakeCrosHealthd::Get()
      ->SetExpectedLastPassedDiagnosticsParametersForTesting(
          std::move(expected_passed_parameters));

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunBatteryChargeRoutine(423, 123,
                                                 future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge);
  EXPECT_TRUE(cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

TEST_F(DiagnosticsServiceAshTest, RunBatteryDischargeRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();
  base::Value::Dict expected_passed_parameters;
  expected_passed_parameters.Set("length_seconds", 423);
  expected_passed_parameters.Set("maximum_discharge_percent_allowed", 123);
  cros_healthd::FakeCrosHealthd::Get()
      ->SetExpectedLastPassedDiagnosticsParametersForTesting(
          std::move(expected_passed_parameters));

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunBatteryDischargeRoutine(423, 123,
                                                    future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(
      result, cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge);
  EXPECT_TRUE(cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

TEST_F(DiagnosticsServiceAshTest, RunBatteryHealthRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunBatteryHealthRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth);
}

TEST_F(DiagnosticsServiceAshTest, RunBluetoothDiscoveryRoutine) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunBluetoothDiscoveryRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(
      result, cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothDiscovery);
}

TEST_F(DiagnosticsServiceAshTest, RunBluetoothPairingRoutine) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();
  base::Value::Dict expected_passed_parameters;
  expected_passed_parameters.Set("peripheral_id", "HEALTHD_TEST_ID");
  cros_healthd::FakeCrosHealthd::Get()
      ->SetExpectedLastPassedDiagnosticsParametersForTesting(
          std::move(expected_passed_parameters));

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunBluetoothPairingRoutine("HEALTHD_TEST_ID",
                                                    future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(
      result, cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothPairing);
}

TEST_F(DiagnosticsServiceAshTest, RunBluetoothPowerRoutine) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunBluetoothPowerRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothPower);
}

TEST_F(DiagnosticsServiceAshTest, RunBluetoothScanningRoutine) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();
  base::Value::Dict expected_passed_parameters;
  expected_passed_parameters.Set("length_seconds", 100);
  cros_healthd::FakeCrosHealthd::Get()
      ->SetExpectedLastPassedDiagnosticsParametersForTesting(
          std::move(expected_passed_parameters));

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunBluetoothScanningRoutine(100, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(
      result, cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothScanning);
}

TEST_F(DiagnosticsServiceAshTest, RunCpuCacheRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();
  base::Value::Dict expected_passed_parameters;
  expected_passed_parameters.Set("length_seconds", 100);
  cros_healthd::FakeCrosHealthd::Get()
      ->SetExpectedLastPassedDiagnosticsParametersForTesting(
          std::move(expected_passed_parameters));

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunCpuCacheRoutine(100, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache);
  EXPECT_TRUE(cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

TEST_F(DiagnosticsServiceAshTest, RunCpuStressRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();
  base::Value::Dict expected_passed_parameters;
  expected_passed_parameters.Set("length_seconds", 100);
  cros_healthd::FakeCrosHealthd::Get()
      ->SetExpectedLastPassedDiagnosticsParametersForTesting(
          std::move(expected_passed_parameters));

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunCpuStressRoutine(100, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress);
  EXPECT_TRUE(cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

TEST_F(DiagnosticsServiceAshTest, RunDiskReadRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();
  base::Value::Dict expected_passed_parameters;
  expected_passed_parameters.Set(
      "type",
      static_cast<int32_t>(
          crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum::kLinearRead));
  expected_passed_parameters.Set("length_seconds", 100);
  expected_passed_parameters.Set("file_size_mb", 32);
  cros_healthd::FakeCrosHealthd::Get()
      ->SetExpectedLastPassedDiagnosticsParametersForTesting(
          std::move(expected_passed_parameters));

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunDiskReadRoutine(
      crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum::kLinearRead, 100, 32,
      future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead);
  EXPECT_TRUE(cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

TEST_F(DiagnosticsServiceAshTest, RunDnsResolutionRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunDnsResolutionRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolution);
}

TEST_F(DiagnosticsServiceAshTest, RunDnsResolverPresentRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunDnsResolverPresentRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(
      result, cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolverPresent);
}

TEST_F(DiagnosticsServiceAshTest, RunEmmcLifetimeRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;

  diagnostics_service()->RunEmmcLifetimeRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kEmmcLifetime);
}

TEST_F(DiagnosticsServiceAshTest, RunFanRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunFanRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result, cros_healthd::mojom::DiagnosticRoutineEnum::kFan);
}

TEST_F(DiagnosticsServiceAshTest, RunFingerprintAliveRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunFingerprintAliveRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(
      result, cros_healthd::mojom::DiagnosticRoutineEnum::kFingerprintAlive);
}

TEST_F(DiagnosticsServiceAshTest, RunFloatingPointAccuracyRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunFloatingPointAccuracyRoutine(100,
                                                         future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(
      result,
      cros_healthd::mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy);
}

TEST_F(DiagnosticsServiceAshTest, RunGatewayCanBePingedRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunGatewayCanBePingedRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(
      result, cros_healthd::mojom::DiagnosticRoutineEnum::kGatewayCanBePinged);
}

TEST_F(DiagnosticsServiceAshTest, RunLanConnectivityRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunLanConnectivityRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(
      result, cros_healthd::mojom::DiagnosticRoutineEnum::kLanConnectivity);
}

TEST_F(DiagnosticsServiceAshTest, RunMemoryRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunMemoryRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result, cros_healthd::mojom::DiagnosticRoutineEnum::kMemory);
}

TEST_F(DiagnosticsServiceAshTest, RunNvmeSelfTestRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();
  base::Value::Dict expected_passed_parameters;
  expected_passed_parameters.Set(
      "nvme_self_test_type",
      static_cast<int32_t>(
          crosapi::mojom::DiagnosticsNvmeSelfTestTypeEnum::kLongSelfTest));
  cros_healthd::FakeCrosHealthd::Get()
      ->SetExpectedLastPassedDiagnosticsParametersForTesting(
          std::move(expected_passed_parameters));

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunNvmeSelfTestRoutine(
      crosapi::mojom::DiagnosticsNvmeSelfTestTypeEnum::kLongSelfTest,
      future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest);
  EXPECT_TRUE(cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

TEST_F(DiagnosticsServiceAshTest, RunPrimeSearchRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();
  base::Value::Dict expected_passed_parameters;
  expected_passed_parameters.Set("length_seconds", 100);
  cros_healthd::FakeCrosHealthd::Get()
      ->SetExpectedLastPassedDiagnosticsParametersForTesting(
          std::move(expected_passed_parameters));

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunPrimeSearchRoutine(100, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch);
  EXPECT_TRUE(cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

TEST_F(DiagnosticsServiceAshTest, RunSensitiveSensorRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunSensitiveSensorRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(
      result, cros_healthd::mojom::DiagnosticRoutineEnum::kSensitiveSensor);
}

TEST_F(DiagnosticsServiceAshTest, RunSignalStrengthRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunSignalStrengthRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kSignalStrength);
}

TEST_F(DiagnosticsServiceAshTest, RunSmartctlCheckRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunSmartctlCheckRoutine(nullptr, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result, cros_healthd::mojom::DiagnosticRoutineEnum::
                               kSmartctlCheckWithPercentageUsed);
}

TEST_F(DiagnosticsServiceAshTest, RunSmartctlCheckRoutineWithParameterSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();
  base::Value::Dict expected_passed_parameters;
  expected_passed_parameters.Set("percentage_used_threshold", 42);
  cros_healthd::FakeCrosHealthd::Get()
      ->SetExpectedLastPassedDiagnosticsParametersForTesting(
          std::move(expected_passed_parameters));

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunSmartctlCheckRoutine(
      crosapi::mojom::UInt32Value::New(42), future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result, cros_healthd::mojom::DiagnosticRoutineEnum::
                               kSmartctlCheckWithPercentageUsed);
  EXPECT_TRUE(cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

TEST_F(DiagnosticsServiceAshTest, RunUfsLifetimeRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunUfsLifetimeRoutine(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kUfsLifetime);
}

TEST_F(DiagnosticsServiceAshTest, RunPowerButtonRoutineSuccess) {
  // Configure FakeCrosHealthd.
  SetSuccessfulRoutineResponse();
  constexpr uint32_t kTimeout = 10;
  base::Value::Dict expected_passed_parameters;
  expected_passed_parameters.Set("timeout_seconds",
                                 static_cast<int32_t>(kTimeout));
  cros_healthd::FakeCrosHealthd::Get()
      ->SetExpectedLastPassedDiagnosticsParametersForTesting(
          std::move(expected_passed_parameters));

  base::test::TestFuture<crosapi::mojom::DiagnosticsRunRoutineResponsePtr>
      future;
  diagnostics_service()->RunPowerButtonRoutine(kTimeout, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ValidateResponse(result,
                   cros_healthd::mojom::DiagnosticRoutineEnum::kPowerButton);
  EXPECT_TRUE(cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

}  // namespace ash
