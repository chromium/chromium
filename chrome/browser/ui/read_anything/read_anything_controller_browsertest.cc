// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_controller.h"

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"

class ReadAnythingControllerBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ReadAnythingControllerBrowserTest()
      : is_immersive_read_anything_enabled_(GetParam()) {}

  void SetUp() override {
    if (is_immersive_read_anything_enabled_) {
      scoped_feature_list_.InitWithFeatures({features::kImmersiveReadAnything},
                                            {});
    } else {
      scoped_feature_list_.InitWithFeatures({},
                                            {features::kImmersiveReadAnything});
    }
    InProcessBrowserTest::SetUp();
  }

 protected:
  const bool is_immersive_read_anything_enabled_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ReadAnythingControllerBrowserTest,
                       ShowSidePanelFromAppMenu) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);

  auto* controller = ReadAnythingController::From(tab);

  if (is_immersive_read_anything_enabled_) {
    ASSERT_TRUE(controller);
  } else {
    ASSERT_FALSE(controller);
  }

  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));

  chrome::ExecuteCommand(browser(), IDC_SHOW_READING_MODE_SIDE_PANEL);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
}

IN_PROC_BROWSER_TEST_P(ReadAnythingControllerBrowserTest,
                       ShowSidePanelFromContextMenu) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);

  auto* controller = ReadAnythingController::From(tab);

  if (is_immersive_read_anything_enabled_) {
    ASSERT_TRUE(controller);
  } else {
    ASSERT_FALSE(controller);
  }

  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));

  content::WebContents* web_contents = tab->GetContents();
  TestRenderViewContextMenu menu(*web_contents->GetPrimaryMainFrame(),
                                 content::ContextMenuParams());
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE, 0);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
}

IN_PROC_BROWSER_TEST_P(ReadAnythingControllerBrowserTest,
                       ToggleSidePanelViaActionItem) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);

  auto* controller = ReadAnythingController::From(tab);

  if (is_immersive_read_anything_enabled_) {
    ASSERT_TRUE(controller);
  } else {
    ASSERT_FALSE(controller);
  }

  auto& action_manager = actions::ActionManager::Get();
  auto* const read_anything_action =
      action_manager.FindAction(kActionSidePanelShowReadAnything);
  ASSERT_TRUE(read_anything_action);

  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));

  // Create a context with a valid trigger for the action.
  actions::ActionInvocationContext context =
      actions::ActionInvocationContext::Builder()
          .SetProperty(
              kSidePanelOpenTriggerKey,
              static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                  SidePanelOpenTrigger::kToolbarButton))
          .Build();

  read_anything_action->InvokeAction(std::move(context));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  // Invoke the action again to close the side panel.
  // Create a new context for the second invocation.
  actions::ActionInvocationContext context2 =
      actions::ActionInvocationContext::Builder()
          .SetProperty(
              kSidePanelOpenTriggerKey,
              static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                  SidePanelOpenTrigger::kToolbarButton))
          .Build();
  read_anything_action->InvokeAction(std::move(context2));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ReadAnythingControllerBrowserTest,
                         ::testing::Bool());
