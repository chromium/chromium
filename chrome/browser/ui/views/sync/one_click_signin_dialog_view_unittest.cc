// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/one_click_signin_dialog_view.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/sync/one_click_signin_links_delegate.h"
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

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());

    // Create a widget to host the anchor view.
    anchor_widget_ = new views::Widget;
    views::Widget::InitParams widget_params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Init(std::move(widget_params));
    anchor_widget_->Show();
  }

  void TearDown() override {
    OneClickSigninDialogView::Hide();
    anchor_widget_->Close();
    anchor_widget_ = NULL;
    ChromeViewsTestBase::TearDown();
  }

 protected:
  OneClickSigninDialogView* ShowOneClickSigninDialog() {
    OneClickSigninDialogView::ShowDialog(
        base::string16(),
        std::make_unique<TestOneClickSigninLinksDelegate>(this),
        anchor_widget_->GetNativeWindow(),
        base::Bind(&OneClickSigninDialogViewTest::ConfirmedCallback,
                   base::Unretained(this)));

    OneClickSigninDialogView* view =
        OneClickSigninDialogView::view_for_testing();
    EXPECT_TRUE(view != NULL);
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
  int learn_more_click_count_ = 0;

 private:
  friend class TestOneClickSigninLinksDelegate;

  class TestOneClickSigninLinksDelegate : public OneClickSigninLinksDelegate {
   public:
    // |test| is not owned by this object.
    explicit TestOneClickSigninLinksDelegate(OneClickSigninDialogViewTest* test)
        : test_(test) {}

    // OneClickSigninLinksDelegate:
    void OnLearnMoreLinkClicked(bool is_dialog) override {
      ++test_->learn_more_click_count_;
    }

   private:
    OneClickSigninDialogViewTest* test_;

    DISALLOW_COPY_AND_ASSIGN(TestOneClickSigninLinksDelegate);
  };

  // Widget to host the anchor view of the dialog. Destroys itself when closed.
  views::Widget* anchor_widget_ = nullptr;
  base::RunLoop* run_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninDialogViewTest);
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
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, center, center,
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
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, center, center,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  view->GetCancelButton()->OnMousePressed(event);
  view->GetCancelButton()->OnMouseReleased(event);

  WaitForClose();
  EXPECT_FALSE(OneClickSigninDialogView::IsShowing());
  EXPECT_TRUE(on_confirmed_callback_called_);
  EXPECT_EQ(false, confirmed_);
}

TEST_F(OneClickSigninDialogViewTest, AdvancedLink) {
  OneClickSigninDialogView* view = ShowOneClickSigninDialog();

  // Simulate pressing a link in the dialog.
  views::LinkListener* listener = view;
  listener->LinkClicked(view->advanced_link_, 0);

  WaitForClose();
  EXPECT_TRUE(on_confirmed_callback_called_);
  EXPECT_EQ(true, confirmed_);
  EXPECT_FALSE(OneClickSigninDialogView::IsShowing());
}

TEST_F(OneClickSigninDialogViewTest, LearnMoreLink) {
  OneClickSigninDialogView* view = ShowOneClickSigninDialog();

  views::LinkListener* listener = view;
  listener->LinkClicked(view->learn_more_link_, 0);

  // View should still be showing and the OnLearnMoreLinkClicked method
  // of the delegate should have been called with |is_dialog| == true.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(OneClickSigninDialogView::IsShowing());
  EXPECT_EQ(1, learn_more_click_count_);
}

TEST_F(OneClickSigninDialogViewTest, PressEnterKey) {
  OneClickSigninDialogView* one_click_view = ShowOneClickSigninDialog();
  ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0);
  one_click_view->GetWidget()->OnKeyEvent(&event);

  WaitForClose();
  EXPECT_FALSE(OneClickSigninDialogView::IsShowing());
  EXPECT_TRUE(on_confirmed_callback_called_);
  EXPECT_EQ(true, confirmed_);
}

TEST_F(OneClickSigninDialogViewTest, PressEscapeKey) {
  OneClickSigninDialogView* one_click_view = ShowOneClickSigninDialog();
  ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, 0);
  one_click_view->GetWidget()->OnKeyEvent(&event);

  WaitForClose();
  EXPECT_FALSE(OneClickSigninDialogView::IsShowing());
  EXPECT_TRUE(on_confirmed_callback_called_);
  EXPECT_EQ(false, confirmed_);
}
