// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_INSIGHTS_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_INSIGHTS_SIDE_PANEL_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/mojom/price_insights.mojom.h"
#include "components/commerce/core/mojom/price_tracking.mojom.h"
#include "components/commerce/core/mojom/shopping_service.mojom.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace commerce {
class PriceInsightsHandler;
class PriceTrackingHandler;
class ShoppingServiceHandler;
}  // namespace commerce

class ShoppingInsightsSidePanelUI;

class ShoppingInsightsSidePanelUIConfig
    : public DefaultTopChromeWebUIConfig<ShoppingInsightsSidePanelUI> {
 public:
  ShoppingInsightsSidePanelUIConfig()
      : DefaultTopChromeWebUIConfig(
            content::kChromeUIScheme,
            commerce::kChromeUIShoppingInsightsSidePanelHost) {}
};

class ShoppingInsightsSidePanelUI
    : public TopChromeWebUIController,
      public shopping_service::mojom::ShoppingServiceHandlerFactory,
      public commerce::price_tracking::mojom::PriceTrackingHandlerFactory,
      public commerce::price_insights::mojom::PriceInsightsHandlerFactory {
 public:
  explicit ShoppingInsightsSidePanelUI(content::WebUI* web_ui);
  ShoppingInsightsSidePanelUI(const ShoppingInsightsSidePanelUI&) = delete;
  ShoppingInsightsSidePanelUI& operator=(const ShoppingInsightsSidePanelUI&) =
      delete;
  ~ShoppingInsightsSidePanelUI() override;

  void BindInterface(
      mojo::PendingReceiver<
          shopping_service::mojom::ShoppingServiceHandlerFactory> receiver);

  void BindInterface(
      mojo::PendingReceiver<
          commerce::price_tracking::mojom::PriceTrackingHandlerFactory>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<
          commerce::price_insights::mojom::PriceInsightsHandlerFactory>
          receiver);

  static constexpr std::string_view GetWebUIName() {
    return "ShoppingInsightsSidePanel";
  }

 private:
  // shopping_service::mojom::ShoppingListHandlerFactory:
  void CreateShoppingServiceHandler(
      mojo::PendingReceiver<shopping_service::mojom::ShoppingServiceHandler>
          receiver) override;

  // commerce::price_tracking::mojom::PriceTrackingHandlerFactory:
  void CreatePriceTrackingHandler(
      mojo::PendingRemote<commerce::price_tracking::mojom::Page> page,
      mojo::PendingReceiver<
          commerce::price_tracking::mojom::PriceTrackingHandler> receiver)
      override;

  // commerce::price_insights::mojom::PriceInsightsHandlerFactory:
  void CreatePriceInsightsHandler(
      mojo::PendingReceiver<
          commerce::price_insights::mojom::PriceInsightsHandler> receiver)
      override;

  std::unique_ptr<commerce::ShoppingServiceHandler> shopping_service_handler_;
  mojo::Receiver<shopping_service::mojom::ShoppingServiceHandlerFactory>
      shopping_service_factory_receiver_{this};
  std::unique_ptr<commerce::PriceTrackingHandler> price_tracking_handler_;
  mojo::Receiver<commerce::price_tracking::mojom::PriceTrackingHandlerFactory>
      price_tracking_factory_receiver_{this};
  std::unique_ptr<commerce::PriceInsightsHandler> price_insights_handler_;
  mojo::Receiver<commerce::price_insights::mojom::PriceInsightsHandlerFactory>
      price_insights_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_INSIGHTS_SIDE_PANEL_UI_H_
