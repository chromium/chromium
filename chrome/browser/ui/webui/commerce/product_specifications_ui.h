// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMERCE_PRODUCT_SPECIFICATIONS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_COMMERCE_PRODUCT_SPECIFICATIONS_UI_H_

#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/resources/cr_components/commerce/shopping_service.mojom.h"
#include "url/gurl.h"

namespace base {
class RefCountedMemory;
}

namespace content {
class BrowserContext;
}

namespace ui {
class ColorChangeHandler;
}

namespace commerce {

class ShoppingServiceHandler;

// This UI is used for both product specifications page and disclosure dialog.
// ui::MojoWebUIController works for the former, but we need to make it
// ui::MojoWebDialogUI to achieve both former and latter.
class ProductSpecificationsUI
    : public ui::MojoWebDialogUI,
      public shopping_service::mojom::ShoppingServiceHandlerFactory {
 public:
  explicit ProductSpecificationsUI(content::WebUI* web_ui);
  ~ProductSpecificationsUI() override;

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

  void BindInterface(
      mojo::PendingReceiver<
          shopping_service::mojom::ShoppingServiceHandlerFactory> receiver);

  void CreateShoppingServiceHandler(
      mojo::PendingRemote<shopping_service::mojom::Page> page,
      mojo::PendingReceiver<shopping_service::mojom::ShoppingServiceHandler>
          receiver) override;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

 private:
  mojo::Receiver<shopping_service::mojom::ShoppingServiceHandlerFactory>
      shopping_service_factory_receiver_{this};

  std::unique_ptr<ShoppingServiceHandler> shopping_service_handler_;

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class ProductSpecificationsUIConfig
    : public content::DefaultWebUIConfig<ProductSpecificationsUI> {
 public:
  ProductSpecificationsUIConfig();
  ~ProductSpecificationsUIConfig() override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_WEBUI_COMMERCE_PRODUCT_SPECIFICATIONS_UI_H_
