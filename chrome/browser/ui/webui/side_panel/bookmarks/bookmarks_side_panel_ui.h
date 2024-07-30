// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_BOOKMARKS_BOOKMARKS_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_BOOKMARKS_BOOKMARKS_SIDE_PANEL_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "chrome/common/webui_url_constants.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/resources/cr_components/commerce/shopping_service.mojom.h"

class BookmarksPageHandler;

namespace commerce {
class ShoppingListContextMenuController;
class ShoppingServiceHandler;
}

namespace ui {
class ColorChangeHandler;
}

namespace page_image_service {
class ImageServiceHandler;
}

class BookmarksSidePanelUI;

class BookmarksSidePanelUIConfig
    : public DefaultTopChromeWebUIConfig<BookmarksSidePanelUI> {
 public:
  BookmarksSidePanelUIConfig();

  // DefaultTopChromeWebUIConfig:
  bool IsPreloadable() override;
  std::optional<int> GetCommandIdForTesting() override;
};

class BookmarksSidePanelUI
    : public TopChromeWebUIController,
      public side_panel::mojom::BookmarksPageHandlerFactory,
      public shopping_service::mojom::ShoppingServiceHandlerFactory {
 public:
  explicit BookmarksSidePanelUI(content::WebUI* web_ui);
  BookmarksSidePanelUI(const BookmarksSidePanelUI&) = delete;
  BookmarksSidePanelUI& operator=(const BookmarksSidePanelUI&) = delete;
  ~BookmarksSidePanelUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandlerFactory>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<
          shopping_service::mojom::ShoppingServiceHandlerFactory> receiver);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

  void BindInterface(
      mojo::PendingReceiver<page_image_service::mojom::PageImageServiceHandler>
          pending_image_handler);

  commerce::ShoppingListContextMenuController*
  GetShoppingListContextMenuController();

  static constexpr std::string GetWebUIName() { return "BookmarksSidePanel"; }

 private:
  // side_panel::mojom::BookmarksPageHandlerFactory:
  void CreateBookmarksPageHandler(
      mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver)
      override;

  // shopping_service::mojom::ShoppingServiceHandlerFactory:
  void CreateShoppingServiceHandler(
      mojo::PendingRemote<shopping_service::mojom::Page> page,
      mojo::PendingReceiver<shopping_service::mojom::ShoppingServiceHandler>
          receiver) override;

  bool IsIncognitoModeAvailable();

  std::unique_ptr<BookmarksPageHandler> bookmarks_page_handler_;
  mojo::Receiver<side_panel::mojom::BookmarksPageHandlerFactory>
      bookmarks_page_factory_receiver_{this};
  std::unique_ptr<commerce::ShoppingServiceHandler> shopping_service_handler_;
  mojo::Receiver<shopping_service::mojom::ShoppingServiceHandlerFactory>
      shopping_service_factory_receiver_{this};
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  std::unique_ptr<page_image_service::ImageServiceHandler>
      image_service_handler_;
  std::unique_ptr<commerce::ShoppingListContextMenuController>
      shopping_list_context_menu_controller_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_BOOKMARKS_BOOKMARKS_SIDE_PANEL_UI_H_
