// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_ash.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_exception.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class TestEventObserver : public crosapi::mojom::TelemetryEventObserver {
 public:
  TestEventObserver() : receiver_(this) {}
  TestEventObserver(const TestEventObserver&) = delete;
  TestEventObserver& operator=(const TestEventObserver&) = delete;
  ~TestEventObserver() override = default;

  // crosapi::mojom::TelemetryEventObserver:
  void OnEvent(crosapi::mojom::TelemetryEventInfoPtr info) override {
    event_future_.SetValue(std::move(info));
  }

  crosapi::mojom::TelemetryEventInfoPtr WaitAndGetEvent() {
    return event_future_.Take();
  }

  mojo::PendingRemote<crosapi::mojom::TelemetryEventObserver> GetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  mojo::Receiver<crosapi::mojom::TelemetryEventObserver>& GetReceiver() {
    return receiver_;
  }

  void Reset() { receiver_.reset(); }

 private:
  mojo::Receiver<crosapi::mojom::TelemetryEventObserver> receiver_;
  base::test::TestFuture<crosapi::mojom::TelemetryEventInfoPtr> event_future_;
};

}  // namespace

class TelemetryEventServiceAshTest : public testing::Test {
 public:
  // testing::Test:
  void SetUp() override { cros_healthd::FakeCrosHealthd::Initialize(); }
  void TearDown() override { cros_healthd::FakeCrosHealthd::Shutdown(); }

  crosapi::mojom::TelemetryEventServiceProxy* event_service() const {
    return remote_event_service_.get();
  }

 protected:
  TestEventObserver& observer() { return observer_; }

  void FlushForTesting() { remote_event_service_.FlushForTesting(); }

 private:
  base::test::TaskEnvironment task_environment_;
  TestEventObserver observer_;

  mojo::Remote<crosapi::mojom::TelemetryEventService> remote_event_service_;
  std::unique_ptr<crosapi::mojom::TelemetryEventService> event_service_{
      TelemetryEventServiceAsh::Factory::Create(
          remote_event_service_.BindNewPipeAndPassReceiver())};
  mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
};

TEST_F(TelemetryEventServiceAshTest, AddEventObserver) {
  event_service()->AddEventObserver(
      crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack,
      observer().GetRemote());

  // Flush so that the registration shows up.
  observer().GetReceiver().FlushForTesting();
  FlushForTesting();

  auto audio_jack_info = cros_healthd::mojom::AudioJackEventInfo::New();
  audio_jack_info->state =
      cros_healthd::mojom::AudioJackEventInfo::State::kRemove;
  audio_jack_info->device_type =
      cros_healthd::mojom::AudioJackEventInfo::DeviceType::kHeadphone;

  auto info = cros_healthd::mojom::EventInfo::NewAudioJackEventInfo(
      std::move(audio_jack_info));
  cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
      cros_healthd::mojom::EventCategoryEnum::kAudioJack, std::move(info));

  // Flush so that the result shows up.
  FlushForTesting();

  EXPECT_EQ(observer().WaitAndGetEvent(),
            crosapi::mojom::TelemetryEventInfo::NewAudioJackEventInfo(
                crosapi::mojom::TelemetryAudioJackEventInfo::New(
                    crosapi::mojom::TelemetryAudioJackEventInfo::State::kRemove,
                    crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::
                        kHeadphone)));
}

TEST_F(TelemetryEventServiceAshTest, IsEventSupported) {
  // Set the expected result in cros_healthd.
  auto expected_result = cros_healthd::mojom::SupportStatus::NewSupported(
      cros_healthd::mojom::Supported::New());
  cros_healthd::FakeCrosHealthd::Get()->SetIsEventSupportedResponseForTesting(
      expected_result);

  base::test::TestFuture<crosapi::mojom::TelemetryExtensionSupportStatusPtr>
      future;

  event_service()->IsEventSupported(
      crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack,
      future.GetCallback());

  EXPECT_TRUE(future.Get()->is_supported());
}

TEST_F(TelemetryEventServiceAshTest, OnCrosapiDisconnect) {
  event_service()->AddEventObserver(
      crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack,
      observer().GetRemote());

  // Flush so that the registration shows up.
  observer().GetReceiver().FlushForTesting();
  FlushForTesting();

  mojo::RemoteSet<cros_healthd::mojom::EventObserver>* observers =
      cros_healthd::FakeCrosHealthd::Get()->GetObserversByCategory(
          cros_healthd::mojom::EventCategoryEnum::kAudioJack);

  ASSERT_TRUE(observers);
  EXPECT_EQ(observers->size(), 1UL);
  observer().Reset();

  // Flush so that the reset shows up.
  FlushForTesting();

  EXPECT_EQ(observers->size(), 0UL);
}

TEST_F(TelemetryEventServiceAshTest, OnCrosHealthdDisconnect) {
  constexpr uint32_t kReason = 123;
  constexpr char kMsg[] = "test";

  event_service()->AddEventObserver(
      crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack,
      observer().GetRemote());

  base::test::TestFuture<void> future;
  observer().GetReceiver().set_disconnect_with_reason_handler(
      base::BindLambdaForTesting(
          [&future, kReason, kMsg](uint32_t reason, const std::string& msg) {
            EXPECT_EQ(reason, kReason);
            EXPECT_EQ(msg, kMsg);
            future.SetValue();
          }));

  // Flush so that the registration shows up.
  observer().GetReceiver().FlushForTesting();
  FlushForTesting();

  mojo::RemoteSet<cros_healthd::mojom::EventObserver>* observers =
      cros_healthd::FakeCrosHealthd::Get()->GetObserversByCategory(
          cros_healthd::mojom::EventCategoryEnum::kAudioJack);

  ASSERT_TRUE(observers);
  EXPECT_EQ(observers->size(), 1UL);
  observers->ClearWithReason(kReason, kMsg);

  // Flush so that the reset shows up.
  observers->FlushForTesting();

  EXPECT_TRUE(future.Wait());
}

}  // namespace ash
