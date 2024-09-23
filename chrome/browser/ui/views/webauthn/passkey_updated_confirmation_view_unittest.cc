// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_updated_confirmation_view.h"

#include <utility>

#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"

class PasskeyUpdatedConfirmationViewTest : public PasswordBubbleViewTestBase {
 public:
  PasskeyUpdatedConfirmationViewTest() = default;
  ~PasskeyUpdatedConfirmationViewTest() override = default;

  void CreateViewAndShow() {
    CreateAnchorViewAndShow();
    view_ = new PasskeyUpdatedConfirmationView(
        web_contents(), anchor_view(),
        LocationBarBubbleDelegateView::AUTOMATIC);
    views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
  }

  void TearDown() override {
    std::exchange(view_, nullptr)
        ->GetWidget()
        ->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
    PasswordBubbleViewTestBase::TearDown();
  }

  PasskeyUpdatedConfirmationView* view() { return view_; }

 private:
  raw_ptr<PasskeyUpdatedConfirmationView> view_ = nullptr;
};

TEST_F(PasskeyUpdatedConfirmationViewTest, ShowsTitle) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowWindowTitle());
  EXPECT_FALSE(view()->GetWindowTitle().empty());
}
