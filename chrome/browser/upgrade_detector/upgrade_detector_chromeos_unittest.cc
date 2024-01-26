// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/upgrade_detector_chromeos.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/environment.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestUpgradeDetectorChromeos : public UpgradeDetectorChromeos {
 public:
  explicit TestUpgradeDetectorChromeos(const base::Clock* clock,
                                       const base::TickClock* tick_clock)
      : UpgradeDetectorChromeos(clock, tick_clock) {}

  TestUpgradeDetectorChromeos(const TestUpgradeDetectorChromeos&) = delete;
  TestUpgradeDetectorChromeos& operator=(const TestUpgradeDetectorChromeos&) =
      delete;

  ~TestUpgradeDetectorChromeos() override = default;

  // Exposed for testing.
  using UpgradeDetector::AdjustDeadline;
  using UpgradeDetector::GetDefaultRelaunchWindow;
  using UpgradeDetectorChromeos::UPGRADE_AVAILABLE_REGULAR;

  base::TimeDelta GetHighAnnoyanceLevelDelta() {
    return GetAnnoyanceLevelDeadline(UpgradeDetector::UPGRADE_ANNOYANCE_HIGH) -
           GetAnnoyanceLevelDeadline(
               UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  }
};

class MockUpgradeObserver : public UpgradeObserver {
 public:
  explicit MockUpgradeObserver(UpgradeDetector* upgrade_detector)
      : upgrade_detector_(upgrade_detector) {
    upgrade_detector_->AddObserver(this);
  }

  MockUpgradeObserver(const MockUpgradeObserver&) = delete;
  MockUpgradeObserver& operator=(const MockUpgradeObserver&) = delete;

  ~MockUpgradeObserver() override { upgrade_detector_->RemoveObserver(this); }
  MOCK_METHOD0(OnUpdateOverCellularAvailable, void());
  MOCK_METHOD0(OnUpdateOverCellularOneTimePermissionGranted, void());
  MOCK_METHOD0(OnUpgradeRecommended, void());
  MOCK_METHOD0(OnCriticalUpgradeInstalled, void());
  MOCK_METHOD0(OnOutdatedInstall, void());
  MOCK_METHOD0(OnOutdatedInstallNoAutoUpdate, void());
  MOCK_METHOD1(OnRelaunchOverriddenToRequired, void(bool overridden));

 private:
  const raw_ptr<UpgradeDetector> upgrade_detector_;
};

}  // namespace

class UpgradeDetectorChromeosTest : public ::testing::Test {
 public:
  UpgradeDetectorChromeosTest(const UpgradeDetectorChromeosTest&) = delete;
  UpgradeDetectorChromeosTest& operator=(const UpgradeDetectorChromeosTest&) =
      delete;

 protected:
  UpgradeDetectorChromeosTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_local_state_(TestingBrowserProcess::GetGlobal()),
        env_(base::Environment::Create()) {
    // Disable the detector's check to see if autoupdates are inabled.
    // Without this, tests put the detector into an invalid state by detecting
    // upgrades before the detection task completes.
    scoped_local_state_.Get()->SetUserPref(prefs::kAttemptedToEnableAutoupdate,
                                           std::make_unique<base::Value>(true));

    fake_update_engine_client_ =
        ash::UpdateEngineClient::InitializeFakeForTest();

    // Fast forward to set current time to local 2am . This is done to align the
    // relaunch deadline within the default relaunch window of 2am to 4am so
    // that it is not adjusted in tests.
    std::string env_tz;
    if (env_->GetVar("TZ", &env_tz))
      original_tz_ = env_tz;
    env_->SetVar("TZ", "UTC");
    tzset();
    FastForwardBy(base::Hours(2));
  }

  ~UpgradeDetectorChromeosTest() override {
    // Revert back to the original timezone.
    if (original_tz_) {
      env_->SetVar("TZ", original_tz_.value());
    } else {
      env_->UnSetVar("TZ");
    }
    tzset();

    ash::UpdateEngineClient::Shutdown();
  }

  const base::Clock* GetMockClock() { return task_environment_.GetMockClock(); }

  const base::TickClock* GetMockTickClock() {
    return task_environment_.GetMockTickClock();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void NotifyUpdateReadyToInstall(const std::string& version,
                                  bool is_rollback,
                                  bool will_powerwash) {
    update_engine::StatusResult status;
    if (!version.empty())
      status.set_new_version(version);
    status.set_is_enterprise_rollback(is_rollback);
    status.set_will_powerwash_after_reboot(will_powerwash);
    status.set_current_operation(update_engine::Operation::UPDATED_NEED_REBOOT);
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  void NotifyUpdateDownloading() {
    update_engine::StatusResult status;
    status.set_current_operation(update_engine::Operation::DOWNLOADING);
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  void NotifyStatusIdle() {
    update_engine::StatusResult status;
    status.set_current_operation(update_engine::Operation::IDLE);
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  // Sets the browser.relaunch_notification preference in Local State to
  // |value|.
  void SetIsRelaunchNotificationPolicyEnabled(bool enabled) {
    constexpr int kChromeMenuOnly = 0;     // Disabled.
    constexpr int kRecommendedBubble = 1;  // Enabled.

    scoped_local_state_.Get()->SetManagedPref(
        prefs::kRelaunchNotification,
        std::make_unique<base::Value>(enabled ? kRecommendedBubble
                                              : kChromeMenuOnly));
  }

  // Sets the browser.relaunch_notification_period preference in Local State to
  // |value|.
  void SetNotificationPeriodPref(base::TimeDelta value) {
    if (value.is_zero()) {
      scoped_local_state_.Get()->RemoveManagedPref(
          prefs::kRelaunchNotificationPeriod);
    } else {
      scoped_local_state_.Get()->SetManagedPref(
          prefs::kRelaunchNotificationPeriod,
          std::make_unique<base::Value>(
              base::saturated_cast<int>(value.InMilliseconds())));
    }
  }

  // Sets the browser.relaunch_heads_up_period preference in Local State to
  // |value|.
  void SetHeadsUpPeriodPref(base::TimeDelta value) {
    if (value.is_zero()) {
      scoped_local_state_.Get()->RemoveManagedPref(
          prefs::kRelaunchHeadsUpPeriod);
    } else {
      scoped_local_state_.Get()->SetManagedPref(
          prefs::kRelaunchHeadsUpPeriod,
          std::make_unique<base::Value>(
              base::saturated_cast<int>(value.InMilliseconds())));
    }
  }

  // Fast-forwards virtual time by |delta|.
  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_local_state_;
  std::unique_ptr<base::Environment> env_;
  std::optional<std::string> original_tz_;

  raw_ptr<ash::FakeUpdateEngineClient, DanglingUntriaged>
      fake_update_engine_client_;  // Not owned.
};

TEST_F(UpgradeDetectorChromeosTest, PolicyNotEnabled) {
  // RelaunchNotification policy is disabled.
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();

  // The observer is expected to be notified that an upgrade is recommended.
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());

  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);

  upgrade_detector.Shutdown();
}

TEST_F(UpgradeDetectorChromeosTest, PolicyNotEnabledRollback) {
  // RelaunchNotification policy is disabled.
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();

  // The observer is expected to be notified that an upgrade is recommended.
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());

  NotifyUpdateReadyToInstall("1.0.0.0", true /*is_rollback*/,
                             true /*will_powerwash*/);

  upgrade_detector.Shutdown();
}

TEST_F(UpgradeDetectorChromeosTest, PolicyEnabled) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  // For regular updates the notification is delayed if the policy is enabled.
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();

  // The observer is not expected to be notified yet.
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);
  EXPECT_CALL(mock_observer, OnUpgradeRecommended()).Times(0);

  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);

  upgrade_detector.Shutdown();
}

TEST_F(UpgradeDetectorChromeosTest, PolicyEnabledRollback) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  // Notification should always appear for rollback.
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();

  // The observer is expected to be notified that an upgrade is recommended.
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());

  NotifyUpdateReadyToInstall("1.0.0.0", true /*is_rollback*/,
                             true /*will_powerwash*/);

  upgrade_detector.Shutdown();
}

TEST_F(UpgradeDetectorChromeosTest, PolicyEnabledPowerwash) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  // Notification should always appear if the device is going to be powerwashed.
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();

  // The observer is expected to be notified that an upgrade is recommended.
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());

  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             true /*will_powerwash*/);

  upgrade_detector.Shutdown();
}

TEST_F(UpgradeDetectorChromeosTest, TestHighAnnoyanceDeadline) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  // Observer should get some notifications about new version.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended()).Times(testing::AtLeast(1));
  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);

  const auto deadline = upgrade_detector.GetAnnoyanceLevelDeadline(
      UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);

  // Another new version of ChromeOS is ready to install after high
  // annoyance reached.
  FastForwardBy(upgrade_detector.GetDefaultHighAnnoyanceThreshold());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  // New notification could be sent or not.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .Times(testing::AnyNumber());
  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);

  // Deadline wasn't changed because of new upgrade detected.
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_HIGH),
            deadline);
  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, TestHeadsUpPeriod) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  const auto notification_period = base::Days(7);
  const auto heads_up_period = base::Days(1);
  const auto grace_period = base::Hours(1);
  SetNotificationPeriodPref(notification_period);
  SetHeadsUpPeriodPref(heads_up_period);

  const auto no_notification_till =
      notification_period - heads_up_period - base::Minutes(1);
  const auto first_notification_at = notification_period - heads_up_period;
  const auto second_notification_at = notification_period - grace_period;
  const auto third_notification_at = notification_period;

  // Observer should not get notifications about new version till 6-th day.
  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);
  FastForwardBy(no_notification_till);
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  FastForwardBy(first_notification_at - no_notification_till);
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);

  // Notifications should arrive every 20 minutes after the first one with one
  // final notification at grace annoyance.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .Times(testing::AnyNumber());
  FastForwardBy(second_notification_at - first_notification_at);
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_GRACE);

  // UPGRADE_ANNOYANCE_HIGH at the end of the period.
  // Notifications should arrive every 20 minutes after the first one with one
  // final notification at high annoyance.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .Times(testing::AnyNumber());
  FastForwardBy(third_notification_at - second_notification_at);
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, TestGracePeriodChange) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  SetNotificationPeriodPref(base::Days(4));
  SetHeadsUpPeriodPref(base::Minutes(90));

  // Observer should not get notifications about new version first 3 days.
  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);
  FastForwardBy(base::Days(3));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // Observer should get notifications because of elevated annoyance reached.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  FastForwardBy(base::Days(1) - base::Minutes(90));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);

  // Observer should get notifications because of grace annoyance reached which
  // is midway of elevated and high deadline.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .Times(testing::AnyNumber());
  FastForwardBy(base::Minutes(45));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_GRACE);

  // Observer should get notifications as annoyance level is back to elevated
  // because of HeadsUpPeriod change.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetHeadsUpPeriodPref(base::Minutes(60));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);

  // Observer should get notifications as annoyance level moves to grace because
  // of HeadsUpPeriod change.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetHeadsUpPeriodPref(base::Minutes(120));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_GRACE);

  // Observer should get notifications as annoyance level moves to none because
  // of NotificationPeriod change.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::Days(7));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // UPGRADE_ANNOYANCE_HIGH at the end of the period.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .Times(testing::AnyNumber());
  FastForwardBy(base::Days(3) + base::Minutes(45));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, TestHeadsUpPeriodChange) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  SetNotificationPeriodPref(base::Days(7));
  SetHeadsUpPeriodPref(base::Days(1));

  // Observer should not get notifications about new version first 4 days.
  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);
  FastForwardBy(base::Days(4));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // Observer should get notifications because of HeadsUpPeriod change.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetHeadsUpPeriodPref(base::Days(3));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);

  // UPGRADE_ANNOYANCE_HIGH at the end of the period.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .Times(testing::AnyNumber());
  FastForwardBy(base::Days(3));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, TestHeadsUpPeriodNotificationPeriodChange) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  SetNotificationPeriodPref(base::Days(7));
  SetHeadsUpPeriodPref(base::Days(1));

  // Observer should not get notifications about new version first 4 days.
  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);
  FastForwardBy(base::Days(4));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // Observer should get notifications because of NotificationPeriod change.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::Days(5));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);

  // UPGRADE_ANNOYANCE_HIGH at the end of the period.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .Times(testing::AnyNumber());
  FastForwardBy(base::Days(3));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest,
       TestHeadsUpPeriodOverflowNotificationPeriod) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  SetNotificationPeriodPref(base::Days(7));
  SetHeadsUpPeriodPref(base::Days(8));

  // Observer should get notification because HeadsUpPeriod bigger than
  // NotificationPeriod.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  EXPECT_EQ(upgrade_detector.GetHighAnnoyanceLevelDelta(), base::Days(7));

  // HighAnnoyanceLevelDelta becomes 8 days because period is increased.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::Days(14));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  EXPECT_EQ(upgrade_detector.GetHighAnnoyanceLevelDelta(), base::Days(8));

  // Bring NotificationPeriod back, HighAnnoyanceLevelDelta becomes 7 days.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::Days(7));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  EXPECT_EQ(upgrade_detector.GetHighAnnoyanceLevelDelta(), base::Days(7));

  // UPGRADE_ANNOYANCE_HIGH at the end of the period.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .Times(testing::AnyNumber());
  FastForwardBy(base::Days(7));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, OnUpgradeRecommendedCalledOnce) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  SetNotificationPeriodPref(base::Days(7));
  SetHeadsUpPeriodPref(base::Days(7));

  // Observer should get notification because HeadsUpPeriod is the same as
  // NotificationPeriod.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Both periods are changed but OnUpgradeRecommended called only once.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::Days(8));
  SetHeadsUpPeriodPref(base::Days(8));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);

  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, DeadlineAdjustmentDefaultWindow) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  const auto delta = base::Days(7);

  base::Time detect_time;
  ASSERT_TRUE(base::Time::FromString("1 Jan 2018 06:00", &detect_time));
  base::Time deadline, deadline_lower_border, deadline_upper_border;
  ASSERT_TRUE(
      base::Time::FromString("9 Jan 2018 02:00", &deadline_lower_border));
  ASSERT_TRUE(
      base::Time::FromString("9 Jan 2018 04:00", &deadline_upper_border));
  const auto relaunch_window = upgrade_detector.GetDefaultRelaunchWindow();
  deadline =
      upgrade_detector.AdjustDeadline(detect_time + delta, relaunch_window);
  EXPECT_GE(deadline, deadline_lower_border);
  EXPECT_LE(deadline, deadline_upper_border);

  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, TestOverrideThresholds) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  const auto notification_period = base::Days(7);
  const auto heads_up_period = base::Days(2);
  SetNotificationPeriodPref(notification_period);
  SetHeadsUpPeriodPref(heads_up_period);
  // Simulate update installed.
  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // Overriding the thresholds should change the high annoyance deadline and the
  // notification stage accordingly and notify the observers.
  base::TimeDelta delta = base::Hours(2);
  base::Time deadline = upgrade_detector.upgrade_detected_time() + delta;
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  upgrade_detector.OverrideHighAnnoyanceDeadline(deadline);
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_HIGH),
            deadline);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  upgrade_detector.ResetOverriddenDeadline();
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, TestOverrideNotificationType) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // Observer should get some notification about the overridden relaunch
  // notification style.
  EXPECT_CALL(mock_observer, OnRelaunchOverriddenToRequired(true));
  upgrade_detector.OverrideRelaunchNotificationToRequired(true);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Observer should get some notification about the resetting the overridden
  // relaunch notification style.
  EXPECT_CALL(mock_observer, OnRelaunchOverriddenToRequired(false));
  upgrade_detector.OverrideRelaunchNotificationToRequired(false);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, TestUpdateInProgress) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  // Finish the first update and set annoyance level to ELEVATED.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended()).Times(testing::AtLeast(1));
  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);
  FastForwardBy(upgrade_detector.GetDefaultElevatedAnnoyanceThreshold());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);

  // Start a second update with a new version and make sure observers are
  // notified when annoyance level changes to NONE during download.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  NotifyUpdateDownloading();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // Complete update and make sure observers are notified when the annoyance
  // level goes back to the previous state.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  NotifyUpdateReadyToInstall("2.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);

  // Change level to HIGH and make sure observers are notified.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended()).Times(testing::AtLeast(1));
  FastForwardBy(upgrade_detector.GetHighAnnoyanceLevelDelta());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);

  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, TestInvalidateUpdate) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  // Finish the first update and set annoyance level to ELEVATED.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended()).Times(testing::AtLeast(1));
  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);
  FastForwardBy(upgrade_detector.GetDefaultElevatedAnnoyanceThreshold());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);

  // Invalidate update by resetting the status. Annoyance level should go
  // back down to NONE.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  NotifyStatusIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // Make sure any future updates complete successfully and observers are
  // notified.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended()).Times(testing::AtLeast(1));
  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);
  FastForwardBy(upgrade_detector.GetDefaultElevatedAnnoyanceThreshold());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);

  upgrade_detector.Shutdown();
  RunUntilIdle();
}

// Tests correct deadlines are set when an upgrade is detected.
TEST_F(UpgradeDetectorChromeosTest, AnnoyanceLevelDeadlines) {
  // Enable the relaunch notification policy.
  SetIsRelaunchNotificationPolicyEnabled(true /*enabled*/);

  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);
  upgrade_detector.Init();

  // Deadline not set before upgrade detected.
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_HIGH),
            base::Time());

  // Pretend that an upgrade was just detected now.
  NotifyUpdateReadyToInstall("1.0.0.0", false /*is_rollback*/,
                             false /*will_powerwash*/);
  base::Time detect_time = upgrade_detector.upgrade_detected_time();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_HIGH),
            detect_time + base::Days(7));
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_GRACE),
            detect_time + base::Days(7) - base::Hours(1));
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED),
            detect_time + base::Days(4));
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_LOW),
            detect_time);
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW),
            detect_time);
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_NONE),
            detect_time);

  // Drop the period and notice change in the deadlines.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::Days(1));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_HIGH),
            detect_time + base::Hours(24));
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_GRACE),
            detect_time + base::Hours(23));
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED),
            detect_time);
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_LOW),
            detect_time);
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW),
            detect_time);
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_NONE),
            detect_time);

  // Set heads up period and notice change in the deadlines.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetHeadsUpPeriodPref(base::Hours(10));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_HIGH),
            detect_time + base::Hours(24));
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_GRACE),
            detect_time + base::Hours(23));
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED),
            detect_time + base::Hours(14));
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_LOW),
            detect_time);
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW),
            detect_time);
  EXPECT_EQ(upgrade_detector.GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_NONE),
            detect_time);

  upgrade_detector.Shutdown();
}
