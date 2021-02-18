// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/content_test_utils.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/test/test_web_dialog_delegate.h"

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

// Initial size of WebDialog for SizeWindow test case. Note the height must be
// at least 59 on Windows.
const int kInitialWidth = 60;
const int kInitialHeight = 60;

class TestWebDialogView : public views::WebDialogView {
 public:
  TestWebDialogView(content::BrowserContext* context,
                    ui::WebDialogDelegate* delegate,
                    bool* observed_destroy)
      : views::WebDialogView(context,
                             delegate,
                             std::make_unique<ChromeWebContentsHandler>()),
        should_quit_on_size_change_(false),
        observed_destroy_(observed_destroy) {
    EXPECT_FALSE(*observed_destroy_);
    delegate->GetDialogSize(&last_size_);
  }

  ~TestWebDialogView() override { *observed_destroy_ = true; }

  void set_should_quit_on_size_change(bool should_quit) {
    should_quit_on_size_change_ = should_quit;
  }

 private:
  // TODO(xiyuan): Update this when WidgetDelegate has bounds change hook.
  void SaveWindowPlacement(const gfx::Rect& bounds,
                           ui::WindowShowState show_state) override {
    if (should_quit_on_size_change_ && last_size_ != bounds.size()) {
      // Schedule message loop quit because we could be called while
      // the bounds change call is on the stack and not in the nested message
      // loop.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&base::RunLoop::QuitCurrentWhenIdleDeprecated));
    }

    last_size_ = bounds.size();
  }

  void OnDialogClosed(const std::string& json_retval) override {
    should_quit_on_size_change_ = false;  // No quit when we are closing.
    views::WebDialogView::OnDialogClosed(json_retval);  // Deletes this.
  }

  // Whether we should quit message loop when size change is detected.
  bool should_quit_on_size_change_;
  gfx::Size last_size_;
  bool* observed_destroy_;

  DISALLOW_COPY_AND_ASSIGN(TestWebDialogView);
};

class WebDialogBrowserTest : public InProcessBrowserTest {
 public:
  WebDialogBrowserTest() {}

  // content::BrowserTestBase:
  void SetUpOnMainThread() override;

 protected:
  void SimulateEscapeKey();

  TestWebDialogView* view_ = nullptr;
  bool web_dialog_delegate_destroyed_ = false;
  bool web_dialog_view_destroyed_ = false;
  ui::test::TestWebDialogDelegate* delegate_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDialogBrowserTest);
};

void WebDialogBrowserTest::SetUpOnMainThread() {
  ui::test::TestWebDialogDelegate* delegate =
      new ui::test::TestWebDialogDelegate(GURL(chrome::kChromeUIChromeURLsURL));
  delegate->set_size(kInitialWidth, kInitialHeight);
  delegate->SetDeleteOnClosedAndObserve(&web_dialog_delegate_destroyed_);

  // Store the delegate so that we can update ShouldCloseDialogOnEscape().
  delegate_ = delegate;

  view_ = new TestWebDialogView(browser()->profile(), delegate,
                                &web_dialog_view_destroyed_);
  gfx::NativeView parent_view =
      browser()->tab_strip_model()->GetActiveWebContents()->GetNativeView();
  views::Widget::CreateWindowWithParent(view_, parent_view);
  view_->GetWidget()->Show();
}

void WebDialogBrowserTest::SimulateEscapeKey() {
  ui::KeyEvent escape_event(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, ui::EF_NONE);
  if (view_->GetFocusManager()->OnKeyEvent(escape_event)) {
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        view_->GetWidget()->GetNativeWindow(), ui::VKEY_ESCAPE, false, false,
        false, false));
  }
}

}  // namespace

// Windows has some issues resizing windows. An off by one problem, and a
// minimum size that seems too big. See http://crbug.com/52602.
#if defined(OS_WIN)
#define MAYBE_SizeWindow DISABLED_SizeWindow
#else
#define MAYBE_SizeWindow SizeWindow
#endif
IN_PROC_BROWSER_TEST_F(WebDialogBrowserTest, MAYBE_SizeWindow) {
  bool centered_in_window = false;
#if defined(OS_MAC)
  // On macOS 11 (and presumably later) the new mechanism for sheets, which are
  // used for window modals like this dialog, always centers them within the
  // parent window regardless of the requested origin. The size is still
  // honored.
  if (base::mac::IsAtLeastOS11())
    centered_in_window = true;
#endif

  // TestWebDialogView should quit current message loop on size change.
  view_->set_should_quit_on_size_change(true);

  gfx::Rect set_bounds = view_->GetWidget()->GetClientAreaBoundsInScreen();
  gfx::Rect actual_bounds, rwhv_bounds;

  // Bigger than the default in both dimensions.
  set_bounds.set_width(400);
  set_bounds.set_height(300);

  auto check_bounds = [&](const gfx::Rect& set, const gfx::Rect& actual) {
    if (centered_in_window) {
      gfx::Rect expected = browser()->window()->GetBounds();
      expected.ClampToCenteredSize(set.size());
      EXPECT_EQ(expected, actual);
    } else {
      EXPECT_EQ(set, actual);
    }
  };

  // WebDialogView ignores the WebContents* |source| argument to
  // SetContentsBounds. We could pass view_->web_contents(), but it's not
  // relevant for the test.
  view_->SetContentsBounds(nullptr, set_bounds);
  base::RunLoop().Run();  // TestWebDialogView will quit.
  actual_bounds = view_->GetWidget()->GetClientAreaBoundsInScreen();
  check_bounds(set_bounds, actual_bounds);

  rwhv_bounds =
      view_->web_contents()->GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Larger in one dimension and smaller in the other.
  set_bounds.set_width(550);
  set_bounds.set_height(250);

  view_->SetContentsBounds(nullptr, set_bounds);
  base::RunLoop().Run();  // TestWebDialogView will quit.
  actual_bounds = view_->GetWidget()->GetClientAreaBoundsInScreen();
  check_bounds(set_bounds, actual_bounds);

  rwhv_bounds =
      view_->web_contents()->GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Get very small.
  const gfx::Size min_size = view_->GetWidget()->GetMinimumSize();
  EXPECT_LT(0, min_size.width());
  EXPECT_LT(0, min_size.height());

  set_bounds.set_size(min_size);

  view_->SetContentsBounds(nullptr, set_bounds);
  base::RunLoop().Run();  // TestWebDialogView will quit.
  actual_bounds = view_->GetWidget()->GetClientAreaBoundsInScreen();
  check_bounds(set_bounds, actual_bounds);

  rwhv_bounds =
      view_->web_contents()->GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Check to make sure we can't get to 0x0. First expand beyond the minimum
  // size that was set above so that TestWebDialogView has a change to pick up.
  set_bounds.set_height(250);
  view_->SetContentsBounds(nullptr, set_bounds);
  base::RunLoop().Run();  // TestWebDialogView will quit.
  actual_bounds = view_->GetWidget()->GetClientAreaBoundsInScreen();
  check_bounds(set_bounds, actual_bounds);

  // Now verify that attempts to re-size to 0x0 enforces the minimum size.
  set_bounds.set_width(0);
  set_bounds.set_height(0);

  view_->SetContentsBounds(nullptr, set_bounds);
  base::RunLoop().Run();  // TestWebDialogView will quit.
  actual_bounds = view_->GetWidget()->GetClientAreaBoundsInScreen();
  EXPECT_EQ(min_size, actual_bounds.size());

  // And that the render view is also non-zero.
  rwhv_bounds =
      view_->web_contents()->GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());

  // WebDialogView::CanClose() returns true only after before-unload handlers
  // have run (or the dialog has none and gets fast-closed via
  // RenderViewHostImpl::ClosePageIgnoringUnloadEvents which is the case here).
  // Close via WebContents for more authentic coverage (vs Widget::CloseNow()).
  EXPECT_FALSE(web_dialog_delegate_destroyed_);
  view_->web_contents()->Close();
  EXPECT_TRUE(web_dialog_delegate_destroyed_);

  // The close of the actual widget should happen asynchronously.
  EXPECT_FALSE(web_dialog_view_destroyed_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(web_dialog_view_destroyed_);
}

// Test that closing the parent of a window-modal web dialog properly destroys
// the dialog and delegate.
IN_PROC_BROWSER_TEST_F(WebDialogBrowserTest, CloseParentWindow) {
  // Open a second browser window so we don't trigger shutdown.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_NONE);

  // TestWebDialogDelegate defaults to window-modal, so closing the browser
  // Window (as opposed to closing merely the tab) should close the dialog.
  EXPECT_EQ(ui::MODAL_TYPE_WINDOW,
            view_->GetWidget()->widget_delegate()->GetModalType());

  // Close the parent window. Tear down may happen asynchronously.
  EXPECT_FALSE(web_dialog_delegate_destroyed_);
  EXPECT_FALSE(web_dialog_view_destroyed_);
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(web_dialog_delegate_destroyed_);
  EXPECT_TRUE(web_dialog_view_destroyed_);
}

// Tests the Escape key behavior when ShouldCloseDialogOnEscape() is enabled.
#if defined(OS_WIN) && !defined(NDEBUG)
// Flaky on win7 tests dbg: https://crbug.com/1035439
#define MAYBE_CloseDialogOnEscapeEnabled DISABLED_CloseDialogOnEscapeEnabled
#else
#define MAYBE_CloseDialogOnEscapeEnabled CloseDialogOnEscapeEnabled
#endif
IN_PROC_BROWSER_TEST_F(WebDialogBrowserTest, MAYBE_CloseDialogOnEscapeEnabled) {
  ui_controls::EnableUIControls();

  // Open a second browser window so we don't trigger shutdown.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_NONE);

  // If ShouldCloseDialogOnEscape() is true, pressing Escape should close the
  // dialog.
  delegate_->SetCloseOnEscape(true);
  SimulateEscapeKey();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(web_dialog_delegate_destroyed_);
  EXPECT_TRUE(web_dialog_view_destroyed_);
}

// Tests the Escape key behavior when ShouldCloseDialogOnEscape() is disabled.
IN_PROC_BROWSER_TEST_F(WebDialogBrowserTest, CloseDialogOnEscapeDisabled) {
  ui_controls::EnableUIControls();

  // Open a second browser window so we don't trigger shutdown.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_NONE);

  // If ShouldCloseDialogOnEscape() is false, pressing Escape does nothing.
  delegate_->SetCloseOnEscape(false);
  SimulateEscapeKey();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(web_dialog_delegate_destroyed_);
  EXPECT_FALSE(web_dialog_view_destroyed_);
}

// Test that key event is translated to a text input properly.
IN_PROC_BROWSER_TEST_F(WebDialogBrowserTest, TextInputViaKeyEvent) {
  TestTextInputViaKeyEvent(view_->web_contents());
}
