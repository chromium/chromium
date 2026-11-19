// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_web_dialog.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace ash {

using LoginWebDialogTest = ::InProcessBrowserTest;

// Tests that LoginWebDialog is not minimizable.
IN_PROC_BROWSER_TEST_F(LoginWebDialogTest, CannotMinimize) {
  LoginWebDialog* dialog = new LoginWebDialog(
      browser()->GetProfile(), browser()->GetWindow()->GetNativeWindow(),
      std::u16string(), GURL());
  dialog->Show();
  aura::Window* window = dialog->get_dialog_window_for_test();
  ASSERT_TRUE(window);
  EXPECT_EQ(0, window->GetProperty(aura::client::kResizeBehaviorKey) &
                   aura::client::kResizeBehaviorCanMinimize);
}

// Tests that LoginWebDialog can be closed by 'Shift + BrowserBack' accelerator.
IN_PROC_BROWSER_TEST_F(LoginWebDialogTest, CloseDialogByAccelerator) {
  LoginWebDialog* dialog = new LoginWebDialog(
      browser()->GetProfile(), browser()->GetWindow()->GetNativeWindow(),
      std::u16string(), GURL());
  dialog->Show();
  gfx::NativeWindow window = dialog->get_dialog_window_for_test();
  ASSERT_TRUE(window);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  views::test::WidgetDestroyedWaiter closing_observer(widget);
  ui::test::EventGenerator generator(window->GetRootWindow());
  generator.PressKey(ui::VKEY_BROWSER_BACK, ui::EF_SHIFT_DOWN);
  closing_observer.Wait();
}

// Tests that LoginWebDialog does not crash with missing parent window.
IN_PROC_BROWSER_TEST_F(LoginWebDialogTest, NoParentWindow) {
  LoginWebDialog* dialog = new LoginWebDialog(browser()->GetProfile(), nullptr,
                                              std::u16string(), GURL());
  dialog->Show();
  aura::Window* window = dialog->get_dialog_window_for_test();
  ASSERT_TRUE(window);
  EXPECT_EQ((gfx::Size{1024, 752}), window->bounds().size());
}

// Tests that showing a constrained web dialog inside `LoginWebDialog` does not
// crash and positions the child dialog below the host dialog's header.
IN_PROC_BROWSER_TEST_F(LoginWebDialogTest, ShowConstrainedWebDialog) {
  LoginWebDialog* dialog = new LoginWebDialog(
      browser()->GetProfile(), browser()->GetWindow()->GetNativeWindow(),
      u"Title", GURL());
  dialog->Show();

  aura::Window* window = dialog->get_dialog_window_for_test();
  ASSERT_TRUE(window);
  views::Widget* host_widget = views::Widget::GetWidgetForNativeWindow(window);
  ASSERT_TRUE(host_widget);
  ASSERT_GT(host_widget->client_view()->GetBoundsInScreen().y(),
            host_widget->GetWindowBoundsInScreen().y());
  content::WebContents* web_contents =
      static_cast<views::WebDialogView*>(host_widget->widget_delegate())
          ->web_contents();
  ASSERT_TRUE(web_contents);

  auto delegate = std::make_unique<ui::WebDialogDelegate>();
  delegate->set_can_close(true);
  delegate->set_delete_on_close(false);
  delegate->set_dialog_size({800, 1000});
  delegate->set_dialog_content_url(GURL(url::kAboutBlankURL));
  ConstrainedWebDialogDelegate* constrained_delegate = ShowConstrainedWebDialog(
      browser()->GetProfile(), std::move(delegate), web_contents);
  ASSERT_TRUE(constrained_delegate);
  ASSERT_TRUE(constrained_delegate->GetNativeDialog());

  views::Widget* child_widget = views::Widget::GetWidgetForNativeWindow(
      constrained_delegate->GetNativeDialog());
  ASSERT_TRUE(child_widget);
  EXPECT_EQ(
      host_widget->client_view()->GetBoundsInScreen().y(),
      child_widget->GetWindowBoundsInScreen().y() +
          child_widget->non_client_view()->frame_view()->GetInsets().top());
  views::test::WidgetDestroyedWaiter child_waiter(child_widget);
  views::test::WidgetDestroyedWaiter host_waiter(host_widget);
  host_widget->Close();
  child_waiter.Wait();
  host_waiter.Wait();
}

}  // namespace ash
