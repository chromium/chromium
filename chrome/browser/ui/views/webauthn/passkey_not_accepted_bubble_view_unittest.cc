// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_not_accepted_bubble_view.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"

class PasskeyNotAcceptedBubbleViewTest : public PasswordBubbleViewTestBase {
 public:
  PasskeyNotAcceptedBubbleViewTest() = default;
  ~PasskeyNotAcceptedBubbleViewTest() override = default;

  void CreateViewAndShow() {
    CreateAnchorViewAndShow();
    view_ = new PasskeyNotAcceptedBubbleView(
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

  PasskeyNotAcceptedBubbleView* view() { return view_; }

 private:
  raw_ptr<PasskeyNotAcceptedBubbleView> view_ = nullptr;
};

TEST_F(PasskeyNotAcceptedBubbleViewTest, ShowsTitle) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowWindowTitle());
  EXPECT_FALSE(view()->GetWindowTitle().empty());
}
