// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_CONSENT_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_CONSENT_BUBBLE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/wallet/walletable_pass_bubble_controller_base.h"

namespace tabs {
class TabInterface;
}  // namespace tabs
namespace wallet {

// Manages the walletable pass consent bubble.
//
// A WalletablePassConsentBubbleController is responsible for showing a bubble
// that asks for the user's consent to save a walletable pass to their Google
// Wallet.
class WalletablePassConsentBubbleController
    : public WalletablePassBubbleControllerBase {
 public:
  explicit WalletablePassConsentBubbleController(tabs::TabInterface* tab);
  ~WalletablePassConsentBubbleController() override;

  // BubbleControllerBase:
  autofill::BubbleType GetBubbleType() const override;
  base::WeakPtr<autofill::BubbleControllerBase> GetBubbleControllerBaseWeakPtr()
      override;

  // WalletablePassBubbleControllerBase:
  base::WeakPtr<WalletablePassBubbleControllerBase>
  GetWalletablePassBubbleControllerBaseWeakPtr() override;

  // Shows the consent bubble. `callback` will be run when the user makes a
  // decision.
  void SetUpAndShowConsentBubble(
      optimization_guide::proto::PassCategory pass_category,
      WalletablePassClient::WalletablePassBubbleResultCallback callback);

  optimization_guide::proto::PassCategory pass_category() const;

  base::WeakPtr<WalletablePassConsentBubbleController> GetWeakPtr();

  void OnLearnMoreClicked();

 protected:
  void ShowBubble() override;

 private:
  std::optional<optimization_guide::proto::PassCategory> pass_category_;

  base::WeakPtrFactory<WalletablePassConsentBubbleController> weak_ptr_factory_{
      this};
};

}  // namespace wallet

#endif  // CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_CONSENT_BUBBLE_CONTROLLER_H_
