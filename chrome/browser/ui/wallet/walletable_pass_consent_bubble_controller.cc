// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_consent_bubble_controller.h"

#include "chrome/browser/ui/wallet/walletable_pass_consent_bubble_view.h"
#include "content/public/browser/web_contents.h"

namespace wallet {

WalletablePassConsentBubbleController::WalletablePassConsentBubbleController(
    tabs::TabInterface* tab)
    : WalletablePassBubbleControllerBase(tab) {}

WalletablePassConsentBubbleController::
    ~WalletablePassConsentBubbleController() = default;

autofill::BubbleType WalletablePassConsentBubbleController::GetBubbleType()
    const {
  return autofill::BubbleType::kWalletablePassConsent;
}

void WalletablePassConsentBubbleController::ShowBubble() {
  // TODO(crbug.com/445826875): Create and set the actual bubble view here.
}

void WalletablePassConsentBubbleController::SetUpAndShowConsentBubble(
    WalletablePassClient::WalletablePassBubbleResultCallback callback) {
  SetCallback(std::move(callback));
  QueueOrShowBubble();
}

base::WeakPtr<autofill::BubbleControllerBase>
WalletablePassConsentBubbleController::GetBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<WalletablePassBubbleControllerBase>
WalletablePassConsentBubbleController::
    GetWalletablePassBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace wallet
