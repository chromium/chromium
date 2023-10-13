// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_BIDDING_AND_AUCTION_CONSENTED_DEBUGGING_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_BIDDING_AND_AUCTION_CONSENTED_DEBUGGING_INFOBAR_DELEGATE_H_

#include <string>

#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// An infobar for Chrome for Testing, which displays a message saying that this
// flavor of chrome is unsupported and does not auto-update.
class BiddingAndAuctionConsentedDebuggingDelegate
    : public ConfirmInfoBarDelegate {
 public:
  static void Create(content::WebContents* web_contents);

  BiddingAndAuctionConsentedDebuggingDelegate(
      const BiddingAndAuctionConsentedDebuggingDelegate&) = delete;
  BiddingAndAuctionConsentedDebuggingDelegate& operator=(
      const BiddingAndAuctionConsentedDebuggingDelegate&) = delete;
  BiddingAndAuctionConsentedDebuggingDelegate() = default;
  ~BiddingAndAuctionConsentedDebuggingDelegate() override = default;

 private:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetMessageText() const override;
  std::u16string GetLinkText() const override;
  GURL GetLinkURL() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool ShouldAnimate() const override;
  int GetButtons() const override;
  bool IsCloseable() const override;
};

#endif  // CHROME_BROWSER_UI_STARTUP_BIDDING_AND_AUCTION_CONSENTED_DEBUGGING_INFOBAR_DELEGATE_H_
