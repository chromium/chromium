// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_omnibox_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/actions/action_id.h"
#include "url/url_constants.h"

class TestReadAnythingOmniboxController : public ReadAnythingOmniboxController {
 public:
  explicit TestReadAnythingOmniboxController(tabs::TabInterface* tab)
      : ReadAnythingOmniboxController(tab) {}

  int CheckCount() { return checks_; }

  void ResetCheckCount() { checks_ = 0; }

 protected:
  void CheckIfShouldSuggestReadingMode() override { checks_++; }

 private:
  int checks_ = 0;
};

class ReadAnythingOmniboxControllerBrowserTest : public InProcessBrowserTest {
 public:
  ReadAnythingOmniboxControllerBrowserTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnythingOmniboxChip, features::kPageActionsMigration},
        {features::kImmersiveReadAnything});
    InProcessBrowserTest::SetUp();
  }

  std::unique_ptr<TestReadAnythingOmniboxController> CreateController() {
    tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
    CHECK(tab);
    return std::make_unique<TestReadAnythingOmniboxController>(tab);
  }

  void TearDownOnMainThread() override {
    controller_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<TestReadAnythingOmniboxController> controller_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxControllerBrowserTest,
                       DidStopLoadingIsDebounced) {
  base::ScopedMockTimeMessageLoopTaskRunner mocked_task_runner;
  controller_ = CreateController();

  // Ensure the check does not run immediately on load.
  controller_->DidStopLoading();
  EXPECT_EQ(controller_->CheckCount(), 0);

  // Fast forward less than the delay and ensure the check has not run.
  mocked_task_runner->FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(controller_->CheckCount(), 0);

  // The timer should restart when DidStopLoading is called again.
  controller_->DidStopLoading();
  mocked_task_runner->FastForwardBy(base::Milliseconds(900));
  EXPECT_EQ(controller_->CheckCount(), 0);

  // Now after the full delay, the check should run once.
  mocked_task_runner->FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(controller_->CheckCount(), 1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxControllerBrowserTest,
                       TabForegroundedIsDebounced) {
  base::ScopedMockTimeMessageLoopTaskRunner mocked_task_runner;
  controller_ = CreateController();

  chrome::NewTab(browser());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  // Ensure the check does not run immediately on foreground.
  browser()->tab_strip_model()->ActivateTabAt(1);
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(controller_->CheckCount(), 0);

  // Fast forward less than the delay and ensure the check has not run.
  mocked_task_runner->FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(controller_->CheckCount(), 0);

  // The timer should restart when tab is forgrounded again.
  browser()->tab_strip_model()->ActivateTabAt(1);
  browser()->tab_strip_model()->ActivateTabAt(0);
  mocked_task_runner->FastForwardBy(base::Milliseconds(900));
  EXPECT_EQ(controller_->CheckCount(), 0);

  // Now after the full delay, the check should run once.
  mocked_task_runner->FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(controller_->CheckCount(), 1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxControllerBrowserTest,
                       Activate_LogsOmniboxEntrypointAfterOmniboxClicked) {
  base::HistogramTester histogram_tester;
  controller_ = CreateController();

  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  tab->GetTabFeatures()->page_action_controller()->Show(
      kActionSidePanelShowReadAnything);

  controller_->Activate(true, ReadAnythingOpenTrigger::kOmniboxChip);

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.EntryPointAfterOmnibox",
      ReadAnythingOpenTrigger::kOmniboxChip, 1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxControllerBrowserTest,
                       Activate_LogsNotOmniboxEntrypointAfterOmniboxShown) {
  base::HistogramTester histogram_tester;
  controller_ = CreateController();

  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  tab->GetTabFeatures()->page_action_controller()->Show(
      kActionSidePanelShowReadAnything);

  controller_->Activate(true,
                        ReadAnythingOpenTrigger::kReadAnythingContextMenu);

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.EntryPointAfterOmnibox",
      ReadAnythingOpenTrigger::kReadAnythingContextMenu, 1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxControllerBrowserTest,
                       Activate_DoesNotLogTogglePresentationAfterOmniboxShown) {
  base::HistogramTester histogram_tester;
  controller_ = CreateController();

  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  tab->GetTabFeatures()->page_action_controller()->Show(
      kActionSidePanelShowReadAnything);

  controller_->Activate(
      true, ReadAnythingOpenTrigger::kReadAnythingTogglePresentationButton);

  histogram_tester.ExpectTotalCount(
      "Accessibility.ReadAnything.EntryPointAfterOmnibox", 0);
}
