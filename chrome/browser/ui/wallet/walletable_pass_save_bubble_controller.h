// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_SAVE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_SAVE_BUBBLE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/wallet/walletable_pass_bubble_controller_base.h"
#include "components/optimization_guide/proto/features/walletable_pass_extraction.pb.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace wallet {

// Manages the walletable pass save bubble.
//
// A WalletablePassSaveBubbleController is responsible for showing a bubble that
// allows the user to save a walletable pass to their Google Wallet.
class WalletablePassSaveBubbleController
    : public WalletablePassBubbleControllerBase {
 public:
  explicit WalletablePassSaveBubbleController(tabs::TabInterface* tab);
  ~WalletablePassSaveBubbleController() override;

  // BubbleControllerBase:
  autofill::BubbleType GetBubbleType() const override;
  base::WeakPtr<autofill::BubbleControllerBase> GetBubbleControllerBaseWeakPtr()
      override;

  // WalletablePassBubbleControllerBase:
  base::WeakPtr<WalletablePassBubbleControllerBase>
  GetWalletablePassBubbleControllerBaseWeakPtr() override;

  // Shows the save bubble. `callback` will be run when the user makes a
  // decision.
  void SetUpAndShowSaveBubble(
      WalletablePass pass,
      WalletablePassClient::WalletablePassBubbleResultCallback callback);

  const WalletablePass& pass() const;

  // Returns the primary account email of the user.
  std::u16string GetPrimaryAccountEmail();

  base::WeakPtr<WalletablePassSaveBubbleController> GetWeakPtr();

  void OnGoToWalletClicked();

 protected:
  void ShowBubble() override;

 private:
  std::optional<WalletablePass> pass_;

  base::WeakPtrFactory<WalletablePassSaveBubbleController> weak_ptr_factory_{
      this};
};

}  // namespace wallet

#endif  // CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_SAVE_BUBBLE_CONTROLLER_H_
