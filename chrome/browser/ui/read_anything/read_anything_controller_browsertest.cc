// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/read_anything/read_anything_immersive_web_view.h"
#include "chrome/browser/ui/read_anything/read_anything_lifecycle_observer.h"
#include "chrome/browser/ui/read_anything/read_anything_service.h"
#include "chrome/browser/ui/read_anything/read_anything_service_factory.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_utils.h"

class MockReadAnythingLifecycleObserver : public ReadAnythingLifecycleObserver {
 public:
  MOCK_METHOD(void,
              Activate,
              (bool active, std::optional<ReadAnythingOpenTrigger>),
              (override));
  MOCK_METHOD(void, OnDestroyed, (), (override));
  MOCK_METHOD(void, OnTabWillDetach, (), (override));
  MOCK_METHOD(void, OnReadingModePresenterChanged, (), (override));
};

class MockReadAnythingService : public ReadAnythingService {
 public:
  explicit MockReadAnythingService(Profile* profile)
      : ReadAnythingService(profile) {}
  ~MockReadAnythingService() override = default;

  MOCK_METHOD(void, OnReadAnythingShown, (), (override));
  MOCK_METHOD(void, OnReadAnythingHidden, (), (override));
};

class ReadAnythingControllerBrowserTest : public InProcessBrowserTest {
 public:
  ReadAnythingControllerBrowserTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kImmersiveReadAnything,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
         features::kWasmTtsEngineAutoInstallDisabled
#endif
        },
        {});
    ReadAnythingController::SetFreezeDistillationOnCreationForTesting(true);
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    ReadAnythingController::SetFreezeDistillationOnCreationForTesting(false);
    InProcessBrowserTest::TearDown();
  }

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

  views::View* GetImmersiveOverlay(Browser* browser_ptr = nullptr) {
    if (!browser_ptr) {
      browser_ptr = browser();
    }
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser_ptr);
    return browser_view->GetWidget()->GetContentsView()->GetViewByID(
        VIEW_ID_READ_ANYTHING_OVERLAY);
  }

  views::View* GetImmersiveOverlayForTab(int tab_index) {
    auto* contents = browser()->tab_strip_model()->GetWebContentsAt(tab_index);
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetContentsContainerViewFor(contents)->GetViewByID(
        VIEW_ID_READ_ANYTHING_OVERLAY);
  }

  content::WebContents* GetImmersiveWebContents(
      Browser* browser_ptr = nullptr) {
    views::View* overlay_view = GetImmersiveOverlay(browser_ptr);
    if (!overlay_view || !overlay_view->GetVisible() ||
        overlay_view->children().empty()) {
      return nullptr;
    }
    views::WebView* web_view =
        static_cast<views::WebView*>(overlay_view->children()[0]);
    return web_view->GetWebContents();
  }

  void EmitWebUIShowEvent() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    views::View* overlay_view =
        browser_view->GetWidget()->GetContentsView()->GetViewByID(
            VIEW_ID_READ_ANYTHING_OVERLAY);
    ASSERT_TRUE(overlay_view);
    EmitWebUIShowEvent(overlay_view);
  }

  void EmitWebUIShowEvent(views::View* overlay_view) {
    ReadAnythingImmersiveWebView* web_view =
        static_cast<ReadAnythingImmersiveWebView*>(overlay_view->children()[0]);
    web_view->ShowUI();
  }

  void WaitForOverlayVisibility(bool visible) {
    views::View* overlay_view = GetImmersiveOverlay();
    ASSERT_TRUE(overlay_view);
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return visible == overlay_view->GetVisible(); }));
  }

  void AssertOverlayVisibility(bool visible) {
    views::View* overlay_view = GetImmersiveOverlay();
    ASSERT_TRUE(overlay_view);
    EXPECT_EQ(visible, overlay_view->GetVisible());
    if (visible) {
      ASSERT_FALSE(overlay_view->children().empty());
      views::WebView* web_view =
          static_cast<views::WebView*>(overlay_view->children()[0]);
      ASSERT_TRUE(web_view->GetWebContents());
    } else {
      if (!overlay_view->children().empty()) {
        views::WebView* web_view =
            static_cast<views::WebView*>(overlay_view->children()[0]);
        ASSERT_FALSE(web_view->GetWebContents());
      }
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ShowImmersiveUI_NotifiesObservers) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  MockReadAnythingLifecycleObserver observer;
  controller->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer, Activate(true, testing::_)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  run_loop.Run();

  // Cleanup
  controller->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       OnEntryShown_CalledWhenWebUIIsReused) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  MockReadAnythingLifecycleObserver observer;
  controller->AddObserver(&observer);

  // 1. Show Immersive UI (first time)
  base::RunLoop run_loop_1;
  EXPECT_CALL(observer, Activate(true, testing::_)).WillOnce([&run_loop_1]() {
    run_loop_1.Quit();
  });
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  run_loop_1.Run();

  // 2. Close Immersive UI
  base::RunLoop run_loop_2;
  EXPECT_CALL(observer, Activate(false, testing::_)).WillOnce([&run_loop_2]() {
    run_loop_2.Quit();
  });
  controller->CloseImmersiveUI();
  run_loop_2.Run();

  // 3. Show Immersive UI (second time - reuse WebUI)
  base::RunLoop run_loop_3;
  EXPECT_CALL(observer, Activate(true, testing::_)).WillOnce([&run_loop_3]() {
    run_loop_3.Quit();
  });
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  run_loop_3.Run();

  // Cleanup
  controller->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       CloseImmersiveUI_NotifiesObservers) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Show it first
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  MockReadAnythingLifecycleObserver observer;
  controller->AddObserver(&observer);

  // Close it
  base::RunLoop run_loop;
  EXPECT_CALL(observer, Activate(false, testing::_)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });
  controller->CloseImmersiveUI();
  run_loop.Run();

  // Cleanup
  controller->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       TabDetached_NotifiesObservers) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  MockReadAnythingLifecycleObserver observer;
  controller->AddObserver(&observer);

  // Show IRM so that it is still showing when the tab is detached.
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  EXPECT_CALL(observer, OnTabWillDetach()).Times(1);
  EXPECT_CALL(observer, OnDestroyed()).Times(0);

  // Detach the tab and attach it to a new browser.
  Browser* new_browser = CreateBrowser(browser()->profile());
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser()->tab_strip_model()->DetachTabAtForInsertion(0);
  new_browser->tab_strip_model()->AppendTab(std::move(detached_tab), true);

  testing::Mock::VerifyAndClearExpectations(&observer);

  // Cleanup
  controller->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       OnDestroyed_NotifiesObservers) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  MockReadAnythingLifecycleObserver observer;
  controller->AddObserver(&observer);

  // Show IRM so that it is still showing when the tab is closed.
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  EXPECT_CALL(observer, OnTabWillDetach()).Times(1);
  EXPECT_CALL(observer, OnDestroyed()).WillOnce([&controller, &observer]() {
    // Cleanup
    controller->RemoveObserver(&observer);
  });

  tab->Close();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       SetPresentationState_NotifiesObservers) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  MockReadAnythingLifecycleObserver observer;
  controller->AddObserver(&observer);

  EXPECT_CALL(observer, OnReadingModePresenterChanged()).Times(1);

  controller->SetPresentationState(
      ReadAnythingController::PresentationState::kInSidePanel);

  // Cleanup
  controller->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ShowImmersiveFromAppMenu) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  AssertOverlayVisibility(/*visible=*/false);

  chrome::ExecuteCommand(browser(), IDC_SHOW_READING_MODE_SIDE_PANEL);
  EmitWebUIShowEvent();

  AssertOverlayVisibility(/*visible=*/true);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ShowImmersiveFromContextMenu) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);

  auto* controller = ReadAnythingController::From(tab);

  ASSERT_TRUE(controller);

  AssertOverlayVisibility(/*visible=*/false);

  content::WebContents* web_contents = tab->GetContents();
  TestRenderViewContextMenu menu(*web_contents->GetPrimaryMainFrame(),
                                 content::ContextMenuParams());
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE, 0);
  EmitWebUIShowEvent();

  AssertOverlayVisibility(/*visible=*/true);
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

  EmitWebUIShowEvent(overlay_view);
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
  EmitWebUIShowEvent();
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
  EmitWebUIShowEvent();
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
  EmitWebUIShowEvent();
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
                       ToggleImmersiveViaActionItem) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);

  auto* controller = ReadAnythingController::From(tab);

  ASSERT_TRUE(controller);

  auto& action_manager = actions::ActionManager::Get();
  auto* const read_anything_action =
      action_manager.FindAction(kActionSidePanelShowReadAnything);
  ASSERT_TRUE(read_anything_action);

  AssertOverlayVisibility(/*visible=*/false);

  // Create a context with a valid trigger for the action.
  actions::ActionInvocationContext context =
      actions::ActionInvocationContext::Builder()
          .SetProperty(
              kSidePanelOpenTriggerKey,
              static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                  SidePanelOpenTrigger::kPinnedEntryToolbarButton))
          .Build();

  read_anything_action->InvokeAction(std::move(context));
  EmitWebUIShowEvent();

  AssertOverlayVisibility(/*visible=*/true);

  // Invoke the action again to close the immersive view.
  // Create a new context for the second invocation.
  actions::ActionInvocationContext context2 =
      actions::ActionInvocationContext::Builder()
          .SetProperty(
              kSidePanelOpenTriggerKey,
              static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                  SidePanelOpenTrigger::kPinnedEntryToolbarButton))
          .Build();
  read_anything_action->InvokeAction(std::move(context2));

  AssertOverlayVisibility(/*visible=*/false);
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

  controller->ShowSidePanelUI(SidePanelOpenTrigger::kAppMenu);

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

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
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
  controller->ShowSidePanelUI(SidePanelOpenTrigger::kAppMenu);
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
    ReadAnythingControllerBrowserTest,
    OnTabStripModelChanged_ImmersiveShowsWhenTabBecomesActiveAgain) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* tab1 =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(0));
  ReadAnythingController* controller1 = ReadAnythingController::From(tab1);
  ASSERT_TRUE(controller1);
  controller1->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  views::View* overlay_view = GetImmersiveOverlayForTab(0);
  EmitWebUIShowEvent(overlay_view);

  // Confirm that IRM is shown.
  ASSERT_TRUE(overlay_view);
  ASSERT_TRUE(overlay_view->GetVisible());
  ASSERT_FALSE(overlay_view->children().empty());
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return controller1->has_shown_ui(); }));

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
    ReadAnythingControllerBrowserTest,
    OnTabStripModelChanged_NewBackgroundTabIsInactive_DoesNotCloseImmersive) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* tab1 =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(0));
  ReadAnythingController* controller1 = ReadAnythingController::From(tab1);
  controller1->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();

  // Add a new tab in the background (inactive).
  chrome::AddTabAt(browser(), GURL("about:blank"), /* index= */ 1,
                   /* foreground= */ false);

  // Confirm that IRM is still shown.
  views::View* overlay_view = GetImmersiveOverlay();
  ASSERT_TRUE(overlay_view);
  ASSERT_TRUE(overlay_view->GetVisible());
  ASSERT_FALSE(overlay_view->children().empty());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       WebContentsObserverPrimaryPageChangedCrossNavigation) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Show Immersive UI
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return controller->has_shown_ui(); }));
  AssertOverlayVisibility(/*visible=*/true);

  // Navigate to a new page
  GURL url("about:blank");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Verify Immersive UI is closed
  AssertOverlayVisibility(/*visible=*/false);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInactive);

  // Show Immersive UI again
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  AssertOverlayVisibility(/*visible=*/true);

  // Navigate to another page
  GURL url2("https://www.example.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  // Verify Immersive UI is closed again
  AssertOverlayVisibility(/*visible=*/false);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInactive);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerBrowserTest,
    WebContentsObserverPrimaryPageChangedFragmentNavigation) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Navigate to initial page
  GURL url("about:blank");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Show Immersive UI
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
  AssertOverlayVisibility(/*visible=*/true);

  // Perform fragment navigation
  GURL same_doc_url("about:blank#same");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_doc_url));

  // Verify Immersive UI is still open (PrimaryPageChanged is not called)
  AssertOverlayVisibility(/*visible=*/true);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInImmersiveOverlay);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerBrowserTest,
    ShowImmersiveUIImmediatelyFollowedByShowSidePanelUI_DoesNotCrash) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Open Side Panel immediately followed by opening Immersive UI (before the
  // WebUI has a chance to load)
  controller->ShowSidePanelUI(SidePanelOpenTrigger::kAppMenu);
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();

  // Verify Immersive UI is open (and did not crash)
  views::View* overlay_view = GetImmersiveOverlay();
  ASSERT_TRUE(overlay_view);
  EXPECT_TRUE(overlay_view->GetVisible());
  EXPECT_FALSE(overlay_view->children().empty());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       UnresponsiveRenderer_ClosesImmersive) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  WaitForOverlayVisibility(true);
  AssertOverlayVisibility(true);
  content::WaitForLoadStop(GetImmersiveWebContents());

  content::SimulateUnresponsiveRenderer(
      GetImmersiveWebContents(),
      GetImmersiveWebContents()->GetPrimaryMainFrame()->GetRenderWidgetHost());

  WaitForOverlayVisibility(false);
  AssertOverlayVisibility(false);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ShowImmersive_AfterUnresponsiveRenderer_DoesNotCrash) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  WaitForOverlayVisibility(true);
  AssertOverlayVisibility(true);
  content::WebContents* starting_contents = GetImmersiveWebContents();
  content::WaitForLoadStop(starting_contents);

  content::SimulateUnresponsiveRenderer(
      starting_contents,
      starting_contents->GetPrimaryMainFrame()->GetRenderWidgetHost());
  WaitForOverlayVisibility(false);
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  WaitForOverlayVisibility(true);
  AssertOverlayVisibility(true);
  // The web contents would be the same if it was not recreated.
  EXPECT_NE(GetImmersiveWebContents(), starting_contents);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       UnresponsiveRenderer_ClosesSidePanel) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  controller->ShowSidePanelUI(SidePanelOpenTrigger::kReadAnythingOmniboxChip);
  // Wait until the side panel is showing.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  content::SimulateUnresponsiveRenderer(
      GetSidePanelWebContents(),
      GetSidePanelWebContents()->GetPrimaryMainFrame()->GetRenderWidgetHost());

  // Verify Side Panel is closed
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ShowSidePanelUI_AfterUnresponsiveRenderer_DoesNotCrash) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  controller->ShowSidePanelUI(SidePanelOpenTrigger::kReadAnythingOmniboxChip);
  // Wait until the side panel is showing.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  content::WebContents* starting_contents = GetSidePanelWebContents();

  content::SimulateUnresponsiveRenderer(
      starting_contents,
      starting_contents->GetPrimaryMainFrame()->GetRenderWidgetHost());
  // Wait until Side Panel is closed.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  controller->ShowSidePanelUI(SidePanelOpenTrigger::kReadAnythingOmniboxChip);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  // The web contents would be the same if it was not recreated.
  EXPECT_NE(GetSidePanelWebContents(), starting_contents);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       RecreateWebUIWrapper_RecreatesWebUIWrapperOnNextShow) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
  content::WebContents* starting_contents = GetImmersiveWebContents();

  controller->CloseImmersiveUI();
  controller->RecreateWebUIWrapper();
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();

  // The web contents would be the same if it was not recreated.
  EXPECT_NE(GetImmersiveWebContents(), starting_contents);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ShowImmersiveUI_ClosesSidePanel) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();

  // Open Side Panel
  controller->ShowSidePanelUI(SidePanelOpenTrigger::kAppMenu);

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

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ShowSidePanelUI_ClosesImmersiveUI) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();

  // Open Immersive UI
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInImmersiveOverlay);

  // Get the WebUI from the immersive overlay
  content::WebContents* immersive_ui_web_contents = GetImmersiveWebContents();
  ASSERT_TRUE(immersive_ui_web_contents);

  // Open Side Panel
  controller->ShowSidePanelUI(SidePanelOpenTrigger::kAppMenu);

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

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ToggleReadAnythingSidePanel_ClosesImmersiveUI) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();

  // Open Immersive UI
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
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

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerBrowserTest,
    DetachAndAttachToNewWindow_PreservesWebUI_AndTabSwitchObserver) {
  // 1. Open IRM in initial window
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
  content::WebContents* initial_web_contents = GetImmersiveWebContents();
  ASSERT_TRUE(initial_web_contents);

  // 2. Create new window
  Browser* new_browser = CreateBrowser(browser()->profile());

  // 3. Detach tab and attach to new window
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser()->tab_strip_model()->DetachTabAtForInsertion(0);
  new_browser->tab_strip_model()->AppendTab(std::move(detached_tab), true);

  // 4. Open IRM in new window
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  // 5. Verify same WebContents
  content::WebContents* new_web_contents = GetImmersiveWebContents(new_browser);
  EXPECT_EQ(initial_web_contents, new_web_contents);

  // 6. Open new tab in new window and verify IRM closes, confirming that we're
  // still tracking tab switches in the new window.
  chrome::AddTabAt(new_browser, GURL("about:blank"), -1, true);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInactive);
  EXPECT_FALSE(GetImmersiveWebContents(new_browser));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ToggleUI_OpensImmersive) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Toggle UI (open)
  controller->ToggleUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();

  // Verify Immersive UI is open
  AssertOverlayVisibility(/*visible=*/true);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInImmersiveOverlay);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ToggleUI_ClosesImmersive) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Show Immersive UI
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  // Toggle UI (close)
  controller->ToggleUI(ReadAnythingOpenTrigger::kOmniboxChip);

  // Verify Immersive UI is closed
  AssertOverlayVisibility(/*visible=*/false);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInactive);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ToggleUI_ClosesSidePanel) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();

  // Open Side Panel
  controller->ShowSidePanelUI(SidePanelOpenTrigger::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  // Toggle UI (with side panel open)
  controller->ToggleUI(ReadAnythingOpenTrigger::kOmniboxChip);

  // Verify Side Panel is closed
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInactive);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       TogglePresentation_FromImmersive_OpensSidePanel) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();

  // Open Immersive UI
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
  AssertOverlayVisibility(/*visible=*/true);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInImmersiveOverlay);

  // Toggle Presentation
  controller->TogglePresentation();

  // Verify Immersive UI is closed and Side Panel is open
  AssertOverlayVisibility(/*visible=*/false);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInSidePanel);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerBrowserTest,
    DISABLED_TogglePresentation_FromSidePanel_OpensImmersive) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();

  // Open Side Panel
  controller->ShowSidePanelUI(SidePanelOpenTrigger::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInSidePanel);

  // Toggle Presentation
  controller->TogglePresentation();

  // Verify Side Panel is closed and Immersive UI is open
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  AssertOverlayVisibility(/*visible=*/true);
  EXPECT_EQ(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInImmersiveOverlay);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       TogglePresentation_WhenClosed_DoesNothing) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();

  // Ensure initially closed
  AssertOverlayVisibility(/*visible=*/false);
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));
  EXPECT_NE(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInImmersiveOverlay);
  EXPECT_NE(controller->GetPresentationState(),
            ReadAnythingController::PresentationState::kInSidePanel);

  // Toggle Presentation
  controller->TogglePresentation();

  // Verify still closed
  AssertOverlayVisibility(/*visible=*/false);
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ShowImmersiveUI_SetsMainPageAccessibility) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* contents_view =
      browser_view->GetContentsContainerViewFor(tab->GetContents())
          ->GetViewByID(VIEW_ID_TAB_CONTAINER);
  ASSERT_TRUE(contents_view);

  // 1) Before IRM is open, main webpage is accessible to accessibility
  // technology and keyboard focus.
  ASSERT_FALSE(contents_view->GetViewAccessibility().GetIsIgnored());
  ASSERT_TRUE(contents_view->GetViewAccessibility().IsAccessibilityFocusable());
  EXPECT_TRUE(contents_view->IsFocusable());

  // 2) Show Immersive UI
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
  AssertOverlayVisibility(/*visible=*/true);

  // Main webpage is NOT accessible to accessibility technology or keyboard
  // focus while IRM is open.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return contents_view->GetViewAccessibility().GetIsIgnored(); }));
  ASSERT_FALSE(
      contents_view->GetViewAccessibility().IsAccessibilityFocusable());
  ASSERT_FALSE(contents_view->IsFocusable());

  // 3) Close Immersive UI
  controller->CloseImmersiveUI();
  AssertOverlayVisibility(/*visible=*/false);

  // Main webpage is accessible again
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !contents_view->GetViewAccessibility().GetIsIgnored(); }));
  ASSERT_TRUE(contents_view->GetViewAccessibility().IsAccessibilityFocusable());
  ASSERT_TRUE(contents_view->IsFocusable());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       OnEntryShownAndHidden_NotifiesService) {
  auto* service = static_cast<MockReadAnythingService*>(
      ReadAnythingServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser()->profile(),
          base::BindRepeating([](content::BrowserContext* context)
                                  -> std::unique_ptr<KeyedService> {
            return std::make_unique<MockReadAnythingService>(
                Profile::FromBrowserContext(context));
          })));

  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Expect service to be notified of show.
  EXPECT_CALL(*service, OnReadAnythingShown()).Times(1);
  controller->OnEntryShown(ReadAnythingOpenTrigger::kOmniboxChip);
  testing::Mock::VerifyAndClearExpectations(service);

  // Expect service to be notified of hide.
  EXPECT_CALL(*service, OnReadAnythingHidden()).Times(1);
  controller->OnEntryHidden();
  testing::Mock::VerifyAndClearExpectations(service);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       ShowImmersiveUI_OverlayIsVisibleAfterWebUIShown) {
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
  EXPECT_FALSE(overlay_view->GetVisible());
  EXPECT_FALSE(overlay_view->children().empty());

  EmitWebUIShowEvent(overlay_view);
  ASSERT_TRUE(overlay_view->GetVisible());
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerBrowserTest,
    OnDistillationStateChanged_EmptyContentInImmersive_TogglesToSidePanel) {
  base::HistogramTester histogram_tester;
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  controller->UnlockDistillationStateForTesting();
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();

  // Show Immersive UI.
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
  AssertOverlayVisibility(/*visible=*/true);

  // Distillation returns empty content.
  controller->OnDistillationStateChanged(
      ReadAnythingController::DistillationState::kDistillationEmpty);

  // Verify Immersive UI is closed and Side Panel is open.
  AssertOverlayVisibility(/*visible=*/false);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.SidePanelTriggeredByEmptyState",
      ReadAnythingOpenTrigger::kReadAnythingTogglePresentationButton, 1);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerBrowserTest,
    OnDistillationStateChanged_WithContentInImmersive_StaysImmersive) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  controller->UnlockDistillationStateForTesting();
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();

  // Show Immersive UI.
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
  AssertOverlayVisibility(/*visible=*/true);

  // Distillation returns content.
  controller->OnDistillationStateChanged(
      ReadAnythingController::DistillationState::kDistillationWithContent);

  // Verify Immersive UI is still open and Side Panel is not.
  AssertOverlayVisibility(/*visible=*/true);
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerBrowserTest,
    OnDistillationStateChanged_EmptyInSidePanel_StaysInSidePanel) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  controller->UnlockDistillationStateForTesting();
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();

  // Show Side Panel UI.
  controller->ShowSidePanelUI(SidePanelOpenTrigger::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  AssertOverlayVisibility(/*visible=*/false);

  // Distillation returns empty content.
  controller->OnDistillationStateChanged(
      ReadAnythingController::DistillationState::kDistillationEmpty);

  // Verify Side Panel is still open and Immersive UI is not.
  ASSERT_TRUE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));
  AssertOverlayVisibility(/*visible=*/false);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerBrowserTest,
    OnDistillationStateChanged_OpenWithDistillationEmpty_OpensInSidePanel) {
  base::HistogramTester histogram_tester;
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);
  controller->UnlockDistillationStateForTesting();
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();

  // Start with reading mode closed.
  AssertOverlayVisibility(/*visible=*/false);
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));

  // Set Reading Mode presentation state to inactive to represent it was
  // previously open and is now closed.
  controller->SetPresentationState(
      read_anything::mojom::ReadAnythingPresentationState::kInactive);

  // Distillation returns empty content.
  controller->OnDistillationStateChanged(
      ReadAnythingController::DistillationState::kDistillationEmpty);

  // Try to open immersive UI.
  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  // Verify Immersive UI is closed and Side Panel is open instead.
  AssertOverlayVisibility(/*visible=*/false);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.SidePanelTriggeredByEmptyState",
      ReadAnythingOpenTrigger::kOmniboxChip, 1);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerBrowserTest,
    HandleKeyboardEvent_WhenFullscreenInImmersiveMode_EscapeClosesFullscreen) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  controller->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
  views::View* overlay_view = GetImmersiveOverlay();
  AssertOverlayVisibility(/*visible=*/true);

  // Get the ReadAnythingImmersiveWebView
  ASSERT_FALSE(overlay_view->children().empty());
  ReadAnythingImmersiveWebView* web_view =
      static_cast<ReadAnythingImmersiveWebView*>(overlay_view->children()[0]);

  // Put the browser in fullscreen mode
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ASSERT_TRUE(browser()->window()->IsFullscreen());

  // Create an event that holds down the escape button
  input::NativeWebKeyboardEvent escape_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now());
  escape_event.windows_key_code = ui::VKEY_ESCAPE;
  web_view->HandleKeyboardEvent(web_view->GetWebContents(), escape_event);

  // Wait for the "press and hold" timer (1.5s) to trigger the exit.
  // We use a FullscreenWaiter to wait for the state change.
  ui_test_utils::FullscreenWaiter waiter(browser(),
                                         {.browser_fullscreen = false});
  waiter.Wait();

  // Verify the browser has exited fullscreen.
  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       AddTabToSplitView_IrmStaysOnSourceTab) {
  // Setup tabs A and B
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->count());
  tabs::TabInterface* tab_a = tab_strip_model->GetActiveTab();
  ASSERT_TRUE(tab_a);
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  // Tab B is active (newly added tab)
  ASSERT_EQ(2, tab_strip_model->count());
  ASSERT_EQ(1, tab_strip_model->active_index());
  content::WaitForLoadStop(tab_strip_model->GetWebContentsAt(1));
  tabs::TabInterface* tab_b = tab_strip_model->GetActiveTab();
  ReadAnythingController* ra_controller_tab_b =
      ReadAnythingController::From(tab_b);
  ASSERT_TRUE(ra_controller_tab_b);

  // Open IRM on Tab B.
  ra_controller_tab_b->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();

  // Verify IRM is shown on Tab B.
  AssertOverlayVisibility(/*visible=*/true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return ra_controller_tab_b->has_shown_ui(); }));

  // Add Tab A to split view with current window (Tab B).
  ASSERT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandAddToSplit));
  tab_strip_model->ExecuteContextMenuCommand(0,
                                             TabStripModel::CommandAddToSplit);

  // Verify IRM is still shown on Tab B.
  ASSERT_TRUE(GetImmersiveOverlayForTab(1)->GetVisible());

  // Verify IRM is not shown on Tab A.
  ASSERT_FALSE(GetImmersiveOverlayForTab(0)->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       AddTabsToSplitView_IrmStaysOnBothTabs) {
  // Setup tab A and get the RAController
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->count());
  tabs::TabInterface* tab_a = tab_strip_model->GetActiveTab();
  ReadAnythingController* ra_controller_tab_a =
      ReadAnythingController::From(tab_a);
  ASSERT_TRUE(ra_controller_tab_a);
  ASSERT_EQ(1, tab_strip_model->count());
  ASSERT_EQ(0, tab_strip_model->active_index());

  // Open IRM on Tab A.
  ra_controller_tab_a->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
  // Verify IRM is shown.
  AssertOverlayVisibility(/*visible=*/true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return ra_controller_tab_a->has_shown_ui(); }));

  // Setup tab B and get the RAController
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  // Tab B is active
  ASSERT_EQ(2, tab_strip_model->count());
  ASSERT_EQ(1, tab_strip_model->active_index());
  content::WaitForLoadStop(tab_strip_model->GetWebContentsAt(1));

  // Confirm overlay is closed due to tab switch.
  AssertOverlayVisibility(/*visible=*/false);
  tabs::TabInterface* tab_b = tab_strip_model->GetActiveTab();
  ReadAnythingController* ra_controller_tab_b =
      ReadAnythingController::From(tab_b);
  ASSERT_TRUE(ra_controller_tab_b);

  // Open IRM on Tab B.
  ra_controller_tab_b->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();

  // Verify IRM is shown.
  AssertOverlayVisibility(/*visible=*/true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return ra_controller_tab_b->has_shown_ui(); }));

  // Add Tab A (index 0) to split view with current window (Tab B).
  // This simulates right-clicking Tab A and selecting "Add to split view".
  ASSERT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandAddToSplit));
  tab_strip_model->ExecuteContextMenuCommand(0,
                                             TabStripModel::CommandAddToSplit);
  // Verify split creation
  std::optional<split_tabs::SplitTabId> split_id =
      tab_strip_model->GetSplitForTab(1);
  ASSERT_TRUE(split_id.has_value());
  EXPECT_EQ(split_id, tab_strip_model->GetSplitForTab(0));

  // Verify IRM is visible on both tabs
  views::View* overlay_view0 = GetImmersiveOverlayForTab(0);
  ASSERT_TRUE(overlay_view0);
  ASSERT_TRUE(overlay_view0->GetVisible());

  views::View* overlay_view1 = GetImmersiveOverlayForTab(1);
  ASSERT_TRUE(overlay_view1);
  ASSERT_TRUE(overlay_view1->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       OpenIrmInSplitView_ShowsOnActiveSide) {
  // Setup split view
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  content::WaitForLoadStop(tab_strip_model->GetWebContentsAt(1));
  ASSERT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandAddToSplit));
  tab_strip_model->ExecuteContextMenuCommand(0,
                                             TabStripModel::CommandAddToSplit);
  ASSERT_EQ(1, tab_strip_model->active_index());

  // Open IRM on active tab (Tab B)
  tabs::TabInterface* tab_b = tab_strip_model->GetActiveTab();
  auto* controller_b = ReadAnythingController::From(tab_b);
  controller_b->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent(GetImmersiveOverlayForTab(1));
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return controller_b->has_shown_ui(); }));

  // Verify IRM on Tab B
  views::View* overlay_view1 = GetImmersiveOverlayForTab(1);
  ASSERT_TRUE(overlay_view1);
  ASSERT_TRUE(overlay_view1->GetVisible());

  // Verify no IRM on Tab A
  views::View* overlay_view0 = GetImmersiveOverlayForTab(0);
  ASSERT_TRUE(overlay_view0);
  ASSERT_FALSE(overlay_view0->GetVisible());

  // Activate Tab A
  tab_strip_model->ActivateTabAt(0);

  // Open IRM on active tab (Tab A)
  tabs::TabInterface* tab_a = tab_strip_model->GetActiveTab();
  auto* controller_a = ReadAnythingController::From(tab_a);
  controller_a->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent(GetImmersiveOverlayForTab(0));
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return controller_a->has_shown_ui(); }));

  // Verify IRM on Tab A
  overlay_view0 = GetImmersiveOverlayForTab(0);
  ASSERT_TRUE(overlay_view0);
  ASSERT_TRUE(overlay_view0->GetVisible());

  // Verify IRM still on Tab B (it shouldn't close just because we activated
  // another tab in the split group, unlike normal tab switching)
  overlay_view1 = GetImmersiveOverlayForTab(1);
  ASSERT_TRUE(overlay_view1);
  ASSERT_TRUE(overlay_view1->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       CloseIrmInSplitView_ClosesOnActiveSide) {
  // Setup split view with IRM open on both sides
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* tab_a = tab_strip_model->GetActiveTab();
  auto* controller_a = ReadAnythingController::From(tab_a);
  controller_a->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
  AssertOverlayVisibility(/*visible=*/true);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return controller_a->has_shown_ui(); }));

  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  content::WaitForLoadStop(tab_strip_model->GetWebContentsAt(1));
  tabs::TabInterface* tab_b = tab_strip_model->GetActiveTab();
  auto* controller_b = ReadAnythingController::From(tab_b);
  controller_b->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
  AssertOverlayVisibility(/*visible=*/true);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return controller_b->has_shown_ui(); }));

  ASSERT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandAddToSplit));
  tab_strip_model->ExecuteContextMenuCommand(0,
                                             TabStripModel::CommandAddToSplit);

  // Both should be visible
  ASSERT_TRUE(GetImmersiveOverlayForTab(0)->GetVisible());
  ASSERT_TRUE(GetImmersiveOverlayForTab(1)->GetVisible());

  // Close IRM on active tab (Tab B)
  ReadAnythingController::From(tab_strip_model->GetActiveTab())
      ->CloseImmersiveUI();

  // Verify Tab B IRM closed
  ASSERT_FALSE(GetImmersiveOverlayForTab(1)->GetVisible());

  // Verify Tab A IRM still open
  ASSERT_TRUE(GetImmersiveOverlayForTab(0)->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       CloseTabWithIrmInSplitView_ClosesIrm) {
  // Setup 2 tabs
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->count());
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  // Tab B is active
  ASSERT_EQ(2, tab_strip_model->count());
  ASSERT_EQ(1, tab_strip_model->active_index());

  tabs::TabInterface* tab_b = tab_strip_model->GetActiveTab();

  ReadAnythingController* ra_controller_tab_b =
      ReadAnythingController::From(tab_b);
  ASSERT_TRUE(ra_controller_tab_b);

  // Open IRM on Tab B.
  ra_controller_tab_b->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();

  // Verify IRM is shown.
  AssertOverlayVisibility(/*visible=*/true);

  // Add Tab A (index 0) to split view with current window (Tab B).
  // This simulates right-clicking Tab A and selecting "Add to split view".
  EXPECT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandAddToSplit));
  tab_strip_model->ExecuteContextMenuCommand(0,
                                             TabStripModel::CommandAddToSplit);
  // Verify split creation
  std::optional<split_tabs::SplitTabId> split_id =
      tab_strip_model->GetSplitForTab(1);
  EXPECT_TRUE(split_id.has_value());

  EXPECT_EQ(split_id, tab_strip_model->GetSplitForTab(0));

  // Verify that closing Tab B (the tab with IRM) does not crash
  tab_strip_model->CloseWebContentsAt(1, TabCloseTypes::CLOSE_USER_GESTURE);

  // Verify Tab B is closed and we only have Tab A left.
  ASSERT_EQ(1, tab_strip_model->count());
  ASSERT_EQ(0, tab_strip_model->active_index());

  // Verify IRM is not shown on the remaining tab (Tab A).
  views::View* overlay_view = GetImmersiveOverlayForTab(0);
  ASSERT_TRUE(overlay_view);
  ASSERT_FALSE(overlay_view->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       FocusInactiveIrmInSplitView_ActivatesTab) {
  // Setup Tab A and open IRM
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->count());
  tabs::TabInterface* tab_a = tab_strip_model->GetActiveTab();
  ReadAnythingController* controller_a = ReadAnythingController::From(tab_a);
  ASSERT_TRUE(controller_a);

  controller_a->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EmitWebUIShowEvent();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return controller_a->has_shown_ui(); }));

  // Add Tab B (becomes active)
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  ASSERT_EQ(2, tab_strip_model->count());
  ASSERT_EQ(1, tab_strip_model->active_index());

  // Add Tab A to split view with current window (Tab B).
  ASSERT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandAddToSplit));
  tab_strip_model->ExecuteContextMenuCommand(0,
                                             TabStripModel::CommandAddToSplit);

  // Verify Tab A is not active.
  ASSERT_NE(0, tab_strip_model->active_index());

  // Verify IRM on Tab A is visible (even though it's inactive)
  views::View* overlay_view_a = GetImmersiveOverlayForTab(0);
  ASSERT_TRUE(overlay_view_a);
  ASSERT_TRUE(overlay_view_a->GetVisible());

  // Simulate focus on the IRM WebView of Tab A.
  auto* web_view = static_cast<views::WebView*>(overlay_view_a->children()[0]);
  // Trigger the focus callback via RequestFocus.
  web_view->RequestFocus();

  // Verify Tab A becomes active.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_strip_model->active_index() == 0; }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingControllerBrowserTest,
                       GlueAttachedAndDetachedCorrectly) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Open Side Panel
  controller->ShowSidePanelUI(SidePanelOpenTrigger::kAppMenu);

  // Wait for Side Panel to show
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  content::WebContents* side_panel_contents = GetSidePanelWebContents();
  ASSERT_TRUE(side_panel_contents);

  // Verify Glue is attached
  EXPECT_TRUE(ReadAnythingControllerGlue::FromWebContents(side_panel_contents));
  EXPECT_EQ(controller,
            ReadAnythingControllerGlue::FromWebContents(side_panel_contents)
                ->controller());

  // Close the tab. This destroys ReadAnythingController.
  // We expect no crashes here.
  tab->Close();
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerBrowserTest,
    CloseBackgroundTabWithSidePanelOpenOnForegroundTab_DoesNotCrash) {
  // 1. Get the active tab (Tab A) and controller.
  tabs::TabInterface* tab_a = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab_a);
  auto* controller_a = ReadAnythingController::From(tab_a);
  ASSERT_TRUE(controller_a);

  // 2. Open the Reading Mode Side Panel on Tab A.
  controller_a->ShowSidePanelUI(SidePanelOpenTrigger::kAppMenu);

  // 3. Wait for the side panel to be fully visible.
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));

  // 4. Create a new tab (Tab B) in the background.
  chrome::AddTabAt(browser(), GURL("about:blank"), /*index=*/1,
                   /*foreground=*/false);
  tabs::TabInterface* tab_b = browser()->tab_strip_model()->GetTabAtIndex(1);
  ASSERT_TRUE(tab_b);
  ASSERT_NE(tab_a, tab_b);

  // 5. Close Tab B (the background tab).
  // This triggers a scenario that previously crashed (where Tab B deregistered
  // its entry, but the SidePanelCoordinator sees the side panel is open for Tab
  // A and tries to close it, iterating over all tabs including the one being
  // destroyed).
  tab_b->Close();
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingControllerBrowserTest,
    OnDiscardContents_InternalControllersTrackNewWebContents) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  // Initial state check
  content::WebContents* old_contents = tab->GetContents();
  auto* old_side_panel_ptr = controller->GetSidePanelControllerForTesting();
  ASSERT_EQ(old_side_panel_ptr->web_contents(), old_contents);

  // Create and discard with new contents
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  content::WebContents* new_contents_ptr = new_contents.get();

  browser()->tab_strip_model()->DiscardWebContentsAt(0,
                                                     std::move(new_contents));

  // Verify new controller is observing the new contents
  EXPECT_EQ(controller->GetSidePanelControllerForTesting()->web_contents(),
            new_contents_ptr);

  // Verify that the new contents can be navigated without crashing the
  // controllers
  ASSERT_TRUE(content::NavigateToURL(new_contents_ptr, GURL("about:blank")));
}
