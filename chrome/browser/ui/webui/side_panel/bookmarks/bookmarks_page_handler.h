// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_BOOKMARKS_BOOKMARKS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_BOOKMARKS_BOOKMARKS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_observer.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class BookmarksSidePanelUI;
class BookmarkMergedSurfaceService;
class BrowserWindowInterface;

namespace content {
class WebUI;
}

class BookmarksPageHandler : public side_panel::mojom::BookmarksPageHandler,
                             public BookmarkMergedSurfaceServiceObserver {
 public:
  // `bookmark_merged_surface` must not be null and must outlive this object.
  explicit BookmarksPageHandler(
      mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver,
      mojo::PendingRemote<side_panel::mojom::BookmarksPage> page,
      BookmarksSidePanelUI* bookmarks_ui,
      content::WebUI* web_ui);
  BookmarksPageHandler(const BookmarksPageHandler&) = delete;
  BookmarksPageHandler& operator=(const BookmarksPageHandler&) = delete;
  ~BookmarksPageHandler() override;

  // side_panel::mojom::BookmarksPageHandler:
  void BookmarkCurrentTabInFolder(const std::string& folder_id) override;
  void CreateFolder(const std::string& folder_id,
                    const std::string& title,
                    CreateFolderCallback callback) override;
  void DropBookmarks(const std::string& folder_id,
                     DropBookmarksCallback callback) override;
  void ExecuteOpenInNewTabCommand(
      const std::vector<std::string>& side_panel_ids,
      side_panel::mojom::ActionSource source) override;
  void ExecuteOpenInNewWindowCommand(
      const std::vector<std::string>& side_panel_ids,
      side_panel::mojom::ActionSource source) override;
  void ExecuteOpenInIncognitoWindowCommand(
      const std::vector<std::string>& side_panel_ids,
      side_panel::mojom::ActionSource source) override;
  void ExecuteOpenInNewTabGroupCommand(
      const std::vector<std::string>& side_panel_ids,
      side_panel::mojom::ActionSource source) override;
  void ExecuteOpenInSplitViewCommand(
      const std::vector<int64_t>& node_ids,
      side_panel::mojom::ActionSource source) override;
  void ExecuteEditCommand(const std::vector<int64_t>& node_ids,
                          side_panel::mojom::ActionSource source) override;
  void ExecuteMoveCommand(const std::vector<int64_t>& node_ids,
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
  void Undo() override;
  void RenameBookmark(int64_t node_id, const std::string& new_title) override;
  void MoveBookmark(int64_t node_id, const std::string& folder_id) override;
  void RemoveBookmarks(const std::vector<int64_t>& node_ids,
                       RemoveBookmarksCallback callback) override;
  void SetSortOrder(side_panel::mojom::SortOrder sort_order) override;
  void SetViewType(side_panel::mojom::ViewType view_type) override;
  void ShowContextMenu(const std::string& id,
                       const gfx::Point& point,
                       side_panel::mojom::ActionSource source) override;
  void ShowUI() override;
  void GetAllBookmarks(GetAllBookmarksCallback callback) override;

  // BookmarkMergedSurfaceServiceObserver:
  void BookmarkMergedSurfaceServiceLoaded() override;
  void BookmarkNodeAdded(const BookmarkParentFolder& parent,
                         size_t index) override;
  void BookmarkNodesRemoved(
      const BookmarkParentFolder& parent,
      const base::flat_set<const bookmarks::BookmarkNode*>& nodes) override;
  void BookmarkNodeMoved(const BookmarkParentFolder& old_parent,
                         size_t old_index,
                         const BookmarkParentFolder& new_parent,
                         size_t new_index) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkParentFolderChildrenReordered(
      const BookmarkParentFolder& folder) override;
  void BookmarkAllUserNodesRemoved() override {}

 private:
  // Compute and sends all the bookmark through the input `callback`,
  // redirecting the values to the TS side.
  void SendAllBookmarks(GetAllBookmarksCallback callback);

  mojo::Receiver<side_panel::mojom::BookmarksPageHandler> receiver_;
  mojo::Remote<side_panel::mojom::BookmarksPage> page_;
  const raw_ptr<content::WebUI> web_ui_;
  raw_ptr<BookmarksSidePanelUI> bookmarks_ui_ = nullptr;
  raw_ptr<BookmarkMergedSurfaceService> bookmark_merged_surface_ = nullptr;
  raw_ptr<BrowserWindowInterface> browser_window_interface_ = nullptr;

  // This value is needed when the request from the Ui comes in before the
  // bookmarks are loaded. The callback will be executed upon bookmark load in
  // this case.
  GetAllBookmarksCallback get_all_bookmarks_callback_;

  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      scoped_bookmark_merged_service_observation_{this};
};

std::string GetFolderSidePanelIDForTesting(const BookmarkParentFolder& folder);

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_BOOKMARKS_BOOKMARKS_PAGE_HANDLER_H_
