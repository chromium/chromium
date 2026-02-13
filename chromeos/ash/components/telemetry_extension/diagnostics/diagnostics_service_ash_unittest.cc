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

 private:
  base::test::TaskEnvironment task_environment_;
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;

  mojo::Remote<crosapi::mojom::DiagnosticsService> remote_diagnostics_service_;
  std::unique_ptr<crosapi::mojom::DiagnosticsService> diagnostics_service_{
      DiagnosticsServiceAsh::Factory::Create(
          remote_diagnostics_service_.BindNewPipeAndPassReceiver())};
};

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

    base::DictValue expected_passed_parameters;
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

}  // namespace ash
