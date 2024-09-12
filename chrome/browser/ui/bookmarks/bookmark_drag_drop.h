// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_DRAG_DROP_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_DRAG_DROP_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace bookmarks {
class BookmarkNode;
struct BookmarkNodeData;
}

namespace content {
class WebContents;
}

namespace ui {
class OSExchangeData;
}

namespace chrome {

// Callback for implementing a system drag based on gathered bookmark drag data.
// Used in testing.
using DoBookmarkDragCallback =
    base::OnceCallback<void(std::unique_ptr<ui::OSExchangeData> drag_data,
                            gfx::NativeView native_view,
                            ui::mojom::DragEventSource source,
                            gfx::Point start_point,
                            int operation)>;

struct BookmarkDragParams {
  BookmarkDragParams(
      std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
          nodes,
      int drag_node_index,
      content::WebContents* web_contents,
      ui::mojom::DragEventSource source,
      gfx::Point start_point);
  ~BookmarkDragParams();

  // The bookmark nodes to be dragged.
  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>> nodes;

  // The index of the main dragged node.
  int drag_node_index;

  // The web contents that initiated the drag.
  raw_ptr<content::WebContents> web_contents;

  // The source of the drag.
  ui::mojom::DragEventSource source;

  // The point the drag started.
  gfx::Point start_point;
};

// LINT.IfChange(BookmarkReorderDropTarget)
enum class BookmarkReorderDropTarget {
  kBookmarkBarView = 0,
  kBookmarkManagerAPI = 1,
  kBookmarkMenu = 2,
  kMaxValue = kBookmarkMenu,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/bookmarks/enums.xml:BookmarkReorderDropTarget)

// Starts the process of dragging a folder of bookmarks.
void DragBookmarks(Profile* profile, const BookmarkDragParams& params);

void DragBookmarksForTest(Profile* profile,
                          const BookmarkDragParams& params,
                          DoBookmarkDragCallback do_drag_callback);

// Drops the bookmark nodes that are in |data| onto |parent_node| at |index|.
// |copy| indicates the source operation: if true then the bookmarks in |data|
// are copied, otherwise they are moved if they belong to the same |profile|.
// Returns the drop type used.
ui::mojom::DragOperation DropBookmarks(
    Profile* profile,
    const bookmarks::BookmarkNodeData& data,
    const bookmarks::BookmarkNode* parent_node,
    size_t index,
    bool copy,
    BookmarkReorderDropTarget target);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_DRAG_DROP_H_
