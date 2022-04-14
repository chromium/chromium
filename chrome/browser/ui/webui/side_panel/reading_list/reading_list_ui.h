// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READING_LIST_READING_LIST_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READING_LIST_READING_LIST_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks.mojom.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything.mojom.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list.mojom.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

class BookmarksPageHandler;
class ReadAnythingPageHandler;
class ReadingListPageHandler;

class ReadingListUI : public ui::MojoBubbleWebUIController,
                      public reading_list::mojom::PageHandlerFactory,
                      public side_panel::mojom::BookmarksPageHandlerFactory,
                      public read_anything::mojom::PageHandlerFactory {
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

  void SetActiveTabURL(const GURL& url);

 private:
  // reading_list::mojom::PageHandlerFactory:
  void CreatePageHandler(mojo::PendingRemote<reading_list::mojom::Page> page,
                         mojo::PendingReceiver<reading_list::mojom::PageHandler>
                             receiver) override;

  // side_panel::mojom::BookmarksPageHandlerFactory
  void CreateBookmarksPageHandler(
      mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver)
      override;

  // read_anything::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<read_anything::mojom::Page> page,
      mojo::PendingReceiver<read_anything::mojom::PageHandler> receiver)
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

  WebuiLoadTimer webui_load_timer_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READING_LIST_READING_LIST_UI_H_
