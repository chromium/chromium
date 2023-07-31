// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/bidding_and_auction_consented_debugging_infobar_delegate.h"

#include <memory>
#include <string>

#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "components/infobars/core/infobar.h"
#else
#include "chrome/browser/devtools/global_confirm_info_bar.h"
#endif

// static
void BiddingAndAuctionConsentedDebuggingDelegate::Create(
    content::WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  infobars::ContentInfoBarManager::FromWebContents(web_contents)
      ->AddInfoBar(CreateConfirmInfoBar(
          std::make_unique<BiddingAndAuctionConsentedDebuggingDelegate>()));
#else
  GlobalConfirmInfoBar::Show(
      std::make_unique<BiddingAndAuctionConsentedDebuggingDelegate>());
#endif
}

const gfx::VectorIcon&
BiddingAndAuctionConsentedDebuggingDelegate::GetVectorIcon() const {
  return vector_icons::kErrorOutlineIcon;
}

infobars::InfoBarDelegate::InfoBarIdentifier
BiddingAndAuctionConsentedDebuggingDelegate::GetIdentifier() const {
  return BIDDING_AND_AUCTION_CONSENTED_DEBUGGING_DELEGATE;
}

std::u16string BiddingAndAuctionConsentedDebuggingDelegate::GetMessageText()
    const {
  return l10n_util::GetStringUTF16(IDS_PROTECTED_AUDIENCE_DEBUGGING_DISCLAIMER);
}

std::u16string BiddingAndAuctionConsentedDebuggingDelegate::GetLinkText()
    const {
  return l10n_util::GetStringUTF16(IDS_DISABLE);
}

GURL BiddingAndAuctionConsentedDebuggingDelegate::GetLinkURL() const {
  return GURL("chrome://flags/#protected-audience-debugging");
}

bool BiddingAndAuctionConsentedDebuggingDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

bool BiddingAndAuctionConsentedDebuggingDelegate::ShouldAnimate() const {
  // Animating the infobar also animates the content area size which can trigger
  // a flood of page layout, compositing, texture reallocations, etc.  Since
  // this infobar is primarily used for automated testing do not animate the
  // infobar to reduce noise in tests.
  return false;
}

int BiddingAndAuctionConsentedDebuggingDelegate::GetButtons() const {
  return BUTTON_NONE;
}

bool BiddingAndAuctionConsentedDebuggingDelegate::IsCloseable() const {
  return false;
}
