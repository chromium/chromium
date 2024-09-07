// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/public/cpp/update_types.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/power_monitor_test.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/test/action_logger.h"
#include "ui/display/manager/test/test_native_display_delegate.h"
#else
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using ::testing::_;
using ::testing::Eq;
using ::testing::ResultOf;
using ::testing::Return;

namespace {

// A delegate interface for handling the actions taken by the controller.
class ControllerDelegate {
 public:
  virtual ~ControllerDelegate() = default;
  virtual void NotifyRelaunchRecommended() = 0;
  virtual void NotifyRelaunchRequired() = 0;
  virtual void Close() = 0;
  virtual void OnRelaunchDeadlineExpired() = 0;

 protected:
  ControllerDelegate() = default;
};

// A fake controller that asks a delegate to do work.
class FakeRelaunchNotificationController
    : public RelaunchNotificationController {
 public:
  FakeRelaunchNotificationController(UpgradeDetector* upgrade_detector,
                                     const base::Clock* clock,
                                     const base::TickClock* tick_clock,
                                     ControllerDelegate* delegate)
      : RelaunchNotificationController(upgrade_detector, clock, tick_clock),
        delegate_(delegate) {}

  FakeRelaunchNotificationController(
      const FakeRelaunchNotificationController&) = delete;
  FakeRelaunchNotificationController& operator=(
      const FakeRelaunchNotificationController&) = delete;

  using RelaunchNotificationController::kRelaunchGracePeriod;

  base::Time IncreaseRelaunchDeadlineOnShow() {
    return RelaunchNotificationController::IncreaseRelaunchDeadlineOnShow();
  }

 private:
  void DoNotifyRelaunchRecommended(bool /*past_deadline*/) override {
    delegate_->NotifyRelaunchRecommended();
  }

  void DoNotifyRelaunchRequired(
      base::Time deadline,
      base::OnceCallback<base::Time()> on_visible) override {
    delegate_->NotifyRelaunchRequired();
  }

  void Close() override { delegate_->Close(); }

  void OnRelaunchDeadlineExpired() override {
    delegate_->OnRelaunchDeadlineExpired();
  }

  raw_ptr<ControllerDelegate> delegate_;
};

// A mock delegate for testing.
class MockControllerDelegate : public ControllerDelegate {
 public:
  MOCK_METHOD(void, NotifyRelaunchRecommended, (), (override));
  MOCK_METHOD(void, NotifyRelaunchRequired, (), (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(void, OnRelaunchDeadlineExpired, (), (override));
};

// A fake UpgradeDetector.
class FakeUpgradeDetector : public UpgradeDetector {
 public:
  explicit FakeUpgradeDetector(const base::Clock* clock,
                               const base::TickClock* tick_clock)
      : UpgradeDetector(clock, tick_clock) {
    set_upgrade_detected_time(this->clock()->Now());
  }

  FakeUpgradeDetector(const FakeUpgradeDetector&) = delete;
  FakeUpgradeDetector& operator=(const FakeUpgradeDetector&) = delete;

  base::TimeDelta GetHighAnnoyanceLevelDelta() {
    return GetAnnoyanceLevelDeadline(UpgradeDetector::UPGRADE_ANNOYANCE_HIGH) -
           GetAnnoyanceLevelDeadline(
               UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  }

  // UpgradeDetector:
  base::Time GetAnnoyanceLevelDeadline(
      FakeUpgradeDetector::UpgradeNotificationAnnoyanceLevel level) override {
    switch (level) {
      case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
        return upgrade_detected_time() + (2 / 3 * high_threshold_);
      case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
        return upgrade_detected_time() + high_threshold_;
      case UpgradeDetector::UPGRADE_ANNOYANCE_GRACE:
      case UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW:
      case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
      case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
      case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
        return base::Time();
    }
  }

  // Sets the annoyance level to |level| and broadcasts the change to all
  // observers.
  void BroadcastLevelChange(UpgradeNotificationAnnoyanceLevel level) {
    set_upgrade_notification_stage(level);
    NotifyUpgrade();
  }

  // Sets the high annoyance threshold to |high_threshold| and broadcasts the
  // change to all observers.
  void BroadcastHighThresholdChange(base::TimeDelta high_threshold) {
    high_threshold_ = high_threshold;
    NotifyUpgrade();
  }

  void BroadcastNotificationTypeOverriden(bool overridden) {
    NotifyRelaunchOverriddenToRequired(overridden);
  }

  base::TimeDelta high_threshold() const { return high_threshold_; }

 private:
  base::TimeDelta high_threshold_ = base::Days(7);
};

}  // namespace

// A test harness that provides facilities for manipulating the relaunch
// notification policy setting and for broadcasting upgrade notifications.
class RelaunchNotificationControllerTest : public ::testing::Test {
 public:
  RelaunchNotificationControllerTest(
      const RelaunchNotificationControllerTest&) = delete;
  RelaunchNotificationControllerTest& operator=(
      const RelaunchNotificationControllerTest&) = delete;

 protected:
  RelaunchNotificationControllerTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        scoped_local_state_(TestingBrowserProcess::GetGlobal()),
        upgrade_detector_(task_environment_.GetMockClock(),
                          task_environment_.GetMockTickClock()) {
    // Unittests failed when the system is on battery. This class is using a
    // mock power monitor source `power_monitor_source_` to ensure no real
    // power state or power notifications are delivered to the unittests.
    EXPECT_FALSE(base::PowerMonitor::GetInstance()->IsOnBatteryPower());
  }

  UpgradeDetector* upgrade_detector() { return &upgrade_detector_; }
  FakeUpgradeDetector& fake_upgrade_detector() { return upgrade_detector_; }

  // Sets the browser.relaunch_notification preference in Local State to
  // |value|.
  void SetNotificationPref(int value) {
    scoped_local_state_.Get()->SetManagedPref(
        prefs::kRelaunchNotification, std::make_unique<base::Value>(value));
  }

  // Returns the TaskEnvironment's MockClock.
  const base::Clock* GetMockClock() { return task_environment_.GetMockClock(); }

  // Returns the TaskEnvironment's MockTickClock.
  const base::TickClock* GetMockTickClock() {
    return task_environment_.GetMockTickClock();
  }

  // Fast-forwards virtual time by |delta|.
  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  // Use a mock power monitor source to ensure the test is in control of the
  // power notifications.
  base::test::ScopedPowerMonitorTestSource power_monitor_source_;

  base::test::TaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_local_state_;
  FakeUpgradeDetector upgrade_detector_;
};

TEST_F(RelaunchNotificationControllerTest, CreateDestroy) {
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);
}

// Without the browser.relaunch_notification preference set, the controller
// should not be observing the UpgradeDetector, and should therefore never
// attempt to show any notifications.

// TODO(crbug.com/40099078) Disabled due to race condition.
#if defined(THREAD_SANATIZER)
#define MAYBE_PolicyUnset DISABLED_PolicyUnset
#else
#define MAYBE_PolicyUnset PolicyUnset
#endif
TEST_F(RelaunchNotificationControllerTest, MAYBE_PolicyUnset) {
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_GRACE);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
}

// With the browser.relaunch_notification preference set to 1, the controller
// should be observing the UpgradeDetector and should show "Requested"
// notifications on each level change above "very low".
TEST_F(RelaunchNotificationControllerTest, RecommendedByPolicy) {
  SetNotificationPref(1);
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  // Nothing shown if the level is broadcast at NONE or VERY_LOW.
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Show for each level change, but not for repeat notifications.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRecommended());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRecommended());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRecommended());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_GRACE);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_GRACE);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // First move time to the high annoyance deadline.
  base::Time high_annoyance_deadline =
      upgrade_detector()->GetAnnoyanceLevelDeadline(
          UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  FastForwardBy(high_annoyance_deadline - GetMockClock()->Now());

  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRecommended());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // The timer should be running to reshow at the detector's delta.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRecommended());
  FastForwardBy(fake_upgrade_detector().GetHighAnnoyanceLevelDelta());
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRecommended());
  FastForwardBy(fake_upgrade_detector().GetHighAnnoyanceLevelDelta());
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Drop back to elevated to stop the reshows and ensure there are none.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRecommended());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  FastForwardBy(fake_upgrade_detector().GetHighAnnoyanceLevelDelta());
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // And closed if the level drops back to very low.
  EXPECT_CALL(mock_controller_delegate, Close());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Back up to elevated brings the bubble back.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRecommended());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // And it is closed if the level drops back to none.
  EXPECT_CALL(mock_controller_delegate, Close());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

// With the browser.relaunch_notification preference set to 2, the controller
// should be observing the UpgradeDetector and should show "Required"
// notifications on each level change.
TEST_F(RelaunchNotificationControllerTest, RequiredByPolicy) {
  SetNotificationPref(2);
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  // Nothing shown if the level is broadcast at NONE.
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Show for each change to a higher level, but not for repeat notifications.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_GRACE);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_GRACE);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // And closed if the level drops back to none.
  EXPECT_CALL(mock_controller_delegate, Close());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

// Flipping the policy should have no effect when at level NONE or VERY_LOW.
TEST_F(RelaunchNotificationControllerTest, PolicyChangesNoUpgrade) {
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  SetNotificationPref(1);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  SetNotificationPref(2);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  SetNotificationPref(3);  // Bogus value!
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  SetNotificationPref(0);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  SetNotificationPref(1);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  SetNotificationPref(2);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  SetNotificationPref(3);  // Bogus value!
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  SetNotificationPref(0);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

// Policy changes at an elevated level should show the appropriate notification.
TEST_F(RelaunchNotificationControllerTest, PolicyChangesWithUpgrade) {
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRecommended());
  SetNotificationPref(1);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, Close());
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  SetNotificationPref(2);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, Close());
  SetNotificationPref(0);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

// Relaunch is forced when the deadline is reached.
TEST_F(RelaunchNotificationControllerTest, RequiredDeadlineReached) {
  SetNotificationPref(2);
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  // As in the RequiredByPolicy test, the dialog should be shown.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());

  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // And the relaunch should be forced after the deadline passes.
  EXPECT_CALL(mock_controller_delegate, OnRelaunchDeadlineExpired());
  FastForwardBy(fake_upgrade_detector().high_threshold());
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

// No forced relaunch if the dialog is closed.
TEST_F(RelaunchNotificationControllerTest, RequiredDeadlineReachedNoPolicy) {
  SetNotificationPref(2);
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  // As in the RequiredByPolicy test, the dialog should be shown.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());

  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // And then closed if the policy is cleared.
  EXPECT_CALL(mock_controller_delegate, Close());
  SetNotificationPref(0);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // And no relaunch should take place.
  FastForwardBy(fake_upgrade_detector().high_threshold());
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

// NotificationPeriod changes should do nothing at any policy setting when the
// annoyance level is at none.
TEST_F(RelaunchNotificationControllerTest, NonePeriodChange) {
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  // Reduce the period.
  fake_upgrade_detector().BroadcastHighThresholdChange(base::Days(1));
  FastForwardBy(fake_upgrade_detector().high_threshold());
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  SetNotificationPref(1);
  fake_upgrade_detector().BroadcastHighThresholdChange(base::Hours(23));
  FastForwardBy(fake_upgrade_detector().high_threshold());
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  SetNotificationPref(2);
  fake_upgrade_detector().BroadcastHighThresholdChange(base::Hours(22));
  FastForwardBy(fake_upgrade_detector().high_threshold());
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

// NotificationPeriod changes should do nothing at any policy setting when the
// annoyance level is at very low.
TEST_F(RelaunchNotificationControllerTest, VeryLowPeriodChange) {
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Reduce the period.
  fake_upgrade_detector().BroadcastHighThresholdChange(base::Days(1));
  FastForwardBy(fake_upgrade_detector().high_threshold());
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  SetNotificationPref(1);
  fake_upgrade_detector().BroadcastHighThresholdChange(base::Hours(23));
  FastForwardBy(fake_upgrade_detector().high_threshold());
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  SetNotificationPref(2);
  fake_upgrade_detector().BroadcastHighThresholdChange(base::Hours(22));
  FastForwardBy(fake_upgrade_detector().high_threshold());
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

// NotificationPeriod changes impact reshows of the relaunch recommended bubble.
TEST_F(RelaunchNotificationControllerTest, PeriodChangeRecommended) {
  SetNotificationPref(1);
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  // First move time to the high annoyance deadline.
  base::Time high_annoyance_deadline =
      upgrade_detector()->GetAnnoyanceLevelDeadline(
          UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  FastForwardBy(high_annoyance_deadline - GetMockClock()->Now());

  // Get up to high annoyance so that the reshow timer is running.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRecommended());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Advance time partway to the reshow, but not all the way there.
  FastForwardBy(fake_upgrade_detector().GetHighAnnoyanceLevelDelta() * 0.9);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Now shorten the period dramatically and expect an immediate reshow.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRecommended());
  fake_upgrade_detector().BroadcastHighThresholdChange(
      fake_upgrade_detector().high_threshold() / 10);
  RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // And expect another reshow at the new delta.
  base::TimeDelta short_reshow_delta =
      fake_upgrade_detector().GetHighAnnoyanceLevelDelta();
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRecommended());
  FastForwardBy(short_reshow_delta);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Now lengthen the period and expect no immediate reshow.
  fake_upgrade_detector().BroadcastHighThresholdChange(
      fake_upgrade_detector().high_threshold() * 10);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Move forward by the short delta to be sure there's no reshow there.
  FastForwardBy(short_reshow_delta);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Move forward the rest of the way to the new delta and expect a reshow.
  base::TimeDelta long_reshow_delta =
      fake_upgrade_detector().GetHighAnnoyanceLevelDelta();
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRecommended());
  FastForwardBy(long_reshow_delta - short_reshow_delta);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Similar to the above, move time forward a little bit.
  FastForwardBy(long_reshow_delta * 0.1);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Shorten the period a bit, but not enough to force a reshow.
  fake_upgrade_detector().BroadcastHighThresholdChange(
      fake_upgrade_detector().high_threshold() * 0.9);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // And ensure that moving forward the rest of the way to the new delta causes
  // a reshow.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRecommended());
  FastForwardBy(fake_upgrade_detector().GetHighAnnoyanceLevelDelta() -
                long_reshow_delta * 0.1);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

// NotificationPeriod changes impact reshows of the relaunch required dialog.
TEST_F(RelaunchNotificationControllerTest, PeriodChangeRequired) {
  SetNotificationPref(2);
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());

  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Move forward partway to the current deadline. Nothing should happen.
  base::Time high_annoyance_deadline =
      upgrade_detector()->GetAnnoyanceLevelDeadline(
          UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  FastForwardBy((high_annoyance_deadline - GetMockClock()->Now()) / 2);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Lengthen the period, thereby pushing out the deadline.
  fake_upgrade_detector().BroadcastHighThresholdChange(
      fake_upgrade_detector().high_threshold() * 2);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Ensure that nothing happens when the old deadline passes.
  FastForwardBy(high_annoyance_deadline - GetMockClock()->Now());
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Now we enter elevated annoyance level and show the dialog.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Now we enter grace annoyance level and again show the dialog.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_GRACE);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Jumping to the new deadline relaunches the browser.
  EXPECT_CALL(mock_controller_delegate, OnRelaunchDeadlineExpired());
  FastForwardBy(upgrade_detector()->GetAnnoyanceLevelDeadline(
                    UpgradeDetector::UPGRADE_ANNOYANCE_HIGH) -
                GetMockClock()->Now());
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Shorten the period, bringing in the deadline. Expect the dialog to show and
  // a relaunch after the grace period passes.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastHighThresholdChange(
      fake_upgrade_detector().high_threshold() / 2);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  EXPECT_CALL(mock_controller_delegate, OnRelaunchDeadlineExpired());
  FastForwardBy(FakeRelaunchNotificationController::kRelaunchGracePeriod);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

// Test that grace period is given to the user to relaunch if the deadline is
// shortened to be in the past due to change in notification period.
TEST_F(RelaunchNotificationControllerTest, DeadlineShortenGracePeriod) {
  SetNotificationPref(2);
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Move forward partway to the current deadline. Nothing should happen.
  base::Time high_annoyance_deadline =
      upgrade_detector()->GetAnnoyanceLevelDeadline(
          UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  FastForwardBy((high_annoyance_deadline - GetMockClock()->Now()) / 2);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Shorten the period, thereby pushing the deadline in the past. Expect the
  // dialog to show and a relaunch after the grace period passes.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastHighThresholdChange(
      fake_upgrade_detector().high_threshold() / 3);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  EXPECT_CALL(mock_controller_delegate, OnRelaunchDeadlineExpired());
  FastForwardBy(FakeRelaunchNotificationController::kRelaunchGracePeriod);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

// Test that grace period is given to the user to relaunch if the device goes to
// sleep beyond the deadline before showing the notification.
TEST_F(RelaunchNotificationControllerTest, DeviceSleepBeforeNotification) {
  SetNotificationPref(2);
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Assume device goes to sleep beyond the deadline.
  base::Time high_annoyance_deadline =
      upgrade_detector()->GetAnnoyanceLevelDeadline(
          UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  FastForwardBy((high_annoyance_deadline - GetMockClock()->Now()) * 1.2);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // As device awakes high annoyance is notified. Expect the
  // dialog to show and a relaunch after the grace period passes.
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  EXPECT_CALL(mock_controller_delegate, OnRelaunchDeadlineExpired());
  FastForwardBy(FakeRelaunchNotificationController::kRelaunchGracePeriod);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

// Test that the deadline is extended by the grace period when the
// notification is potentially seen.
TEST_F(RelaunchNotificationControllerTest, DeferredRequired) {
  SetNotificationPref(2);
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());

  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Move time just before the original deadline.
  FastForwardBy(fake_upgrade_detector().high_threshold() -
                0.5 * FakeRelaunchNotificationController::kRelaunchGracePeriod);

  // Suddenly, the UX becomes available.
  base::Time deadline = controller.IncreaseRelaunchDeadlineOnShow();
  ASSERT_EQ(deadline,
            GetMockClock()->Now() +
                FakeRelaunchNotificationController::kRelaunchGracePeriod);

  // And the relaunch is extended by the grace period.
  EXPECT_CALL(mock_controller_delegate, OnRelaunchDeadlineExpired());
  FastForwardBy(FakeRelaunchNotificationController::kRelaunchGracePeriod);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

// Call to override the current relaunch notification type should override it to
// required and policy change should not affect it.
TEST_F(RelaunchNotificationControllerTest, OverriddenToRequired) {
  SetNotificationPref(1);
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  fake_upgrade_detector().BroadcastNotificationTypeOverriden(true);

  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  SetNotificationPref(0);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, Close());
  fake_upgrade_detector().BroadcastNotificationTypeOverriden(false);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

// Tests that the required notification is shown all three times when the clock
// moves along with the elevations.
TEST_F(RelaunchNotificationControllerTest, NotifyAllWithShortestPeriod) {
  SetNotificationPref(2);

  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;
  FakeRelaunchNotificationController controller(
      upgrade_detector(), GetMockClock(), GetMockTickClock(),
      &mock_controller_delegate);

  // Advance to the low threshold and raise the annoyance level.
  const auto delta = fake_upgrade_detector().high_threshold() / 3;
  FastForwardBy(delta);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Advance to the elevated threshold and raise the annoyance level.
  FastForwardBy(fake_upgrade_detector().high_threshold() - delta * 2);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Advance to the grace threshold and raise the annoyance level.
  FastForwardBy(delta - base::Hours(1));
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_GRACE);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, NotifyRelaunchRequired());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);

  // Advance to the deadline to restart.
  EXPECT_CALL(mock_controller_delegate, OnRelaunchDeadlineExpired());
  FastForwardBy(base::Hours(1));
  ASSERT_EQ(GetMockClock()->Now(),
            upgrade_detector()->GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_HIGH));
  ::testing::Mock::VerifyAndClearExpectations(&mock_controller_delegate);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

class RelaunchNotificationControllerPlatformImplTest : public ::testing::Test {
 protected:
  RelaunchNotificationControllerPlatformImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~RelaunchNotificationControllerPlatformImplTest() override = default;

  void SetUp() override {
    ash::AshTestHelper::InitParams init_params;
    init_params.start_session = false;
    ash_test_helper_.SetUp(std::move(init_params));

    user_manager_ = new ash::FakeChromeUserManager();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(user_manager_.get()));

    const char test_user_email[] = "test_user@example.com";
    const AccountId test_account_id(AccountId::FromUserEmail(test_user_email));
    user_manager_->AddUser(test_account_id);
    user_manager_->LoginUser(test_account_id);

    // SessionManager is created by
    // |AshTestHelper::bluetooth_config_test_helper()|.
    session_manager::SessionManager::Get()->CreateSession(
        test_account_id, test_user_email, false);
    session_manager::SessionManager::Get()->SetSessionState(
        session_manager::SessionState::ACTIVE);

    logger_ = std::make_unique<display::test::ActionLogger>();
    native_display_delegate_ =
        new display::test::TestNativeDisplayDelegate(logger_.get());
    ash::Shell::Get()->display_configurator()->SetDelegateForTesting(
        std::unique_ptr<display::NativeDisplayDelegate>(
            native_display_delegate_));
  }

  void LockScreen() {
    session_manager::SessionManager::Get()->SetSessionState(
        session_manager::SessionState::LOCKED);
  }

  void UnLockScreen() {
    session_manager::SessionManager::Get()->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

  void TurnDisplayOff() {
    ash::Shell::Get()->display_configurator()->SetDisplayPower(
        chromeos::DISPLAY_POWER_ALL_OFF, 0, base::DoNothing());
  }

  void TurnDisplayOn() {
    ash::Shell::Get()->display_configurator()->SetDisplayPower(
        chromeos::DISPLAY_POWER_ALL_ON, 0, base::DoNothing());
  }

  RelaunchNotificationControllerPlatformImpl& platform_impl() { return impl_; }

  // Returns the TaskEnvironment's MockClock.
  const base::Clock* GetMockClock() { return task_environment_.GetMockClock(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  RelaunchNotificationControllerPlatformImpl impl_;
  ash::AshTestHelper ash_test_helper_;
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<display::test::ActionLogger> logger_;
  raw_ptr<display::NativeDisplayDelegate> native_display_delegate_;
};

// SynchronousNotification
TEST_F(RelaunchNotificationControllerPlatformImplTest,
       SynchronousNotification) {
  UnLockScreen();
  TurnDisplayOn();

  // Expect the platform_impl to query for the deadline synchronously.
  ::testing::StrictMock<base::MockOnceCallback<base::Time()>> callback;
  platform_impl().NotifyRelaunchRequired(
      GetMockClock()->Now(), /*is_notification_type_overriden=*/false,
      callback.Get());
  ::testing::Mock::VerifyAndClearExpectations(&callback);
}

// Deferred Notification when the display is off then on.
TEST_F(RelaunchNotificationControllerPlatformImplTest,
       DeferredNotificationDisplayOff) {
  TurnDisplayOff();

  ::testing::StrictMock<base::MockOnceCallback<base::Time()>> callback;

  platform_impl().NotifyRelaunchRequired(
      GetMockClock()->Now(), /*is_notification_type_overriden=*/false,
      callback.Get());
  ::testing::Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(callback, Run());
  TurnDisplayOn();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
}

// Deferred Notification when the display is off then on.
TEST_F(RelaunchNotificationControllerPlatformImplTest,
       DeferredNotificationSessionLocked) {
  LockScreen();

  ::testing::StrictMock<base::MockOnceCallback<base::Time()>> callback;

  platform_impl().NotifyRelaunchRequired(
      GetMockClock()->Now(), /*is_notification_type_overriden=*/false,
      callback.Get());
  ::testing::Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(callback, Run());
  UnLockScreen();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
}

// Multiple screen on & off.
TEST_F(RelaunchNotificationControllerPlatformImplTest,
       RequiredDeadlineReachedAfterMultipleResume) {
  TurnDisplayOff();

  ::testing::StrictMock<base::MockOnceCallback<base::Time()>> callback;

  platform_impl().NotifyRelaunchRequired(
      GetMockClock()->Now(), /*is_notification_type_overriden=*/false,
      callback.Get());
  ::testing::Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(callback, Run());
  TurnDisplayOn();

  TurnDisplayOff();
  TurnDisplayOn();
  TurnDisplayOff();
  TurnDisplayOn();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
}

// Multiple session locks & unlocks.
TEST_F(RelaunchNotificationControllerPlatformImplTest,
       RequiredDeadlineReachedBeforeMultipleUnlock) {
  LockScreen();

  ::testing::StrictMock<base::MockOnceCallback<base::Time()>> callback;

  platform_impl().NotifyRelaunchRequired(
      GetMockClock()->Now(), /*is_notification_type_overriden=*/false,
      callback.Get());
  ::testing::Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(callback, Run());
  UnLockScreen();

  LockScreen();
  UnLockScreen();
  LockScreen();
  UnLockScreen();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
}

class MockSystemTrayClientImpl : public SystemTrayClientImpl {
 public:
  MockSystemTrayClientImpl() : SystemTrayClientImpl(this) {}
  MOCK_METHOD(void,
              SetRelaunchNotificationState,
              (const ash::RelaunchNotificationState&),
              (override));
};

// Correct relaunch notification state for required notification type.
TEST_F(RelaunchNotificationControllerPlatformImplTest,
       RelaunchNotificationStateRequired) {
  base::MockOnceCallback<base::Time()> callback;
  MockSystemTrayClientImpl system_tray_client_impl;
  ash::RelaunchNotificationState relaunch_notification_state;
  EXPECT_CALL(system_tray_client_impl, SetRelaunchNotificationState(_))
      .WillOnce(testing::SaveArg<0>(&relaunch_notification_state));

  platform_impl().NotifyRelaunchRequired(
      GetMockClock()->Now(), /*is_notification_type_overriden=*/false,
      callback.Get());

  EXPECT_EQ(relaunch_notification_state.requirement_type,
            ash::RelaunchNotificationState::kRequired);
  EXPECT_EQ(relaunch_notification_state.policy_source,
            ash::RelaunchNotificationState::kUser);
  EXPECT_LE(relaunch_notification_state.rounded_time_until_reboot_required,
            base::Seconds(1));
}

// Correct relaunch notification state for required notification type with an
// override.
TEST_F(RelaunchNotificationControllerPlatformImplTest,
       RelaunchNotificationStateRequiredWithOverride) {
  base::MockOnceCallback<base::Time()> callback;
  MockSystemTrayClientImpl system_tray_client_impl;
  ash::RelaunchNotificationState relaunch_notification_state;
  EXPECT_CALL(system_tray_client_impl, SetRelaunchNotificationState(_))
      .WillOnce(testing::SaveArg<0>(&relaunch_notification_state));

  platform_impl().NotifyRelaunchRequired(
      GetMockClock()->Now(), /*is_notification_type_overriden=*/true,
      callback.Get());

  EXPECT_EQ(relaunch_notification_state.requirement_type,
            ash::RelaunchNotificationState::kRequired);
  EXPECT_EQ(relaunch_notification_state.policy_source,
            ash::RelaunchNotificationState::kDevice);
  EXPECT_LE(relaunch_notification_state.rounded_time_until_reboot_required,
            base::Seconds(1));
}

#else  // BUILDFLAG(IS_CHROMEOS_ASH)

class RelaunchNotificationControllerPlatformImplTest
    : public TestWithBrowserView {
 protected:
  RelaunchNotificationControllerPlatformImplTest() : TestWithBrowserView() {}

  void SetUp() override {
    TestWithBrowserView::SetUp();
    impl_.emplace();
  }

  void SetVisibility(bool is_visible) {
    if (is_visible)
      browser_view()->Show();
    else
      browser_view()->Hide();

    // Allow UI tasks to run so that the browser becomes fully active/inactive.
    task_environment()->RunUntilIdle();
  }

  RelaunchNotificationControllerPlatformImpl& platform_impl() { return *impl_; }

 private:
  std::optional<RelaunchNotificationControllerPlatformImpl> impl_;
};

// Flaky on all platforms: https://crbug.com/1294032
TEST_F(RelaunchNotificationControllerPlatformImplTest,
       DISABLED_SynchronousNotification) {
  // Make the UX visible to the user so that no delay will be incurred
  ASSERT_NO_FATAL_FAILURE(SetVisibility(true));

  // Expect the platform_impl to show the notification synchronously.
  ::testing::StrictMock<base::MockOnceCallback<base::Time()>> callback;

  base::Time deadline = base::Time::Now() + base::Hours(1);

  // There should be no query at the time of showing.
  platform_impl().NotifyRelaunchRequired(deadline, callback.Get());
  ::testing::Mock::VerifyAndClearExpectations(&callback);

  ASSERT_NO_FATAL_FAILURE(SetVisibility(false));

  // There should be no query because the browser isn't visible.
  platform_impl().NotifyRelaunchRequired(deadline, callback.Get());
  ::testing::Mock::VerifyAndClearExpectations(&callback);

  // There should be no query because this isn't the first time to show the
  // notification.
  ASSERT_NO_FATAL_FAILURE(SetVisibility(true));
  ::testing::Mock::VerifyAndClearExpectations(&callback);
}

//  Flaky on Mac: https://crbug.com/1312578
#if BUILDFLAG(IS_MAC)
#define MAYBE_DeferredDeadline DISABLED_DeferredDeadline
#else
#define MAYBE_DeferredDeadline DeferredDeadline
#endif
TEST_F(RelaunchNotificationControllerPlatformImplTest, MAYBE_DeferredDeadline) {
  ::testing::StrictMock<base::MockOnceCallback<base::Time()>> callback;

  base::Time deadline = base::Time::Now() + base::Hours(1);

  // There should be no query because the browser isn't visible.
  platform_impl().NotifyRelaunchRequired(deadline, callback.Get());
  ::testing::Mock::VerifyAndClearExpectations(&callback);

  // The query should happen once the notification is potentially seen.
  EXPECT_CALL(callback, Run()).WillOnce(Return(deadline));
  ASSERT_NO_FATAL_FAILURE(SetVisibility(true));
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // We should not depend on BrowserView::Show() (called by SetVisibility(true))
  // to call BrowserList::SetLastActive() to set the associated browser to be
  // last active one. We are planning to remove that call for Lacros to make
  // updating of last active browser completely asynchronous (b/325634285).
  // Therefore, for the unit test depending on browser() to be set as the last
  // active one, we need to explicit set it as the last active one.
  BrowserList::GetInstance()->SetLastActive(browser());
#endif
  ::testing::Mock::VerifyAndClearExpectations(&callback);

  ASSERT_NO_FATAL_FAILURE(SetVisibility(false));

  // There should be no query because the browser isn't visible.
  platform_impl().NotifyRelaunchRequired(deadline, callback.Get());
  ::testing::Mock::VerifyAndClearExpectations(&callback);

  // There should be no query because this isn't the first time to show the
  // notification.
  ASSERT_NO_FATAL_FAILURE(SetVisibility(true));
  ::testing::Mock::VerifyAndClearExpectations(&callback);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
