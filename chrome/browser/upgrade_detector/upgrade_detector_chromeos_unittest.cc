// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/upgrade_detector_chromeos.h"

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
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
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestUpgradeDetectorChromeos : public UpgradeDetectorChromeos {
 public:
  explicit TestUpgradeDetectorChromeos(const base::Clock* clock,
                                       const base::TickClock* tick_clock)
      : UpgradeDetectorChromeos(clock, tick_clock) {}
  ~TestUpgradeDetectorChromeos() override = default;

  // Exposed for testing.
  using UpgradeDetector::AdjustDeadline;
  using UpgradeDetectorChromeos::UPGRADE_AVAILABLE_REGULAR;

  DISALLOW_COPY_AND_ASSIGN(TestUpgradeDetectorChromeos);
};

class MockUpgradeObserver : public UpgradeObserver {
 public:
  explicit MockUpgradeObserver(UpgradeDetector* upgrade_detector)
      : upgrade_detector_(upgrade_detector) {
    upgrade_detector_->AddObserver(this);
  }
  ~MockUpgradeObserver() override { upgrade_detector_->RemoveObserver(this); }
  MOCK_METHOD0(OnUpdateOverCellularAvailable, void());
  MOCK_METHOD0(OnUpdateOverCellularOneTimePermissionGranted, void());
  MOCK_METHOD0(OnUpgradeRecommended, void());
  MOCK_METHOD0(OnCriticalUpgradeInstalled, void());
  MOCK_METHOD0(OnOutdatedInstall, void());
  MOCK_METHOD0(OnOutdatedInstallNoAutoUpdate, void());
  MOCK_METHOD1(OnRelaunchOverriddenToRequired, void(bool override));

 private:
  UpgradeDetector* const upgrade_detector_;
  DISALLOW_COPY_AND_ASSIGN(MockUpgradeObserver);
};

}  // namespace

class UpgradeDetectorChromeosTest : public ::testing::Test {
 protected:
  UpgradeDetectorChromeosTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_local_state_(TestingBrowserProcess::GetGlobal()) {
    // By default, test with the relaunch policy enabled.
    SetIsRelaunchNotificationPolicyEnabled(true /* enabled */);

    // Disable the detector's check to see if autoupdates are inabled.
    // Without this, tests put the detector into an invalid state by detecting
    // upgrades before the detection task completes.
    scoped_local_state_.Get()->SetUserPref(prefs::kAttemptedToEnableAutoupdate,
                                           std::make_unique<base::Value>(true));

    fake_update_engine_client_ = new chromeos::FakeUpdateEngineClient();
    chromeos::DBusThreadManager::Initialize();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
        std::unique_ptr<chromeos::UpdateEngineClient>(
            fake_update_engine_client_));

    // Fast forward to set current time to local 2am . This is done to align the
    // relaunch deadline within the default relaunch window of 2am to 4am so
    // that it is not adjusted in tests.
    const char* tz = getenv("TZ");
    if (tz)
      old_tz_ = tz;
    setenv("TZ", "UTC", 1);
    tzset();
    FastForwardBy(base::TimeDelta::FromHours(2));
  }

  ~UpgradeDetectorChromeosTest() override {
    if (!old_tz_.empty()) {
      setenv("TZ", old_tz_.c_str(), 1);
    } else {
      unsetenv("TZ");
    }
    tzset();

    chromeos::DBusThreadManager::Shutdown();
  }

  const base::Clock* GetMockClock() { return task_environment_.GetMockClock(); }

  const base::TickClock* GetMockTickClock() {
    return task_environment_.GetMockTickClock();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void NotifyUpdateReadyToInstall(const std::string& version) {
    update_engine::StatusResult status;
    if (!version.empty())
      status.set_new_version(version);
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
  std::string old_tz_;

  chromeos::FakeUpdateEngineClient* fake_update_engine_client_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(UpgradeDetectorChromeosTest);
};

TEST_F(UpgradeDetectorChromeosTest, PolicyNotEnabled) {
  // Disable the relaunch notification policy as a whole.
  SetIsRelaunchNotificationPolicyEnabled(false /* enabled */);

  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();

  // The observer is expected to be notified that an upgrade is recommended.
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());

  NotifyUpdateReadyToInstall("1.0.0.0");

  upgrade_detector.Shutdown();
}

TEST_F(UpgradeDetectorChromeosTest, TestHighAnnoyanceDeadline) {
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  // Observer should get some notifications about new version.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended()).Times(testing::AtLeast(1));
  NotifyUpdateReadyToInstall("1.0.0.0");

  const auto deadline = upgrade_detector.GetHighAnnoyanceDeadline();

  // Another new version of ChromeOS is ready to install after high
  // annoyance reached.
  FastForwardBy(upgrade_detector.GetDefaultHighAnnoyanceThreshold());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  // New notification could be sent or not.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .Times(testing::AnyNumber());
  NotifyUpdateReadyToInstall("1.0.0.0");

  // Deadline wasn't changed because of new upgrade detected.
  EXPECT_EQ(upgrade_detector.GetHighAnnoyanceDeadline(), deadline);
  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, TestHeadsUpPeriod) {
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  const auto notification_period = base::TimeDelta::FromDays(7);
  const auto heads_up_period = base::TimeDelta::FromDays(1);
  SetNotificationPeriodPref(notification_period);
  SetHeadsUpPeriodPref(heads_up_period);

  const auto no_notification_till =
      notification_period - heads_up_period - base::TimeDelta::FromMinutes(1);
  const auto first_notification_at = notification_period - heads_up_period;
  const auto second_notification_at = notification_period;

  // Observer should not get notifications about new version till 6-th day.
  NotifyUpdateReadyToInstall("1.0.0.0");
  FastForwardBy(no_notification_till);
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  FastForwardBy(first_notification_at - no_notification_till);
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);

  // UPGRADE_ANNOYANCE_HIGH at the end of the period.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .Times(testing::AnyNumber());
  FastForwardBy(second_notification_at - first_notification_at);
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, TestHeadsUpPeriodChange) {
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  SetNotificationPeriodPref(base::TimeDelta::FromDays(7));
  SetHeadsUpPeriodPref(base::TimeDelta::FromDays(1));

  // Observer should not get notifications about new version first 4 days.
  NotifyUpdateReadyToInstall("1.0.0.0");
  FastForwardBy(base::TimeDelta::FromDays(4));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // Observer should get notifications because of HeadsUpPeriod change.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetHeadsUpPeriodPref(base::TimeDelta::FromDays(3));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);

  // UPGRADE_ANNOYANCE_HIGH at the end of the period.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .Times(testing::AnyNumber());
  FastForwardBy(base::TimeDelta::FromDays(3));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, TestHeadsUpPeriodNotificationPeriodChange) {
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  SetNotificationPeriodPref(base::TimeDelta::FromDays(7));
  SetHeadsUpPeriodPref(base::TimeDelta::FromDays(1));

  // Observer should not get notifications about new version first 4 days.
  NotifyUpdateReadyToInstall("1.0.0.0");
  FastForwardBy(base::TimeDelta::FromDays(4));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // Observer should get notifications because of NotificationPeriod change.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta::FromDays(5));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);

  // UPGRADE_ANNOYANCE_HIGH at the end of the period.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .Times(testing::AnyNumber());
  FastForwardBy(base::TimeDelta::FromDays(3));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest,
       TestHeadsUpPeriodOverflowNotificationPeriod) {
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  SetNotificationPeriodPref(base::TimeDelta::FromDays(7));
  SetHeadsUpPeriodPref(base::TimeDelta::FromDays(8));

  // Observer should get notification because HeadsUpPeriod bigger than
  // NotificationPeriod.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  NotifyUpdateReadyToInstall("1.0.0.0");
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  EXPECT_EQ(upgrade_detector.GetHighAnnoyanceLevelDelta(),
            base::TimeDelta::FromDays(7));

  // HighAnnoyanceLevelDelta becomes 8 days because period is increased.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta::FromDays(14));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  EXPECT_EQ(upgrade_detector.GetHighAnnoyanceLevelDelta(),
            base::TimeDelta::FromDays(8));

  // Bring NotificationPeriod back, HighAnnoyanceLevelDelta becomes 7 days.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta::FromDays(7));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  EXPECT_EQ(upgrade_detector.GetHighAnnoyanceLevelDelta(),
            base::TimeDelta::FromDays(7));

  // UPGRADE_ANNOYANCE_HIGH at the end of the period.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .Times(testing::AnyNumber());
  FastForwardBy(base::TimeDelta::FromDays(7));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, OnUpgradeRecommendedCalledOnce) {
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  SetNotificationPeriodPref(base::TimeDelta::FromDays(7));
  SetHeadsUpPeriodPref(base::TimeDelta::FromDays(7));

  // Observer should get notification because HeadsUpPeriod is the same as
  // NotificationPeriod.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  NotifyUpdateReadyToInstall("1.0.0.0");
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Both periods are changed but OnUpgradeRecommended called only once.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta::FromDays(8));
  SetHeadsUpPeriodPref(base::TimeDelta::FromDays(8));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_observer);

  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, DeadlineAdjustmentDefaultWindow) {
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  const auto delta = base::TimeDelta::FromDays(7);

  base::Time detect_time;
  ASSERT_TRUE(base::Time::FromString("1 Jan 2018 06:00", &detect_time));
  base::Time deadline, deadline_lower_border, deadline_upper_border;
  ASSERT_TRUE(
      base::Time::FromString("9 Jan 2018 02:00", &deadline_lower_border));
  ASSERT_TRUE(
      base::Time::FromString("9 Jan 2018 04:00", &deadline_upper_border));
  deadline = upgrade_detector.AdjustDeadline(detect_time + delta);
  EXPECT_GE(deadline, deadline_lower_border);
  EXPECT_LE(deadline, deadline_upper_border);

  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorChromeosTest, TestOverrideThresholds) {
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  const auto notification_period = base::TimeDelta::FromDays(7);
  const auto heads_up_period = base::TimeDelta::FromDays(2);
  SetNotificationPeriodPref(notification_period);
  SetHeadsUpPeriodPref(heads_up_period);
  // Simulate update installed.
  NotifyUpdateReadyToInstall("1.0.0.0");
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // Overriding the thresholds should change the high annoyance deadline and the
  // notification stage accordingly and notify the observers.
  base::TimeDelta delta = base::TimeDelta::FromHours(2);
  base::Time deadline = upgrade_detector.upgrade_detected_time() + delta;
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  upgrade_detector.OverrideHighAnnoyanceDeadline(deadline);
  EXPECT_EQ(upgrade_detector.GetHighAnnoyanceDeadline(), deadline);
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
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  NotifyUpdateReadyToInstall("1.0.0.0");
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
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  // Finish the first update and set annoyance level to ELEVATED.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended()).Times(testing::AtLeast(1));
  NotifyUpdateReadyToInstall("1.0.0.0");
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
  NotifyUpdateReadyToInstall("2.0.0.0");
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
