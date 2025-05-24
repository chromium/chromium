// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_deleted_confirmation_view.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

namespace {

class PasskeyDeletedConfirmationViewTest : public PasswordBubbleViewTestBase {
 public:
  PasskeyDeletedConfirmationViewTest() = default;
  ~PasskeyDeletedConfirmationViewTest() override = default;

  void CreateViewAndShow() {
    CreateAnchorViewAndShow();
    view_ = new PasskeyDeletedConfirmationView(
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

  PasskeyDeletedConfirmationView* view() { return view_; }

 private:
  raw_ptr<PasskeyDeletedConfirmationView> view_ = nullptr;
};

TEST_F(PasskeyDeletedConfirmationViewTest, ShowsTitle) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowWindowTitle());
  EXPECT_FALSE(view()->GetWindowTitle().empty());
}

}  // namespace
