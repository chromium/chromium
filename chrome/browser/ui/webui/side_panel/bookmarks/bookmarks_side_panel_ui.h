// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_BOOKMARKS_BOOKMARKS_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_BOOKMARKS_BOOKMARKS_SIDE_PANEL_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks.mojom.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

class BookmarksPageHandler;

class BookmarksSidePanelUI
    : public ui::MojoBubbleWebUIController,
      public side_panel::mojom::BookmarksPageHandlerFactory {
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

 private:
  // side_panel::mojom::BookmarksPageHandlerFactory
  void CreateBookmarksPageHandler(
      mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver)
      override;

  std::unique_ptr<BookmarksPageHandler> bookmarks_page_handler_;
  mojo::Receiver<side_panel::mojom::BookmarksPageHandlerFactory>
      bookmarks_page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_BOOKMARKS_BOOKMARKS_SIDE_PANEL_UI_H_
