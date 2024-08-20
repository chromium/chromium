// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/move_to_account_store_bubble_view.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/events/event_target.h"
#include "ui/events/event_target_iterator.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"

class MoveToAccountStoreBubbleViewTest : public PasswordBubbleViewTestBase {
 public:
  MoveToAccountStoreBubbleViewTest() {
    password_manager::PasswordForm pending_password;
    pending_password.url = GURL("www.example.com");
    ON_CALL(*model_delegate_mock(), GetPendingPassword)
        .WillByDefault(testing::ReturnRef(pending_password));
  }
  ~MoveToAccountStoreBubbleViewTest() override = default;

  void CreateViewAndShow();

  void TearDown() override;

 protected:
  raw_ptr<MoveToAccountStoreBubbleView> view_ = nullptr;
};

void MoveToAccountStoreBubbleViewTest::CreateViewAndShow() {
  // The move bubble is shown only to signed in users. Make sure there is one.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager, "test@email.com", signin::ConsentLevel::kSync);

  CreateAnchorViewAndShow();

  view_ = new MoveToAccountStoreBubbleView(web_contents(), anchor_view());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

void MoveToAccountStoreBubbleViewTest::TearDown() {
  std::exchange(view_, nullptr)
      ->GetWidget()
      ->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);

  PasswordBubbleViewTestBase::TearDown();
}

TEST_F(MoveToAccountStoreBubbleViewTest, HasTwoButtons) {
  CreateViewAndShow();
  ASSERT_TRUE(view_->GetOkButton());
  ASSERT_TRUE(view_->GetCancelButton());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_SAVE_IN_ACCOUNT_BUBBLE_SAVE_BUTTON),
            view_->GetDialogButtonLabel(ui::mojom::DialogButton::kOk));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MOVE_BUBBLE_CANCEL_BUTTON),
      view_->GetDialogButtonLabel(ui::mojom::DialogButton::kCancel));
}

TEST_F(MoveToAccountStoreBubbleViewTest, HasDescription) {
  CreateViewAndShow();

  ASSERT_EQ(view_->children().size(), 2u);
  views::Label* description =
      static_cast<views::Label*>(*view_->children().begin());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_PASSWORD_MANAGER_SAVE_IN_ACCOUNT_BUBBLE_DESCRIPTION,
                u"test@email.com"),
            description->GetText());
}
