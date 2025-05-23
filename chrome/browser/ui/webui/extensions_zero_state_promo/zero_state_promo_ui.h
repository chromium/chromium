// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_ZERO_STATE_PROMO_ZERO_STATE_PROMO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_ZERO_STATE_PROMO_ZERO_STATE_PROMO_UI_H_

#include <memory>

#include "chrome/browser/ui/views/user_education/custom_webui_help_bubble.h"
#include "chrome/browser/ui/webui/extensions_zero_state_promo/zero_state_promo.mojom.h"
#include "chrome/browser/ui/webui/extensions_zero_state_promo/zero_state_promo_page_handler.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace extensions {

class ZeroStatePromoController
    : public TopChromeWebUIController,
      public CustomWebUIHelpBubbleController,
      public zero_state_promo::mojom::PageHandlerFactory {
 public:
  explicit ZeroStatePromoController(content::WebUI* web_ui);

  ZeroStatePromoController(const ZeroStatePromoController&) = delete;
  ZeroStatePromoController& operator=(const ZeroStatePromoController&) = delete;

  ~ZeroStatePromoController() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<zero_state_promo::mojom::PageHandlerFactory>
          receiver);

  void BindInterface(mojo::PendingReceiver<
                     custom_help_bubble::mojom::CustomHelpBubbleHandlerFactory>
                         pending_receiver);

  static constexpr std::string GetWebUIName() {
    return "ExtensionsWebStoreZeroStatePromo";
  }

 private:
  // zero_state_promo::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<zero_state_promo::mojom::PageHandler> receiver)
      override;

  std::unique_ptr<ZeroStatePromoPageHandler> page_handler_;

  mojo::Receiver<zero_state_promo::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

DECLARE_TOP_CHROME_WEBUI_CONFIG(ZeroStatePromoController,
                                chrome::kChromeUIExtensionsZeroStatePromoHost);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_ZERO_STATE_PROMO_ZERO_STATE_PROMO_UI_H_
