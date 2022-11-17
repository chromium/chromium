// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READING_LIST_READING_LIST_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READING_LIST_READING_LIST_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks.mojom.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list.mojom.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "components/commerce/core/mojom/shopping_list.mojom.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"

class BookmarksPageHandler;
class ReadAnythingPageHandler;
class ReadingListPageHandler;

namespace commerce {
class ShoppingListHandler;
}

class ReadingListUI : public ui::MojoBubbleWebUIController,
                      public reading_list::mojom::PageHandlerFactory,
                      public side_panel::mojom::BookmarksPageHandlerFactory,
                      public read_anything::mojom::PageHandlerFactory,
                      public shopping_list::mojom::ShoppingListHandlerFactory,
                      public help_bubble::mojom::HelpBubbleHandlerFactory {
 public:
  explicit ReadingListUI(content::WebUI* web_ui);
  ReadingListUI(const ReadingListUI&) = delete;
  ReadingListUI& operator=(const ReadingListUI&) = delete;
  ~ReadingListUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<reading_list::mojom::PageHandlerFactory> receiver);

  void BindInterface(
      mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandlerFactory>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<read_anything::mojom::PageHandlerFactory> receiver);

  void BindInterface(
      mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandlerFactory>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          pending_receiver);

  void SetActiveTabURL(const GURL& url);

 private:
  // reading_list::mojom::PageHandlerFactory:
  void CreatePageHandler(mojo::PendingRemote<reading_list::mojom::Page> page,
                         mojo::PendingReceiver<reading_list::mojom::PageHandler>
                             receiver) override;

  // side_panel::mojom::BookmarksPageHandlerFactory:
  void CreateBookmarksPageHandler(
      mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver)
      override;

  // read_anything::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<read_anything::mojom::Page> page,
      mojo::PendingReceiver<read_anything::mojom::PageHandler> receiver)
      override;

  // shopping_list::mojom::ShoppingListHandlerFactory:
  void CreateShoppingListHandler(
      mojo::PendingRemote<shopping_list::mojom::Page> page,
      mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandler> receiver)
      override;

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  std::unique_ptr<ReadingListPageHandler> page_handler_;
  mojo::Receiver<reading_list::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  std::unique_ptr<BookmarksPageHandler> bookmarks_page_handler_;
  mojo::Receiver<side_panel::mojom::BookmarksPageHandlerFactory>
      bookmarks_page_factory_receiver_{this};

  std::unique_ptr<ReadAnythingPageHandler> read_anything_page_handler_;
  mojo::Receiver<read_anything::mojom::PageHandlerFactory>
      read_anything_page_factory_receiver_{this};

  std::unique_ptr<commerce::ShoppingListHandler> shopping_list_handler_;
  mojo::Receiver<shopping_list::mojom::ShoppingListHandlerFactory>
      shopping_list_factory_receiver_{this};

  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};

  WebuiLoadTimer webui_load_timer_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READING_LIST_READING_LIST_UI_H_
