// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_INSIGHTS_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_INSIGHTS_SIDE_PANEL_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/resources/cr_components/commerce/shopping_service.mojom.h"

namespace ui {
class ColorChangeHandler;
}

namespace commerce {
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
      public shopping_service::mojom::ShoppingServiceHandlerFactory {
 public:
  explicit ShoppingInsightsSidePanelUI(content::WebUI* web_ui);
  ShoppingInsightsSidePanelUI(const ShoppingInsightsSidePanelUI&) = delete;
  ShoppingInsightsSidePanelUI& operator=(const ShoppingInsightsSidePanelUI&) =
      delete;
  ~ShoppingInsightsSidePanelUI() override;

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

  void BindInterface(
      mojo::PendingReceiver<
          shopping_service::mojom::ShoppingServiceHandlerFactory> receiver);

  static constexpr std::string GetWebUIName() {
    return "ShoppingInsightsSidePanel";
  }

 private:
  // shopping_service::mojom::ShoppingListHandlerFactory:
  void CreateShoppingServiceHandler(
      mojo::PendingRemote<shopping_service::mojom::Page> page,
      mojo::PendingReceiver<shopping_service::mojom::ShoppingServiceHandler>
          receiver) override;

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  std::unique_ptr<commerce::ShoppingServiceHandler> shopping_service_handler_;
  mojo::Receiver<shopping_service::mojom::ShoppingServiceHandlerFactory>
      shopping_service_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_INSIGHTS_SIDE_PANEL_UI_H_
