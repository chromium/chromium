// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_search_bubble_host.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"
#endif

class TabSearchBubbleHostBrowserTest : public InProcessBrowserTest {
 public:
  TabSearchBubbleHostBrowserTest() {
    feature_list_.InitWithFeatures({features::kTabstripDeclutter}, {});
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabSearchBubbleHost* tab_search_bubble_host() {
    return browser_view()->GetTabSearchBubbleHost();
  }

  WebUIBubbleManager* bubble_manager() {
    return tab_search_bubble_host()->webui_bubble_manager_for_testing();
  }

  void RunUntilBubbleWidgetDestroyed() {
    ASSERT_NE(nullptr, bubble_manager()->GetBubbleWidget());
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabSearchBubbleHostBrowserTest,
                       BubbleShowTimerTriggersCorrectly) {
  ASSERT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  tab_search_bubble_host()->ShowTabSearchBubble();

  // |bubble_created_time_| should be set as soon as the bubble widget is
  // created.
  EXPECT_FALSE(bubble_manager()->GetBubbleWidget()->IsVisible());
  EXPECT_TRUE(tab_search_bubble_host()->bubble_created_time_for_testing());

  // Showing the bubble should reset the timestamp.
  bubble_manager()->bubble_view_for_testing()->ShowUI();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsVisible());
  EXPECT_FALSE(tab_search_bubble_host()->bubble_created_time_for_testing());

  tab_search_bubble_host()->CloseTabSearchBubble();
  RunUntilBubbleWidgetDestroyed();
}

IN_PROC_BROWSER_TEST_F(TabSearchBubbleHostBrowserTest,
                       TabDeclutterControllerAttachedToTabSearchUIOnShow) {
  ASSERT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  tab_search_bubble_host()->ShowTabSearchBubble();
  content::WebUI* web_ui =
      bubble_manager()->GetContentsWrapper()->web_contents()->GetWebUI();
  EXPECT_EQ(
      web_ui->GetController()->GetAs<TabSearchUI>()->tab_declutter_controller(),
      browser()->GetFeatures().tab_declutter_controller());
}

IN_PROC_BROWSER_TEST_F(TabSearchBubbleHostBrowserTest,
                       BubbleShowCorrectlyInFullscreen) {
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  gfx::Rect rect(20, 4, 0, 0);
  bubble_manager()->ShowBubble(rect);

  bubble_manager()->bubble_view_for_testing()->ShowUI();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsVisible());

  gfx::Rect bound =
      bubble_manager()->bubble_view_for_testing()->GetAnchorRect();
  EXPECT_EQ(bound, rect);

  tab_search_bubble_host()->CloseTabSearchBubble();
  RunUntilBubbleWidgetDestroyed();
}

// On macOS, most accelerators are handled by CommandDispatcher.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(TabSearchBubbleHostBrowserTest,
                       KeyboardShortcutTriggersBubble) {
  ASSERT_EQ(nullptr, bubble_manager()->GetBubbleWidget());

  auto accelerator = ui::Accelerator(
      ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);
  browser_view()->AcceleratorPressed(accelerator);

  // Accelerator keys should have created the tab search bubble.
  ASSERT_NE(nullptr, bubble_manager()->GetBubbleWidget());

  tab_search_bubble_host()->CloseTabSearchBubble();
  ASSERT_TRUE(bubble_manager()->GetBubbleWidget()->IsClosed());

  RunUntilBubbleWidgetDestroyed();
}
#endif

class FullscreenTabSearchBubbleDialogTest : public DialogBrowserTest {
 public:
  FullscreenTabSearchBubbleDialogTest() {
#if BUILDFLAG(IS_CHROMEOS)
    chromeos::MultitaskMenuNudgeController::SetSuppressNudgeForTesting(true);
#endif
  }

  FullscreenTabSearchBubbleDialogTest(
      const FullscreenTabSearchBubbleDialogTest&) = delete;
  FullscreenTabSearchBubbleDialogTest& operator=(
      const FullscreenTabSearchBubbleDialogTest&) = delete;

  void ShowUi(const std::string& name) override {
    ui_test_utils::ToggleFullscreenModeAndWait(browser());
    BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser());
    view->CreateTabSearchBubble();
  }
};

IN_PROC_BROWSER_TEST_F(FullscreenTabSearchBubbleDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
