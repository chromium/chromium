// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/move_to_account_store_bubble_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_target.h"
#include "ui/events/event_target_iterator.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"

class MoveToAccountStoreBubbleViewTest : public PasswordBubbleViewTestBase {
 public:
  MoveToAccountStoreBubbleViewTest() {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kEnablePasswordsAccountStorage);

    password_manager::PasswordForm pending_password;
    pending_password.url = GURL("www.example.com");
    ON_CALL(*model_delegate_mock(), GetPendingPassword)
        .WillByDefault(testing::ReturnRef(pending_password));
  }
  ~MoveToAccountStoreBubbleViewTest() override = default;

  void CreateViewAndShow();

  void TearDown() override;

 protected:
  base::test::ScopedFeatureList feature_list_;
  MoveToAccountStoreBubbleView* view_;
};

void MoveToAccountStoreBubbleViewTest::CreateViewAndShow() {
  // The move bubble is shown only to signed in users. Make sure there is one.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo info =
      signin::MakePrimaryAccountAvailable(identity_manager, "test@email.com");

  CreateAnchorViewAndShow();

  view_ = new MoveToAccountStoreBubbleView(web_contents(), anchor_view());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

void MoveToAccountStoreBubbleViewTest::TearDown() {
  view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);

  PasswordBubbleViewTestBase::TearDown();
}

TEST_F(MoveToAccountStoreBubbleViewTest, HasTwoButtons) {
  CreateViewAndShow();
  ASSERT_TRUE(view_->GetOkButton());
  ASSERT_TRUE(view_->GetCancelButton());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MOVE_BUBBLE_OK_BUTTON),
      view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MOVE_BUBBLE_CANCEL_BUTTON),
      view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL));
}

TEST_F(MoveToAccountStoreBubbleViewTest, HasDescription) {
  CreateViewAndShow();

  ASSERT_EQ(view_->children().size(), 2u);
  views::Label* description =
      static_cast<views::Label*>(*view_->children().begin());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MOVE_HINT),
            description->GetText());
}
