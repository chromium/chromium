// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_saved_confirmation_view.h"

#include <utility>

#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"

class PasskeySavedConfirmationViewTest : public PasswordBubbleViewTestBase {
 public:
  PasskeySavedConfirmationViewTest() = default;
  ~PasskeySavedConfirmationViewTest() override = default;

  void CreateViewAndShow() {
    CreateAnchorViewAndShow();
    view_ = new PasskeySavedConfirmationView(web_contents(), anchor_view());
    views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
  }

  void TearDown() override {
    std::exchange(view_, nullptr)
        ->GetWidget()
        ->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
    PasswordBubbleViewTestBase::TearDown();
  }

  PasskeySavedConfirmationView* view() { return view_; }

 private:
  raw_ptr<PasskeySavedConfirmationView> view_ = nullptr;
};

TEST_F(PasskeySavedConfirmationViewTest, ShowsTitle) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowWindowTitle());
}
