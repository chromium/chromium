// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/base/ime/virtual_keyboard_controller_stub.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

class VirtualKeyboardWaiter : public ui::VirtualKeyboardControllerObserver {
 public:
  VirtualKeyboardWaiter(ui::VirtualKeyboardController* controller,
                        base::RepeatingClosure quit_closure)
      : controller_(controller), quit_closure_(quit_closure) {
    controller_->AddObserver(this);
  }
  VirtualKeyboardWaiter(const VirtualKeyboardWaiter&) = delete;
  VirtualKeyboardWaiter operator=(const VirtualKeyboardWaiter&) = delete;
  ~VirtualKeyboardWaiter() override { controller_->RemoveObserver(this); }

 private:
  // ui::VirtualKeyboardControllerObserver overrides
  void OnKeyboardVisible(const gfx::Rect& keyboard_rect) override {
    std::move(quit_closure_).Run();
  }
  void OnKeyboardHidden() override { std::move(quit_closure_).Run(); }

  raw_ptr<ui::VirtualKeyboardController> controller_ = nullptr;
  base::RepeatingClosure quit_closure_;
};

class BrowserAppMenuButtonVirtualKeyboardBrowserTest
    : public InProcessBrowserTest {
 public:
  BrowserAppMenuButtonVirtualKeyboardBrowserTest() = default;
  BrowserAppMenuButtonVirtualKeyboardBrowserTest(
      const BrowserAppMenuButtonVirtualKeyboardBrowserTest&) = delete;
  BrowserAppMenuButtonVirtualKeyboardBrowserTest& operator=(
      const BrowserAppMenuButtonVirtualKeyboardBrowserTest&) = delete;
  ~BrowserAppMenuButtonVirtualKeyboardBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    GURL test_url =
        ui_test_utils::GetTestUrl(base::FilePath("chromeos/virtual_keyboard"),
                                  base::FilePath("form.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents_);

    // TODO(crbug.com/40233608): Make it work without needing a fake controller.
    GetWebContentInputMethod()->SetVirtualKeyboardControllerForTesting(
        std::make_unique<ui::VirtualKeyboardControllerStub>());

    ASSERT_FALSE(IsVirtualKeyboardVisible());
  }

  ui::InputMethod* GetWebContentInputMethod() {
    return views::Widget::GetTopLevelWidgetForNativeView(
               web_contents_->GetRenderWidgetHostView()->GetNativeView())
        ->GetInputMethod();
  }

  ui::VirtualKeyboardController* GetVKController() {
    return GetWebContentInputMethod()->GetVirtualKeyboardController();
  }

  bool IsVirtualKeyboardVisible() {
    return GetVKController()->IsKeyboardVisible();
  }

 protected:
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_ = nullptr;
};

// Regression test for crbug.com/1334994.
IN_PROC_BROWSER_TEST_F(BrowserAppMenuButtonVirtualKeyboardBrowserTest,
                       ShowMenuDismissesVirtualKeyboard) {
  {
    base::RunLoop run_loop;
    VirtualKeyboardWaiter waiter(GetVKController(), run_loop.QuitClosure());
    SimulateMouseClickOrTapElementWithId(web_contents_, "username");
    run_loop.Run();
    EXPECT_TRUE(IsVirtualKeyboardVisible());
  }
  {
    base::RunLoop run_loop;
    VirtualKeyboardWaiter waiter(GetVKController(), run_loop.QuitClosure());
    // Show the AppMenu.
    BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->app_menu_button()
        ->ShowMenu(views::MenuRunner::NO_FLAGS);
    run_loop.Run();
    EXPECT_FALSE(IsVirtualKeyboardVisible());
  }
}

}  // namespace
