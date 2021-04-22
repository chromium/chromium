// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/arc_splash_screen_dialog_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/views_test_base.h"

namespace arc {
namespace {

void ClickOnView(views::View* view) {
  ui::MouseEvent click(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  view->OnMousePressed(click);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  view->OnMouseReleased(release);
}

}  // namespace

class ArcSplashScreenDialogViewTest : public views::ViewsTestBase {
 public:
  ArcSplashScreenDialogViewTest() = default;
  ArcSplashScreenDialogViewTest(const ArcSplashScreenDialogViewTest& other) =
      delete;
  ArcSplashScreenDialogViewTest& operator=(
      const ArcSplashScreenDialogViewTest& other) = delete;
  ~ArcSplashScreenDialogViewTest() override = default;

  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    widget_->SetBounds(gfx::Rect(800, 800));
    auto dialog_view = BuildSplashScreenDialogView(
        base::BindRepeating(&ArcSplashScreenDialogViewTest::OnCloseCallback,
                            base::Unretained(this)));
    dialog_view_ = widget_->SetContentsView(std::move(dialog_view));
  }

  void TearDown() override {
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  ArcSplashScreenDialogView* dialog_view() { return dialog_view_; }

  void OnCloseCallback() { on_close_callback_called = true; }

  bool on_close_callback_called = false;

 private:
  ArcSplashScreenDialogView* dialog_view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(ArcSplashScreenDialogViewTest, TestBuildSplashScreenDialogView) {
  ArcSplashScreenDialogView::TestApi dialog_view_test(dialog_view());
  EXPECT_TRUE(dialog_view_test.close_button()->GetVisible());
  ClickOnView(dialog_view_test.close_button());
  EXPECT_TRUE(on_close_callback_called);
}

}  // namespace arc
