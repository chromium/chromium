// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/post_save_compromised_bubble_view.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"

namespace {
using testing::Return;

class PostSaveCompromisedBubbleViewTest : public PasswordBubbleViewTestBase {
 public:
  PostSaveCompromisedBubbleViewTest() = default;
  ~PostSaveCompromisedBubbleViewTest() override = default;

  void CreateViewAndShow(password_manager::ui::State state);

  void TearDown() override;

 protected:
  raw_ptr<PostSaveCompromisedBubbleView> view_ = nullptr;
};

void PostSaveCompromisedBubbleViewTest::CreateViewAndShow(
    password_manager::ui::State state) {
  CreateAnchorViewAndShow();

  EXPECT_CALL(*model_delegate_mock(), GetState).WillOnce(Return(state));
  view_ = new PostSaveCompromisedBubbleView(web_contents(), anchor_view());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

void PostSaveCompromisedBubbleViewTest::TearDown() {
  std::exchange(view_, nullptr)
      ->GetWidget()
      ->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);

  PasswordBubbleViewTestBase::TearDown();
}

TEST_F(PostSaveCompromisedBubbleViewTest, SafeState) {
  CreateViewAndShow(password_manager::ui::PASSWORD_UPDATED_SAFE_STATE);
  EXPECT_FALSE(view_->GetOkButton());
  EXPECT_FALSE(view_->GetCancelButton());
}

TEST_F(PostSaveCompromisedBubbleViewTest, MoreToFixState) {
  CreateViewAndShow(password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX);
  EXPECT_TRUE(view_->GetOkButton());
  EXPECT_FALSE(view_->GetCancelButton());

  EXPECT_CALL(*model_delegate_mock(), NavigateToPasswordCheckup);
  view_->AcceptDialog();
}

}  // namespace
