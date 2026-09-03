// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_ACTION_ADAPTER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_ACTION_ADAPTER_H_

#include <vector>

#include "base/functional/callback.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"

enum class WindowOpenDisposition;

namespace bookmarks {
class BookmarkNode;
}

namespace gfx {
class Point;
}

class BookmarkBarActionAdapter {
 public:
  virtual ~BookmarkBarActionAdapter() = default;

  // Opens the bookmark app page.
  virtual void OpenAppsPage(WindowOpenDisposition disposition) = 0;

  // Opens the given bookmark node.
  virtual void OpenBookmark(const bookmarks::BookmarkNode* node,
                            WindowOpenDisposition disposition) = 0;

  // Notifies the adapter that a folder menu was opened.
  virtual void NotifyFolderOpened() = 0;

  // Opens all given bookmark nodes using the given disposition.
  virtual void OpenFolderNodes(
      const std::vector<const bookmarks::BookmarkNode*>& nodes,
      WindowOpenDisposition disposition) = 0;

  // Shows the context menu for the given selection at the screen point.
  virtual void ShowContextMenu(
      const std::vector<const bookmarks::BookmarkNode*>& selection,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type,
      bool can_paste,
      base::OnceClosure on_close) = 0;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_ACTION_ADAPTER_H_
