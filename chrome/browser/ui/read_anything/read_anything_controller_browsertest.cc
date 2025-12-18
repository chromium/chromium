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
#include "chrome/test/base/ui_test_utils.h"
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

class ReadAnythingControllerTestBase
    : public ReadAnythingControllerBrowserTest {
 public:
  content::WebContents* GetSidePanelWebContents() {
    auto* side_panel = BrowserView::GetBrowserViewForBrowser(browser())
                           ->contents_height_side_panel();
    auto* content_wrapper = side_panel->GetContentParentView();
    if (content_wrapper->children().empty()) {
      return nullptr;
    }
    auto* side_panel_view =
        static_cast<SidePanelWebUIView*>(content_wrapper->children()[0]);
    return side_panel_view->web_contents();
  }

  views::View* GetImmersiveOverlay() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetWidget()->GetContentsView()->GetViewByID(
        VIEW_ID_READ_ANYTHING_OVERLAY);
  }

  content::WebContents* GetImmersiveWebContents() {
    views::View* overlay_view = GetImmersiveOverlay();
    if (!overlay_view || !overlay_view->GetVisible() ||
        overlay_view->children().empty()) {
      return nullptr;
    }
    views::WebView* web_view =
        static_cast<views::WebView*>(overlay_view->children()[0]);
    return web_view->GetWebContents();
  }
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
                       ShowImmersiveUI_SetsPresentationState) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  // Check presentation state.
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInImmersiveOverlay);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ShowImmersiveUI_OverlayIsVisible) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  // Check overlay visibility and content.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* overlay_view =
      browser_view->GetWidget()->GetContentsView()->GetViewByID(
          VIEW_ID_READ_ANYTHING_OVERLAY);
  ASSERT_TRUE(overlay_view);
  EXPECT_TRUE(overlay_view->GetVisible());
  EXPECT_FALSE(overlay_view->children().empty());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ShowImmersiveUI_CapturesMainPageWebContents) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  content::WebContents* main_contents = tab->GetContents();
  ASSERT_TRUE(main_contents);
  // Initial main-contents capture state is not being captured yet
  EXPECT_FALSE(main_contents->IsBeingVisiblyCaptured());

  // Show Immersive UI.
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  // Wait for capture to start, because it requires Reading Mode to become
  // visible to trigger the capture.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return main_contents->IsBeingVisiblyCaptured(); }));
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInImmersiveOverlay);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ShowImmersiveUI_Idempotency) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Show immersive mode
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInImmersiveOverlay);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* overlay_view =
      browser_view->GetWidget()->GetContentsView()->GetViewByID(
          VIEW_ID_READ_ANYTHING_OVERLAY);
  ASSERT_TRUE(overlay_view);
  ASSERT_TRUE(overlay_view->GetVisible());
  ASSERT_EQ(1u, overlay_view->children().size());

  // Call ShowImmersiveUI again even though it's already showing
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInImmersiveOverlay);

  // Verify overlay is still visible and has only one child (and that there's no
  // crash)
  ASSERT_TRUE(overlay_view->GetVisible());
  ASSERT_EQ(1u, overlay_view->children().size());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       CloseImmersiveUI_SetsPresentationState) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInImmersiveOverlay);

  controller->CloseImmersiveUI();
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInactive);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       CloseImmersiveUI_HidesOverlay) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Show immersive mode and confirm it's showing
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* overlay_view =
      browser_view->GetWidget()->GetContentsView()->GetViewByID(
          VIEW_ID_READ_ANYTHING_OVERLAY);
  ASSERT_TRUE(overlay_view);
  ASSERT_TRUE(overlay_view->GetVisible());
  ASSERT_FALSE(overlay_view->children().empty());

  // Close immersive mode and confirm it's hidden
  controller->CloseImmersiveUI();
  EXPECT_FALSE(overlay_view->GetVisible());
  EXPECT_TRUE(overlay_view->children().empty());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       CloseImmersiveUI_ReleasesMainPageCapture) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  content::WebContents* main_contents = tab->GetContents();
  ASSERT_TRUE(main_contents);

  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return main_contents->IsBeingVisiblyCaptured(); }));

  controller->CloseImmersiveUI();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !main_contents->IsBeingVisiblyCaptured(); }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       CloseImmersiveUI_PreservesWebUI) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Show immersive mode
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  // Get the WebUI used in immersive mode
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* overlay_view =
      browser_view->GetWidget()->GetContentsView()->GetViewByID(
          VIEW_ID_READ_ANYTHING_OVERLAY);
  // The first child should be the web view. We cast to WebView to get the
  // WebContents.
  views::WebView* web_view =
      static_cast<views::WebView*>(overlay_view->children()[0]);
  content::WebContents* web_contents1 = web_view->GetWebContents();
  ASSERT_TRUE(web_contents1);

  // Close immersive mode
  controller->CloseImmersiveUI();

  // Get the WebUI wrapper again (should be inactive now)
  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>> wrapper =
      controller->GetOrCreateWebUIWrapper(
          ReadAnythingController::PresentationState::kInactive);
  ASSERT_TRUE(wrapper->web_contents());

  // Verify it is the same WebContents
  EXPECT_EQ(web_contents1, wrapper->web_contents());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       TabSwitch_ClosesImmersiveUI) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* tab1 =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(0));
  ReadAnythingController* controller1 = ReadAnythingController::From(tab1);

  // Show immersive mode on first tab
  controller1->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_EQ(controller1->GetPresentationState(),
            ReadAnythingController::PresentationState::kInImmersiveOverlay);

  // Add and switch to a second tab
  chrome::AddTabAt(browser(), GURL("about:blank"), /* index= */ 1,
                   /* foreground= */ true);

  // Verify controller1 is no longer in immersive mode
  EXPECT_EQ(controller1->GetPresentationState(),
            ReadAnythingController::PresentationState::kInactive);
  // Verify overlay is hidden
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* overlay_view =
      browser_view->GetWidget()->GetContentsView()->GetViewByID(
          VIEW_ID_READ_ANYTHING_OVERLAY);
  EXPECT_FALSE(overlay_view->GetVisible());
  EXPECT_TRUE(overlay_view->children().empty());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       CloseImmersiveUI_Idempotency) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Ensure state is inactive
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kUndefined);

  // Calling CloseImmersiveUI shouldn't crash or change state
  controller->CloseImmersiveUI();
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kUndefined);
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
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       GetOrCreateWebUIWrapper_SetsState) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kUndefined);

  // The wrapper is moved to the caller, so we must keep it alive.
  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>> wrapper =
      controller->GetOrCreateWebUIWrapper(
          ReadAnythingController::PresentationState::kInSidePanel);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInSidePanel);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       TransferWebUiOwnership_ResetsState) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  auto wrapper = controller->GetOrCreateWebUIWrapper(
      ReadAnythingController::PresentationState::kInSidePanel);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInSidePanel);

  controller->TransferWebUiOwnership(std::move(wrapper));
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
      controller->GetOrCreateWebUIWrapper(
          ReadAnythingController::PresentationState::kInactive);
  EXPECT_TRUE(wrapper);
  EXPECT_TRUE(wrapper->web_contents());
  EXPECT_TRUE(wrapper->web_contents()->GetWebUI());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerTestBase,
                       WebUIContentsWrapperIsPassedToSidePanel) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Create the WebUI contents and get a pointer to it.
  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>> wrapper =
      controller->GetOrCreateWebUIWrapper(
          ReadAnythingController::PresentationState::kInactive);
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
  content::WebContents* side_panel_web_contents = GetSidePanelWebContents();
  ASSERT_TRUE(side_panel_web_contents);

  EXPECT_EQ(controller_web_contents, side_panel_web_contents);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerTestBase,
    OnTabStripModelChanged_ImmersiveShowsWhenTabBecomesActiveAgain) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* tab1 =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(0));
  ReadAnythingController* controller1 = ReadAnythingController::From(tab1);
  ASSERT_TRUE(controller1);
  controller1->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  // Confirm that IRM is shown.
  views::View* overlay_view = GetImmersiveOverlay();
  ASSERT_TRUE(overlay_view);
  ASSERT_TRUE(overlay_view->GetVisible());
  ASSERT_FALSE(overlay_view->children().empty());

  // Add a second tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), /* index= */ 1,
                   /* foreground= */ true);

  // Confirm that IRM is hidden after tab switch.
  ASSERT_FALSE(overlay_view->GetVisible());
  ASSERT_TRUE(overlay_view->children().empty());

  // Switch back to the first tab.
  tab_strip_model->ActivateTabAt(0);

  // Confirm that IRM is shown again automatically.
  ASSERT_TRUE(overlay_view->GetVisible());
  ASSERT_FALSE(overlay_view->children().empty());
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerTestBase,
    OnTabStripModelChanged_NewBackgroundTabIsInactive_DoesNotCloseImmersive) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* tab1 =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(0));
  ReadAnythingController* controller1 = ReadAnythingController::From(tab1);
  controller1->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  // Add a new tab in the background (inactive).
  chrome::AddTabAt(browser(), GURL("about:blank"), /* index= */ 1,
                   /* foreground= */ false);

  // Confirm that IRM is still shown.
  views::View* overlay_view = GetImmersiveOverlay();
  ASSERT_TRUE(overlay_view);
  ASSERT_TRUE(overlay_view->GetVisible());
  ASSERT_FALSE(overlay_view->children().empty());
}

// TODO(crbug.com/463939639): Change this test to confirm that the WebUI is
// passed back and forth between IRM and SP, instead of checking that the same
// WebUI is reused on open and close of the SP.
IN_PROC_BROWSER_TEST_F(ReadAnythingControllerTestBase,
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
  content::WebContents* web_contents1 = GetSidePanelWebContents();
  ASSERT_TRUE(web_contents1);

  // Close the side panel.
  controller->ToggleReadAnythingSidePanel(SidePanelOpenTrigger::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  // Ensure the WebUI is now owned by the controller.
  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>> wrapper =
      controller->GetOrCreateWebUIWrapper(
          ReadAnythingController::PresentationState::kInactive);
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
  content::WebContents* web_contents2 = GetSidePanelWebContents();
  ASSERT_TRUE(web_contents2);

  EXPECT_EQ(web_contents1, web_contents2);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       WebContentsObserverPrimaryPageChangedCrossNavigation) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);

  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  ASSERT_EQ(controller->GetNavCounterForTesting(), 1);

  GURL url("about:blank");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_EQ(controller->GetNavCounterForTesting(), 2);

  GURL url2("https://www.example.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  ASSERT_EQ(controller->GetNavCounterForTesting(), 3);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerBrowserTest,
    WebContentsObserverPrimaryPageChangedFragmentNavigation) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);

  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  GURL url("about:blank");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_EQ(controller->GetNavCounterForTesting(), 2);

  GURL same_doc_url("about:blank#same");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_doc_url));

  ASSERT_EQ(controller->GetNavCounterForTesting(), 2);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerTestBase,
    ShowImmersiveUIImmediatelyFollowedByShowSidePanelUI_DoesNotCrash) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Open Side Panel immediately followed by opening Immersive UI (before the
  // WebUI has a chance to load)
  controller->ShowUI(SidePanelOpenTrigger::kAppMenu);
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  // Verify Immersive UI is open (and did not crash)
  views::View* overlay_view = GetImmersiveOverlay();
  ASSERT_TRUE(overlay_view);
  EXPECT_TRUE(overlay_view->GetVisible());
  EXPECT_FALSE(overlay_view->children().empty());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerTestBase,
                       ShowImmersiveUI_ClosesSidePanel) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();

  // Open Side Panel
  controller->ShowUI(SidePanelOpenTrigger::kAppMenu);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  // Get the WebUI from the side panel
  content::WebContents* side_panel_web_contents = GetSidePanelWebContents();
  ASSERT_TRUE(side_panel_web_contents);

  // Open Immersive UI
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  // Verify Side Panel is closed
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  // Verify Immersive UI is open
  views::View* overlay_view = GetImmersiveOverlay();
  ASSERT_TRUE(overlay_view);
  EXPECT_TRUE(overlay_view->GetVisible());
  EXPECT_FALSE(overlay_view->children().empty());

  // Verify the same WebUI is used in the immersive overlay
  EXPECT_EQ(side_panel_web_contents, GetImmersiveWebContents());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerTestBase,
                       ShowSidePanelUI_ClosesImmersiveUI) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();

  // Open Immersive UI
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInImmersiveOverlay);

  // Get the WebUI from the immersive overlay
  content::WebContents* immersive_ui_web_contents = GetImmersiveWebContents();
  ASSERT_TRUE(immersive_ui_web_contents);

  // Open Side Panel via ShowUI
  controller->ShowUI(SidePanelOpenTrigger::kAppMenu);

  // Verify Immersive UI is closed
  views::View* overlay_view = GetImmersiveOverlay();
  ASSERT_TRUE(overlay_view);
  EXPECT_FALSE(overlay_view->GetVisible());
  EXPECT_TRUE(overlay_view->children().empty());

  // Verify Side Panel is showing
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  // Verify the same WebUI is used in the side panel
  EXPECT_EQ(immersive_ui_web_contents, GetSidePanelWebContents());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerTestBase,
                       ToggleReadAnythingSidePanel_ClosesImmersiveUI) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();

  // Open Immersive UI
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInImmersiveOverlay);

  // Get the WebUI from the immersive overlay
  content::WebContents* immersive_ui_web_contents = GetImmersiveWebContents();
  ASSERT_TRUE(immersive_ui_web_contents);

  // Toggle Side Panel
  controller->ToggleReadAnythingSidePanel(SidePanelOpenTrigger::kAppMenu);

  // Verify Immersive UI is closed
  views::View* overlay_view = GetImmersiveOverlay();
  ASSERT_TRUE(overlay_view);
  EXPECT_FALSE(overlay_view->GetVisible());
  EXPECT_TRUE(overlay_view->children().empty());

  // Verify Side Panel is showing
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  // Verify the same WebUI is used in the side panel
  EXPECT_EQ(immersive_ui_web_contents, GetSidePanelWebContents());
}
