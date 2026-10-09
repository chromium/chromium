// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_cross_device_signin_promo_handler.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/cross_device_signin_promo_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

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
  // The promo card is never shown while the feature is disabled, so only a
  // misbehaving renderer can get here. Report a bad message rather than
  // reaching the `CHECK()` on the feature in `OpenSigninToPhoneQrCodeBubble()`.
  if (!base::FeatureList::IsEnabled(switches::kCrossDeviceSigninFromDesktop)) {
    std::move(callback).Run();
    receiver_.ReportBadMessage(
        "OnPromoCardActionClicked called while "
        "kCrossDeviceSigninFromDesktop is disabled.");
    return;
  }

  // May be null if this WebUI is not currently hosted in a browser window.
  // `OpenSigninToPhoneQrCodeBubble()` handles that by dropping the bubble.
  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_contents_);
  // When no bubble is opened, the closing callback is dropped without being
  // run; the wrapper then replies right away, as Mojo requires every responder
  // to be run.
  OpenSigninToPhoneQrCodeBubble(
      browser, CrossDeviceSigninPromoEntryPoint::kHistoryPage,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback)));
}

void HistoryCrossDeviceSigninPromoHandler::SetPage(
    mojo::PendingRemote<history_cross_device_signin_promo::mojom::
                            HistoryCrossDeviceSigninPromoPage> page) {
  page_.reset();
  page_.Bind(std::move(page));
  page_.set_disconnect_handler(
      base::BindOnce(&HistoryCrossDeviceSigninPromoHandler::OnPageDisconnected,
                     base::Unretained(this)));

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  // `base::Unretained()` is safe: the subscription is a member of `this`, so it
  // is destroyed - and the callback unregistered - before `this` goes away.
  bubble_state_subscription_ =
      RegisterCrossDeviceSigninPromoBubbleStateCallback(
          profile,
          base::BindRepeating(
              &HistoryCrossDeviceSigninPromoHandler::OnBubbleOpenStateChanged,
              base::Unretained(this)));
  // Push the current state unconditionally: a page that loads while a bubble is
  // already open must start out with a disabled action button, and a page that
  // reconnects after the bubble closed must not stay disabled.
  OnBubbleOpenStateChanged(IsCrossDeviceSigninPromoBubbleOpen(profile));
}

void HistoryCrossDeviceSigninPromoHandler::OnBubbleOpenStateChanged(
    bool is_open) {
  if (page_.is_bound()) {
    page_->OnPromoBubbleStateChanged(is_open);
  }
}

void HistoryCrossDeviceSigninPromoHandler::OnPageDisconnected() {
  page_.reset();
  bubble_state_subscription_ = {};
}
