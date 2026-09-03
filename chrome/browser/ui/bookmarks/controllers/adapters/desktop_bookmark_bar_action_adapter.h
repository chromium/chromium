// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_DESKTOP_BOOKMARK_BAR_ACTION_ADAPTER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_DESKTOP_BOOKMARK_BAR_ACTION_ADAPTER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/bookmark_bar_action_adapter.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"

class BrowserWindowInterface;

namespace bookmarks {
class BookmarkNode;
}

class DesktopBookmarkBarActionAdapter : public BookmarkBarActionAdapter,
                                        public BookmarkContextMenuObserver {
 public:
  explicit DesktopBookmarkBarActionAdapter(BrowserWindowInterface* browser);
  ~DesktopBookmarkBarActionAdapter() override;

  // BookmarkBarActionAdapter overrides:
  void OpenAppsPage(WindowOpenDisposition disposition) override;
  void OpenBookmark(const bookmarks::BookmarkNode* node,
                    WindowOpenDisposition disposition) override;
  void NotifyFolderOpened() override;
  void OpenFolderNodes(const std::vector<const bookmarks::BookmarkNode*>& nodes,
                       WindowOpenDisposition disposition) override;
  void ShowContextMenu(
      const std::vector<const bookmarks::BookmarkNode*>& selection,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type,
      bool can_paste,
      base::OnceClosure on_close) override;

  // BookmarkContextMenuObserver overrides:
  void WillRemoveBookmarks(
      const std::vector<raw_ptr<const bookmarks::BookmarkNode,
                                VectorExperimental>>& bookmarks) override {}
  void DidRemoveBookmarks() override {}
  void OnContextMenuClosed() override;

 private:
  raw_ptr<BrowserWindowInterface> browser_;
  std::unique_ptr<BookmarkContextMenu> context_menu_;
  base::ScopedObservation<BookmarkContextMenu, BookmarkContextMenuObserver>
      context_menu_observation_{this};
  base::OnceClosure on_context_menu_closed_callback_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_DESKTOP_BOOKMARK_BAR_ACTION_ADAPTER_H_
