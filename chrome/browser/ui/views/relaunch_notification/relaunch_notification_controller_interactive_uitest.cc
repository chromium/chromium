// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_dialog_view.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "chrome/common/chrome_version.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

// Waits for the UpgradeDetector to broadcast to its observers that an upgrade
// has been detected.
class UpgradeRecommendedWaiter : public UpgradeObserver {
 public:
  UpgradeRecommendedWaiter() {
    UpgradeDetector::GetInstance()->AddObserver(this);
  }
  ~UpgradeRecommendedWaiter() override {
    UpgradeDetector::GetInstance()->RemoveObserver(this);
  }
  void Wait() { run_loop_.Run(); }

  // UpgradeObserver:
  void OnUpgradeRecommended() override { run_loop_.QuitWhenIdle(); }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

class RelaunchNotificationControllerUiTest : public policy::PolicyTest {
 protected:
  // policy::PolicyTest:
  void SetUpOnMainThread() override {
    policy::PolicyTest::SetUpOnMainThread();

    // Configure required relaunch notifications.
    SetRelaunchNotificationPolicies();
  }

  void TearDownOnMainThread() override {
    // Disable notifications so that the timer doesn't fire during teardown.
    DisableRelaunchNotifications();

    policy::PolicyTest::TearDownOnMainThread();
  }

  // Sets the RelaunchNotification policies to show the required notification
  // at the default deadline.
  void SetRelaunchNotificationPolicies() {
    policies_.Set(policy::key::kRelaunchNotification,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                  policy::POLICY_SOURCE_PLATFORM,
                  base::Value(2),  // Required
                  nullptr);
    policies_.Set(policy::key::kRelaunchNotificationPeriod,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                  policy::POLICY_SOURCE_PLATFORM,
                  base::Value(base::saturated_cast<int>(
                      UpgradeDetector::GetDefaultHighAnnoyanceThreshold()
                          .InMilliseconds())),
                  nullptr);
    UpdateProviderPolicy(policies_);
  }

  // Disables relaunch notifications.
  void DisableRelaunchNotifications() {
    policies_.Set(policy::key::kRelaunchNotification,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                  policy::POLICY_SOURCE_PLATFORM,
                  base::Value(0),  // Disabled
                  nullptr);
    UpdateProviderPolicy(policies_);
  }

  // Simulates that an update is available.
  static void SimulateUpdate() {
    g_browser_process->GetBuildState()->SetUpdate(
        BuildState::UpdateType::kNormalUpdate,
        base::Version({CHROME_VERSION_MAJOR, CHROME_VERSION_MINOR,
                       CHROME_VERSION_BUILD, CHROME_VERSION_PATCH + 1}),
        std::nullopt);
  }

  // Triggers the annoyance level `level` to be announced to observers of
  // UpgradeDetector.
  void TriggerAnnoyanceLevel(
      UpgradeDetector::UpgradeNotificationAnnoyanceLevel level) {
    // Move the time at which the update was detected so that the original
    // detection time is, relative to the new time, at the moment that `level`
    // takes effect.
    auto* const upgrade_detector = UpgradeDetector::GetInstance();
    upgrade_detector->set_upgrade_detected_time(
        upgrade_detector->upgrade_detected_time() -
        (upgrade_detector->GetAnnoyanceLevelDeadline(level) -
         upgrade_detector->upgrade_detected_time()));

    // Force the UpgradeDetector to recompute the deadlines by reducing the
    // period by 1ms. This will also be broadcast to observers.
    policies_.Set(policy::key::kRelaunchNotificationPeriod,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                  policy::POLICY_SOURCE_PLATFORM,
                  base::Value(base::saturated_cast<int>(
                      UpgradeDetector::GetDefaultHighAnnoyanceThreshold()
                          .InMilliseconds() -
                      1)),
                  nullptr);
    UpdateProviderPolicy(policies_);
  }

  // Closes `dialog` and waits for it to be destroyed.
  static void CloseDialog(RelaunchRequiredDialogView* dialog) {
    views::test::WidgetDestroyedWaiter waiter(dialog->GetWidget());
    std::exchange(dialog, nullptr)->CancelDialog();
    waiter.Wait();
  }

  // Minimizes `browser_view` and waits for it to be deactivated.
  static void MinimizeBrowser(BrowserView* browser_view) {
    browser_view->Minimize();
    views::test::WaitForWidgetActive(browser_view->GetWidget(),
                                     /*active=*/false);
    // Pump all pending UI events so that the window manager isn't racing with
    // the test.
    base::RunLoop().RunUntilIdle();
  }

  // Sets the RelaunchNotificationPeriod so that the deadline is `delta` in
  // the future. Returns the new deadline.
  base::Time AdjustPeriodToDeadlineIn(base::TimeDelta delta) {
    UpgradeRecommendedWaiter waiter;
    const base::Time deadline = base::Time::Now() + delta;
    const base::TimeDelta period =
        deadline - UpgradeDetector::GetInstance()->upgrade_detected_time();
    policies_.Set(
        policy::key::kRelaunchNotificationPeriod,
        policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
        policy::POLICY_SOURCE_PLATFORM,
        base::Value(base::saturated_cast<int>(period.InMilliseconds())),
        nullptr);
    UpdateProviderPolicy(policies_);
    waiter.Wait();
    return deadline;
  }

 private:
  policy::PolicyMap policies_;
};

// Tests that reactivating a browser window after the deadline has passed does
// not show a negative delta.
// Fails on mac64; see https://crbug.com/1462892.
IN_PROC_BROWSER_TEST_F(RelaunchNotificationControllerUiTest,
                       ReactivateAfterDeadline) {
  // Make sure a browser window is active.
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view->IsActive());
  ASSERT_EQ(chrome::FindBrowserWithActiveWindow(), browser());

  // Simulate an update and wait for the notification to show.
  RelaunchRequiredDialogView* dialog = nullptr;
  {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "RelaunchRequiredDialog");
    SimulateUpdate();
    TriggerAnnoyanceLevel(UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
    auto* widget = waiter.WaitIfNeededAndGet();
    ASSERT_TRUE(widget);
    dialog = RelaunchRequiredDialogView::FromWidget(widget);
  }

  // Dismiss the notification and wait for its destruction.
  CloseDialog(std::exchange(dialog, nullptr));

  // Deactivate the browser window.
  MinimizeBrowser(browser_view);
  ASSERT_FALSE(chrome::FindBrowserWithActiveWindow());

  // The code below assumes that `action_timeout` is greater than 2.5 seconds.
  ASSERT_GT(TestTimeouts::action_timeout(), base::Milliseconds(2500));

  // Reduce the relaunch notification period to a deadline that is just in the
  // future. The browser is not active, so the controller will install an
  // observer and wait for it to become active. The controller caches this
  // deadline.
  auto deadline = AdjustPeriodToDeadlineIn(TestTimeouts::action_timeout() / 5);

  // Increase the relaunch notification period a bit. In the buggy case, the
  // controller does not update the cached value of the deadline.
  AdjustPeriodToDeadlineIn(TestTimeouts::action_timeout() / 2.5);

  // Advance 1/2 second past the first revised deadline. This lets the clock
  // move past the bad deadline cached by the controller for the dialog, but is
  // still before the true relaunch deadline. It is significant that the clock
  // is at least 1/2 second ahead so that the rounded delta to be shown in the
  // UX is less than zero in the buggy case.
  {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        (deadline + base::Milliseconds(500)) - base::Time::Now());
    run_loop.Run();
  }

  // Activate the browser window and wait for the notification to show.
  {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "RelaunchRequiredDialog");
    browser_view->Restore();
    auto* widget = waiter.WaitIfNeededAndGet();
    ASSERT_TRUE(widget);
    dialog = RelaunchRequiredDialogView::FromWidget(widget);
  }

  ASSERT_TRUE(dialog);
  ASSERT_GE(dialog->deadline(), base::Time::Now());
}
