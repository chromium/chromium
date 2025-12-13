// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_BUBBLE_CONTROLLER_BASE_H_
#define CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_BUBBLE_CONTROLLER_BASE_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/autofill/bubble_controller_base.h"
#include "components/tabs/public/tab_interface.h"
#include "components/wallet/core/browser/walletable_pass_client.h"
#include "content/public/browser/web_contents.h"

namespace wallet {

class WalletablePassBubbleViewBase;

// Base class for the controller of the walletable pass bubble. This controller
// is responsible for showing, hiding, and handling user interactions with the
// bubble.
class WalletablePassBubbleControllerBase
    : public autofill::BubbleControllerBase {
 public:
  enum WalletablePassBubbleClosedReason {
    kUnknown = 0,
    kLostFocus = 1,
    kClosed = 2,
    kAccepted = 3,
    kDeclined = 4,
    kMaxValue = kDeclined
  };

  explicit WalletablePassBubbleControllerBase(tabs::TabInterface* tab);

  ~WalletablePassBubbleControllerBase() override;

  WalletablePassBubbleControllerBase(
      const WalletablePassBubbleControllerBase&) = delete;

  WalletablePassBubbleControllerBase& operator=(
      const WalletablePassBubbleControllerBase&) = delete;

  // BubbleControllerBase:
  void OnBubbleDiscarded() override;
  bool CanBeReshown() const override;
  void HideBubble(bool initiated_by_bubble_manager) override;
  bool IsShowingBubble() const override;
  bool IsMouseHovered() const override;

  // Called when the bubble is closed.
  void OnBubbleClosed(WalletablePassBubbleClosedReason reason);

  virtual base::WeakPtr<WalletablePassBubbleControllerBase>
  GetWalletablePassBubbleControllerBaseWeakPtr() = 0;

 protected:
  void SetBubbleView(WalletablePassBubbleViewBase& bubble_view);

  void SetCallback(
      WalletablePassClient::WalletablePassBubbleResultCallback callback);

  // Sets whether the bubble should be reshown when the tab is activated.
  void SetReshowOnActivation(bool reshow);

  void QueueOrShowBubble(bool force_show = false);

  void ResetBubbleViewAndInformBubbleManager();

  content::WebContents* web_contents() { return tab_->GetContents(); }

 private:
  tabs::TabInterface& tab() { return tab_.get(); }

  void OnTabActivated(tabs::TabInterface* tab);

  // Weak reference. Will be nullptr if no bubble is currently shown.
  raw_ptr<WalletablePassBubbleViewBase> bubble_view_ = nullptr;

  // The tab that the bubble is associated with.
  const raw_ref<tabs::TabInterface> tab_;

  WalletablePassClient::WalletablePassBubbleResultCallback callback_;

  // If true, the bubble will be reshown when the tab is activated.
  bool reshow_bubble_on_activation_ = false;

  base::CallbackListSubscription tab_activation_subscription_;
};

}  // namespace wallet

#endif  // CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_BUBBLE_CONTROLLER_BASE_H_
