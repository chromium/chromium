// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_upgrade_bubble_view.h"

#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/test_event.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr char kRpId[] = "example.com";

class PasskeyUpgradeBubbleViewTest : public PasswordBubbleViewTestBase {
 protected:
  PasskeyUpgradeBubbleViewTest() = default;
  ~PasskeyUpgradeBubbleViewTest() override = default;

  void CreateViewAndShow() {
    CreateAnchorViewAndShow();
    view_ = new PasskeyUpgradeBubbleView(
        web_contents(), anchor_view(), LocationBarBubbleDelegateView::AUTOMATIC,
        kRpId);
    views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
  }

  void TearDown() override {
    std::exchange(view_, nullptr)
        ->GetWidget()
        ->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
    PasswordBubbleViewTestBase::TearDown();
  }

  void SimulateManagePasskeyButtonClick() {
    views::test::ButtonTestApi(view().manage_passkeys_button_for_testing())
        .NotifyClick(ui::test::TestEvent());
  }

  PasskeyUpgradeBubbleView& view() {
    CHECK(view_);
    return *view_;
  }

 private:
  raw_ptr<PasskeyUpgradeBubbleView> view_ = nullptr;
};

TEST_F(PasskeyUpgradeBubbleViewTest, HasTitleAndNoStandardButtons) {
  CreateViewAndShow();

  EXPECT_TRUE(view().ShouldShowWindowTitle());
  EXPECT_FALSE(view().GetOkButton());
  EXPECT_FALSE(view().GetCancelButton());
}

TEST_F(PasskeyUpgradeBubbleViewTest, ExtraButtonOpensPasswordDetailsPage) {
  CreateViewAndShow();

  EXPECT_CALL(
      *model_delegate_mock(),
      NavigateToPasswordDetailsPageInPasswordManager(
          kRpId,
          password_manager::ManagePasswordsReferrer::kPasskeyUpgradeBubble));
  SimulateManagePasskeyButtonClick();
}

}  // namespace
