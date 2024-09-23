// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/compositor/compositor.h"
#include "ui/views/widget/widget_observer.h"

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

  bool DidRunLoopComplete() { return run_loop_.AnyQuitCalled(); }

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

    if (widget) {
      ObserveWidget(widget);
    }
  }

 private:
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observer_{this};
  base::ScopedObservation<WebUIBubbleManager, WebUIBubbleManagerObserver>
      webui_bubble_manager_observer_{this};
  raw_ptr<WebUIBubbleManager> webui_bubble_manager_ = nullptr;
  base::RunLoop run_loop_;
};

class TabSearchBubbleUIBrowserTest : public DialogBrowserTest {
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
    BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser());
    TabSearchBubbleHost* host = view->GetTabSearchBubbleHost();
    WebUIChangeObserver webui_change_observer =
        WebUIChangeObserver(host->webui_bubble_manager_for_testing());
    view->CreateTabSearchBubble();
    // Do not wait if the dialog has already been painted. This can happen on
    // very fast runs.
    if (!webui_change_observer.DidRunLoopComplete()) {
      webui_change_observer.Wait();
    }
  }

 private:
#if BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kSkiaFontService};
#endif
};

IN_PROC_BROWSER_TEST_F(TabSearchBubbleUIBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
