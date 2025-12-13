// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_BUBBLE_VIEW_FACTORY_H_
#define CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_BUBBLE_VIEW_FACTORY_H_

namespace content {
class WebContents;
}  // namespace content

namespace wallet {

class WalletablePassConsentBubbleView;
class WalletablePassConsentBubbleController;
class WalletablePassSaveBubbleView;
class WalletablePassSaveBubbleController;

// Factory for creating walletable pass UI elements.
class WalletablePassBubbleViewFactory {
 public:
  // Creates and returns a `WalletablePassConsentBubbleView`.
  static WalletablePassConsentBubbleView* CreateConsentBubbleView(
      content::WebContents* web_contents,
      WalletablePassConsentBubbleController* controller);

  // Creates and returns a `WalletablePassSaveBubbleView`.
  static WalletablePassSaveBubbleView* CreateSaveBubbleView(
      content::WebContents* web_contents,
      WalletablePassSaveBubbleController* controller);
};

}  // namespace wallet

#endif  // CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_BUBBLE_VIEW_FACTORY_H_
