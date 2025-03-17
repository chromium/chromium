// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/successful_password_change_view.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/password_manager/password_change_delegate_mock.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_change/successful_password_change_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_view_ids.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/test_event.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/button_test_api.h"
#include "url/gurl.h"

using testing::Return;
using testing::ReturnRef;

namespace {
const std::u16string kDomain = u"demo.com";
const std::u16string kTestEmail = u"elisa.buckett@gmail.com";
const std::u16string kPassword = u"cE1L45Vgxyzlu8";

}  // namespace

class SuccessfulPasswordChangeViewTest : public PasswordBubbleViewTestBase {
 public:
  SuccessfulPasswordChangeViewTest() = default;
  ~SuccessfulPasswordChangeViewTest() override = default;

  void SetUp() override {
    PasswordBubbleViewTestBase::SetUp();
    password_change_delegate_ = std::make_unique<PasswordChangeDelegateMock>();
    ON_CALL(*model_delegate_mock(), GetPasswordChangeDelegate())
        .WillByDefault(Return(password_change_delegate_.get()));
    ON_CALL(*password_change_delegate_, GetDisplayOrigin())
        .WillByDefault(Return(kDomain));
    ON_CALL(*password_change_delegate_, GetUsername())
        .WillByDefault(ReturnRef(kTestEmail));
    ON_CALL(*password_change_delegate_, GetGeneratedPassword())
        .WillByDefault(ReturnRef(kPassword));
  }

  void TearDown() override {
    view_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
    view_ = nullptr;
    PasswordBubbleViewTestBase::TearDown();
  }

  void CreateAndShowView() {
    CreateAnchorViewAndShow();

    view_ = new SuccessfulPasswordChangeView(web_contents(), anchor_view());
    views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
  }

  PasswordChangeDelegateMock* password_change_delegate() {
    return password_change_delegate_.get();
  }

  SuccessfulPasswordChangeView* view() { return view_; }

  views::Label* GetLabelById(int id) {
    return static_cast<views::Label*>(view()->GetViewByID(id));
  }

 private:
  std::unique_ptr<PasswordChangeDelegateMock> password_change_delegate_;
  raw_ptr<SuccessfulPasswordChangeView> view_;
};

TEST_F(SuccessfulPasswordChangeViewTest, BubbleLayout) {
  CreateAndShowView();
  EXPECT_EQ(view()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGED_TITLE));

  EXPECT_EQ(
      kDomain,
      GetLabelById(SuccessfulPasswordChangeView::kBodyTextLabelId)->GetText());
  EXPECT_EQ(
      kTestEmail,
      GetLabelById(SuccessfulPasswordChangeView::kUsernameLabelId)->GetText());
  EXPECT_EQ(
      kPassword,
      GetLabelById(SuccessfulPasswordChangeView::kPasswordLabelId)->GetText());

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

  EXPECT_CALL(*password_change_delegate(), Stop);
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

TEST_F(SuccessfulPasswordChangeViewTest, PasswordChangeLinkClicked) {
  CreateAndShowView();

  EXPECT_CALL(*model_delegate_mock(), NavigateToPasswordChangeSettings);
  auto* controller = static_cast<SuccessfulPasswordChangeBubbleController*>(
      static_cast<PasswordBubbleViewBase*>(view())->GetController());
  controller->NavigateToPasswordChangeSettings();
}
