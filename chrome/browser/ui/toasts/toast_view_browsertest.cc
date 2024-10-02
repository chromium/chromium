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
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/window/dialog_client_view.h"

class ToastViewTest : public DialogBrowserTest {
 public:
  ToastViewTest() = default;

  void ShowUi(const std::string& name) override {
    views::View* anchor_view =
        BrowserView::GetBrowserViewForBrowser(browser())->top_container();
    const std::u16string& toast_text =
        l10n_util::GetStringUTF16(IDS_LINK_COPIED);
    const gfx::VectorIcon& icon = vector_icons::kLinkIcon;
    std::unique_ptr<toasts::ToastView> toast =
        std::make_unique<toasts::ToastView>(anchor_view, toast_text, icon,
                                            false, base::DoNothing());
    if (name == "CloseButton") {
      toast->AddCloseButton(base::DoNothing());
    }

    if (name == "ActionButton") {
      toast->AddActionButton(l10n_util::GetStringUTF16(IDS_APP_OK));
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
    toast_ = nullptr;
    widget_ = nullptr;
  }

  toasts::ToastView* toast() { return toast_; }

 private:
  raw_ptr<toasts::ToastView> toast_;
  raw_ptr<views::Widget> widget_;
};

IN_PROC_BROWSER_TEST_F(ToastViewTest, InvokeUi_Basic) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ToastViewTest, InvokeUi_ActionButton) {
  ShowUi("ActionButton");
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
  ShowUi("CloseButton");
  ASSERT_TRUE(VerifyUi());
  ChromeLayoutProvider* lp = ChromeLayoutProvider::Get();
  EXPECT_TRUE(toast()->close_button_for_testing());
  EXPECT_EQ(
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_CLOSE_BUTTON),
      toast()->GetDialogClientView()->width() - toast()->bounds().right());
  DismissUi();
}
