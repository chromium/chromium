// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/enterprise/arc_data_snapshotd_bridge.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/arc/arc_data_snapshotd_client.h"
#include "chromeos/dbus/arc/fake_arc_data_snapshotd_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace data_snapshotd {

namespace {

void RunGenerateKeyPair(ArcDataSnapshotdBridge* bridge, bool expected_result) {
  base::RunLoop run_loop;
  bridge->GenerateKeyPair(
      base::BindLambdaForTesting([expected_result, &run_loop](bool success) {
        EXPECT_EQ(expected_result, success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Tests ArcDataSnapshotdBridge class instance.
class ArcDataSnapshotdBridgeTest : public testing::Test {
 protected:
  ArcDataSnapshotdBridgeTest() {
    chromeos::DBusThreadManager::Initialize();
    EXPECT_TRUE(chromeos::DBusThreadManager::Get()->IsUsingFakes());
  }

  ~ArcDataSnapshotdBridgeTest() override {
    chromeos::DBusThreadManager::Shutdown();
  }

  chromeos::FakeArcDataSnapshotdClient* dbus_client() {
    auto* client =
        chromeos::DBusThreadManager::Get()->GetArcDataSnapshotdClient();
    DCHECK(client);
    return static_cast<chromeos::FakeArcDataSnapshotdClient*>(client);
  }

  void FastForwardAttempt() {
    task_environment_.FastForwardBy(
        ArcDataSnapshotdBridge::connection_attempt_interval_for_testing());
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Test basic scenario: D-Bus service is available immediately.
TEST_F(ArcDataSnapshotdBridgeTest, ServiceAvailable) {
  dbus_client()->set_available(true /* is_available */);
  ArcDataSnapshotdBridge bridge{base::DoNothing()};
  EXPECT_FALSE(bridge.is_available_for_testing());
  RunGenerateKeyPair(&bridge, false /* expected_result */);

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(bridge.is_available_for_testing());
  RunGenerateKeyPair(&bridge, true /* expected_result */);
}

// Test basic scenario: D-Bus service is not available.
TEST_F(ArcDataSnapshotdBridgeTest, ServiceUnavailable) {
  dbus_client()->set_available(false /* is_available */);
  ArcDataSnapshotdBridge bridge{base::DoNothing()};

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(bridge.is_available_for_testing());
  RunGenerateKeyPair(&bridge, false /* expected_result */);
}

// Test that service is available from the max attempt.
TEST_F(ArcDataSnapshotdBridgeTest, ServiceAvailableMaxAttempt) {
  dbus_client()->set_available(false /* is_available */);
  ArcDataSnapshotdBridge bridge{base::DoNothing()};

  // Not available from the first attempt.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(bridge.is_available_for_testing());

  size_t attempts_number =
      ArcDataSnapshotdBridge::max_connection_attempt_count_for_testing() - 1;
  for (size_t i = 1; i < attempts_number; i++) {
    // Not available from the next max - 2 attempts.
    FastForwardAttempt();
    EXPECT_FALSE(bridge.is_available_for_testing());
  }
  // Available from the max attempt.
  dbus_client()->set_available(true /* is_available */);
  FastForwardAttempt();
  EXPECT_TRUE(bridge.is_available_for_testing());
  RunGenerateKeyPair(&bridge, true /* expected_result */);
}

// Test that service is available from the max + 1 attempt and is not picked up.
TEST_F(ArcDataSnapshotdBridgeTest, ServiceUnavailableMaxAttempts) {
  dbus_client()->set_available(false /* is_available */);
  ArcDataSnapshotdBridge bridge{base::DoNothing()};

  // Not available from the first attempt.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(bridge.is_available_for_testing());

  size_t attempts_number =
      ArcDataSnapshotdBridge::max_connection_attempt_count_for_testing();
  for (size_t i = 1; i < attempts_number; i++) {
    // Not available from the next max - 1 attempts.
    FastForwardAttempt();
    EXPECT_FALSE(bridge.is_available_for_testing());
  }
  // Available from the max + 1 attempt, but bridge is not listening.
  dbus_client()->set_available(true /* is_available */);
  FastForwardAttempt();
  EXPECT_FALSE(bridge.is_available_for_testing());
  RunGenerateKeyPair(&bridge, false /* expected_result */);
}

}  // namespace

}  // namespace data_snapshotd
}  // namespace arc
