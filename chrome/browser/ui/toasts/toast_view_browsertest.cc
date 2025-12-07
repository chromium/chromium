// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_view.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

class TestMenuModel : public ui::SimpleMenuModel,
                      ui::SimpleMenuModel::Delegate {
 public:
  TestMenuModel() : ui::SimpleMenuModel(/*delegate=*/this) {}

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {}
};

class ToastViewTest : public DialogBrowserTest {
 public:
  ToastViewTest() = default;

  struct ToastOptions {
    std::u16string text;
    bool add_close_button = false;
    bool add_action_button = false;
    bool add_menu = false;
    bool add_image_override = false;
  };

  void ConfigureToast(const ToastOptions& options) { options_ = options; }

  void ShowUi(const std::string& name) override {
    anchor_view_ =
        BrowserView::GetBrowserViewForBrowser(browser())->top_container();
    std::u16string toast_text = l10n_util::GetStringUTF16(IDS_LINK_COPIED);
    if (!options_.text.empty()) {
      toast_text = options_.text;
    }
    if (options_.add_image_override) {
      int size = toasts::ToastView::GetIconSize();
      image_override_ =
          std::make_unique<ui::ImageModel>(ui::ImageModel::FromImage(
              gfx::test::CreateImage(size, size, 0xff0000)));
    }
    std::unique_ptr<toasts::ToastView> toast =
        std::make_unique<toasts::ToastView>(
            anchor_view_, toast_text, vector_icons::kLinkIcon,
            image_override_.get(), false, base::DoNothing());
    if (options_.add_close_button) {
      toast->AddCloseButton(base::DoNothing());
    }
    if (options_.add_action_button) {
      toast->AddActionButton(l10n_util::GetStringUTF16(IDS_APP_OK));
    }
    if (options_.add_menu) {
      toast->AddMenu(std::make_unique<TestMenuModel>());
    }
    toast_ = toast.get();
    widget_ = views::BubbleDialogDelegateView::CreateBubble(std::move(toast));
    widget_->ShowInactive();
  }

  bool VerifyUi() override {
    if (!widget_ || !widget_->IsVisible()) {
      return false;
    }
    ChromeLayoutProvider* lp = ChromeLayoutProvider::Get();
    EXPECT_EQ(u"Link copied",
              toast_->label_for_testing()->GetDisplayTextForTesting());
    EXPECT_EQ(lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT),
              toast_->GetDialogClientView()->height());
    return true;
  }

  void DismissUi() override {
    anchor_view_ = nullptr;
    toast_ = nullptr;
    widget_ = nullptr;
  }

  views::View* anchor_view() { return anchor_view_; }
  toasts::ToastView* toast() { return toast_; }

 private:
  raw_ptr<views::View> anchor_view_;
  std::unique_ptr<ui::ImageModel> image_override_;
  raw_ptr<toasts::ToastView> toast_;
  raw_ptr<views::Widget> widget_;
  ToastOptions options_;
};

IN_PROC_BROWSER_TEST_F(ToastViewTest, InvokeUi_Basic) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ToastViewTest, InvokeUi_ActionButton) {
  ConfigureToast({
      .add_action_button = true,
  });
  ShowUi("");
  ASSERT_TRUE(VerifyUi());
  ChromeLayoutProvider* lp = ChromeLayoutProvider::Get();
  EXPECT_TRUE(toast()->action_button_for_testing());
  EXPECT_EQ(lp->GetDistanceMetric(
                DISTANCE_TOAST_BUBBLE_BETWEEN_LABEL_ACTION_BUTTON_SPACING),
            toast()->action_button_for_testing()->x() -
                toast()->label_for_testing()->bounds().right());
  EXPECT_EQ(
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_ACTION_BUTTON),
      toast()->GetDialogClientView()->width() - toast()->bounds().right());
  DismissUi();
}

IN_PROC_BROWSER_TEST_F(ToastViewTest, InvokeUi_CloseButton) {
  ConfigureToast({
      .add_close_button = true,
  });
  ShowUi("");
  ASSERT_TRUE(VerifyUi());
  ChromeLayoutProvider* lp = ChromeLayoutProvider::Get();
  EXPECT_TRUE(toast()->close_button_for_testing());
  EXPECT_EQ(
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_CLOSE_BUTTON),
      toast()->GetDialogClientView()->width() - toast()->bounds().right());
  DismissUi();
}

IN_PROC_BROWSER_TEST_F(ToastViewTest, InvokeUi_Image) {
  ConfigureToast({
      .add_image_override = true,
  });
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ToastViewTest, InvokeUi_Menu) {
  ConfigureToast({
      .add_menu = true,
  });
  ShowAndVerifyUi();
}

// http://crbug.com/371579791
IN_PROC_BROWSER_TEST_F(ToastViewTest, InvokeUi_ShrinkToFitWindow) {
  ConfigureToast({
      // Use an arbitrarily-long label string to force a wide toast.
      .text = std::u16string(1000, 'a'),
  });
  ShowUi("");
  const views::View* bubble = toast()->GetBubbleFrameView();
  const int expected_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                  DISTANCE_TOAST_BUBBLE_BROWSER_WINDOW_MARGIN) -
                              views::BubbleBorder::kShadowBlur;
  EXPECT_GT(bubble->GetPreferredSize().width(), bubble->width());
  EXPECT_EQ(bubble->GetBoundsInScreen().x(),
            anchor_view()->GetBoundsInScreen().x() + expected_margin);
  EXPECT_EQ(bubble->GetBoundsInScreen().right(),
            anchor_view()->GetBoundsInScreen().right() - expected_margin);
  DismissUi();
}

}  // namespace
