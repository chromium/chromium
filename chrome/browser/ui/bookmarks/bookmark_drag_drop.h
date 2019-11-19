// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_DRAG_DROP_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_DRAG_DROP_H_

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace bookmarks {
class BookmarkNode;
struct BookmarkNodeData;
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
                            ui::DragDropTypes::DragEventSource source,
                            gfx::Point start_point,
                            int operation)>;

struct BookmarkDragParams {
  BookmarkDragParams(std::vector<const bookmarks::BookmarkNode*> nodes,
                     int drag_node_index,
                     gfx::NativeView view,
                     ui::DragDropTypes::DragEventSource source,
                     gfx::Point start_point);
  ~BookmarkDragParams();

  // The bookmark nodes to be dragged.
  std::vector<const bookmarks::BookmarkNode*> nodes;

  // The index of the main dragged node.
  int drag_node_index;

  // The native view that initiated the drag.
  gfx::NativeView view;

  // The source of the drag.
  ui::DragDropTypes::DragEventSource source;

  // The point the drag started.
  gfx::Point start_point;
};

// Starts the process of dragging a folder of bookmarks.
void DragBookmarks(Profile* profile, const BookmarkDragParams& params);

void DragBookmarksForTest(Profile* profile,
                          const BookmarkDragParams& params,
                          DoBookmarkDragCallback do_drag_callback);

// Drops the bookmark nodes that are in |data| onto |parent_node| at |index|.
// |copy| indicates the source operation: if true then the bookmarks in |data|
// are copied, otherwise they are moved if they belong to the same |profile|.
// Returns the drop type used.
int DropBookmarks(Profile* profile,
                  const bookmarks::BookmarkNodeData& data,
                  const bookmarks::BookmarkNode* parent_node,
                  size_t index,
                  bool copy);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_DRAG_DROP_H_
