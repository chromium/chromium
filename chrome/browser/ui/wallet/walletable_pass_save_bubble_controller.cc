// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_controller.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/wallet/walletable_pass_bubble_view_factory.h"
#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_view.h"
#include "chrome/common/url_constants.h"
#include "components/signin/public/identity_manager/identity_manager.h"
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
    WalletablePass pass,
    WalletablePassClient::WalletablePassBubbleResultCallback callback) {
  pass_ = std::move(pass);
  SetCallback(std::move(callback));
  QueueOrShowBubble();
}

const WalletablePass& WalletablePassSaveBubbleController::pass() const {
  CHECK(pass_.has_value());
  return *pass_;
}

std::u16string WalletablePassSaveBubbleController::GetPrimaryAccountEmail() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!profile) {
    return std::u16string();
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return std::u16string();
  }
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return base::UTF8ToUTF16(account_info.email);
}

base::WeakPtr<WalletablePassSaveBubbleController>
WalletablePassSaveBubbleController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
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

void WalletablePassSaveBubbleController::OnGoToWalletClicked() {
  if (Browser* browser = chrome::FindBrowserWithTab(web_contents())) {
    SetReshowOnActivation(true);
    ShowSingletonTab(browser, GURL(chrome::kWalletPassesPageURL));
  }
}

}  // namespace wallet
