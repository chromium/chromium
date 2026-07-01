// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_CROSS_DEVICE_SIGNIN_PROMO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_CROSS_DEVICE_SIGNIN_PROMO_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
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

 private:
  mojo::Receiver<history_cross_device_signin_promo::mojom::
                     HistoryCrossDeviceSigninPromoHandler>
      receiver_;
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_CROSS_DEVICE_SIGNIN_PROMO_HANDLER_H_
