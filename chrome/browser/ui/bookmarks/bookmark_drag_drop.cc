// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace chrome {

BookmarkDragParams::BookmarkDragParams(
    std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
        nodes,
    int drag_node_index,
    content::WebContents* web_contents,
    ui::mojom::DragEventSource source,
    gfx::Point start_point)
    : nodes(std::move(nodes)),
      drag_node_index(drag_node_index),
      web_contents(web_contents),
      source(source),
      start_point(start_point) {}
BookmarkDragParams::~BookmarkDragParams() = default;

}  // namespace chrome
