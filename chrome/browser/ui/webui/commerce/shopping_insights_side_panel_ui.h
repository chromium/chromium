// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_INSIGHTS_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_INSIGHTS_SIDE_PANEL_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "components/commerce/core/mojom/shopping_list.mojom.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}

namespace commerce {
class ShoppingListHandler;
}  // namespace commerce

class ShoppingInsightsSidePanelUI
    : public ui::MojoBubbleWebUIController,
      public shopping_list::mojom::ShoppingListHandlerFactory {
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
      mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandlerFactory>
          receiver);

 private:
  // shopping_list::mojom::ShoppingListHandlerFactory:
  void CreateShoppingListHandler(
      mojo::PendingRemote<shopping_list::mojom::Page> page,
      mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandler> receiver)
      override;

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  std::unique_ptr<commerce::ShoppingListHandler> shopping_list_handler_;
  mojo::Receiver<shopping_list::mojom::ShoppingListHandlerFactory>
      shopping_list_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_INSIGHTS_SIDE_PANEL_UI_H_
