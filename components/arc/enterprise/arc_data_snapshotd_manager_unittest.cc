// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/enterprise/arc_data_snapshotd_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/upstart/fake_upstart_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::WithArgs;

namespace arc {
namespace data_snapshotd {

namespace {

class TestUpstartClient : public chromeos::FakeUpstartClient {
 public:
  // FakeUpstartClient overrides:
  MOCK_METHOD(void,
              StartArcDataSnapshotd,
              (chromeos::VoidDBusMethodCallback),
              (override));

  MOCK_METHOD(void,
              StopArcDataSnapshotd,
              (chromeos::VoidDBusMethodCallback),
              (override));
};

// Tests ArcDataSnapshotdManager class instance.
class ArcDataSnapshotdManagerTest : public testing::Test {
 protected:
  ArcDataSnapshotdManagerTest() {
    chromeos::DBusThreadManager::Initialize();
    EXPECT_TRUE(chromeos::DBusThreadManager::Get()->IsUsingFakes());

    upstart_client_ = std::make_unique<TestUpstartClient>();
  }

  ~ArcDataSnapshotdManagerTest() override {
    chromeos::DBusThreadManager::Shutdown();
  }

  void ExpectStartDaemon(bool success) {
    EXPECT_CALL(*upstart_client(), StartArcDataSnapshotd(_))
        .WillOnce(WithArgs<0>(
            Invoke([success](chromeos::VoidDBusMethodCallback callback) {
              std::move(callback).Run(success);
            })));
  }

  void ExpectStopDaemon(bool success) {
    EXPECT_CALL(*upstart_client(), StopArcDataSnapshotd(_))
        .WillOnce(WithArgs<0>(
            Invoke([success](chromeos::VoidDBusMethodCallback callback) {
              std::move(callback).Run(success);
            })));
  }

  void SetUpRestoredSessionCommandLine() {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(chromeos::switches::kLoginUser);
  }

  TestUpstartClient* upstart_client() { return upstart_client_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestUpstartClient> upstart_client_;
};

// Test basic scenario: start / stop arc-data-snapshotd.
TEST_F(ArcDataSnapshotdManagerTest, Basic) {
  ArcDataSnapshotdManager manager;
  EXPECT_FALSE(manager.bridge());

  ExpectStartDaemon(true /* success */);
  manager.StartDaemon();
  EXPECT_TRUE(manager.bridge());

  ExpectStopDaemon(true /* success */);
  manager.StopDaemon();
  EXPECT_FALSE(manager.bridge());

  // The attempt to stop daemon from dtor. The daemon is already stopped.
  ExpectStopDaemon(false /* success */);
}

// Test a double start scenario: start arc-data-snapshotd twice.
// Upstart job returns "false" if the job is already running.
TEST_F(ArcDataSnapshotdManagerTest, DoubleStart) {
  ArcDataSnapshotdManager manager;
  EXPECT_FALSE(manager.bridge());

  ExpectStartDaemon(true /* success */);
  manager.StartDaemon();
  EXPECT_TRUE(manager.bridge());

  // The attempt to start the already running daemon.
  ExpectStartDaemon(false /* success */);
  manager.StartDaemon();
  EXPECT_TRUE(manager.bridge());

  // Stop daemon from dtor.
  ExpectStopDaemon(true /* success */);
}

TEST_F(ArcDataSnapshotdManagerTest, RestoredAfterCrash) {
  SetUpRestoredSessionCommandLine();
  // The attempt to stop the daemon, started before crash.
  ExpectStopDaemon(true /* success */);
  ArcDataSnapshotdManager manager;
  EXPECT_FALSE(manager.bridge());

  ExpectStartDaemon(true /* success */);
  manager.StartDaemon();

  // Stop daemon from dtor.
  ExpectStopDaemon(true /* success */);
}
}  // namespace

}  // namespace data_snapshotd
}  // namespace arc
