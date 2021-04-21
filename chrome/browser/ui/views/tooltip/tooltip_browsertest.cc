// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/corewm/tooltip_controller_test_helper.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

using content::RenderWidgetHostView;
using content::WebContents;
using views::corewm::TooltipAura;
using views::corewm::TooltipController;
using views::corewm::TooltipStateManager;
using views::corewm::test::TooltipControllerTestHelper;

class TooltipWidgetMonitor {
 public:
  TooltipWidgetMonitor() {
    observer_ = std::make_unique<views::AnyWidgetObserver>(
        views::test::AnyWidgetTestPasskey());
    observer_->set_shown_callback(base::BindRepeating(
        &TooltipWidgetMonitor::OnWidgetShown, base::Unretained(this)));
  }
  TooltipWidgetMonitor(const TooltipWidgetMonitor&) = delete;
  TooltipWidgetMonitor& operator=(const TooltipWidgetMonitor&) = delete;
  ~TooltipWidgetMonitor() = default;

  void WaitUntilTooltipShown() {
    if (!active_widget_)
      run_loop_.Run();
  }

  void OnWidgetShown(views::Widget* widget) {
    if (widget->GetName() == TooltipAura::kWidgetName) {
      active_widget_ = widget;
      if (run_loop_.running())
        run_loop_.Quit();
    }
  }

 private:
  // This attribute will help us avoid starting the |run_loop_| when the widget
  // is already shown. Otherwise, we would be waiting infinitely for an event
  // that already occurred.
  views::Widget* active_widget_ = nullptr;
  std::unique_ptr<views::AnyWidgetObserver> observer_;
  base::RunLoop run_loop_;
};  // class TooltipWidgetMonitor

class TooltipBrowserTest : public InProcessBrowserTest {
 public:
  TooltipBrowserTest() = default;
  ~TooltipBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    gfx::NativeWindow root_window =
        browser()->window()->GetNativeWindow()->GetRootWindow();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(root_window);
    helper_ = std::make_unique<TooltipControllerTestHelper>(
        static_cast<TooltipController*>(wm::GetTooltipClient(root_window)));
    tooltip_monitor_ = std::make_unique<TooltipWidgetMonitor>();
  }

 protected:
  void NavigateToURL(const std::string& relative_url) {
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("a.com", relative_url));
    rwhv_ = browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetRenderWidgetHostView();
  }

  gfx::Point WebContentPositionToScreenCoordinate(int x, int y) {
    return gfx::Point(x, y) + rwhv_->GetViewBounds().OffsetFromOrigin();
  }

  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }
  TooltipControllerTestHelper* helper() { return helper_.get(); }
  TooltipWidgetMonitor* tooltip_monitor() { return tooltip_monitor_.get(); }

 private:
  std::unique_ptr<ui::test::EventGenerator> event_generator_ = nullptr;
  RenderWidgetHostView* rwhv_ = nullptr;

  std::unique_ptr<TooltipControllerTestHelper> helper_;
  std::unique_ptr<TooltipWidgetMonitor> tooltip_monitor_ = nullptr;
};  // class TooltipBrowserTest

IN_PROC_BROWSER_TEST_F(TooltipBrowserTest, ShowTooltipFromWebContent) {
  NavigateToURL("/tooltip.html");
  std::u16string expected_text = u"my tooltip";

  // Trigger the tooltip from the cursor.
  // This will position the cursor right above a button with the title attribute
  // set and will show the tooltip.
  gfx::Point position = WebContentPositionToScreenCoordinate(10, 10);
  event_generator()->MoveMouseTo(position);
  tooltip_monitor()->WaitUntilTooltipShown();
  EXPECT_TRUE(helper()->IsTooltipVisible());
  EXPECT_EQ(expected_text, helper()->GetTooltipText());

  helper()->HideAndReset();

  // TODO(bebeaudr): Trigger a tooltip by setting focus with the keyboard on
  // that web content button.
}
