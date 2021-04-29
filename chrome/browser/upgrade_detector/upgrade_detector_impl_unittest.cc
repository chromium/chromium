// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/upgrade_detector_impl.h"

#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/upgrade_detector/installed_version_poller.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/test/scoped_install_details.h"
#endif  // defined(OS_WIN)

using ::testing::AnyNumber;

namespace {

class TestUpgradeDetectorImpl : public UpgradeDetectorImpl {
 public:
  explicit TestUpgradeDetectorImpl(const base::Clock* clock,
                                   const base::TickClock* tick_clock)
      : UpgradeDetectorImpl(clock, tick_clock) {}
  ~TestUpgradeDetectorImpl() override = default;

  // Exposed for testing.
  using UpgradeDetectorImpl::clock;
  using UpgradeDetectorImpl::GetThresholdForLevel;
  using UpgradeDetectorImpl::NotifyOnUpgradeWithTimePassed;
  using UpgradeDetectorImpl::OnExperimentChangesDetected;
  using UpgradeDetectorImpl::UPGRADE_AVAILABLE_REGULAR;
  using UpgradeDetectorImpl::UpgradeDetected;

  // UpgradeDetector:
  void TriggerCriticalUpdate() override {
    ++trigger_critical_update_call_count_;
  }

  int trigger_critical_update_call_count() const {
    return trigger_critical_update_call_count_;
  }

 private:
  // How many times TriggerCriticalUpdate() has been called. Expected to either
  // be 0 or 1.
  int trigger_critical_update_call_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestUpgradeDetectorImpl);
};

class TestUpgradeNotificationListener : public UpgradeObserver {
 public:
  explicit TestUpgradeNotificationListener(UpgradeDetector* detector)
      : notifications_count_(0), detector_(detector) {
    DCHECK(detector_);
    detector_->AddObserver(this);
  }
  ~TestUpgradeNotificationListener() override {
    detector_->RemoveObserver(this);
  }

  int notification_count() const { return notifications_count_; }

 private:
  // UpgradeObserver:
  void OnUpgradeRecommended() override { ++notifications_count_; }

  // The number of upgrade recommended notifications received.
  int notifications_count_;

  UpgradeDetector* detector_;

  DISALLOW_COPY_AND_ASSIGN(TestUpgradeNotificationListener);
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

 private:
  UpgradeDetector* const upgrade_detector_;
  DISALLOW_COPY_AND_ASSIGN(MockUpgradeObserver);
};

}  // namespace

class UpgradeDetectorImplTest : public ::testing::Test {
 protected:
  UpgradeDetectorImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_local_state_(TestingBrowserProcess::GetGlobal()),
        scoped_poller_disabler_(
            InstalledVersionPoller::MakeScopedDisableForTesting()) {
    // Disable the detector's check to see if autoupdates are enabled.
    // Without this, tests put the detector into an invalid state by detecting
    // upgrades before the detection task completes.
    scoped_local_state_.Get()->SetUserPref(prefs::kAttemptedToEnableAutoupdate,
                                           std::make_unique<base::Value>(true));
    UpgradeDetector::GetInstance()->Init();
  }

  ~UpgradeDetectorImplTest() override {
    UpgradeDetector::GetInstance()->Shutdown();
  }

  const base::Clock* GetMockClock() { return task_environment_.GetMockClock(); }

  const base::TickClock* GetMockTickClock() {
    return task_environment_.GetMockTickClock();
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

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  // Fast-forwards virtual time by |delta|.
  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_local_state_;
  InstalledVersionPoller::ScopedDisableForTesting scoped_poller_disabler_;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::FakeBrowserDMTokenStorage dm_token_storage_;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  DISALLOW_COPY_AND_ASSIGN(UpgradeDetectorImplTest);
};

TEST_F(UpgradeDetectorImplTest, VariationsChanges) {
  TestUpgradeDetectorImpl detector(GetMockClock(), GetMockTickClock());
  TestUpgradeNotificationListener notifications_listener(&detector);
  detector.Init();
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_EQ(0, notifications_listener.notification_count());

  detector.OnExperimentChangesDetected(
      variations::VariationsService::Observer::BEST_EFFORT);
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_EQ(0, notifications_listener.notification_count());

  detector.NotifyOnUpgradeWithTimePassed(base::TimeDelta::FromDays(30));
  EXPECT_TRUE(detector.notify_upgrade());
  EXPECT_EQ(1, notifications_listener.notification_count());
  EXPECT_EQ(0, detector.trigger_critical_update_call_count());

  // Execute tasks posted by |detector| referencing it while it's still in
  // scope.
  RunUntilIdle();
  detector.Shutdown();
}

TEST_F(UpgradeDetectorImplTest, VariationsCriticalChanges) {
  TestUpgradeDetectorImpl detector(GetMockClock(), GetMockTickClock());
  TestUpgradeNotificationListener notifications_listener(&detector);
  detector.Init();
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_EQ(0, notifications_listener.notification_count());

  // Users are notified about critical updates immediately.
  detector.OnExperimentChangesDetected(
      variations::VariationsService::Observer::CRITICAL);
  EXPECT_TRUE(detector.notify_upgrade());
  EXPECT_EQ(1, notifications_listener.notification_count());
  EXPECT_EQ(1, detector.trigger_critical_update_call_count());

  // Execute tasks posted by |detector| referencing it while it's still in
  // scope.
  RunUntilIdle();

  detector.Shutdown();
}

// Tests that the proper notifications are sent for the expected stages as the
// RelaunchNotificationPeriod policy is changed. The period is set to one day,
// such that the thresholds for the annoyance levels are expected to be:
// very low: 1h
// low: 8h
// elevated: 16h
// high: 24h
TEST_F(UpgradeDetectorImplTest, TestPeriodChanges) {
  TestUpgradeDetectorImpl upgrade_detector(GetMockClock(), GetMockTickClock());
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);
  upgrade_detector.Init();

  // Changing the period when no upgrade has been detected updates the
  // thresholds and nothing else.
  SetNotificationPeriodPref(base::TimeDelta::FromDays(1));

  EXPECT_EQ(upgrade_detector.GetThresholdForLevel(
                UpgradeDetector::UPGRADE_ANNOYANCE_HIGH),
            base::TimeDelta::FromDays(1));
  EXPECT_EQ(upgrade_detector.GetThresholdForLevel(
                UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED),
            base::TimeDelta::FromHours(16));
  EXPECT_EQ(upgrade_detector.GetThresholdForLevel(
                UpgradeDetector::UPGRADE_ANNOYANCE_LOW),
            base::TimeDelta::FromHours(8));
  EXPECT_EQ(upgrade_detector.GetThresholdForLevel(
                UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW),
            base::TimeDelta::FromHours(1));
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Back to default.
  SetNotificationPeriodPref(base::TimeDelta());
  EXPECT_EQ(upgrade_detector.GetThresholdForLevel(
                UpgradeDetector::UPGRADE_ANNOYANCE_HIGH),
            base::TimeDelta::FromDays(7));
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Pretend that an upgrade was just detected now.
  upgrade_detector.UpgradeDetected(
      TestUpgradeDetectorImpl::UPGRADE_AVAILABLE_REGULAR);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Fast forward an amount that is still in the "don't annoy me" period at the
  // default period.
  FastForwardBy(base::TimeDelta::FromMinutes(59));
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Drop the period and notice that nothing changes (still below "very low").
  SetNotificationPeriodPref(base::TimeDelta::FromDays(1));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // Fast forward to the "very low" annoyance level.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  FastForwardBy(base::TimeDelta::FromMinutes(1));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);

  // Drop the period; staying within "very low".
  SetNotificationPeriodPref(base::TimeDelta::FromDays(1));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);

  // Bring it back up.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);

  // Fast forward an amount that is still in the "very low" level at the default
  // period.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended()).Times(AnyNumber());
  FastForwardBy(base::TimeDelta::FromHours(7));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);

  // Drop the period so that the current time is in the "low" annoyance level.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta::FromDays(1));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_LOW);

  // Bring it back up.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);

  // Fast forward an amount that is still in the "very low" period at the
  // default period.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended()).Times(AnyNumber());
  FastForwardBy(base::TimeDelta::FromHours(8));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);

  // Drop the period so that the current time is in the "elevated" level.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta::FromDays(1));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);

  // Bring it back up.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);

  // Fast forward an amount that is still in the "very low" level at the default
  // period.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended()).Times(AnyNumber());
  FastForwardBy(base::TimeDelta::FromHours(8));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);

  // Drop the period so that the current time is in the "high" level.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta::FromDays(1));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);

  // Expect no new notifications even if some time passes.
  FastForwardBy(base::TimeDelta::FromHours(1));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);

  // Bring the period back up.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);

  // Fast forward an amount that is still in the "very low" level at the default
  // period.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended()).Times(AnyNumber());
  FastForwardBy(base::TimeDelta::FromHours(12));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);

  // Drop the period so that the current time is deep in the "high" level.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta::FromDays(1));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);

  // Bring it back up.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);

  upgrade_detector.Shutdown();
}

// Appends the time and stage from detector to |notifications|.
ACTION_P2(AppendTicksAndStage, detector, notifications) {
  notifications->emplace_back(detector->clock()->Now(),
                              detector->upgrade_notification_stage());
}

// A value parameterized test fixture for running tests with different
// RelaunchNotificationPeriod settings.
class UpgradeDetectorImplTimerTest : public UpgradeDetectorImplTest,
                                     public ::testing::WithParamInterface<int> {
 protected:
  UpgradeDetectorImplTimerTest() {
    const int period_ms = GetParam();
    if (period_ms)
      SetNotificationPeriodPref(base::TimeDelta::FromMilliseconds(period_ms));
  }

 private:
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::FakeBrowserDMTokenStorage dm_token_storage_;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  DISALLOW_COPY_AND_ASSIGN(UpgradeDetectorImplTimerTest);
};

INSTANTIATE_TEST_SUITE_P(All,
                         UpgradeDetectorImplTimerTest,
                         ::testing::Values(0,           // Default period of 7d.
                                           86400000,    // 1d.
                                           11100000));  // 3:05:00.

// Tests that the notification timer is handled as desired.
TEST_P(UpgradeDetectorImplTimerTest, TestNotificationTimer) {
  using TimeAndStage =
      std::pair<base::Time, UpgradeDetector::UpgradeNotificationAnnoyanceLevel>;
  using Notifications = std::vector<TimeAndStage>;
  static constexpr base::TimeDelta kTwentyMinues =
      base::TimeDelta::FromMinutes(20);

  TestUpgradeDetectorImpl detector(GetMockClock(), GetMockTickClock());
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&detector);
  detector.Init();

  // Cache the thresholds for the detector's annoyance levels.
  const base::TimeDelta thresholds[4] = {
      detector.GetThresholdForLevel(
          UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW),
      detector.GetThresholdForLevel(UpgradeDetector::UPGRADE_ANNOYANCE_LOW),
      detector.GetThresholdForLevel(
          UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED),
      detector.GetThresholdForLevel(UpgradeDetector::UPGRADE_ANNOYANCE_HIGH),
  };

  // Pretend that there's an update.
  detector.UpgradeDetected(TestUpgradeDetectorImpl::UPGRADE_AVAILABLE_REGULAR);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Fast forward to the time that very low annoyance should be reached. One
  // notification should come in at exactly the very low annoyance threshold.
  Notifications notifications;
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .WillOnce(AppendTicksAndStage(&detector, &notifications));
  FastForwardBy(thresholds[0]);
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_THAT(notifications,
              ::testing::ContainerEq(Notifications({TimeAndStage(
                  detector.upgrade_detected_time() + thresholds[0],
                  UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW)})));

  // Move to the time that low annoyance should be reached. Notifications at
  // very low annoyance should arrive every 20 minutes with one final
  // notification at elevated annoyance.
  notifications.clear();
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .WillRepeatedly(AppendTicksAndStage(&detector, &notifications));
  FastForwardBy(thresholds[1] - thresholds[0]);
  ::testing::Mock::VerifyAndClear(&mock_observer);
  if (thresholds[1] - thresholds[0] >= kTwentyMinues) {
    ASSERT_GT(notifications.size(), 1U);
    EXPECT_EQ((notifications.end() - 2)->second,
              UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);
  } else {
    EXPECT_EQ(notifications.size(), 1U);
  }
  EXPECT_EQ(notifications.back(),
            TimeAndStage(detector.upgrade_detected_time() + thresholds[1],
                         UpgradeDetector::UPGRADE_ANNOYANCE_LOW));

  // Move to the time that elevated annoyance should be reached. Notifications
  // at low annoyance should arrive every 20 minutes with one final notification
  // at elevated annoyance.
  notifications.clear();
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .WillRepeatedly(AppendTicksAndStage(&detector, &notifications));
  FastForwardBy(thresholds[2] - thresholds[1]);
  ::testing::Mock::VerifyAndClear(&mock_observer);
  ASSERT_GT(notifications.size(), 1U);
  EXPECT_EQ((notifications.end() - 2)->second,
            UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  EXPECT_EQ(notifications.back(),
            TimeAndStage(detector.upgrade_detected_time() + thresholds[2],
                         UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED));

  // Move to the time that high annoyance should be reached. Notifications at
  // elevated annoyance should arrive every 20 minutes with one final
  // notification at high annoyance.
  notifications.clear();
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .WillRepeatedly(AppendTicksAndStage(&detector, &notifications));
  FastForwardBy(thresholds[3] - thresholds[2]);
  ::testing::Mock::VerifyAndClear(&mock_observer);
  ASSERT_GT(notifications.size(), 1U);
  EXPECT_EQ((notifications.end() - 2)->second,
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  EXPECT_EQ(notifications.back(),
            TimeAndStage(detector.upgrade_detected_time() + thresholds[3],
                         UpgradeDetector::UPGRADE_ANNOYANCE_HIGH));

  // No new notifications after high annoyance has been reached.
  FastForwardBy(thresholds[3]);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  detector.Shutdown();
}
