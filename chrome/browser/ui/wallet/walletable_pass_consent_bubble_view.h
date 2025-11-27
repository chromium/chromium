// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_CONSENT_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_CONSENT_BUBBLE_VIEW_H_

#include "chrome/browser/ui/wallet/walletable_pass_bubble_view_base.h"
#include "components/optimization_guide/proto/features/walletable_pass_extraction.pb.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class StyledLabel;
}  // namespace views
namespace wallet {

class WalletablePassConsentBubbleController;

// This bubble view is displayed when a walletable pass is found. It allows the
// user to save the pass to their wallet.
class WalletablePassConsentBubbleView : public WalletablePassBubbleViewBase {
  METADATA_HEADER(WalletablePassConsentBubbleView, WalletablePassBubbleViewBase)

 public:
  WalletablePassConsentBubbleView(
      views::View* anchor_view,
      content::WebContents* web_contents,
      WalletablePassConsentBubbleController* controller);
  ~WalletablePassConsentBubbleView() override;

  // LocationBarBubbleDelegateView:
  void AddedToWidget() override;

 private:
  std::unique_ptr<views::StyledLabel> GetSubtitleDescriptionLabel();

  std::unique_ptr<views::StyledLabel> GetSubtitleActionLabel();

  int GetHeaderImageResourceId() const;

  optimization_guide::proto::PassCategory pass_category_;

  base::WeakPtr<WalletablePassConsentBubbleController> controller_;
};

}  // namespace wallet

#endif  // CHROME_BROWSER_UI_WALLET_WALLETABLE_PASS_CONSENT_BUBBLE_VIEW_H_
