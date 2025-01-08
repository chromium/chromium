// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/failed_password_change_view.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/password_manager/password_change_delegate_mock.h"
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
    view_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
    view_ = nullptr;
    PasswordBubbleViewTestBase::TearDown();
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
