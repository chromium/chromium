// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_search_bubble_host.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"
#endif

class TabSearchBubbleHostBrowserTest : public InProcessBrowserTest {
 public:
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

class WebUIChangeObserver : public views::WidgetObserver,
                            public WebUIBubbleManagerObserver {
 public:
  explicit WebUIChangeObserver(WebUIBubbleManager* webui_bubble_manager)
      : webui_bubble_manager_(webui_bubble_manager) {
    webui_bubble_manager_observer_.Observe(webui_bubble_manager);
  }
  WebUIChangeObserver(const WebUIChangeObserver&) = delete;
  WebUIChangeObserver& operator=(const WebUIChangeObserver&) = delete;
  ~WebUIChangeObserver() override = default;

  void Wait() { run_loop_.Run(); }

  void ObserveWidget(views::Widget* widget) {
    widget_observer_.Observe(widget);
  }

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override {
    if (visible) {
      widget->GetCompositor()->RequestSuccessfulPresentationTimeForNextFrame(
          base::BindOnce(
              [](base::RunLoop* run_loop,
                 const viz::FrameTimingDetails& frame_timing_details) {
                run_loop->Quit();
              },
              &run_loop_));
    }
  }

  // WebUIBubbleManagerObserver:
  void BeforeBubbleWidgetShowed(views::Widget* widget) override {
    // In the TabSearch UI, 'last_active_elapsed_text' records the time ticks
    // since the tab was active. This causes issues with pixel tests since the
    // string is often different depending on the run.
    //
    // Set the last active elapsed text to "0" to be consistent.
    TabSearchUI* tab_search_ui =
        webui_bubble_manager_->bubble_view_for_testing()
            ->get_contents_wrapper_for_testing()
            ->web_contents()
            ->GetWebUI()
            ->GetController()
            ->template GetAs<TabSearchUI>();

    tab_search_ui->set_page_handler_creation_callback_for_testing(
        base::BindOnce(
            [](TabSearchUI* tab_search_ui) {
              tab_search_ui->page_handler_for_testing()
                  ->disable_last_active_elapsed_text_for_testing();
            },
            tab_search_ui));
  }

 private:
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observer_{this};
  base::ScopedObservation<WebUIBubbleManager, WebUIBubbleManagerObserver>
      webui_bubble_manager_observer_{this};

  raw_ptr<WebUIBubbleManager> webui_bubble_manager_ = nullptr;
  base::RunLoop run_loop_;
};

class TabSearchBubbleHostUIBrowserTest : public DialogBrowserTest {
 public:
  // Launching TabSearch is an async event. To capture the dialog for a pixel
  // test, this requires a couple of observers + callbacks to get the timing
  // right.
  //
  // There are 2 observers:
  // 1) WidgetObserver will wait until the tab search widget has been painted.
  // 2) WebUIBubbleManagerObserver will wait until the tab search has been
  // created (but before it's painted) to modify the UI data slightly such that
  // the tab search pixel test is consistent.
  void ShowUi(const std::string& name) override {
    base::RunLoop run_loop;
    BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser());
    TabSearchBubbleHost* host = view->GetTabSearchBubbleHost();
    WebUIChangeObserver webui_change_observer =
        WebUIChangeObserver(host->webui_bubble_manager_for_testing());
    view->CreateTabSearchBubble();
    views::Widget* widget = view->GetTabSearchBubbleHost()
                                ->webui_bubble_manager_for_testing()
                                ->GetBubbleWidget();

    if (widget) {
      webui_change_observer.ObserveWidget(widget);
      webui_change_observer.Wait();
    }
  }
};

IN_PROC_BROWSER_TEST_F(TabSearchBubbleHostUIBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
