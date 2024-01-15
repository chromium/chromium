// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/installed_version_poller.h"

#include "base/memory/raw_ptr.h"
#include "stdint.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/browser/upgrade_detector/installed_version_monitor.h"
#include "chrome/browser/upgrade_detector/mock_build_state_observer.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Property;
using ::testing::Return;

namespace {

class FakeMonitor final : public InstalledVersionMonitor {
 public:
  FakeMonitor() = default;

  // Simulate that either a change was detected (|error| is false) or that an
  // error occurred (|error| is true).
  void Notify(bool error) { callback_.Run(error); }

  // InstalledVersionMonitor:
  void Start(Callback callback) override { callback_ = std::move(callback); }

 private:
  Callback callback_;
};

}  // namespace

class InstalledVersionPollerTest : public ::testing::Test {
 protected:
  InstalledVersionPollerTest() { build_state_.AddObserver(&mock_observer_); }
  ~InstalledVersionPollerTest() override {
    build_state_.RemoveObserver(&mock_observer_);
  }

  // Returns a version somewhat higher than the running version.
  static base::Version GetUpgradeVersion() {
    std::vector<uint32_t> components = version_info::GetVersion().components();
    components[3] += 2;
    return base::Version(components);
  }

  // Returns a version between the running version and the upgrade version
  // above.
  static base::Version GetCriticalVersion() {
    std::vector<uint32_t> components = version_info::GetVersion().components();
    components[3] += 1;
    return base::Version(components);
  }

  // Returns a version lower than the running version.
  static base::Version GetRollbackVersion() {
    std::vector<uint32_t> components = version_info::GetVersion().components();
    components[0] -= 1;
    return base::Version(components);
  }

  // Returns an InstalledAndCriticalVersion instance indicating no update.
  static InstalledAndCriticalVersion MakeNoUpdateVersions() {
    return InstalledAndCriticalVersion(version_info::GetVersion());
  }

  // Returns an InstalledAndCriticalVersion instance indicating an upgrade.
  static InstalledAndCriticalVersion MakeUpgradeVersions() {
    return InstalledAndCriticalVersion(GetUpgradeVersion());
  }

  // Returns an InstalledAndCriticalVersion instance indicating an upgrade with
  // a critical version.
  static InstalledAndCriticalVersion MakeCriticalUpgradeVersions() {
    return InstalledAndCriticalVersion(GetUpgradeVersion(),
                                       GetCriticalVersion());
  }

  // Returns an InstalledAndCriticalVersion instance with an invalid version.
  static InstalledAndCriticalVersion MakeErrorVersions() {
    return InstalledAndCriticalVersion(base::Version());
  }

  // Returns an InstalledAndCriticalVersion instance for a version rollback.
  static InstalledAndCriticalVersion MakeRollbackVersions() {
    return InstalledAndCriticalVersion(GetRollbackVersion());
  }

  std::unique_ptr<InstalledVersionMonitor> MakeMonitor() {
    EXPECT_FALSE(fake_monitor_);
    auto monitor = std::make_unique<FakeMonitor>();
    fake_monitor_ = monitor.get();
    return monitor;
  }

  void TriggerMonitor() {
    ASSERT_NE(fake_monitor_, nullptr);
    fake_monitor_->Notify(false);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ::testing::StrictMock<MockBuildStateObserver> mock_observer_;
  BuildState build_state_;
  raw_ptr<FakeMonitor, DanglingUntriaged> fake_monitor_ = nullptr;
};

// Tests that a poll returning the current version does not update the
// BuildState.
TEST_F(InstalledVersionPollerTest, TestNoUpdate) {
  base::MockRepeatingCallback<void(InstalledVersionCallback)> callback;
  EXPECT_CALL(callback, Run(_)).WillOnce([](InstalledVersionCallback callback) {
    std::move(callback).Run(MakeNoUpdateVersions());
  });

  InstalledVersionPoller poller(&build_state_, callback.Get(), MakeMonitor(),
                                task_environment_.GetMockTickClock());
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // A second poll with the same version likewise does nothing.
  EXPECT_CALL(callback, Run(_)).WillOnce([](InstalledVersionCallback callback) {
    std::move(callback).Run(MakeNoUpdateVersions());
  });
  task_environment_.FastForwardBy(
      InstalledVersionPoller::kDefaultPollingInterval);
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);
}

// Tests that a poll with an update is reported to the BuildState.
TEST_F(InstalledVersionPollerTest, TestUpgrade) {
  base::MockRepeatingCallback<void(InstalledVersionCallback)> callback;

  // No update the first time.
  EXPECT_CALL(callback, Run(_)).WillOnce([](InstalledVersionCallback callback) {
    std::move(callback).Run(MakeNoUpdateVersions());
  });
  InstalledVersionPoller poller(&build_state_, callback.Get(), MakeMonitor(),
                                task_environment_.GetMockTickClock());
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // Followed by an update, which is reported.
  EXPECT_CALL(callback, Run(_)).WillOnce([](InstalledVersionCallback callback) {
    std::move(callback).Run(MakeUpgradeVersions());
  });
  EXPECT_CALL(
      mock_observer_,
      OnUpdate(
          AllOf(Eq(&build_state_),
                Property(&BuildState::update_type,
                         Eq(BuildState::UpdateType::kNormalUpdate)),
                Property(&BuildState::installed_version, IsTrue()),
                Property(&BuildState::installed_version,
                         Eq(std::optional<base::Version>(GetUpgradeVersion()))),
                Property(&BuildState::critical_version, IsFalse()))));
  task_environment_.FastForwardBy(
      InstalledVersionPoller::kDefaultPollingInterval);
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // Followed by the same update, which is not reported.
  EXPECT_CALL(callback, Run(_)).WillOnce([](InstalledVersionCallback callback) {
    std::move(callback).Run(MakeUpgradeVersions());
  });
  task_environment_.FastForwardBy(
      InstalledVersionPoller::kDefaultPollingInterval);
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);
}

// Tests that a poll with an update is reported to the BuildState and that a
// subsequent poll back to the original version is also reported.
TEST_F(InstalledVersionPollerTest, TestUpgradeThenDowngrade) {
  base::MockRepeatingCallback<void(InstalledVersionCallback)> callback;

  // An update is found.
  EXPECT_CALL(callback, Run(_)).WillOnce([](InstalledVersionCallback callback) {
    std::move(callback).Run(MakeUpgradeVersions());
  });
  EXPECT_CALL(
      mock_observer_,
      OnUpdate(
          AllOf(Eq(&build_state_),
                Property(&BuildState::update_type,
                         Eq(BuildState::UpdateType::kNormalUpdate)),
                Property(&BuildState::installed_version, IsTrue()),
                Property(&BuildState::installed_version,
                         Eq(std::optional<base::Version>(GetUpgradeVersion()))),
                Property(&BuildState::critical_version, IsFalse()))));
  InstalledVersionPoller poller(&build_state_, callback.Get(), MakeMonitor(),
                                task_environment_.GetMockTickClock());
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // Which is then reverted back to the running version.
  EXPECT_CALL(callback, Run(_)).WillOnce([](InstalledVersionCallback callback) {
    std::move(callback).Run(MakeNoUpdateVersions());
  });
  EXPECT_CALL(
      mock_observer_,
      OnUpdate(AllOf(
          Eq(&build_state_),
          Property(&BuildState::update_type, Eq(BuildState::UpdateType::kNone)),
          Property(&BuildState::installed_version, IsFalse()),
          Property(&BuildState::critical_version, IsFalse()))));
  task_environment_.FastForwardBy(
      InstalledVersionPoller::kDefaultPollingInterval);

  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);
}

// Tests that a poll with a critical update is reported to the BuildState.
TEST_F(InstalledVersionPollerTest, TestCriticalUpgrade) {
  base::MockRepeatingCallback<void(InstalledVersionCallback)> callback;

  EXPECT_CALL(callback, Run(_)).WillOnce([](InstalledVersionCallback callback) {
    std::move(callback).Run(MakeCriticalUpgradeVersions());
  });
  EXPECT_CALL(
      mock_observer_,
      OnUpdate(AllOf(
          Eq(&build_state_),
          Property(&BuildState::update_type,
                   Eq(BuildState::UpdateType::kNormalUpdate)),
          Property(&BuildState::installed_version, IsTrue()),
          Property(&BuildState::installed_version,
                   Eq(std::optional<base::Version>(GetUpgradeVersion()))),
          Property(&BuildState::critical_version, IsTrue()),
          Property(&BuildState::critical_version,
                   Eq(std::optional<base::Version>(GetCriticalVersion()))))));
  InstalledVersionPoller poller(&build_state_, callback.Get(), MakeMonitor(),
                                task_environment_.GetMockTickClock());
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);
}

// Tests that a poll that failed to find a version reports an update anyway.
TEST_F(InstalledVersionPollerTest, TestMissingVersion) {
  base::MockRepeatingCallback<void(InstalledVersionCallback)> callback;

  EXPECT_CALL(callback, Run(_)).WillOnce([](InstalledVersionCallback callback) {
    std::move(callback).Run(MakeErrorVersions());
  });
  EXPECT_CALL(
      mock_observer_,
      OnUpdate(AllOf(Eq(&build_state_),
                     Property(&BuildState::update_type,
                              Eq(BuildState::UpdateType::kNormalUpdate)),
                     Property(&BuildState::installed_version, IsFalse()),
                     Property(&BuildState::critical_version, IsFalse()))));
  InstalledVersionPoller poller(&build_state_, callback.Get(), MakeMonitor(),
                                task_environment_.GetMockTickClock());
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);
}

// Tests that a version downgrade (a rollback) is reported as such.
TEST_F(InstalledVersionPollerTest, TestRollback) {
  base::MockRepeatingCallback<void(InstalledVersionCallback)> callback;

  EXPECT_CALL(callback, Run(_)).WillOnce([](InstalledVersionCallback callback) {
    std::move(callback).Run(MakeRollbackVersions());
  });
  EXPECT_CALL(
      mock_observer_,
      OnUpdate(AllOf(
          Eq(&build_state_),
          Property(&BuildState::update_type,
                   Eq(BuildState::UpdateType::kEnterpriseRollback)),
          Property(&BuildState::installed_version, IsTrue()),
          Property(&BuildState::installed_version,
                   Eq(std::optional<base::Version>(GetRollbackVersion()))),
          Property(&BuildState::critical_version, IsFalse()))));
  InstalledVersionPoller poller(&build_state_, callback.Get(), MakeMonitor(),
                                task_environment_.GetMockTickClock());
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);
}

// Tests that a modification in the monitored location triggers a poll.
TEST_F(InstalledVersionPollerTest, TestMonitor) {
  // Provide a GetInstalledVersionCallback that always reports no update, and
  // don't make any noise about it being called.
  ::testing::NiceMock<
      base::MockRepeatingCallback<void(InstalledVersionCallback)>>
      callback;
  ON_CALL(callback, Run(_))
      .WillByDefault([](InstalledVersionCallback callback) {
        std::move(callback).Run(MakeNoUpdateVersions());
      });

  InstalledVersionPoller poller(&build_state_, callback.Get(), MakeMonitor(),
                                task_environment_.GetMockTickClock());
  task_environment_.RunUntilIdle();

  // Poke the monitor so that it announces a change.
  TriggerMonitor();
  ::testing::Mock::VerifyAndClearExpectations(&callback);

  // Expect a poll in ten seconds.
  EXPECT_CALL(callback, Run(_));
  task_environment_.FastForwardBy(base::Seconds(10));
}
