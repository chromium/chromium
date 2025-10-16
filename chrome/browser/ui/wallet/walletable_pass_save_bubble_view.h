// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_SAVE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_SAVE_BUBBLE_VIEW_H_

#include "chrome/browser/ui/wallet/walletable_pass_bubble_view_base.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace wallet {

class WalletablePassSaveBubbleController;

// This bubble view is displayed when a walletable pass is found. It allows the
// user to save the pass to their wallet.
class WalletablePassSaveBubbleView : public WalletablePassBubbleViewBase {
  METADATA_HEADER(WalletablePassSaveBubbleView, WalletablePassBubbleViewBase)

 public:
  WalletablePassSaveBubbleView(views::View* anchor_view,
                               content::WebContents* web_contents,
                               WalletablePassSaveBubbleController* controller);
  ~WalletablePassSaveBubbleView() override;
};

}  // namespace wallet

#endif  // CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_SAVE_BUBBLE_VIEW_H_
