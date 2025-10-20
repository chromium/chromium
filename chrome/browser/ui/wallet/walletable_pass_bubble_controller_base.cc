// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_bubble_controller_base.h"

#include "base/check_deref.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "chrome/browser/ui/wallet/walletable_pass_bubble_view_base.h"
#include "components/autofill/core/common/autofill_features.h"

namespace wallet {
namespace {

using enum WalletablePassBubbleControllerBase::WalletablePassBubbleClosedReason;

WalletablePassClient::WalletablePassBubbleResult GetResult(
    WalletablePassBubbleControllerBase::WalletablePassBubbleClosedReason
        close_reason) {
  switch (close_reason) {
    case kUnknown:
      return WalletablePassClient::WalletablePassBubbleResult::kUnknown;
    case kLostFocus:
      return WalletablePassClient::WalletablePassBubbleResult::kLostFocus;
    case kClosed:
      return WalletablePassClient::WalletablePassBubbleResult::kClosed;
    case kAccepted:
      return WalletablePassClient::WalletablePassBubbleResult::kAccepted;
    case kDeclined:
      return WalletablePassClient::WalletablePassBubbleResult::kDeclined;
  }
}

}  // namespace

WalletablePassBubbleControllerBase::WalletablePassBubbleControllerBase(
    tabs::TabInterface* tab)
    : tab_(CHECK_DEREF(tab)) {}

WalletablePassBubbleControllerBase::~WalletablePassBubbleControllerBase() =
    default;

bool WalletablePassBubbleControllerBase::CanBeReshown() const {
  return true;
}

bool WalletablePassBubbleControllerBase::IsShowingBubble() const {
  return bubble_view_ != nullptr;
}

void WalletablePassBubbleControllerBase::HideBubble(
    bool initiated_by_bubble_manager) {
  if (IsShowingBubble()) {
    bubble_view_->CloseBubble();
    ResetBubbleViewAndInformBubbleManager();
  }
}

bool WalletablePassBubbleControllerBase::IsMouseHovered() const {
  return IsShowingBubble() && bubble_view_->IsMouseHovered();
}

void WalletablePassBubbleControllerBase::OnBubbleClosed(
    WalletablePassBubbleClosedReason reason) {
  // TODO(crbug.com/432429605): BubbleManager can show and hide the bubble
  // multiple times. The callback should run only on user action.
  if (callback_) {
    std::move(callback_).Run(GetResult(reason));
  }
  ResetBubbleViewAndInformBubbleManager();
}

void WalletablePassBubbleControllerBase::SetBubbleView(
    WalletablePassBubbleViewBase& bubble_view) {
  bubble_view_ = &bubble_view;
}

void WalletablePassBubbleControllerBase::SetCallback(
    WalletablePassClient::WalletablePassBubbleResultCallback callback) {
  callback_ = std::move(callback);
}

void WalletablePassBubbleControllerBase::QueueOrShowBubble(bool force_show) {
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillShowBubblesBasedOnPriorities)) {
    if (autofill::BubbleManager* manager =
            autofill::BubbleManager::GetForTab(&tab())) {
      manager->RequestShowController(*this, force_show);
    }
    return;
  }

  ShowBubble();
}

void WalletablePassBubbleControllerBase::
    ResetBubbleViewAndInformBubbleManager() {
  if (IsShowingBubble() &&
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillShowBubblesBasedOnPriorities)) {
    if (autofill::BubbleManager* manager =
            autofill::BubbleManager::GetForTab(&tab())) {
      manager->OnBubbleHiddenByController(*this, /*show_next_bubble=*/true);
    }
  }
  bubble_view_ = nullptr;
}

}  // namespace wallet
