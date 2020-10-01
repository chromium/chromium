// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OFFICIAL_BUILD)
#error System events service unit tests should only be included in unofficial builds.
#endif

#include "chromeos/components/telemetry_extension_ui/system_events_service.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/components/telemetry_extension_ui/mojom/system_events_service.mojom.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

class MockLidObserver : public health::mojom::LidObserver {
 public:
  MockLidObserver() : receiver_{this} {}
  MockLidObserver(const MockLidObserver&) = delete;
  MockLidObserver& operator=(const MockLidObserver&) = delete;

  mojo::PendingRemote<health::mojom::LidObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void, OnLidClosed, (), (override));
  MOCK_METHOD(void, OnLidOpened, (), (override));

 private:
  mojo::Receiver<health::mojom::LidObserver> receiver_;
};

}  // namespace

class SystemEventsServiceTest : public testing::Test {
 public:
  SystemEventsServiceTest() {
    CrosHealthdClient::InitializeFake();
    system_events_service_ = std::make_unique<SystemEventsService>(
        remote_system_events_service_.BindNewPipeAndPassReceiver());
    mock_lid_observer_ =
        std::make_unique<testing::StrictMock<MockLidObserver>>();

    // Force other tasks to be processed.
    cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();
  }

  ~SystemEventsServiceTest() override {
    CrosHealthdClient::Shutdown();
    cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();
  }

  health::mojom::SystemEventsServiceProxy* remote_system_events_service()
      const {
    return remote_system_events_service_.get();
  }

  SystemEventsService* system_events_service() const {
    return system_events_service_.get();
  }

  mojo::PendingRemote<health::mojom::LidObserver> lid_observer() const {
    return mock_lid_observer_->pending_remote();
  }

  MockLidObserver* mock_lid_observer() const {
    return mock_lid_observer_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<health::mojom::SystemEventsService>
      remote_system_events_service_;
  std::unique_ptr<SystemEventsService> system_events_service_;
  std::unique_ptr<testing::StrictMock<MockLidObserver>> mock_lid_observer_;
};

// Tests that in case of cros_healthd crash Lid Observer will reconnect.
TEST_F(SystemEventsServiceTest, LidObserverReconnect) {
  remote_system_events_service()->AddLidObserver(lid_observer());

  base::RunLoop run_loop1;
  EXPECT_CALL(*mock_lid_observer(), OnLidClosed).WillOnce([&run_loop1]() {
    run_loop1.Quit();
  });
  cros_healthd::FakeCrosHealthdClient::Get()->EmitLidClosedEventForTesting();
  run_loop1.Run();

  // Shutdown cros_healthd to simulate crash.
  CrosHealthdClient::Shutdown();

  // Ensure ServiceConnection is disconnected from cros_healthd.
  cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();

  // Restart cros_healthd.
  CrosHealthdClient::InitializeFake();

  // Ensure disconnect handler is called for lid observer from System Event
  // Service. After this call, we will have a Mojo pending connection task in
  // Mojo message queue.
  system_events_service()->FlushForTesting();

  // Ensure that Mojo pending connection task from lid observer gets processed
  // and observer is bound. After this call, we are sure that lid observer
  // reconnected and we can safely emit events.
  cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();

  base::RunLoop run_loop2;
  EXPECT_CALL(*mock_lid_observer(), OnLidClosed).WillOnce([&run_loop2]() {
    run_loop2.Quit();
  });

  cros_healthd::FakeCrosHealthdClient::Get()->EmitLidClosedEventForTesting();
  run_loop2.Run();
}

}  // namespace chromeos
