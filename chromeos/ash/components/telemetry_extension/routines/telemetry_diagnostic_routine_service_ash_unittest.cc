// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/routines/telemetry_diagnostic_routine_service_ash.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_exception.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

namespace crosapi = ::crosapi::mojom;
namespace healthd = cros_healthd::mojom;

class TestRoutineObserver : crosapi::TelemetryDiagnosticRoutineObserver {
 public:
  TestRoutineObserver() = default;
  TestRoutineObserver(const TestRoutineObserver&) = delete;
  TestRoutineObserver& operator=(const TestRoutineObserver&) = delete;
  ~TestRoutineObserver() override = default;

  // `TelemetryDiagnosticRoutineObserver`:
  void OnRoutineStateChange(
      crosapi::TelemetryDiagnosticRoutineStatePtr state) override {
    future_.AddValue(std::move(state));
  }

  crosapi::TelemetryDiagnosticRoutineStatePtr WaitForNextValue() {
    return future_.Take();
  }

  mojo::PendingRemote<crosapi::TelemetryDiagnosticRoutineObserver>
  GetPendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  mojo::Receiver<crosapi::TelemetryDiagnosticRoutineObserver>& GetReceiver() {
    return receiver_;
  }

  void Reset() { receiver_.reset(); }

 private:
  base::test::RepeatingTestFuture<crosapi::TelemetryDiagnosticRoutineStatePtr>
      future_;
  mojo::Receiver<crosapi::TelemetryDiagnosticRoutineObserver> receiver_{this};
};

}  // namespace

class TelemetryDiagnosticsRoutineServiceAshTest : public testing::Test {
 public:
  void SetUp() override { cros_healthd::FakeCrosHealthd::Initialize(); }
  void TearDown() override { cros_healthd::FakeCrosHealthd::Shutdown(); }

  crosapi::TelemetryDiagnosticRoutinesServiceProxy* routines_service() const {
    return remote_routines_service_.get();
  }

  mojo::PendingRemote<crosapi::TelemetryDiagnosticRoutineObserver>
  GetEmptyObserver() {
    return mojo::PendingRemote<crosapi::TelemetryDiagnosticRoutineObserver>();
  }

 protected:
  void FlushForTesting() {
    remote_routines_service_.FlushForTesting();
    cros_healthd::FakeCrosHealthd::Get()->FlushRoutineServiceForTesting();
  }

  void ResetDiagnosticsRoutinesService() { routines_service_.reset(); }

 private:
  base::test::TaskEnvironment task_environment_;

  // Remote which is usually used by code in Lacros to call into Ash.
  mojo::Remote<crosapi::TelemetryDiagnosticRoutinesService>
      remote_routines_service_;
  // Ash-side implementation of the interface.
  std::unique_ptr<crosapi::TelemetryDiagnosticRoutinesService>
      routines_service_{TelemetryDiagnosticsRoutineServiceAsh::Factory::Create(
          remote_routines_service_.BindNewPipeAndPassReceiver())};
  mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
};

TEST_F(TelemetryDiagnosticsRoutineServiceAshTest, CreateRoutine) {
  mojo::Remote<crosapi::TelemetryDiagnosticRoutineControl> control_remote;

  auto arg =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);
  routines_service()->CreateRoutine(std::move(arg),
                                    control_remote.BindNewPipeAndPassReceiver(),
                                    GetEmptyObserver());

  FlushForTesting();

  EXPECT_TRUE(
      cros_healthd::FakeCrosHealthd::Get()->GetRoutineControlForArgumentTag(
          healthd::RoutineArgument::Tag::kUnrecognizedArgument));
}

TEST_F(TelemetryDiagnosticsRoutineServiceAshTest, IsRoutineArgumentSupported) {
  auto status = healthd::SupportStatus::NewSupported(healthd::Supported::New());
  cros_healthd::FakeCrosHealthd::Get()
      ->SetIsRoutineArgumentSupportedResponseForTesting(status);

  auto arg =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);
  base::test::TestFuture<crosapi::TelemetryExtensionSupportStatusPtr> future;
  routines_service()->IsRoutineArgumentSupported(std::move(arg),
                                                 future.GetCallback());

  FlushForTesting();

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Take(),
            crosapi::TelemetryExtensionSupportStatus::NewSupported(
                crosapi::TelemetryExtensionSupported::New()));
}

TEST_F(TelemetryDiagnosticsRoutineServiceAshTest, StartRoutine) {
  mojo::Remote<crosapi::TelemetryDiagnosticRoutineControl> control_remote;

  auto arg =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);
  routines_service()->CreateRoutine(std::move(arg),
                                    control_remote.BindNewPipeAndPassReceiver(),
                                    GetEmptyObserver());

  FlushForTesting();

  auto* fake_controller =
      cros_healthd::FakeCrosHealthd::Get()->GetRoutineControlForArgumentTag(
          healthd::RoutineArgument::Tag::kUnrecognizedArgument);
  ASSERT_TRUE(fake_controller);

  control_remote->Start();

  control_remote.FlushForTesting();
  FlushForTesting();

  EXPECT_TRUE(fake_controller->has_start_been_called());
}

TEST_F(TelemetryDiagnosticsRoutineServiceAshTest, GetState) {
  constexpr uint8_t kPercentage = 50;
  mojo::Remote<crosapi::TelemetryDiagnosticRoutineControl> control_remote;

  auto arg =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);
  routines_service()->CreateRoutine(std::move(arg),
                                    control_remote.BindNewPipeAndPassReceiver(),
                                    GetEmptyObserver());

  FlushForTesting();

  auto* fake_controller =
      cros_healthd::FakeCrosHealthd::Get()->GetRoutineControlForArgumentTag(
          healthd::RoutineArgument::Tag::kUnrecognizedArgument);
  ASSERT_TRUE(fake_controller);

  auto routine_state = healthd::RoutineState::New();
  routine_state->percentage = kPercentage;
  routine_state->state_union =
      healthd::RoutineStateUnion::NewUnrecognizedArgument(true);

  fake_controller->SetGetStateResponse(routine_state);

  base::test::TestFuture<crosapi::TelemetryDiagnosticRoutineStatePtr> future;
  control_remote->GetState(future.GetCallback());

  control_remote.FlushForTesting();
  FlushForTesting();

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(result->percentage, kPercentage);
  EXPECT_EQ(
      result->state_union,
      crosapi::TelemetryDiagnosticRoutineStateUnion::NewUnrecognizedArgument(
          true));
}

TEST_F(TelemetryDiagnosticsRoutineServiceAshTest, ReplyToInquiry) {
  mojo::Remote<crosapi::TelemetryDiagnosticRoutineControl> control_remote;

  auto arg =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);
  routines_service()->CreateRoutine(std::move(arg),
                                    control_remote.BindNewPipeAndPassReceiver(),
                                    GetEmptyObserver());

  FlushForTesting();

  auto* fake_controller =
      cros_healthd::FakeCrosHealthd::Get()->GetRoutineControlForArgumentTag(
          healthd::RoutineArgument::Tag::kUnrecognizedArgument);
  ASSERT_TRUE(fake_controller);

  control_remote->ReplyToInquiry(
      crosapi::TelemetryDiagnosticRoutineInquiryReply::NewUnrecognizedReply(
          true));

  control_remote.FlushForTesting();
  FlushForTesting();

  auto last_reply = fake_controller->GetLastInquiryReply();
  ASSERT_TRUE(!last_reply.is_null());
  EXPECT_TRUE(last_reply->is_unrecognizedReply());
}

TEST_F(TelemetryDiagnosticsRoutineServiceAshTest, CreateAndStartRoutine) {
  TestRoutineObserver observer;
  mojo::Remote<crosapi::TelemetryDiagnosticRoutineControl> control_remote;

  auto arg =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);
  routines_service()->CreateRoutine(std::move(arg),
                                    control_remote.BindNewPipeAndPassReceiver(),
                                    observer.GetPendingRemote());

  control_remote->Start();

  FlushForTesting();

  auto* fake_controller =
      cros_healthd::FakeCrosHealthd::Get()->GetRoutineControlForArgumentTag(
          healthd::RoutineArgument::Tag::kUnrecognizedArgument);
  ASSERT_TRUE(fake_controller);

  EXPECT_EQ(
      observer.WaitForNextValue(),
      crosapi::TelemetryDiagnosticRoutineState::New(
          0, crosapi::TelemetryDiagnosticRoutineStateUnion::NewInitialized(
                 crosapi::TelemetryDiagnosticRoutineStateInitialized::New())));
  EXPECT_TRUE(fake_controller->has_start_been_called());
}

TEST_F(TelemetryDiagnosticsRoutineServiceAshTest, RoutineObserver) {
  constexpr uint8_t kPercentage = 50;
  mojo::Remote<crosapi::TelemetryDiagnosticRoutineControl> control_remote;
  TestRoutineObserver observer;

  auto arg =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);
  routines_service()->CreateRoutine(std::move(arg),
                                    control_remote.BindNewPipeAndPassReceiver(),
                                    observer.GetPendingRemote());

  FlushForTesting();

  auto* fake_controller =
      cros_healthd::FakeCrosHealthd::Get()->GetRoutineControlForArgumentTag(
          healthd::RoutineArgument::Tag::kUnrecognizedArgument);
  ASSERT_TRUE(fake_controller);
  auto* observer_remote = fake_controller->GetObserver();
  ASSERT_TRUE(observer_remote);

  healthd::RoutineStatePtr routine_state = healthd::RoutineState::New();
  routine_state->state_union =
      healthd::RoutineStateUnion::NewUnrecognizedArgument(true);
  routine_state->percentage = kPercentage;

  observer_remote->get()->OnRoutineStateChange(std::move(routine_state));

  FlushForTesting();

  // The first event we observe is always the init event.
  EXPECT_EQ(
      observer.WaitForNextValue(),
      crosapi::TelemetryDiagnosticRoutineState::New(
          0, crosapi::TelemetryDiagnosticRoutineStateUnion::NewInitialized(
                 crosapi::TelemetryDiagnosticRoutineStateInitialized::New())));
  EXPECT_EQ(observer.WaitForNextValue(),
            crosapi::TelemetryDiagnosticRoutineState::New(
                kPercentage, crosapi::TelemetryDiagnosticRoutineStateUnion::
                                 NewUnrecognizedArgument(true)));
}

TEST_F(TelemetryDiagnosticsRoutineServiceAshTest, OnCrosapiDisconnectControl) {
  mojo::Remote<crosapi::TelemetryDiagnosticRoutineControl> control_remote;

  auto arg =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);
  routines_service()->CreateRoutine(std::move(arg),
                                    control_remote.BindNewPipeAndPassReceiver(),
                                    GetEmptyObserver());

  FlushForTesting();

  auto* fake_controller =
      cros_healthd::FakeCrosHealthd::Get()->GetRoutineControlForArgumentTag(
          healthd::RoutineArgument::Tag::kUnrecognizedArgument);
  ASSERT_TRUE(fake_controller);

  base::test::TestFuture<void> future;
  fake_controller->GetReceiver()->set_disconnect_handler(future.GetCallback());

  control_remote.reset();

  EXPECT_TRUE(future.Wait());
}

TEST_F(TelemetryDiagnosticsRoutineServiceAshTest,
       OnCrosHealthdDisconnectControl) {
  constexpr uint32_t kReason = 123;
  constexpr char kMsg[] = "test";

  mojo::Remote<crosapi::TelemetryDiagnosticRoutineControl> control_remote;

  auto arg =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);
  routines_service()->CreateRoutine(std::move(arg),
                                    control_remote.BindNewPipeAndPassReceiver(),
                                    GetEmptyObserver());

  FlushForTesting();

  auto* fake_controller =
      cros_healthd::FakeCrosHealthd::Get()->GetRoutineControlForArgumentTag(
          healthd::RoutineArgument::Tag::kUnrecognizedArgument);
  ASSERT_TRUE(fake_controller);

  base::test::TestFuture<uint32_t, const std::string&> future;
  control_remote.set_disconnect_with_reason_handler(future.GetCallback());

  fake_controller->GetReceiver()->ResetWithReason(kReason, kMsg);

  FlushForTesting();

  ASSERT_TRUE(future.Wait());
  auto [actual_reason, actual_msg] = future.Take();
  EXPECT_EQ(actual_reason, kReason);
  EXPECT_EQ(actual_msg, kMsg);
}

TEST_F(TelemetryDiagnosticsRoutineServiceAshTest, OnCrosapiDisconnectObserver) {
  mojo::Remote<crosapi::TelemetryDiagnosticRoutineControl> control_remote;
  TestRoutineObserver observer;

  auto arg =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);
  routines_service()->CreateRoutine(std::move(arg),
                                    control_remote.BindNewPipeAndPassReceiver(),
                                    observer.GetPendingRemote());

  FlushForTesting();

  auto* fake_controller =
      cros_healthd::FakeCrosHealthd::Get()->GetRoutineControlForArgumentTag(
          healthd::RoutineArgument::Tag::kUnrecognizedArgument);
  ASSERT_TRUE(fake_controller);
  ASSERT_TRUE(fake_controller->GetObserver());

  base::test::TestFuture<void> future;
  fake_controller->GetObserver()->set_disconnect_handler(future.GetCallback());

  observer.Reset();

  EXPECT_TRUE(future.Wait());
}

TEST_F(TelemetryDiagnosticsRoutineServiceAshTest,
       OnCrosHealthdDisconnectObserver) {
  TestRoutineObserver observer;

  mojo::Remote<crosapi::TelemetryDiagnosticRoutineControl> control_remote;

  auto arg =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);
  routines_service()->CreateRoutine(std::move(arg),
                                    control_remote.BindNewPipeAndPassReceiver(),
                                    observer.GetPendingRemote());

  FlushForTesting();

  auto* fake_controller =
      cros_healthd::FakeCrosHealthd::Get()->GetRoutineControlForArgumentTag(
          healthd::RoutineArgument::Tag::kUnrecognizedArgument);
  ASSERT_TRUE(fake_controller);
  ASSERT_TRUE(fake_controller->GetObserver());

  base::test::TestFuture<void> future;
  observer.GetReceiver().set_disconnect_handler(future.GetCallback());

  fake_controller->GetObserver()->reset();

  EXPECT_TRUE(future.Wait());
}

TEST_F(TelemetryDiagnosticsRoutineServiceAshTest,
       OnTelemetryDiagnosticsServiceAshDestroyed) {
  TestRoutineObserver observer;

  mojo::Remote<crosapi::TelemetryDiagnosticRoutineControl> control_remote;

  auto arg =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);
  routines_service()->CreateRoutine(std::move(arg),
                                    control_remote.BindNewPipeAndPassReceiver(),
                                    observer.GetPendingRemote());

  FlushForTesting();

  auto* fake_controller =
      cros_healthd::FakeCrosHealthd::Get()->GetRoutineControlForArgumentTag(
          healthd::RoutineArgument::Tag::kUnrecognizedArgument);
  ASSERT_TRUE(fake_controller);
  ASSERT_TRUE(fake_controller->GetObserver());

  base::test::TestFuture<void> crosapi_control;
  base::test::TestFuture<void> crosapi_observer;
  base::test::TestFuture<void> cros_healthd_control;
  base::test::TestFuture<void> cros_healthd_observer;

  control_remote.set_disconnect_handler(crosapi_control.GetCallback());
  observer.GetReceiver().set_disconnect_handler(crosapi_observer.GetCallback());
  fake_controller->GetReceiver()->set_disconnect_handler(
      cros_healthd_control.GetCallback());
  fake_controller->GetObserver()->set_disconnect_handler(
      cros_healthd_observer.GetCallback());

  ResetDiagnosticsRoutinesService();

  FlushForTesting();

  EXPECT_TRUE(crosapi_control.Wait());
  EXPECT_TRUE(crosapi_observer.Wait());
  EXPECT_TRUE(cros_healthd_control.Wait());
  EXPECT_TRUE(cros_healthd_observer.Wait());
}

}  // namespace ash
