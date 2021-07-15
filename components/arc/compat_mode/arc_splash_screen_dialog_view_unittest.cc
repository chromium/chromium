// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/arc_splash_screen_dialog_view.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "components/arc/compat_mode/test/compat_mode_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/controls/button/md_text_button.h"

namespace arc {

class ArcSplashScreenDialogViewTest : public CompatModeTestBase {
 public:
  ArcSplashScreenDialogViewTest() = default;
  ArcSplashScreenDialogViewTest(const ArcSplashScreenDialogViewTest& other) =
      delete;
  ArcSplashScreenDialogViewTest& operator=(
      const ArcSplashScreenDialogViewTest& other) = delete;
  ~ArcSplashScreenDialogViewTest() override = default;

  // CompatModeTestBase:
  void SetUp() override {
    CompatModeTestBase::SetUp();
    parent_widget_ = CreateWidget();

    anchor_ = parent_widget_->GetRootView()->AddChildView(
        std::make_unique<views::View>());
  }

  void TearDown() override {
    parent_widget_->CloseNow();
    parent_widget_.reset();
    CompatModeTestBase::TearDown();
  }

 protected:
  void ShowAsBubble(std::unique_ptr<ArcSplashScreenDialogView> dialog_view) {
    views::BubbleDialogDelegateView::CreateBubble(std::move(dialog_view))
        ->Show();
  }

  views::View* anchor() { return anchor_; }
  aura::Window* parent_window() { return parent_widget_->GetNativeView(); }
  views::Widget* parent_widget() { return parent_widget_.get(); }

 private:
  std::unique_ptr<views::Widget> parent_widget_;
  views::View* anchor_;
};

TEST_F(ArcSplashScreenDialogViewTest, TestCloseButton) {
  for (const bool is_for_unresizable : {true, false}) {
    bool on_close_callback_called = false;
    auto dialog_view = std::make_unique<ArcSplashScreenDialogView>(
        base::BindLambdaForTesting([&]() { on_close_callback_called = true; }),
        parent_window(), anchor(), is_for_unresizable);
    ArcSplashScreenDialogView::TestApi dialog_view_test(dialog_view.get());
    ShowAsBubble(std::move(dialog_view));
    EXPECT_TRUE(dialog_view_test.close_button()->GetVisible());
    EXPECT_FALSE(on_close_callback_called);
    LeftClickOnView(parent_widget(), dialog_view_test.close_button());
    EXPECT_TRUE(on_close_callback_called);
  }
}

TEST_F(ArcSplashScreenDialogViewTest, TestAnchorHighlight) {
  for (const bool is_for_unresizable : {true, false}) {
    auto dialog_view = std::make_unique<ArcSplashScreenDialogView>(
        base::DoNothing(), parent_window(), anchor(), is_for_unresizable);
    ArcSplashScreenDialogView::TestApi dialog_view_test(dialog_view.get());
    ShowAsBubble(std::move(dialog_view));
    EXPECT_NE(-1, anchor()->GetIndexOf(dialog_view_test.highlight_border()));
    LeftClickOnView(parent_widget(), dialog_view_test.close_button());
    EXPECT_EQ(-1, anchor()->GetIndexOf(dialog_view_test.highlight_border()));
  }
}

TEST_F(ArcSplashScreenDialogViewTest,
       TestSplashScreenInFullscreenOrMaximinzedWindow) {
  for (const bool is_for_unresizable : {true, false}) {
    for (const auto state :
         {ui::SHOW_STATE_FULLSCREEN, ui::SHOW_STATE_MAXIMIZED}) {
      bool on_close_callback_called = false;
      auto dialog_view = std::make_unique<ArcSplashScreenDialogView>(
          base::BindLambdaForTesting(
              [&]() { on_close_callback_called = true; }),
          parent_window(), anchor(), is_for_unresizable);
      ShowAsBubble(std::move(dialog_view));
      EXPECT_FALSE(on_close_callback_called);
      parent_window()->SetProperty(aura::client::kShowStateKey, state);
      EXPECT_TRUE(on_close_callback_called);
    }
  }
}

}  // namespace arc
