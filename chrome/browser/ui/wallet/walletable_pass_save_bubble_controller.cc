// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_controller.h"

#include "base/check.h"
#include "chrome/browser/ui/wallet/walletable_pass_bubble_view_factory.h"
#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_view.h"
#include "components/tabs/public/tab_interface.h"
#include "components/wallet/core/browser/walletable_pass_client.h"
#include "content/public/browser/web_contents.h"

namespace wallet {

WalletablePassSaveBubbleController::WalletablePassSaveBubbleController(
    tabs::TabInterface* tab)
    : WalletablePassBubbleControllerBase(tab) {}

WalletablePassSaveBubbleController::~WalletablePassSaveBubbleController() =
    default;

autofill::BubbleType WalletablePassSaveBubbleController::GetBubbleType() const {
  return autofill::BubbleType::kWalletablePassSave;
}

void WalletablePassSaveBubbleController::ShowBubble() {
  SetBubbleView(*WalletablePassBubbleViewFactory::CreateSaveBubbleView(
      web_contents(), this));
}

void WalletablePassSaveBubbleController::SetUpAndShowSaveBubble(
    const optimization_guide::proto::WalletablePass& pass,
    WalletablePassClient::WalletablePassBubbleResultCallback callback) {
  pass_ = pass;
  SetCallback(std::move(callback));
  QueueOrShowBubble();
}

const optimization_guide::proto::WalletablePass&
WalletablePassSaveBubbleController::pass() const {
  CHECK(pass_.has_value());
  return *pass_;
}

base::WeakPtr<autofill::BubbleControllerBase>
WalletablePassSaveBubbleController::GetBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<WalletablePassBubbleControllerBase>
WalletablePassSaveBubbleController::
    GetWalletablePassBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace wallet
