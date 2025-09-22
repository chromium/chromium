// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_bubble_controller_base.h"

#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "chrome/browser/ui/wallet/walletable_pass_bubble_view_base.h"

namespace wallet {

inline WalletablePassBubbleControllerBase::
    WalletablePassBubbleControllerBase() = default;

WalletablePassBubbleControllerBase::~WalletablePassBubbleControllerBase() =
    default;

bool WalletablePassBubbleControllerBase::IsShowingBubble() const {
  return bubble_view_ != nullptr;
}

bool WalletablePassBubbleControllerBase::IsMouseHovered() const {
  return IsShowingBubble() && bubble_view_->IsMouseHovered();
}

void WalletablePassBubbleControllerBase::OnBubbleClosed(
    WalletablePassBubbleClosedReason reason) {
  // TODO(crbug.com/441830204): Null the pointer & inform the callback of the
  // result.
}

void WalletablePassBubbleControllerBase::SetBubbleView(
    WalletablePassBubbleViewBase& bubble_view) {
  bubble_view_ = &bubble_view;
}

}  // namespace wallet
