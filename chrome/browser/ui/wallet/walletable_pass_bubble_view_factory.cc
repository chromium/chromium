// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_bubble_view_factory.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/wallet/walletable_pass_consent_bubble_controller.h"
#include "chrome/browser/ui/wallet/walletable_pass_consent_bubble_view.h"
#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_controller.h"
#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view.h"

namespace wallet {

namespace {

static views::View* FindAnchorView(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return nullptr;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view->GetLocationBarView();
}

template <typename BubbleView, typename... Args>
BubbleView* CreateBubbleView(content::WebContents* web_contents,
                             Args&&... args) {
  views::View* anchor_view = FindAnchorView(web_contents);
  auto bubble_view = std::make_unique<BubbleView>(anchor_view, web_contents,
                                                  std::forward<Args>(args)...);
  BubbleView* const ptr = bubble_view.get();
  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  ptr->ShowForReason(LocationBarBubbleDelegateView::AUTOMATIC);
  return ptr;
}

}  // namespace

// Static
WalletablePassConsentBubbleView*
WalletablePassBubbleViewFactory::CreateConsentBubbleView(
    content::WebContents* web_contents,
    WalletablePassConsentBubbleController* controller) {
  return CreateBubbleView<WalletablePassConsentBubbleView>(web_contents,
                                                           controller);
}

// Static
WalletablePassSaveBubbleView*
WalletablePassBubbleViewFactory::CreateSaveBubbleView(
    content::WebContents* web_contents,
    WalletablePassSaveBubbleController* controller) {
  return CreateBubbleView<WalletablePassSaveBubbleView>(web_contents,
                                                        controller);
}

}  // namespace wallet
