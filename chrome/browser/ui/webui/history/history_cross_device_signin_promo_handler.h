// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_CROSS_DEVICE_SIGNIN_PROMO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_CROSS_DEVICE_SIGNIN_PROMO_HANDLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/history/history_cross_device_signin_promo.mojom.h"

namespace content {
class WebContents;
}  // namespace content

class HistoryCrossDeviceSigninPromoHandler
    : public history_cross_device_signin_promo::mojom::
          HistoryCrossDeviceSigninPromoHandler {
 public:
  HistoryCrossDeviceSigninPromoHandler(
      mojo::PendingReceiver<history_cross_device_signin_promo::mojom::
                                HistoryCrossDeviceSigninPromoHandler> receiver,
      content::WebContents* web_contents);

  HistoryCrossDeviceSigninPromoHandler(
      const HistoryCrossDeviceSigninPromoHandler&) = delete;
  HistoryCrossDeviceSigninPromoHandler& operator=(
      const HistoryCrossDeviceSigninPromoHandler&) = delete;

  ~HistoryCrossDeviceSigninPromoHandler() override;

  // history_cross_device_signin_promo::mojom::HistoryCrossDeviceSigninPromoHandler:
  void ShouldShowPromoCard(ShouldShowPromoCardCallback callback) override;
  void OnPromoCardShown() override;
  void OnPromoCardDismissed() override;
  void OnPromoCardActionClicked(
      OnPromoCardActionClickedCallback callback) override;
  void SetPage(
      mojo::PendingRemote<history_cross_device_signin_promo::mojom::
                              HistoryCrossDeviceSigninPromoPage> page) override;

 private:
  // Forwards the bubble open state to `page_`, if bound.
  void OnBubbleOpenStateChanged(bool is_open);

  // Tears down the page connection and stops observing the bubble state.
  void OnPageDisconnected();

  mojo::Receiver<history_cross_device_signin_promo::mojom::
                     HistoryCrossDeviceSigninPromoHandler>
      receiver_;
  mojo::Remote<history_cross_device_signin_promo::mojom::
                   HistoryCrossDeviceSigninPromoPage>
      page_;
  raw_ptr<content::WebContents> web_contents_;
  base::CallbackListSubscription bubble_state_subscription_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_CROSS_DEVICE_SIGNIN_PROMO_HANDLER_H_
