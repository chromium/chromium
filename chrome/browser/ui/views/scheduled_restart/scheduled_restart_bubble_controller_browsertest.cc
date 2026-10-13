// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_bubble_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/scheduled_restart_manager.h"
#include "chrome/browser/lifetime/scheduled_restart_test_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace scheduled_restart {

class ScheduledRestartBubbleControllerBrowserTest
    : public InProcessBrowserTest {
 public:
  ScheduledRestartBubbleControllerBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kScheduledRestart,
          {{features::kScheduledRestartFirstNudgeDelay.name, "0s"}}},
         {features::kScheduledRestartAppMenuItem, {}}},
        {});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kSimulateUpgrade);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->ClearPref(
        prefs::kScheduledRestartLastNudgeTime);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ScheduledRestartBubbleControllerBrowserTest,
                       TriggerBubbleOnNTPCreation) {
  FakeUpgradeDetector upgrade_detector;
  upgrade_detector.SetUpgradeAvailable();
  upgrade_detector.set_upgrade_notification_stage_for_testing(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  upgrade_detector.set_upgrade_detected_time(base::Time::Now() -
                                             base::Days(15));

  ScheduledRestartManager manager(upgrade_detector);

  auto* controller = ScheduledRestartBubbleController::From(g_browser_process);
  ASSERT_TRUE(controller);
  controller->set_scheduled_restart_manager_for_testing(&manager);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Open New Tab Page via standard user command (Ctrl+T / + button).
  chrome::ExecuteCommand(browser(), IDC_NEW_TAB);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* new_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(new_contents));

  // Verify nudge was triggered and recorded in local state.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return controller->is_bubble_showing(); }));
  EXPECT_FALSE(g_browser_process->local_state()
                   ->GetTime(prefs::kScheduledRestartLastNudgeTime)
                   .is_null());

  controller->set_scheduled_restart_manager_for_testing(nullptr);
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartBubbleControllerBrowserTest,
                       TriggerBubbleFromAppMenuCommand) {
  FakeUpgradeDetector upgrade_detector;
  upgrade_detector.SetUpgradeAvailable();
  // Set detection time to now (before any passive nudge delay) to verify that
  // the explicit App Menu command opens the bubble immediately.
  upgrade_detector.set_upgrade_detected_time(base::Time::Now());

  ScheduledRestartManager manager(upgrade_detector);

  auto* controller = ScheduledRestartBubbleController::From(g_browser_process);
  ASSERT_TRUE(controller);
  controller->set_scheduled_restart_manager_for_testing(&manager);

  chrome::ExecuteCommand(browser(), IDC_SCHEDULED_RESTART_DIALOG);

  EXPECT_TRUE(controller->is_bubble_showing());
  EXPECT_FALSE(g_browser_process->local_state()
                   ->GetTime(prefs::kScheduledRestartLastNudgeTime)
                   .is_null());

  controller->set_scheduled_restart_manager_for_testing(nullptr);
}

// Replacing a passive NTP bubble with the App Menu dialog is not a user
// dismissal, so it must not be recorded as one.
IN_PROC_BROWSER_TEST_F(ScheduledRestartBubbleControllerBrowserTest,
                       AppMenuCommandReplacesPassiveBubbleWithoutDismissal) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  FakeUpgradeDetector upgrade_detector;
  upgrade_detector.SetUpgradeAvailable();
  upgrade_detector.set_upgrade_notification_stage_for_testing(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  upgrade_detector.set_upgrade_detected_time(base::Time::Now() -
                                             base::Days(15));

  ScheduledRestartManager manager(upgrade_detector);

  auto* controller = ScheduledRestartBubbleController::From(g_browser_process);
  ASSERT_TRUE(controller);
  controller->set_scheduled_restart_manager_for_testing(&manager);

  chrome::ExecuteCommand(browser(), IDC_NEW_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return controller->is_bubble_showing(); }));

  chrome::ExecuteCommand(browser(), IDC_SCHEDULED_RESTART_DIALOG);

  // A passive bubble is already showing, so is_bubble_showing() alone would
  // pass even if the command were a no-op. Counting shown bubbles verifies
  // that the command created the replacement.
  EXPECT_TRUE(controller->is_bubble_showing());
  EXPECT_EQ(user_action_tester.GetActionCount("ScheduledRestart_BubbleShown"),
            2);
  histogram_tester.ExpectTotalCount("Session.ScheduledRestart.DialogChoice", 0);

  controller->set_scheduled_restart_manager_for_testing(nullptr);
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartBubbleControllerBrowserTest,
                       AppMenuCommandSuppressedWhenAlreadyScheduled) {
  FakeUpgradeDetector upgrade_detector;
  upgrade_detector.SetUpgradeAvailable();
  upgrade_detector.set_upgrade_detected_time(base::Time::Now());

  ScheduledRestartManager manager(upgrade_detector);
  // Scheduling starts idle polling, which could otherwise relaunch the browser
  // if the bot stays idle long enough during the test.
  manager.set_relaunch_callback_for_testing(base::DoNothing());
  manager.ScheduleRestartOnIdle();

  auto* controller = ScheduledRestartBubbleController::From(g_browser_process);
  ASSERT_TRUE(controller);
  controller->set_scheduled_restart_manager_for_testing(&manager);

  chrome::ExecuteCommand(browser(), IDC_SCHEDULED_RESTART_DIALOG);

  EXPECT_FALSE(controller->is_bubble_showing());

  controller->set_scheduled_restart_manager_for_testing(nullptr);
}

}  // namespace scheduled_restart
