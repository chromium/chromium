// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/failed_password_change_view.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/password_manager/password_change_delegate_mock.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_change/failed_password_change_bubble_controller.h"
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

class FailedPasswordChangeViewTest : public PasswordBubbleViewTestBase {
 public:
  FailedPasswordChangeViewTest() = default;
  ~FailedPasswordChangeViewTest() override = default;

  void SetUp() override {
    PasswordBubbleViewTestBase::SetUp();
    password_change_delegate_ = std::make_unique<PasswordChangeDelegateMock>();
    ON_CALL(*model_delegate_mock(), GetPasswordChangeDelegate())
        .WillByDefault(Return(password_change_delegate_.get()));
  }

  void TearDown() override {
    if (view_) {
      CloseBubble();
    }
    PasswordBubbleViewTestBase::TearDown();
  }

  void CloseBubble(views::Widget::ClosedReason reason =
                       views::Widget::ClosedReason::kUnspecified) {
    view_->GetWidget()->CloseWithReason(reason);
    view_ = nullptr;
  }

  void CreateAndShowView() {
    CreateAnchorViewAndShow();

    view_ = new FailedPasswordChangeView(web_contents(), anchor_view());
    views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
  }

  PasswordChangeDelegateMock* password_change_delegate() {
    return password_change_delegate_.get();
  }

  FailedPasswordChangeView* view() { return view_; }

 private:
  std::unique_ptr<PasswordChangeDelegateMock> password_change_delegate_;
  raw_ptr<FailedPasswordChangeView> view_;
};

TEST_F(FailedPasswordChangeViewTest, BubbleLayout) {
  CreateAndShowView();
  EXPECT_EQ(view()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_FAILED_TITLE));

  EXPECT_TRUE(view()->GetOkButton());
  EXPECT_EQ(view()->GetOkButton()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_FAILED_ACTION));
}

TEST_F(FailedPasswordChangeViewTest, FixNowClick) {
  CreateAndShowView();

  EXPECT_CALL(*password_change_delegate(), OpenPasswordChangeTab);
  EXPECT_CALL(*password_change_delegate(), Stop);
  EXPECT_CALL(*model_delegate_mock(), OnBubbleHidden);

  views::test::ButtonTestApi(view()->GetOkButton())
      .NotifyClick(ui::test::TestEvent());
}

TEST_F(FailedPasswordChangeViewTest, SuppressingBubbleDoesNotStopTheFlow) {
  CreateAndShowView();

  EXPECT_CALL(*password_change_delegate(), Stop).Times(0);
  EXPECT_CALL(*model_delegate_mock(), OnBubbleHidden);

  CloseBubble();
}

TEST_F(FailedPasswordChangeViewTest, ClosingBubbleStopsTheFlow) {
  CreateAndShowView();

  EXPECT_CALL(*password_change_delegate(), Stop);
  EXPECT_CALL(*model_delegate_mock(), OnBubbleHidden);

  CloseBubble(views::Widget::ClosedReason::kCloseButtonClicked);
}

TEST_F(FailedPasswordChangeViewTest, PasswordChangeLinkClicked) {
  CreateAndShowView();

  EXPECT_CALL(*model_delegate_mock(), NavigateToPasswordChangeSettings);
  auto* controller = static_cast<FailedPasswordChangeBubbleController*>(
      static_cast<PasswordBubbleViewBase*>(view())->GetController());
  controller->NavigateToPasswordChangeSettings();
}
