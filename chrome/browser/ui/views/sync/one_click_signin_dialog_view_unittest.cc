// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/one_click_signin_dialog_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class OneClickSigninDialogViewTest : public ChromeViewsTestBase,
                                     public views::WidgetObserver {
 public:
  OneClickSigninDialogViewTest() {}

  OneClickSigninDialogViewTest(const OneClickSigninDialogViewTest&) = delete;
  OneClickSigninDialogViewTest& operator=(const OneClickSigninDialogViewTest&) =
      delete;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());

    // Create a widget to host the anchor view.
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    anchor_widget_->Show();
  }

  void TearDown() override {
    OneClickSigninDialogView::Hide();
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  OneClickSigninDialogView* ShowOneClickSigninDialog() {
    OneClickSigninDialogView::ShowDialog(
        std::u16string(), nullptr, anchor_widget_->GetNativeWindow(),
        base::BindOnce(&OneClickSigninDialogViewTest::ConfirmedCallback,
                       base::Unretained(this)));

    OneClickSigninDialogView* view =
        OneClickSigninDialogView::view_for_testing();
    EXPECT_NE(nullptr, view);
    return view;
  }

  void ConfirmedCallback(bool confirmed) {
    on_confirmed_callback_called_ = true;
    confirmed_ = confirmed;
  }

  // Waits for the OneClickSigninDialogView to close, by observing its Widget,
  // then running a RunLoop, which OnWidgetDestroyed will quit. It's not
  // sufficient to wait for the message loop to go idle, as the dialog has a
  // closing animation which may still be running at that point. Instead, wait
  // for its widget to be destroyed.
  void WaitForClose() {
    views::Widget* widget =
        OneClickSigninDialogView::view_for_testing()->GetWidget();
    widget->AddObserver(this);
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    // Block until OnWidgetDestroyed() is fired.
    run_loop.Run();
    // The Widget has been destroyed, so there's no need to remove this as an
    // observer.
  }

  // views::WidgetObserver method:
  void OnWidgetDestroyed(views::Widget* widget) override { run_loop_->Quit(); }

  bool on_confirmed_callback_called_ = false;
  bool confirmed_ = false;

 private:
  // Widget to host the anchor view of the dialog. Destroys itself when closed.
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
};

TEST_F(OneClickSigninDialogViewTest, ShowDialog) {
  ShowOneClickSigninDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(OneClickSigninDialogView::IsShowing());
}

TEST_F(OneClickSigninDialogViewTest, HideDialog) {
  ShowOneClickSigninDialog();

  OneClickSigninDialogView::Hide();
  WaitForClose();
  EXPECT_FALSE(OneClickSigninDialogView::IsShowing());
  EXPECT_TRUE(on_confirmed_callback_called_);
  EXPECT_EQ(false, confirmed_);
}

TEST_F(OneClickSigninDialogViewTest, OkButton) {
  OneClickSigninDialogView* view = ShowOneClickSigninDialog();
  view->ResetViewShownTimeStampForTesting();

  gfx::Point center(10, 10);
  const ui::MouseEvent event(ui::EventType::kMousePressed, center, center,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  view->GetOkButton()->OnMousePressed(event);
  view->GetOkButton()->OnMouseReleased(event);

  WaitForClose();
  EXPECT_FALSE(OneClickSigninDialogView::IsShowing());
  EXPECT_TRUE(on_confirmed_callback_called_);
  EXPECT_EQ(true, confirmed_);
}

TEST_F(OneClickSigninDialogViewTest, UndoButton) {
  OneClickSigninDialogView* view = ShowOneClickSigninDialog();
  view->ResetViewShownTimeStampForTesting();

  gfx::Point center(10, 10);
  const ui::MouseEvent event(ui::EventType::kMousePressed, center, center,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  view->GetCancelButton()->OnMousePressed(event);
  view->GetCancelButton()->OnMouseReleased(event);

  WaitForClose();
  EXPECT_FALSE(OneClickSigninDialogView::IsShowing());
  EXPECT_TRUE(on_confirmed_callback_called_);
  EXPECT_EQ(false, confirmed_);
}

TEST_F(OneClickSigninDialogViewTest, PressEnterKey) {
  OneClickSigninDialogView* one_click_view = ShowOneClickSigninDialog();
  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_RETURN, 0);
  one_click_view->GetWidget()->OnKeyEvent(&event);

  WaitForClose();
  EXPECT_FALSE(OneClickSigninDialogView::IsShowing());
  EXPECT_TRUE(on_confirmed_callback_called_);
  EXPECT_EQ(true, confirmed_);
}

TEST_F(OneClickSigninDialogViewTest, PressEscapeKey) {
  OneClickSigninDialogView* one_click_view = ShowOneClickSigninDialog();
  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE, 0);
  one_click_view->GetWidget()->OnKeyEvent(&event);

  WaitForClose();
  EXPECT_FALSE(OneClickSigninDialogView::IsShowing());
  EXPECT_TRUE(on_confirmed_callback_called_);
  EXPECT_EQ(false, confirmed_);
}
