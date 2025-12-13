// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_BUBBLE_VIEW_BASE_H_
#define CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_BUBBLE_VIEW_BASE_H_

#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class View;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

namespace wallet {

class WalletablePassBubbleControllerBase;

// Base view for the walletable pass bubble. This bubble is shown when a user
// visits a website that has a walletable pass.
class WalletablePassBubbleViewBase : public LocationBarBubbleDelegateView {
  METADATA_HEADER(WalletablePassBubbleViewBase, LocationBarBubbleDelegateView)

 public:
  WalletablePassBubbleViewBase(views::View* anchor_view,
                               content::WebContents* web_contents,
                               WalletablePassBubbleControllerBase* controller);

  ~WalletablePassBubbleViewBase() override;

  // views::BubbleDialogDelegateView:
  void WindowClosing() override;

  // Called from controller to check if the mouse is hovering over the view.
  bool IsMouseHovered() const;

  using LocationBarBubbleDelegateView::CloseBubble;

 private:
  base::WeakPtr<WalletablePassBubbleControllerBase> controller_;
};

}  // namespace wallet

#endif  // CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_BUBBLE_VIEW_BASE_H_
