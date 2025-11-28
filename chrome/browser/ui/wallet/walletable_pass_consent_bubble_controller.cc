// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_consent_bubble_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/wallet/walletable_pass_bubble_view_factory.h"
#include "chrome/browser/ui/wallet/walletable_pass_consent_bubble_view.h"
#include "chrome/common/url_constants.h"
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
  SetBubbleView(*WalletablePassBubbleViewFactory::CreateConsentBubbleView(
      web_contents(), this));
}

void WalletablePassConsentBubbleController::SetUpAndShowConsentBubble(
    optimization_guide::proto::PassCategory pass_category,
    WalletablePassClient::WalletablePassBubbleResultCallback callback) {
  pass_category_ = pass_category;
  SetCallback(std::move(callback));
  QueueOrShowBubble();
}

optimization_guide::proto::PassCategory
WalletablePassConsentBubbleController::pass_category() const {
  CHECK(pass_category_.has_value());
  return *pass_category_;
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

base::WeakPtr<WalletablePassConsentBubbleController>
WalletablePassConsentBubbleController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WalletablePassConsentBubbleController::OnLearnMoreClicked() {
  if (Browser* browser = chrome::FindBrowserWithTab(web_contents())) {
    SetReshowOnActivation(true);
    chrome::ShowSettingsSubPage(browser, chrome::kAutofillAiSubPage);
  }
}

}  // namespace wallet
