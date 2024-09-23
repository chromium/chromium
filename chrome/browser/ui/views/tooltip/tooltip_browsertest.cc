// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/corewm/tooltip_controller_test_helper.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/tooltip_observer.h"

using content::RenderFrameHost;
using content::RenderWidgetHostView;
using content::WebContents;
using views::corewm::TooltipController;
using views::corewm::test::TooltipControllerTestHelper;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class TooltipMonitor : public wm::TooltipObserver {
 public:
  TooltipMonitor() = default;
  TooltipMonitor(const TooltipMonitor&) = delete;
  TooltipMonitor& operator=(const TooltipMonitor&) = delete;
  ~TooltipMonitor() override = default;

  bool IsWidgetActive() const { return is_tooltip_active_; }

  void WaitUntilTooltipShown() {
    if (!is_tooltip_active_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
  }

  void WaitUntilTooltipClosed() {
    if (is_tooltip_active_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
  }

  void OnTooltipShown(aura::Window* window,
                      const std::u16string& text,
                      const gfx::Rect& bounds) override {
    is_tooltip_active_ = true;
    if (run_loop_ && run_loop_->running()) {
      run_loop_->Quit();
    }
  }

  void OnTooltipHidden(aura::Window* window) override {
    is_tooltip_active_ = false;
    if (run_loop_ && run_loop_->running()) {
      run_loop_->Quit();
    }
  }

 private:
  bool is_tooltip_active_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
};
#else
using views::corewm::TooltipAura;

class TooltipMonitor {
 public:
  TooltipMonitor() {
    observer_ = std::make_unique<views::AnyWidgetObserver>(
        views::test::AnyWidgetTestPasskey());
    observer_->set_shown_callback(base::BindRepeating(
        &TooltipMonitor::OnTooltipShown, base::Unretained(this)));
    observer_->set_closing_callback(base::BindRepeating(
        &TooltipMonitor::OnTooltipClosed, base::Unretained(this)));
  }
  TooltipMonitor(const TooltipMonitor&) = delete;
  TooltipMonitor& operator=(const TooltipMonitor&) = delete;
  ~TooltipMonitor() = default;

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
  raw_ptr<views::Widget> active_widget_ = nullptr;
  std::unique_ptr<views::AnyWidgetObserver> observer_;
  std::unique_ptr<base::RunLoop> run_loop_;
};  // class TooltipMonitor
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
    helper_ = std::make_unique<TooltipControllerTestHelper>(root_window);
    tooltip_monitor_ = std::make_unique<TooltipMonitor>();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    helper()->AddObserver(tooltip_monitor_.get());
#endif
  }

  content::WebContents* web_contents() { return web_contents_; }

 protected:
  void NavigateToURL(const std::string& relative_url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("a.com", relative_url)));
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    rwhv_ = web_contents_->GetRenderWidgetHostView();
    content::WaitForHitTestData(web_contents_->GetPrimaryMainFrame());
  }

  void LoadCrossSitePageIntoFrame(const std::string& relative_url,
                                  const std::string& iframe_id) {
    // This function makes the iframe in the web page point to a different
    // origin, making it an OOPIF.
    GURL frame_url(embedded_test_server()->GetURL("b.com", relative_url));
    NavigateIframeToURL(web_contents_, iframe_id, frame_url);

    // Verify that the child frame is an OOPIF with a different SiteInstance.
    RenderFrameHost* child = GetChildRenderFrameHost(0);
    EXPECT_NE(web_contents_->GetSiteInstance(), child->GetSiteInstance());
    EXPECT_TRUE(child->IsCrossProcessSubframe());
    content::WaitForHitTestData(child);
  }

  RenderFrameHost* GetChildRenderFrameHost(size_t index) {
    return ChildFrameAt(web_contents_->GetPrimaryMainFrame(), index);
  }

  gfx::Point WebContentPositionToScreenCoordinate(int x, int y) {
    return gfx::Point(x, y) + rwhv_->GetViewBounds().OffsetFromOrigin();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "KeyboardAccessibleTooltip");
    scoped_feature_list_.InitWithFeatures(
        {features::kKeyboardAccessibleTooltip}, {});
  }

  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }
  TooltipControllerTestHelper* helper() { return helper_.get(); }
  TooltipMonitor* tooltip_monitor() { return tooltip_monitor_.get(); }

 private:
  std::unique_ptr<ui::test::EventGenerator> event_generator_ = nullptr;
  raw_ptr<RenderWidgetHostView, AcrossTasksDanglingUntriaged> rwhv_ = nullptr;
  raw_ptr<WebContents, AcrossTasksDanglingUntriaged> web_contents_ = nullptr;

  std::unique_ptr<TooltipControllerTestHelper> helper_;
  std::unique_ptr<TooltipMonitor> tooltip_monitor_ = nullptr;

  base::test::ScopedFeatureList scoped_feature_list_;
};  // class TooltipBrowserTest

// TOOD(crbug.com/40768202): Flakily fails on Windows
#if BUILDFLAG(IS_WIN)
#define MAYBE_ShowTooltipFromWebContentWithCursor \
  DISABLED_ShowTooltipFromWebContentWithCursor
#else
#define MAYBE_ShowTooltipFromWebContentWithCursor \
  ShowTooltipFromWebContentWithCursor
#endif
IN_PROC_BROWSER_TEST_F(TooltipBrowserTest,
                       MAYBE_ShowTooltipFromWebContentWithCursor) {
  NavigateToURL("/tooltip.html");
  std::u16string expected_text = u"my tooltip";

  // Trigger the tooltip from the cursor.
  // This will position the cursor right above a button with the title attribute
  // set and will show the tooltip.
  gfx::Point position = WebContentPositionToScreenCoordinate(10, 10);

  event_generator()->MoveMouseTo(position);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Sends mouse move event to Ash as well to make server side tooltip work.
  EXPECT_TRUE(ui_controls::SendMouseMove(
      position.x(), position.y(), browser()->window()->GetNativeWindow()));
#endif
  tooltip_monitor()->WaitUntilTooltipShown();
  EXPECT_TRUE(helper()->IsTooltipVisible());
  EXPECT_EQ(expected_text, helper()->GetTooltipText());

  helper()->HideAndReset();
  tooltip_monitor()->WaitUntilTooltipClosed();
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// https://crbug.com/1212403. Flaky on linux-chromeos-rel and other linux bots.
// TODO(elkurin): Keyboard triggered tooltip needs command line switch on server
// side while Lacros cannot set Ash flag.
#define MAYBE_ShowTooltipFromWebContentWithKeyboard \
  DISABLED_ShowTooltipFromWebContentWithKeyboard
#else
#define MAYBE_ShowTooltipFromWebContentWithKeyboard \
  ShowTooltipFromWebContentWithKeyboard
#endif
IN_PROC_BROWSER_TEST_F(TooltipBrowserTest,
                       MAYBE_ShowTooltipFromWebContentWithKeyboard) {
  NavigateToURL("/tooltip.html");
  std::u16string expected_text = u"my tooltip";

  // Trigger the tooltip from the keyboard with a TAB keypress.
  event_generator()->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  event_generator()->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  tooltip_monitor()->WaitUntilTooltipShown();
  EXPECT_TRUE(helper()->IsTooltipVisible());
  EXPECT_EQ(expected_text, helper()->GetTooltipText());

  helper()->HideAndReset();
}

// https://crbug.com/1212403. Flaky on linux-chromeos-rel.
// https://crbug.com/1241736. Flaky on Win.
// TODO(elkurin): Keyboard triggered tooltip needs command line switch on server
// side while Lacros cannot set Ash flag.
IN_PROC_BROWSER_TEST_F(TooltipBrowserTest,
                       DISABLED_ShowTooltipFromIFrameWithKeyboard) {
  // There are two tooltips in this file: one above the iframe and one inside
  // the iframe.
  NavigateToURL("/tooltip_in_iframe.html");
  std::u16string expected_text = u"my tooltip";

  // Make the iframe cross-origin.
  LoadCrossSitePageIntoFrame("/tooltip.html", "iframe1");

  // Move the focus to the button outside of the iframe to get its position.
  // We'll use it in the next step to validate that the tooltip associated with
  // the button inside of the iframe is positioned, well, inside the iframe.
  event_generator()->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  event_generator()->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  tooltip_monitor()->WaitUntilTooltipShown();
  EXPECT_TRUE(helper()->IsTooltipVisible());
  EXPECT_EQ(expected_text, helper()->GetTooltipText());
  gfx::Point first_tooltip_anchor_point = helper()->GetTooltipPosition();

  // Now that we have the anchor point of the first tooltip, move the focus to
  // the tooltip inside of the iframe.
  event_generator()->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  event_generator()->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  tooltip_monitor()->WaitUntilTooltipShown();
  EXPECT_TRUE(helper()->IsTooltipVisible());
  EXPECT_EQ(expected_text, helper()->GetTooltipText());

  // Validate that the tooltip is correctly positioned inside the iframe.
  // Because we explicitly removed the iframe's borders and body margins, the
  // buttons should be positioned at the exact same x value as the button above,
  // and at exactly twice the y value as the one outside the iframe (because the
  // tooltip's anchor point is at the bottom-center of the button).
  int expected_y = 2 * first_tooltip_anchor_point.y();
  EXPECT_EQ(gfx::Point(first_tooltip_anchor_point.x(), expected_y),
            helper()->GetTooltipPosition());

  helper()->HideAndReset();
  tooltip_monitor()->WaitUntilTooltipClosed();
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// https://crbug.com/1212403. Flaky on linux-chromeos-rel.
// https://crbug.com/1241736. Flaky on Win.
#define MAYBE_HideTooltipOnKeyPressTriggeredByCursor \
  DISABLED_HideTooltipOnKeyPressTriggeredByCursor
#else
#define MAYBE_HideTooltipOnKeyPressTriggeredByCursor \
  HideTooltipOnKeyPressTriggeredByCursor
#endif
IN_PROC_BROWSER_TEST_F(TooltipBrowserTest,
                       MAYBE_HideTooltipOnKeyPressTriggeredByCursor) {
  NavigateToURL("/tooltip.html");
  std::u16string expected_text = u"my tooltip";

  // First, trigger the tooltip from the cursor.
  gfx::Point position = WebContentPositionToScreenCoordinate(10, 10);
  event_generator()->MoveMouseTo(position);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Sends mouse move event to Ash as well to make server side tooltip work.
  EXPECT_TRUE(ui_controls::SendMouseMove(
      position.x(), position.y(), browser()->window()->GetNativeWindow()));
#endif
  tooltip_monitor()->WaitUntilTooltipShown();
  EXPECT_TRUE(helper()->IsTooltipVisible());
  EXPECT_EQ(expected_text, helper()->GetTooltipText());

  // Second, send a key press event to test whether the tooltip gets hidden.
  EXPECT_TRUE(tooltip_monitor()->IsWidgetActive());
  ui_controls::SendKeyPress(browser()->window()->GetNativeWindow(), ui::VKEY_A,
                            false, false, false, false);
  tooltip_monitor()->WaitUntilTooltipClosed();
  EXPECT_FALSE(helper()->IsTooltipVisible());
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// https://crbug.com/1212403. Flaky on linux-chromeos-rel.
// https://crbug.com/1241736. Flaky on Win.
// TODO(elkurin): Keyboard triggered tooltip needs command line switch on server
// side while Lacros cannot set Ash flag.
#define MAYBE_HideTooltipOnKeyPressTriggeredByKeyboard \
  DISABLED_HideTooltipOnKeyPressTriggeredByKeyboard
#else
#define MAYBE_HideTooltipOnKeyPressTriggeredByKeyboard \
  HideTooltipOnKeyPressTriggeredByKeyboard
#endif
IN_PROC_BROWSER_TEST_F(TooltipBrowserTest,
                       MAYBE_HideTooltipOnKeyPressTriggeredByKeyboard) {
  NavigateToURL("/tooltip.html");
  std::u16string expected_text = u"my tooltip";

  // Trigger the tooltip from the keyboard with a TAB keypress.
  event_generator()->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  event_generator()->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  tooltip_monitor()->WaitUntilTooltipShown();
  EXPECT_TRUE(helper()->IsTooltipVisible());
  EXPECT_EQ(expected_text, helper()->GetTooltipText());

  // Send a key press event to test whether the tooltip gets hidden.
  EXPECT_TRUE(tooltip_monitor()->IsWidgetActive());
  event_generator()->PressKey(ui::VKEY_A, ui::EF_NONE);
  event_generator()->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  tooltip_monitor()->WaitUntilTooltipClosed();
  EXPECT_FALSE(helper()->IsTooltipVisible());
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// https://crbug.com/1212403. Flaky on linux-chromeos-rel, windows, linux.
// TODO(elkurin): Keyboard triggered tooltip needs command line switch on server
// side while Lacros cannot set Ash flag.
#define MAYBE_ScriptFocusHidesKeyboardTriggeredTooltip \
  DISABLED_ScriptFocusHidesKeyboardTriggeredTooltip
#else
#define MAYBE_ScriptFocusHidesKeyboardTriggeredTooltip \
  ScriptFocusHidesKeyboardTriggeredTooltip
#endif
IN_PROC_BROWSER_TEST_F(TooltipBrowserTest,
                       MAYBE_ScriptFocusHidesKeyboardTriggeredTooltip) {
  NavigateToURL("/tooltip_two_buttons.html");
  std::u16string expected_text_1 = u"my tooltip 1";
  std::u16string expected_text_2 = u"my tooltip 2";

  // Trigger the tooltip from the keyboard with a TAB keypress.
  event_generator()->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  event_generator()->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  tooltip_monitor()->WaitUntilTooltipShown();
  EXPECT_TRUE(helper()->IsTooltipVisible());
  EXPECT_EQ(expected_text_1, helper()->GetTooltipText());
  EXPECT_TRUE(tooltip_monitor()->IsWidgetActive());

  // Validate that a blur event on another element than our focused one doesn't
  // hide the tooltip.
  std::string javascript = "document.getElementById('b2').blur();";
  EXPECT_TRUE(content::ExecJs(web_contents(), javascript));

  EXPECT_TRUE(helper()->IsTooltipVisible());
  EXPECT_EQ(expected_text_1, helper()->GetTooltipText());
  EXPECT_TRUE(tooltip_monitor()->IsWidgetActive());

  // Validate that a focus on another element will hide the tooltip.
  javascript = "document.getElementById('b2').focus();";

  EXPECT_TRUE(content::ExecJs(web_contents(), javascript));
  tooltip_monitor()->WaitUntilTooltipClosed();
  EXPECT_FALSE(tooltip_monitor()->IsWidgetActive());
  EXPECT_FALSE(helper()->IsTooltipVisible());

  // Move the focus again to the first button to test the blur on the focused
  // element.
  event_generator()->PressKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  event_generator()->ReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  tooltip_monitor()->WaitUntilTooltipShown();
  EXPECT_TRUE(helper()->IsTooltipVisible());
  EXPECT_EQ(expected_text_1, helper()->GetTooltipText());
  EXPECT_TRUE(tooltip_monitor()->IsWidgetActive());

  // Validate that the blur call hides the tooltip.
  javascript = "document.getElementById('b1').blur();";
  EXPECT_TRUE(content::ExecJs(web_contents(), javascript));

  EXPECT_FALSE(tooltip_monitor()->IsWidgetActive());
  EXPECT_FALSE(helper()->IsTooltipVisible());
}

IN_PROC_BROWSER_TEST_F(TooltipBrowserTest, ResetTooltipOnClosingWindow) {
  NavigateToURL("/tooltip.html");

  // Trigger the tooltip from the cursor.
  gfx::Point position = WebContentPositionToScreenCoordinate(10, 10);
  event_generator()->MoveMouseTo(position);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Sends mouse move event to Ash as well to make server side tooltip work.
  EXPECT_TRUE(ui_controls::SendMouseMove(
      position.x(), position.y(), browser()->window()->GetNativeWindow()));
#endif
  tooltip_monitor()->WaitUntilTooltipShown();
  EXPECT_TRUE(helper()->IsTooltipVisible());

  // Tooltip should be hidden on closing window.
  chrome::CloseWindow(browser());
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  // Verify tooltip is closed.
  // This is skipped on Lacros since tooltip_controller is destructed before
  // receiving OnTooltipHiddenOnServer.
  tooltip_monitor()->WaitUntilTooltipClosed();
#endif

  // Make sure Chrome won't crash during window destruction.
  ui_test_utils::WaitForBrowserToClose();
}
