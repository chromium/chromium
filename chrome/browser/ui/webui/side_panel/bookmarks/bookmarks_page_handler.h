// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_BOOKMARKS_BOOKMARKS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_BOOKMARKS_BOOKMARKS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class BookmarksSidePanelUI;
class ReadingListUI;

class BookmarksPageHandler : public side_panel::mojom::BookmarksPageHandler {
 public:
  explicit BookmarksPageHandler(
      mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver,
      BookmarksSidePanelUI* bookmarks_ui);
  explicit BookmarksPageHandler(
      mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver,
      ReadingListUI* reading_list_ui);
  BookmarksPageHandler(const BookmarksPageHandler&) = delete;
  BookmarksPageHandler& operator=(const BookmarksPageHandler&) = delete;
  ~BookmarksPageHandler() override;

  // side_panel::mojom::BookmarksPageHandler:
  void BookmarkCurrentTabInFolder(int64_t folder_id) override;
  void ExecuteOpenInNewTabCommand(
      const std::vector<int64_t>& node_ids,
      side_panel::mojom::ActionSource source) override;
  void ExecuteOpenInNewWindowCommand(
      const std::vector<int64_t>& node_ids,
      side_panel::mojom::ActionSource source) override;
  void ExecuteOpenInIncognitoWindowCommand(
      const std::vector<int64_t>& node_ids,
      side_panel::mojom::ActionSource source) override;
  void ExecuteOpenInNewTabGroupCommand(
      const std::vector<int64_t>& node_ids,
      side_panel::mojom::ActionSource source) override;
  void ExecuteAddToBookmarksBarCommand(
      int64_t node_id,
      side_panel::mojom::ActionSource source) override;
  void ExecuteRemoveFromBookmarksBarCommand(
      int64_t node_id,
      side_panel::mojom::ActionSource source) override;
  void ExecuteDeleteCommand(const std::vector<int64_t>& node_ids,
                            side_panel::mojom::ActionSource source) override;
  void ExecuteContextMenuCommand(const std::vector<int64_t>& node_ids,
                                 side_panel::mojom::ActionSource source,
                                 int command_id);
  void OpenBookmark(int64_t node_id,
                    int32_t parent_folder_depth,
                    ui::mojom::ClickModifiersPtr click_modifiers,
                    side_panel::mojom::ActionSource source) override;
  void SetSortOrder(side_panel::mojom::SortOrder sort_order) override;
  void SetViewType(side_panel::mojom::ViewType view_type) override;
  void ShowContextMenu(const std::string& id,
                       const gfx::Point& point,
                       side_panel::mojom::ActionSource source) override;
  void ShowUI() override;

 private:
  mojo::Receiver<side_panel::mojom::BookmarksPageHandler> receiver_;
  raw_ptr<BookmarksSidePanelUI> bookmarks_ui_ = nullptr;
  // TODO(corising): Remove use of ReadingListUI which is only needed prior to
  // kUnifiedSidePanel.
  raw_ptr<ReadingListUI> reading_list_ui_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_BOOKMARKS_BOOKMARKS_PAGE_HANDLER_H_
