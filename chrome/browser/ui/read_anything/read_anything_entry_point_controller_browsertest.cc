// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "chrome/browser/ui/views/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"

class ReadAnythingEntryPointControllerBrowserTest
    : public InProcessBrowserTest {
 public:
  ReadAnythingEntryPointControllerBrowserTest() = default;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingEntryPointControllerBrowserTest,
                       ShowSidePanelFromOmnibox) {
  base::HistogramTester histogram_tester;
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));
  actions::ActionInvocationContext context;
  context.SetProperty(page_actions::kPageActionTriggerKey, 1);

  read_anything::ReadAnythingEntryPointController::InvokePageAction(browser(),
                                                                    context);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  histogram_tester.ExpectUniqueSample(
      "SidePanel.ReadAnything.ShowTriggered",
      SidePanelOpenTrigger::kReadAnythingOmniboxChip, 1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingEntryPointControllerBrowserTest,
                       ShowSidePanelFromPinned) {
  base::HistogramTester histogram_tester;
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));
  actions::ActionInvocationContext context;
  context.SetProperty(page_actions::kPageActionTriggerKey, -1);
  context.SetProperty(
      kSidePanelOpenTriggerKey,
      static_cast<int>(SidePanelOpenTrigger::kPinnedEntryToolbarButton));

  read_anything::ReadAnythingEntryPointController::InvokePageAction(browser(),
                                                                    context);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  histogram_tester.ExpectUniqueSample(
      "SidePanel.ReadAnything.ShowTriggered",
      SidePanelOpenTrigger::kPinnedEntryToolbarButton, 1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingEntryPointControllerBrowserTest,
                       ShowSidePanelFromAppMenu) {
  base::HistogramTester histogram_tester;
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));

  read_anything::ReadAnythingEntryPointController::ShowUI(
      browser(), ReadAnythingOpenTrigger::kAppMenu);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  histogram_tester.ExpectUniqueSample("SidePanel.ReadAnything.ShowTriggered",
                                      SidePanelOpenTrigger::kAppMenu, 1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingEntryPointControllerBrowserTest,
                       ShowSidePanelFromContextMenu) {
  base::HistogramTester histogram_tester;
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));

  read_anything::ReadAnythingEntryPointController::ShowUI(
      browser(), ReadAnythingOpenTrigger::kReadAnythingContextMenu);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  histogram_tester.ExpectUniqueSample(
      "SidePanel.ReadAnything.ShowTriggered",
      SidePanelOpenTrigger::kReadAnythingContextMenu, 1);
}
