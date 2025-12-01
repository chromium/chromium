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
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"

class ReadAnythingControllerBrowserTest : public InProcessBrowserTest {
 public:
  ReadAnythingControllerBrowserTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kImmersiveReadAnything},
                                          {});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ShowSidePanelFromAppMenu) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);

  auto* controller = ReadAnythingController::From(tab);

  ASSERT_TRUE(controller);

  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));

  chrome::ExecuteCommand(browser(), IDC_SHOW_READING_MODE_SIDE_PANEL);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ShowSidePanelFromContextMenu) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);

  auto* controller = ReadAnythingController::From(tab);

  ASSERT_TRUE(controller);

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

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ToggleSidePanelViaActionItem) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);

  auto* controller = ReadAnythingController::From(tab);

  ASSERT_TRUE(controller);

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
                  SidePanelOpenTrigger::kPinnedEntryToolbarButton))
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
                  SidePanelOpenTrigger::kPinnedEntryToolbarButton))
          .Build();
  read_anything_action->InvokeAction(std::move(context2));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       GetPresentationState_InitialState) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInactive);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       GetPresentationState_SidePanelState) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  controller->ShowUI(SidePanelOpenTrigger::kAppMenu);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return controller->GetPresentationState() ==
           ReadAnythingController::PresentationState::kInSidePanel;
  }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       GetOrCreateWebUIWrapper) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>> wrapper =
      controller->GetOrCreateWebUIWrapper();
  EXPECT_TRUE(wrapper);
  EXPECT_TRUE(wrapper->web_contents());
  EXPECT_TRUE(wrapper->web_contents()->GetWebUI());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       WebUIContentsWrapperIsPassedToSidePanel) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Create the WebUI contents and get a pointer to it.
  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>> wrapper =
      controller->GetOrCreateWebUIWrapper();
  content::WebContents* controller_web_contents = wrapper->web_contents();
  ASSERT_TRUE(controller_web_contents);

  // Return the wrapper to the controller so it can be passed to the side panel.
  controller->SetWebUIWrapperForTest(std::move(wrapper));

  // Show Reading Mode.
  controller->ShowUI(SidePanelOpenTrigger::kAppMenu);
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  // Get the WebContents from the side panel and assert it's the same one.
  auto* side_panel = BrowserView::GetBrowserViewForBrowser(browser())
                         ->contents_height_side_panel();
  auto* content_wrapper = side_panel->GetContentParentView();
  ASSERT_EQ(1u, content_wrapper->children().size());
  auto* side_panel_view =
      static_cast<SidePanelWebUIView*>(content_wrapper->children()[0]);
  content::WebContents* side_panel_web_contents =
      side_panel_view->web_contents();
  ASSERT_TRUE(side_panel_web_contents);

  EXPECT_EQ(controller_web_contents, side_panel_web_contents);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       OnTabStripModelChanged_TabBecomesActiveOnStartup) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* tab1 =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(0));
  ReadAnythingController* controller1 = ReadAnythingController::From(tab1);
  ASSERT_TRUE(controller1);

  // Confirm that tab1 became active on browser startup.
  ASSERT_TRUE(controller1->isActiveTab());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       OnTabStripModelChanged_NewTabBecomesActive) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* tab1 =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(0));
  ReadAnythingController* controller1 = ReadAnythingController::From(tab1);

  // Add a second tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), /* index= */ 1,
                   /* foreground= */ true);
  tabs::TabInterface* tab2 =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(1));
  ReadAnythingController* controller2 = ReadAnythingController::From(tab2);
  ASSERT_TRUE(controller2);

  // Confirm that controller1 became inactive and controller2 became active.
  ASSERT_FALSE(controller1->isActiveTab());
  ASSERT_TRUE(controller2->isActiveTab());
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerBrowserTest,
    OnTabStripModelChanged_TabsChangeActiveStateOnTabSwitch) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* tab1 =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(0));
  ReadAnythingController* controller1 = ReadAnythingController::From(tab1);

  // Add a second tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), /* index= */ 1,
                   /* foreground= */ true);
  tabs::TabInterface* tab2 =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(1));
  ReadAnythingController* controller2 = ReadAnythingController::From(tab2);

  // Switch back to the first tab.
  tab_strip_model->ActivateTabAt(0);
  ASSERT_EQ(0, tab_strip_model->active_index());

  // Confirm that controller1 became active and controller2 became inactive.
  ASSERT_TRUE(controller1->isActiveTab());
  ASSERT_FALSE(controller2->isActiveTab());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       OnTabStripModelChanged_NewBackgroundTabIsInactive) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* tab1 =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(0));
  ReadAnythingController* controller1 = ReadAnythingController::From(tab1);

  // Add a new tab in the background (inactive).
  chrome::AddTabAt(browser(), GURL("about:blank"), /* index= */ 1,
                   /* foreground= */ false);
  ASSERT_EQ(2, tab_strip_model->count());
  ASSERT_EQ(0, tab_strip_model->active_index());

  tabs::TabInterface* tab2 =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(1));
  ReadAnythingController* controller2 = ReadAnythingController::From(tab2);

  // Confirm that tab 1 remains active and tab 2 didn't become active.
  ASSERT_TRUE(controller1->isActiveTab());
  ASSERT_FALSE(controller2->isActiveTab());
}

// TODO(crbug.com/463939639): Change this test to confirm that the WebUI is
// passed back and forth between IRM and SP, instead of checking that the same
// WebUI is reused on open and close of the SP.
IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ReusesWebUIOnOpenCloseAndReopen) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));

  // Open side panel for the first time and get the WebContents.
  controller->ToggleReadAnythingSidePanel(SidePanelOpenTrigger::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  auto* side_panel = BrowserView::GetBrowserViewForBrowser(browser())
                         ->contents_height_side_panel();
  auto* content_wrapper = side_panel->GetContentParentView();
  ASSERT_EQ(1u, content_wrapper->children().size());
  auto* side_panel_view =
      static_cast<SidePanelWebUIView*>(content_wrapper->children()[0]);
  content::WebContents* web_contents1 = side_panel_view->web_contents();
  ASSERT_TRUE(web_contents1);

  // Close the side panel.
  controller->ToggleReadAnythingSidePanel(SidePanelOpenTrigger::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  // Ensure the WebUI is now owned by the controller.
  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>> wrapper =
      controller->GetOrCreateWebUIWrapper();
  ASSERT_TRUE(wrapper->web_contents());
  // Return the wrapper to the controller so it can be passed to the side panel.
  controller->SetWebUIWrapperForTest(std::move(wrapper));

  // Re-open the side panel.
  controller->ToggleReadAnythingSidePanel(SidePanelOpenTrigger::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  // Get the new WebContents from the side panel and assert it's the same one.
  side_panel = BrowserView::GetBrowserViewForBrowser(browser())
                   ->contents_height_side_panel();
  content_wrapper = side_panel->GetContentParentView();
  ASSERT_EQ(1u, content_wrapper->children().size());
  side_panel_view =
      static_cast<SidePanelWebUIView*>(content_wrapper->children()[0]);
  content::WebContents* web_contents2 = side_panel_view->web_contents();
  ASSERT_TRUE(web_contents2);

  EXPECT_EQ(web_contents1, web_contents2);
}
