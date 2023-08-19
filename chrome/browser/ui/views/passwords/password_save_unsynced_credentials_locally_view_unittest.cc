// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_save_unsynced_credentials_locally_view.h"

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "components/password_manager/core/browser/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;
using testing::ReturnRef;

class PasswordSaveUnsyncedCredentialsLocallyViewTest
    : public PasswordBubbleViewTestBase {
 public:
  PasswordSaveUnsyncedCredentialsLocallyViewTest() {
    ON_CALL(*model_delegate_mock(), GetUnsyncedCredentials())
        .WillByDefault(ReturnRef(unsynced_credentials_));

    unsynced_credentials_.resize(1);
    unsynced_credentials_[0].username_value = u"user";
    unsynced_credentials_[0].password_value = u"password";
  }
  ~PasswordSaveUnsyncedCredentialsLocallyViewTest() override = default;

  void CreateViewAndShow();

  void TearDown() override;

 protected:
  raw_ptr<PasswordSaveUnsyncedCredentialsLocallyView> view_ = nullptr;
  std::vector<password_manager::PasswordForm> unsynced_credentials_;
};

void PasswordSaveUnsyncedCredentialsLocallyViewTest::CreateViewAndShow() {
  CreateAnchorViewAndShow();

  view_ = new PasswordSaveUnsyncedCredentialsLocallyView(web_contents(),
                                                         anchor_view());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

void PasswordSaveUnsyncedCredentialsLocallyViewTest::TearDown() {
  std::exchange(view_, nullptr)
      ->GetWidget()
      ->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);

  PasswordBubbleViewTestBase::TearDown();
}

TEST_F(PasswordSaveUnsyncedCredentialsLocallyViewTest, HasTitleAndTwoButtons) {
  CreateViewAndShow();
  EXPECT_TRUE(view_->ShouldShowWindowTitle());
  EXPECT_TRUE(view_->GetOkButton());
  EXPECT_TRUE(view_->GetCancelButton());
}
