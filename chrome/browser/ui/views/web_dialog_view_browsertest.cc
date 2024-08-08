// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/web_dialog_view.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
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
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/web_dialogs/test/test_web_dialog_delegate.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

// Initial size of WebDialog for SizeWindow test case. Note the height must be
// at least 59 on Windows.
const int kInitialWidth = 60;
const int kInitialHeight = 60;

class WidgetResizeWaiter : public views::WidgetObserver {
 public:
  explicit WidgetResizeWaiter(views::Widget* widget) {
    old_size_ = widget->GetWindowBoundsInScreen().size();
    observation_.Observe(widget);
  }

  void Wait() { run_loop_.Run(); }

  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& bounds) override {
    if (bounds.size() != old_size_)
      run_loop_.Quit();
  }

 private:
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
  gfx::Size old_size_;
  base::RunLoop run_loop_;
};

class WebDialogBrowserTest : public InProcessBrowserTest {
 public:
  WebDialogBrowserTest() {}

  WebDialogBrowserTest(const WebDialogBrowserTest&) = delete;
  WebDialogBrowserTest& operator=(const WebDialogBrowserTest&) = delete;

  // content::BrowserTestBase:
  void SetUpOnMainThread() override;

 protected:
  void SimulateEscapeKey();

  bool was_view_deleted() const { return !view_tracker_.view(); }

  raw_ptr<views::WebDialogView, DanglingUntriaged> view_ = nullptr;
  bool web_dialog_delegate_destroyed_ = false;
  raw_ptr<ui::test::TestWebDialogDelegate, DanglingUntriaged> delegate_ =
      nullptr;

 private:
  views::ViewTracker view_tracker_;
};

void WebDialogBrowserTest::SetUpOnMainThread() {
  ui::test::TestWebDialogDelegate* delegate =
      new ui::test::TestWebDialogDelegate(GURL(chrome::kChromeUIChromeURLsURL));
  delegate->set_size(kInitialWidth, kInitialHeight);
  delegate->SetDeleteOnClosedAndObserve(&web_dialog_delegate_destroyed_);

  // Store the delegate so that we can update ShouldCloseDialogOnEscape().
  delegate_ = delegate;

  auto view = std::make_unique<views::WebDialogView>(
      browser()->profile(), delegate,
      std::make_unique<ChromeWebContentsHandler>());
  view->SetOwnedByWidget(true);
  gfx::NativeView parent_view =
      browser()->tab_strip_model()->GetActiveWebContents()->GetNativeView();
  view_ = view.get();
  view_tracker_.SetView(view_);

  auto* widget =
      views::Widget::CreateWindowWithParent(std::move(view), parent_view);
  widget->Show();
}

void WebDialogBrowserTest::SimulateEscapeKey() {
  ui::KeyEvent escape_event(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE,
                            ui::EF_NONE);
  if (view_->GetFocusManager()->OnKeyEvent(escape_event)) {
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        view_->GetWidget()->GetNativeWindow(), ui::VKEY_ESCAPE, false, false,
        false, false));
  }
}

}  // namespace

// Windows has some issues resizing windows. An off by one problem, and a
// minimum size that seems too big. See http://crbug.com/52602.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SizeWindow DISABLED_SizeWindow
#else
#define MAYBE_SizeWindow SizeWindow
#endif
IN_PROC_BROWSER_TEST_F(WebDialogBrowserTest, MAYBE_SizeWindow) {
#if BUILDFLAG(IS_MAC)
  // On macOS 11 (and presumably later) the new mechanism for sheets, which are
  // used for window modals like this dialog, always centers them within the
  // parent window regardless of the requested origin. The size is still
  // honored.
  bool centered_in_window = true;
#else
  bool centered_in_window = false;
#endif

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
  {
    WidgetResizeWaiter waiter(view_->GetWidget());
    view_->SetContentsBounds(nullptr, set_bounds);
    waiter.Wait();
  }
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

  {
    WidgetResizeWaiter waiter(view_->GetWidget());
    view_->SetContentsBounds(nullptr, set_bounds);
    waiter.Wait();
  }

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

  {
    WidgetResizeWaiter waiter(view_->GetWidget());
    view_->SetContentsBounds(nullptr, set_bounds);
    waiter.Wait();
  }

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

  {
    WidgetResizeWaiter waiter(view_->GetWidget());
    view_->SetContentsBounds(nullptr, set_bounds);
    waiter.Wait();
  }

  actual_bounds = view_->GetWidget()->GetClientAreaBoundsInScreen();
  check_bounds(set_bounds, actual_bounds);

  // Now verify that attempts to re-size to 0x0 enforces the minimum size.
  set_bounds.set_width(0);
  set_bounds.set_height(0);

  {
    WidgetResizeWaiter waiter(view_->GetWidget());
    view_->SetContentsBounds(nullptr, set_bounds);
    waiter.Wait();
  }

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
  EXPECT_FALSE(was_view_deleted());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(was_view_deleted());
}

// Test that closing the parent of a window-modal web dialog properly destroys
// the dialog and delegate.
IN_PROC_BROWSER_TEST_F(WebDialogBrowserTest, CloseParentWindow) {
  // Open a second browser window so we don't trigger shutdown.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  // TestWebDialogDelegate defaults to window-modal, so closing the browser
  // Window (as opposed to closing merely the tab) should close the dialog.
  EXPECT_EQ(ui::mojom::ModalType::kWindow,
            view_->GetWidget()->widget_delegate()->GetModalType());

  // Close the parent window. Tear down may happen asynchronously.
  EXPECT_FALSE(web_dialog_delegate_destroyed_);
  EXPECT_FALSE(was_view_deleted());
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(web_dialog_delegate_destroyed_);
  EXPECT_TRUE(was_view_deleted());
}

// Tests the Escape key behavior when ShouldCloseDialogOnEscape() is enabled.
IN_PROC_BROWSER_TEST_F(WebDialogBrowserTest, CloseDialogOnEscapeEnabled) {
  // Open a second browser window so we don't trigger shutdown.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  // If ShouldCloseDialogOnEscape() is true, pressing Escape should close the
  // dialog.
  delegate_->SetCloseOnEscape(true);
  SimulateEscapeKey();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(web_dialog_delegate_destroyed_);
  EXPECT_TRUE(was_view_deleted());
}

// Tests the Escape key behavior when ShouldCloseDialogOnEscape() is disabled.
IN_PROC_BROWSER_TEST_F(WebDialogBrowserTest, CloseDialogOnEscapeDisabled) {
  // Open a second browser window so we don't trigger shutdown.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  // If ShouldCloseDialogOnEscape() is false, pressing Escape does nothing.
  delegate_->SetCloseOnEscape(false);
  SimulateEscapeKey();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(web_dialog_delegate_destroyed_);
  EXPECT_FALSE(was_view_deleted());
}

// Test that key event is translated to a text input properly.
IN_PROC_BROWSER_TEST_F(WebDialogBrowserTest, TextInputViaKeyEvent) {
  TestTextInputViaKeyEvent(view_->web_contents());
}
