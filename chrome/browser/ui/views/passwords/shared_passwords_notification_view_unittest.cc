// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/shared_passwords_notification_view.h"

#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "ui/events/test/test_event.h"
#include "ui/views/test/button_test_api.h"

class SharedPasswordsNotificationViewTest : public PasswordBubbleViewTestBase {
 public:
  SharedPasswordsNotificationViewTest() = default;
  ~SharedPasswordsNotificationViewTest() override = default;

  void CreateViewAndShow();
  void SetUp() override;
  void TearDown() override;

 protected:
  raw_ptr<SharedPasswordsNotificationView> view_ = nullptr;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> current_forms_;
};

void SharedPasswordsNotificationViewTest::CreateViewAndShow() {
  CreateAnchorViewAndShow();

  view_ = new SharedPasswordsNotificationView(web_contents(), anchor_view());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

void SharedPasswordsNotificationViewTest::SetUp() {
  PasswordBubbleViewTestBase::SetUp();
  ON_CALL(*model_delegate_mock(), GetState)
      .WillByDefault(testing::Return(
          password_manager::ui::NOTIFY_RECEIVED_SHARED_CREDENTIALS));
  ON_CALL(*model_delegate_mock(), GetCurrentForms)
      .WillByDefault(testing::ReturnRef(current_forms_));
}

void SharedPasswordsNotificationViewTest::TearDown() {
  view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  view_ = nullptr;
  PasswordBubbleViewTestBase::TearDown();
}

TEST_F(SharedPasswordsNotificationViewTest, HasTwoButtons) {
  CreateViewAndShow();
  EXPECT_TRUE(view_->GetOkButton());
  EXPECT_TRUE(view_->GetCancelButton());
}

TEST_F(SharedPasswordsNotificationViewTest,
       ShouldCloseBubbleUponClickOnGotItButton) {
  CreateViewAndShow();
  EXPECT_CALL(*model_delegate_mock(), OnBubbleHidden);
  views::test::ButtonTestApi(view_->GetOkButton())
      .NotifyClick(ui::test::TestEvent());
}

TEST_F(SharedPasswordsNotificationViewTest,
       ShouldNavigateToSettingsUponClickOnManagePasswordsButton) {
  CreateViewAndShow();
  EXPECT_CALL(*model_delegate_mock(),
              NavigateToPasswordManagerSettingsPage(
                  password_manager::ManagePasswordsReferrer::
                      kSharedPasswordsNotificationBubble));
  views::test::ButtonTestApi(view_->GetCancelButton())
      .NotifyClick(ui::test::TestEvent());
}
