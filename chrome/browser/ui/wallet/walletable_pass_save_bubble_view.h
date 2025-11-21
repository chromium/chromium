// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_SAVE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_SAVE_BUBBLE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/wallet/walletable_pass_bubble_view_base.h"
#include "components/optimization_guide/proto/features/walletable_pass_extraction.pb.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class StyledLabel;
class View;
}  // namespace views

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

  // LocationBarBubbleDelegateView:
  void AddedToWidget() override;

 private:
  std::unique_ptr<views::StyledLabel> GetSubtitleLabel();

  std::unique_ptr<views::BoxLayoutView> GetAttributesView();

  std::unique_ptr<views::BoxLayoutView> GetLoyaltyCardAttributesView();

  std::unique_ptr<views::BoxLayoutView> GetEventPassAttributesView();

  int GetDialogTitleResourceId() const;

  int GetHeaderImageResourceId() const;

  void OnGoToWalletClicked();

  base::WeakPtr<WalletablePassSaveBubbleController> controller_;
};

}  // namespace wallet

#endif  // CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_SAVE_BUBBLE_VIEW_H_
