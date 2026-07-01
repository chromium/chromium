// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_cross_device_signin_promo_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/cross_device_signin_promo_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "content/public/browser/web_contents.h"

HistoryCrossDeviceSigninPromoHandler::HistoryCrossDeviceSigninPromoHandler(
    mojo::PendingReceiver<history_cross_device_signin_promo::mojom::
                              HistoryCrossDeviceSigninPromoHandler> receiver,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)), web_contents_(web_contents) {}

HistoryCrossDeviceSigninPromoHandler::~HistoryCrossDeviceSigninPromoHandler() =
    default;

void HistoryCrossDeviceSigninPromoHandler::ShouldShowPromoCard(
    ShouldShowPromoCardCallback callback) {
  bool should_show = ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage,
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  std::move(callback).Run(should_show);
}

void HistoryCrossDeviceSigninPromoHandler::OnPromoCardShown() {
  OnCrossDeviceSigninPromoShown(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage,
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

void HistoryCrossDeviceSigninPromoHandler::OnPromoCardDismissed() {
  OnCrossDeviceSigninPromoDismissed(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage,
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

void HistoryCrossDeviceSigninPromoHandler::OnPromoCardActionClicked(
    OnPromoCardActionClickedCallback callback) {
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(web_contents_);
  OpenSigninToPhoneQrCodeBubble(
      browser, CrossDeviceSigninPromoEntryPoint::kHistoryPage,
      std::move(callback));
}
