// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/enterprise/arc_data_snapshotd_manager.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/arc/fake_arc_data_snapshotd_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/upstart/fake_upstart_client.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/enterprise/arc_data_snapshotd_bridge.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/public/ozone_switches.h"

using testing::_;
using testing::Invoke;
using testing::WithArgs;

class PrefService;

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

// Basic tests for ArcDataSnapshotdManager class instance.
class ArcDataSnapshotdManagerBasicTest : public testing::Test {
 protected:
  ArcDataSnapshotdManagerBasicTest() {
    // Initialize fake D-Bus client.
    chromeos::DBusThreadManager::Initialize();
    EXPECT_TRUE(chromeos::DBusThreadManager::Get()->IsUsingFakes());

    arc::prefs::RegisterLocalStatePrefs(local_state_.registry());

    upstart_client_ = std::make_unique<TestUpstartClient>();
  }

  void SetUp() override { SetDBusClientAvailability(true /* is_available */); }

  ~ArcDataSnapshotdManagerBasicTest() override {
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

  void SetDBusClientAvailability(bool is_available) {
    auto* client = static_cast<chromeos::FakeArcDataSnapshotdClient*>(
        chromeos::DBusThreadManager::Get()->GetArcDataSnapshotdClient());
    DCHECK(client);
    client->set_available(is_available);
  }

  TestUpstartClient* upstart_client() { return upstart_client_.get(); }
  PrefService* local_state() { return &local_state_; }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<TestUpstartClient> upstart_client_;
};

// Tests flows in ArcDataSnapshotdManager:
// * clear snapshot flow.
// * generate key pair flow.
class ArcDataSnapshotdManagerFlowTest
    : public ArcDataSnapshotdManagerBasicTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    SetDBusClientAvailability(is_dbus_client_available());
  }

  bool is_dbus_client_available() { return GetParam(); }

  void CheckHeadlessMode() {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    EXPECT_EQ(command_line->GetSwitchValueASCII(switches::kOzonePlatform),
              "headless");
  }

  // Check number of snapshots in local_state.
  void CheckSnapshotsNumber(int expected_number) {
    ArcDataSnapshotdManager::Snapshot snapshot(local_state());
    snapshot.Parse();
    int actual_number = 0;
    if (snapshot.previous()) {
      actual_number++;
    }
    if (snapshot.last()) {
      actual_number++;
    }
    EXPECT_EQ(expected_number, actual_number);
  }

  // Set up local_state with info for previous and last snapshots and blocked ui
  // mode.
  void SetupLocalState() {
    auto last = ArcDataSnapshotdManager::SnapshotInfo::CreateForTesting(
        "" /* os_version */, "" /* creation_date */, false /* verified */,
        false /* updated */, true /* last */);
    auto previous = ArcDataSnapshotdManager::SnapshotInfo::CreateForTesting(
        "" /* os_version */, "" /* creation_date */, false /* verified */,
        false /* updated */, false /* last */);
    auto snapshot = ArcDataSnapshotdManager::Snapshot::CreateForTesting(
        local_state(), true /* blocked_ui_mode */, "" /* started_date */,
        std::move(last), std::move(previous));
    snapshot->Sync();
  }

  void RunUntilIdle() {
    if (is_dbus_client_available()) {
      task_environment_.RunUntilIdle();
      return;
    }
    size_t attempts_number =
        ArcDataSnapshotdBridge::max_connection_attempt_count_for_testing();
    for (size_t i = 0; i < attempts_number; i++) {
      task_environment_.FastForwardBy(
          ArcDataSnapshotdBridge::connection_attempt_interval_for_testing());
      task_environment_.RunUntilIdle();
    }
  }
};

// Test basic scenario: start / stop arc-data-snapshotd.
TEST_F(ArcDataSnapshotdManagerBasicTest, Basic) {
  ArcDataSnapshotdManager manager(local_state());
  EXPECT_EQ(manager.state(), ArcDataSnapshotdManager::State::kNone);
  EXPECT_FALSE(manager.bridge());

  ExpectStartDaemon(true /*success */);
  manager.EnsureDaemonStarted(base::DoNothing());
  EXPECT_TRUE(manager.bridge());

  ExpectStopDaemon(true /*success */);
  manager.EnsureDaemonStopped(base::DoNothing());
  EXPECT_FALSE(manager.bridge());
}

// Test a double start scenario: start arc-data-snapshotd twice.
// Upstart job returns "false" if the job is already running.
TEST_F(ArcDataSnapshotdManagerBasicTest, DoubleStart) {
  ArcDataSnapshotdManager manager(local_state());
  EXPECT_EQ(manager.state(), ArcDataSnapshotdManager::State::kNone);
  EXPECT_FALSE(manager.bridge());

  ExpectStartDaemon(true /*success */);
  manager.EnsureDaemonStarted(base::DoNothing());
  EXPECT_TRUE(manager.bridge());

  // The attempt to start the already running daemon.
  // upstart client is not aware of this.
  manager.EnsureDaemonStarted(base::DoNothing());
  EXPECT_TRUE(manager.bridge());

  // Stop daemon from dtor.
  ExpectStopDaemon(true /*success */);
}

// Test that arc-data-snapshotd daemon is already running when |manager| gets
// created.
// Test that arc-data-snapshotd daemon is already stopped when |manager| tries
// to stop it.
TEST_F(ArcDataSnapshotdManagerBasicTest, UpstartFailures) {
  ArcDataSnapshotdManager manager(local_state());
  EXPECT_EQ(manager.state(), ArcDataSnapshotdManager::State::kNone);
  EXPECT_FALSE(manager.bridge());

  ExpectStartDaemon(false /* success */);
  manager.EnsureDaemonStarted(base::DoNothing());
  EXPECT_TRUE(manager.bridge());

  ExpectStopDaemon(false /* success */);
  manager.EnsureDaemonStopped(base::DoNothing());
  EXPECT_FALSE(manager.bridge());
}

TEST_F(ArcDataSnapshotdManagerBasicTest, RestoredAfterCrash) {
  SetUpRestoredSessionCommandLine();
  // The attempt to stop the daemon, started before crash.
  ExpectStopDaemon(true /*success */);
  ArcDataSnapshotdManager manager(local_state());
  EXPECT_EQ(manager.state(), ArcDataSnapshotdManager::State::kRestored);
  EXPECT_FALSE(manager.bridge());

  ExpectStartDaemon(true /*success */);
  manager.EnsureDaemonStarted(base::DoNothing());

  // Stop daemon from dtor.
  ExpectStopDaemon(true /*success */);
}

// Test clear snapshots flow.
TEST_P(ArcDataSnapshotdManagerFlowTest, ClearSnapshotsBasic) {
  // Set up two snapshots (previous and last) in local_state.
  SetupLocalState();
  CheckSnapshotsNumber(2 /* expected_number */);

  // Once |manager| is created, it tries to clear both snapshots, because the
  // mechanism is disabled by default, and stop the daemon.
  // Start to clear snapshots.
  ExpectStartDaemon(true /*success */);
  // Stop once finished clearing.
  ExpectStopDaemon(true /*success */);
  ArcDataSnapshotdManager manager(local_state());
  RunUntilIdle();

  // No snapshots in local_state either.
  EXPECT_EQ(manager.state(), ArcDataSnapshotdManager::State::kNone);
  CheckSnapshotsNumber(0 /* expected_number */);

  EXPECT_FALSE(manager.bridge());
}

// Test blocked UI mode flow.
TEST_P(ArcDataSnapshotdManagerFlowTest, BlockedUiBasic) {
  // Set up two snapshots (previous and last) in local_state.
  SetupLocalState();
  CheckSnapshotsNumber(2 /* expected_number */);
  // Enable snapshotting mechanism for testing.
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);

  // Once |manager| is created, it tries to clear both snapshots, because the
  // mechanism is disabled by default, and stop the daemon.
  // Start to clear snapshots.
  ExpectStartDaemon(true /*success */);
  // Stop once finished clearing.
  ExpectStopDaemon(true /*success */);
  ArcDataSnapshotdManager manager(local_state());
  CheckHeadlessMode();
  EXPECT_EQ(manager.state(), ArcDataSnapshotdManager::State::kBlockedUi);
  RunUntilIdle();

  // Snapshots are valid, no need to clear.
  CheckSnapshotsNumber(2 /* expected_number */);

  auto expected_state = is_dbus_client_available()
                            ? ArcDataSnapshotdManager::State::kMgsToLaunch
                            : ArcDataSnapshotdManager::State::kBlockedUi;

  // The communication is established and MGS can be launched.
  EXPECT_EQ(manager.state(), expected_state);
  EXPECT_TRUE(manager.bridge());
}

INSTANTIATE_TEST_SUITE_P(ArcDataSnapshotdManagerFlowTest,
                         ArcDataSnapshotdManagerFlowTest,
                         ::testing::Values(true, false));

}  // namespace

}  // namespace data_snapshotd
}  // namespace arc
