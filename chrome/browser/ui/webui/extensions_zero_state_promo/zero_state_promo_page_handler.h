// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_ZERO_STATE_PROMO_ZERO_STATE_PROMO_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_ZERO_STATE_PROMO_ZERO_STATE_PROMO_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions_zero_state_promo/zero_state_promo.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class ZeroStatePromoPageHandler : public zero_state_promo::mojom::PageHandler {
 public:
  explicit ZeroStatePromoPageHandler(
      Profile* profile,
      mojo::PendingReceiver<zero_state_promo::mojom::PageHandler> receiver);

  ZeroStatePromoPageHandler(const ZeroStatePromoPageHandler&) = delete;
  ZeroStatePromoPageHandler& operator=(const ZeroStatePromoPageHandler&) =
      delete;

  ~ZeroStatePromoPageHandler() override;

  void LaunchWebStoreLink(
      zero_state_promo::mojom::WebStoreLinkClicked link) override;

 private:
  mojo::Receiver<zero_state_promo::mojom::PageHandler> receiver_;
  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_ZERO_STATE_PROMO_ZERO_STATE_PROMO_PAGE_HANDLER_H_
