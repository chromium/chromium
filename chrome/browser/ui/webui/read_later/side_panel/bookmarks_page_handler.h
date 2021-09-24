// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_BOOKMARKS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_BOOKMARKS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/read_later/side_panel/bookmarks.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class GURL;
class ReadLaterUI;

class BookmarksPageHandler : public side_panel::mojom::BookmarksPageHandler {
 public:
  explicit BookmarksPageHandler(
      mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver,
      ReadLaterUI* read_later_ui);
  BookmarksPageHandler(const BookmarksPageHandler&) = delete;
  BookmarksPageHandler& operator=(const BookmarksPageHandler&) = delete;
  ~BookmarksPageHandler() override;

  // side_panel::mojom::BookmarksPageHandler:
  void OpenBookmark(const GURL& url,
                    int32_t parent_folder_depth,
                    ui::mojom::ClickModifiersPtr click_modifiers) override;
  void ShowContextMenu(const std::string& id, const gfx::Point& point) override;

 private:
  mojo::Receiver<side_panel::mojom::BookmarksPageHandler> receiver_;
  const raw_ptr<ReadLaterUI> read_later_ui_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_BOOKMARKS_PAGE_HANDLER_H_
