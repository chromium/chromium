// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/privacy_notice_view.h"

#include "chrome/browser/password_manager/password_change_delegate_mock.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/test_event.h"
#include "ui/views/test/button_test_api.h"

using testing::Return;
using testing::ReturnRef;

class PrivacyNoticeViewTest : public PasswordBubbleViewTestBase {
 public:
  PrivacyNoticeViewTest() = default;
  ~PrivacyNoticeViewTest() override = default;

  void SetUp() override {
    PasswordBubbleViewTestBase::SetUp();
    password_change_delegate_ = std::make_unique<PasswordChangeDelegateMock>();
    ON_CALL(*model_delegate_mock(), GetPasswordChangeDelegate())
        .WillByDefault(Return(password_change_delegate_.get()));
  }

  void TearDown() override {
    view_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
    view_ = nullptr;
    PasswordBubbleViewTestBase::TearDown();
  }

  void CreateAndShowView() {
    CreateAnchorViewAndShow();

    ON_CALL(*password_change_delegate_, GetCurrentState())
        .WillByDefault(
            Return(PasswordChangeDelegate::State::kWaitingForAgreement));
    view_ = new PrivacyNoticeView(web_contents(), anchor_view());
    views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
  }

  PasswordChangeDelegateMock* password_change_delegate() {
    return password_change_delegate_.get();
  }

  PrivacyNoticeView* view() { return view_; }

 private:
  std::unique_ptr<PasswordChangeDelegateMock> password_change_delegate_;
  raw_ptr<PrivacyNoticeView> view_;
};

TEST_F(PrivacyNoticeViewTest, AcceptClosesTheBubbleAndTriggersDelegate) {
  CreateAndShowView();

  EXPECT_CALL(*password_change_delegate(), OnPrivacyNoticeAccepted);
  EXPECT_CALL(*model_delegate_mock(), OnBubbleHidden);
  views::test::ButtonTestApi(view()->GetOkButton())
      .NotifyClick(ui::test::TestEvent());
}

TEST_F(PrivacyNoticeViewTest, CancelClosesTheBubbleAndCancelsTheFlow) {
  CreateAndShowView();

  EXPECT_CALL(*password_change_delegate(), Stop);
  EXPECT_CALL(*model_delegate_mock(), OnBubbleHidden);
  views::test::ButtonTestApi(view()->GetCancelButton())
      .NotifyClick(ui::test::TestEvent());
}
