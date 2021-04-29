// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/windows_version.h"
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
        &TooltipWidgetMonitor::OnTooltipShown, base::Unretained(this)));
    observer_->set_closing_callback(base::BindRepeating(
        &TooltipWidgetMonitor::OnTooltipClosed, base::Unretained(this)));
  }
  TooltipWidgetMonitor(const TooltipWidgetMonitor&) = delete;
  TooltipWidgetMonitor& operator=(const TooltipWidgetMonitor&) = delete;
  ~TooltipWidgetMonitor() = default;

  bool IsWidgetActive() const { return active_widget_; }

  void WaitUntilTooltipShown() {
    if (!active_widget_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
  }

  void WaitUntilTooltipClosed() {
    if (active_widget_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
  }

  void OnTooltipShown(views::Widget* widget) {
    if (widget->GetName() == TooltipAura::kWidgetName) {
      active_widget_ = widget;
      if (run_loop_ && run_loop_->running())
        run_loop_->Quit();
    }
  }

  void OnTooltipClosed(views::Widget* widget) {
    if (active_widget_ == widget) {
      active_widget_ = nullptr;
      if (run_loop_ && run_loop_->running())
        run_loop_->Quit();
    }
  }

 private:
  // This attribute will help us avoid starting the |run_loop_| when the widget
  // is already shown. Otherwise, we would be waiting infinitely for an event
  // that already occurred.
  views::Widget* active_widget_ = nullptr;
  std::unique_ptr<views::AnyWidgetObserver> observer_;
  std::unique_ptr<base::RunLoop> run_loop_;
};  // class TooltipWidgetMonitor

// Browser tests for tooltips on platforms that use TooltipAura to display the
// tooltip.
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

  bool SkipTestForOldWinVersion() const {
#if defined(OS_WIN)
    // On older Windows version, tooltips are displayed with TooltipWin instead
    // of TooltipAura. For TooltipAura, a tooltip is displayed using a Widget
    // and a Label and for TooltipWin, it is displayed using a native win32
    // control. Since the observer we use in this class is the
    // AnyWidgetObserver, we don't receive any update from non-Widget tooltips.
    // This doesn't mean that no tooltip is displayed on older platforms, but
    // that we are unable to execute the browser test successfully because the
    // tooltip displayed is not displayed using a Widget.
    //
    // For now, we can simply skip the tests on older platforms, but it might be
    // a good idea to eventually implement a custom observer (e.g.,
    // TooltipStateObserver) that would work for both TooltipAura and
    // TooltipWin, or remove once and for all TooltipWin. For more information
    // on why we still need TooltipWin on Win7, see https://crbug.com/1201440.
    if (base::win::GetVersion() <= base::win::Version::WIN7)
      return true;
#endif  // defined(OS_WIN)
    return false;
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
  if (SkipTestForOldWinVersion())
    return;

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

IN_PROC_BROWSER_TEST_F(TooltipBrowserTest, HideTooltipOnKeyPress) {
  if (SkipTestForOldWinVersion())
    return;

  NavigateToURL("/tooltip.html");
  std::u16string expected_text = u"my tooltip";

  // First, trigger the tooltip from the cursor.
  gfx::Point position = WebContentPositionToScreenCoordinate(10, 10);
  event_generator()->MoveMouseTo(position);
  tooltip_monitor()->WaitUntilTooltipShown();
  EXPECT_TRUE(helper()->IsTooltipVisible());
  EXPECT_EQ(expected_text, helper()->GetTooltipText());

  // Second, send a key press event to test whether the tooltip gets hidden.
  EXPECT_TRUE(tooltip_monitor()->IsWidgetActive());
  event_generator()->PressKey(ui::VKEY_A, 0);
  tooltip_monitor()->WaitUntilTooltipClosed();
  EXPECT_FALSE(helper()->IsTooltipVisible());

  // TODO(bebeaudr): Also try it with a keyboard triggered tooltip.
}
