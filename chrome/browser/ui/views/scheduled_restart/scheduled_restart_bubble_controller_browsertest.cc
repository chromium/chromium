// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_bubble_controller.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/scheduled_restart_manager.h"
#include "chrome/browser/lifetime/scheduled_restart_test_utils.h"
#include "chrome/browser/ui/browser.h"  // nocheck
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

namespace {

class TestScheduledRestartBubbleController
    : public ScheduledRestartBubbleController {
 public:
  TestScheduledRestartBubbleController() = default;
  ~TestScheduledRestartBubbleController() override = default;

  int bubble_shown_count() const { return bubble_shown_count_; }

 protected:
  views::Widget* ShowBubble(BrowserWindowInterface* browser) override {
    ++bubble_shown_count_;
    return ScheduledRestartBubbleController::ShowBubble(browser);
  }

 private:
  int bubble_shown_count_ = 0;
};

}  // namespace

class ScheduledRestartBubbleControllerBrowserTest
    : public InProcessBrowserTest {
 public:
  ScheduledRestartBubbleControllerBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kScheduledRestart,
        {{features::kScheduledRestartFirstNudgeDelay.name, "0s"}});
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
  EXPECT_TRUE(controller->is_bubble_showing());
  EXPECT_FALSE(g_browser_process->local_state()
                   ->GetTime(prefs::kScheduledRestartLastNudgeTime)
                   .is_null());

  controller->set_scheduled_restart_manager_for_testing(nullptr);
}

}  // namespace scheduled_restart
