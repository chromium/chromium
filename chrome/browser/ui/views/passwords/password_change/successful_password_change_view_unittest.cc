// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/successful_password_change_view.h"

#include <string>

#include "base/test/gmock_callback_support.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/test_event.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/button_test_api.h"

namespace {

using ::testing::Return;
using ::testing::ReturnRef;

const std::u16string kTestEmail = u"elisa.buckett@gmail.com";
const std::u16string kPassword = u"cE1L45Vgxyzlu8";

}  // namespace

class SuccessfulPasswordChangeViewTest : public PasswordBubbleViewTestBase {
 public:
  SuccessfulPasswordChangeViewTest() = default;
  ~SuccessfulPasswordChangeViewTest() override = default;

  void SetUp() override {
    PasswordBubbleViewTestBase::SetUp();
    ON_CALL(*model_delegate_mock(), PasswordChangeUsername())
        .WillByDefault(ReturnRef(kTestEmail));
    ON_CALL(*model_delegate_mock(), PasswordChangeNewPassword())
        .WillByDefault(ReturnRef(kPassword));
  }

  void TearDown() override {
    if (view_) {
      view_->GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kUnspecified);
      view_ = nullptr;
    }
    PasswordBubbleViewTestBase::TearDown();
  }

  void CreateAndShowView() {
    CreateAnchorViewAndShow();

    view_ = new SuccessfulPasswordChangeView(
        web_contents(), views::BubbleAnchor(anchor_view()));
    views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
  }

  SuccessfulPasswordChangeView* view() { return view_; }
  void reset_view() { view_ = nullptr; }

  views::Label* GetLabelById(int id) {
    return static_cast<views::Label*>(view()->GetViewByID(id));
  }

 private:
  raw_ptr<SuccessfulPasswordChangeView> view_;
};

TEST_F(SuccessfulPasswordChangeViewTest, BubbleLayout) {
  CreateAndShowView();
  EXPECT_EQ(view()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGED_TITLE));

  EXPECT_EQ(
      GetLabelById(SuccessfulPasswordChangeView::kUsernameLabelId)->GetText(),
      kTestEmail);
  EXPECT_EQ(
      GetLabelById(SuccessfulPasswordChangeView::kPasswordLabelId)->GetText(),
      kPassword);

  EXPECT_TRUE(
      view()->GetViewByID(SuccessfulPasswordChangeView::kEyeIconButtonId));

  // Verify password is obscured by default.
  EXPECT_TRUE(GetLabelById(SuccessfulPasswordChangeView::kPasswordLabelId)
                  ->GetObscured());

  EXPECT_TRUE(view()->GetViewByID(static_cast<int>(
      SuccessfulPasswordChangeView::kManagePasswordsButtonId)));
}

TEST_F(SuccessfulPasswordChangeViewTest, ManagePasswordButtonClick) {
  CreateAndShowView();

  EXPECT_CALL(*model_delegate_mock(), NavigateToPasswordManagerSettingsPage);
  auto* manage_passwords_button =
      static_cast<views::Button*>(view()->GetViewByID(static_cast<int>(
          SuccessfulPasswordChangeView::kManagePasswordsButtonId)));
  EXPECT_TRUE(manage_passwords_button);

  views::test::ButtonTestApi(manage_passwords_button)
      .NotifyClick(ui::test::TestEvent());
}

TEST_F(SuccessfulPasswordChangeViewTest, EyeButtonClick) {
  CreateAndShowView();

  EXPECT_TRUE(GetLabelById(SuccessfulPasswordChangeView::kPasswordLabelId)
                  ->GetObscured());
  views::Button* eye_icon = static_cast<views::Button*>(
      view()->GetViewByID(SuccessfulPasswordChangeView::kEyeIconButtonId));
  EXPECT_TRUE(eye_icon);

  // Verify that auth is invoked and act like it was successful.
  EXPECT_CALL(*model_delegate_mock(), AuthenticateUserWithMessage)
      .WillOnce(base::test::RunOnceCallback<1>(true));

  views::test::ButtonTestApi(eye_icon).NotifyClick(ui::test::TestEvent());
  // Verify password is revealed.
  EXPECT_FALSE(GetLabelById(SuccessfulPasswordChangeView::kPasswordLabelId)
                   ->GetObscured());

  testing::Mock::VerifyAndClearExpectations(model_delegate_mock());
  // Auth shouldn't be invoked when hiding the password.
  EXPECT_CALL(*model_delegate_mock(), AuthenticateUserWithMessage).Times(0);
  views::test::ButtonTestApi(eye_icon).NotifyClick(ui::test::TestEvent());

  // Verify password is hidden.
  EXPECT_TRUE(GetLabelById(SuccessfulPasswordChangeView::kPasswordLabelId)
                  ->GetObscured());
}

TEST_F(SuccessfulPasswordChangeViewTest, AuthCallbackAfterViewDestruction) {
  CreateAndShowView();

  base::OnceCallback<void(bool)> captured_auth_callback;
  EXPECT_CALL(*model_delegate_mock(), AuthenticateUserWithMessage)
      .WillOnce(
          testing::WithArg<1>([&](base::OnceCallback<void(bool)> callback) {
            captured_auth_callback = std::move(callback);
          }));

  views::Button* eye_icon = static_cast<views::Button*>(
      view()->GetViewByID(SuccessfulPasswordChangeView::kEyeIconButtonId));
  EXPECT_TRUE(eye_icon);

  views::test::ButtonTestApi(eye_icon).NotifyClick(ui::test::TestEvent());
  EXPECT_TRUE(captured_auth_callback);

  // Destroy the widget and view before the async auth callback runs.
  views::Widget* widget = view()->GetWidget();
  reset_view();
  widget->CloseNow();

  // Executing the callback when the view is destroyed must not crash or trigger
  // UAF.
  std::move(captured_auth_callback).Run(true);
}
