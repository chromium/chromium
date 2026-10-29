// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_ADAPTERS_ACTION_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_ADAPTERS_ACTION_ADAPTER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/base/mojom/window_open_disposition.mojom-forward.h"

namespace bookmarks {
class BookmarkNode;
}

namespace gfx {
class Point;
}

namespace bookmark_bar_api {

// Carries out bookmark bar actions in the embedder.
class ActionAdapter {
 public:
  virtual ~ActionAdapter() = default;

  // Opens the apps page.
  virtual void OpenAppsPage(ui::mojom::WindowOpenDisposition disposition) = 0;

  // Opens the given bookmark node.
  virtual void OpenBookmark(const bookmarks::BookmarkNode* node,
                            ui::mojom::WindowOpenDisposition disposition) = 0;

  // Records that a folder menu was opened.
  virtual void RecordFolderOpened() = 0;

  // Opens all given bookmark nodes using the given disposition.
  virtual void OpenFolderNodes(
      const std::vector<const bookmarks::BookmarkNode*>& nodes,
      ui::mojom::WindowOpenDisposition disposition) = 0;

  // Shows the context menu for the given selection at the screen point.
  // `on_close` runs when the menu closes.
  virtual void ShowContextMenu(
      const std::vector<const bookmarks::BookmarkNode*>& selection,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type,
      bool can_paste,
      base::OnceClosure on_close) = 0;
};

}  // namespace bookmark_bar_api

#endif  // COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_ADAPTERS_ACTION_ADAPTER_H_
