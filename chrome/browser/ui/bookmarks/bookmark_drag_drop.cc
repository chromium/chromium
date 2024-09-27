// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/undo/bookmark_undo_service.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"

namespace chrome {

using ::bookmarks::BookmarkModel;
using ::bookmarks::BookmarkNode;
using ::bookmarks::BookmarkNodeData;
using ::ui::mojom::DragOperation;

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

DragOperation DropBookmarks(Profile* profile,
                            const BookmarkNodeData& data,
                            const BookmarkNode* parent_node,
                            size_t index,
                            bool copy,
                            BookmarkReorderDropTarget target) {
  DCHECK(profile);
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);
#if !BUILDFLAG(IS_ANDROID)
  bookmarks::ScopedGroupBookmarkActions group_drops(model);
#endif
  if (data.IsFromProfilePath(profile->GetPath())) {
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>
        dragged_nodes = data.GetNodes(model, profile->GetPath());
    bookmarks::ManagedBookmarkService* managed_service =
        ManagedBookmarkServiceFactory::GetForProfile(profile);
    DCHECK(!managed_service || !managed_service->IsNodeManaged(parent_node));
    DCHECK(copy ||
           chrome::CanAllBeEditedByUser(managed_service, dragged_nodes));
    if (!dragged_nodes.empty()) {
      // Drag from same profile. Copy or move nodes.
      bool is_reorder = !copy && dragged_nodes[0]->parent() == parent_node;
      for (size_t i = 0; i < dragged_nodes.size(); ++i) {
        if (copy) {
          model->Copy(dragged_nodes[i], parent_node, index);
          // Increment `index` so that the next copied node ends up after the
          // one that was just inserted.
          ++index;
        } else {
          model->Move(dragged_nodes[i], parent_node, index);
          index = parent_node->GetIndexOf(dragged_nodes[i]).value() + 1;
        }
      }
      RecordBookmarkDropped(data, parent_node, is_reorder);
      if (is_reorder) {
        base::UmaHistogramEnumeration("Bookmarks.ReorderDropTarget", target);
        const BookmarkNode* parent = dragged_nodes[0]->parent();
        if (parent == model->bookmark_bar_node()) {
          base::UmaHistogramEnumeration(
              "Bookmarks.ReorderDropTarget.InBookmarkBarNode", target);
        } else if (parent == model->other_node()) {
          base::UmaHistogramEnumeration(
              "Bookmarks.ReorderDropTarget.InOtherBookmarkNode", target);
        } else if (parent == model->mobile_node()) {
          base::UmaHistogramEnumeration(
              "Bookmarks.ReorderDropTarget.InMobileBookmarkNode", target);
        }
      }
      return copy ? DragOperation::kCopy : DragOperation::kMove;
    }
    return DragOperation::kNone;
  }
  RecordBookmarksAdded(profile);
  RecordBookmarkDropped(data, parent_node, false);
  // Dropping a folder from different profile. Always accept.
  bookmarks::CloneBookmarkNode(model, data.elements, parent_node, index, true);
  return DragOperation::kCopy;
}

}  // namespace chrome
