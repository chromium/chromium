// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_ui_operations_helper.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"

using ::bookmarks::BookmarkModel;
using ::bookmarks::BookmarkNode;
using ::bookmarks::BookmarkNodeData;
using ::chrome::BookmarkReorderDropTarget;
using ::ui::mojom::DragOperation;

namespace internal {

BookmarkUIOperationsHelper::~BookmarkUIOperationsHelper() = default;

ui::mojom::DragOperation BookmarkUIOperationsHelper::DropBookmarks(
    Profile* profile,
    const bookmarks::BookmarkNodeData& data,
    size_t index,
    bool copy,
    BookmarkReorderDropTarget target) {
  CHECK(!IsParentManaged());
  if (!data.IsFromProfilePath(profile->GetPath())) {
    RecordBookmarksAdded(profile);
    RecordBookmarkDropped(data, IsParentPermanentNode(), false);
    // Dropping a folder from different profile. Always accept.
    CopyBookmarkNodeData(data, profile->GetPath(), index);
    return DragOperation::kCopy;
  }

  const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>
      dragged_nodes = data.GetNodes(model(), profile->GetPath());
  if (dragged_nodes.empty()) {
    return DragOperation::kNone;
  }

  bool is_reorder = !copy && IsParentDirectChild(dragged_nodes[0]->parent());
  RecordBookmarkDropped(data, IsParentPermanentNode(), is_reorder);
  if (is_reorder) {
    base::UmaHistogramEnumeration("Bookmarks.ReorderDropTarget", target);
    switch (GetParentType()) {
      case bookmarks::BookmarkNode::URL:
        NOTREACHED();
      case bookmarks::BookmarkNode::FOLDER:
        break;
      case bookmarks::BookmarkNode::BOOKMARK_BAR:
        base::UmaHistogramEnumeration(
            "Bookmarks.ReorderDropTarget.InBookmarkBarNode", target);
        break;
      case bookmarks::BookmarkNode::OTHER_NODE:
        base::UmaHistogramEnumeration(
            "Bookmarks.ReorderDropTarget.InOtherBookmarkNode", target);
        break;
      case bookmarks::BookmarkNode::MOBILE:
        base::UmaHistogramEnumeration(
            "Bookmarks.ReorderDropTarget.InMobileBookmarkNode", target);
        break;
    }
  }

  // Drag from same profile. Copy or move nodes.
  if (copy) {
    CopyBookmarkNodeData(data, profile->GetPath(), index);
    return DragOperation::kCopy;
  }

  MoveBookmarkNodeData(data, profile->GetPath(), index);
  return DragOperation::kMove;
}

}  // namespace internal

// BookmarkUIOperationsHelperNonMergedSurfaces:

BookmarkUIOperationsHelperNonMergedSurfaces::
    BookmarkUIOperationsHelperNonMergedSurfaces(
        bookmarks::BookmarkModel* model,
        const bookmarks::BookmarkNode* parent)
    : model_(model), parent_(parent) {
  CHECK(model);
  CHECK(parent);
}

BookmarkUIOperationsHelperNonMergedSurfaces::
    ~BookmarkUIOperationsHelperNonMergedSurfaces() = default;

bookmarks::BookmarkModel* BookmarkUIOperationsHelperNonMergedSurfaces::model() {
  return model_;
}

void BookmarkUIOperationsHelperNonMergedSurfaces::CopyBookmarkNodeData(
    const bookmarks::BookmarkNodeData& data,
    const base::FilePath& profile_path,
    size_t index_to_add_at) {
  bookmarks::ScopedGroupBookmarkActions group_drops(model_);
  const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>
      copied_nodes = data.GetNodes(model_, profile_path);
  if (copied_nodes.empty()) {
    bookmarks::CloneBookmarkNode(model_, data.elements, parent_,
                                 index_to_add_at,
                                 /*reset_node_times=*/true);
    return;
  }

  for (const auto& copied_node : copied_nodes) {
    model_->Copy(copied_node, parent_, index_to_add_at);
    // Increment `index` so that the next copied node ends up after the
    // one that was just inserted.
    ++index_to_add_at;
  }
}

void BookmarkUIOperationsHelperNonMergedSurfaces::MoveBookmarkNodeData(
    const bookmarks::BookmarkNodeData& data,
    const base::FilePath& profile_path,
    size_t index_to_add_at) {
  const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>
      moved_nodes = data.GetNodes(model_, profile_path);
  CHECK(!moved_nodes.empty());
  for (const auto& moved_node : moved_nodes) {
    CHECK(!model_->client()->IsNodeManaged(moved_node));
    model_->Move(moved_node, parent_, index_to_add_at);
    index_to_add_at = parent_->GetIndexOf(moved_node).value() + 1;
  }
}

bool BookmarkUIOperationsHelperNonMergedSurfaces::IsParentManaged() const {
  return model_->client()->IsNodeManaged(parent_);
}

bool BookmarkUIOperationsHelperNonMergedSurfaces::IsParentPermanentNode()
    const {
  return parent_->is_permanent_node();
}

bool BookmarkUIOperationsHelperNonMergedSurfaces::IsParentDirectChild(
    const bookmarks::BookmarkNode* node) const {
  return node->parent() == parent_;
}

bookmarks::BookmarkNode::Type
BookmarkUIOperationsHelperNonMergedSurfaces::GetParentType() const {
  return parent_->type();
}

// BookmarkUIOperationsHelperMergedSurfaces:

BookmarkUIOperationsHelperMergedSurfaces::
    BookmarkUIOperationsHelperMergedSurfaces(
        BookmarkMergedSurfaceService* merged_surface_service,
        const BookmarkParentFolder* parent)
    : merged_surface_service_(merged_surface_service), parent_(parent) {
  CHECK(merged_surface_service);
  CHECK(parent);
}

BookmarkUIOperationsHelperMergedSurfaces::
    ~BookmarkUIOperationsHelperMergedSurfaces() = default;

bookmarks::BookmarkModel* BookmarkUIOperationsHelperMergedSurfaces::model() {
  return merged_surface_service_->bookmark_model();
}

void BookmarkUIOperationsHelperMergedSurfaces::CopyBookmarkNodeData(
    const bookmarks::BookmarkNodeData& data,
    const base::FilePath& profile_path,
    size_t index_to_add_at) {
  CHECK_EQ(data.size(), 1u);
  const BookmarkNode* copied_node = data.GetFirstNode(model(), profile_path);
  if (!copied_node) {
    merged_surface_service_->CopyBookmarkNodeDataElement(
        data.elements[0], *parent_, index_to_add_at);
    return;
  }

  merged_surface_service_->Copy(copied_node, *parent_, index_to_add_at);
}

void BookmarkUIOperationsHelperMergedSurfaces::MoveBookmarkNodeData(
    const bookmarks::BookmarkNodeData& data,
    const base::FilePath& profile_path,
    size_t index_to_add_at) {
  CHECK_EQ(data.size(), 1u);
  const BookmarkNode* moved_node = data.GetFirstNode(model(), profile_path);
  CHECK(moved_node);
  CHECK(!merged_surface_service_->IsNodeManaged(moved_node));
  merged_surface_service_->Move(moved_node, *parent_, index_to_add_at);
}

bool BookmarkUIOperationsHelperMergedSurfaces::IsParentManaged() const {
  return merged_surface_service_->IsParentFolderManaged(*parent_);
}

bool BookmarkUIOperationsHelperMergedSurfaces::IsParentPermanentNode() const {
  return !parent_->HoldsNonPermanentFolder();
}

bool BookmarkUIOperationsHelperMergedSurfaces::IsParentDirectChild(
    const bookmarks::BookmarkNode* node) const {
  return parent_->HasDirectChildNode(node);
}

bookmarks::BookmarkNode::Type
BookmarkUIOperationsHelperMergedSurfaces::GetParentType() const {
  if (parent_->HoldsNonPermanentFolder()) {
    return parent_->as_non_permanent_folder()->type();
  }

  switch (*parent_->as_permanent_folder()) {
    case BookmarkParentFolder::PermanentFolderType::kBookmarkBarNode:
      return bookmarks::BookmarkNode::Type::BOOKMARK_BAR;
    case BookmarkParentFolder::PermanentFolderType::kOtherNode:
      return bookmarks::BookmarkNode::Type::OTHER_NODE;
    case BookmarkParentFolder::PermanentFolderType::kMobileNode:
      return bookmarks::BookmarkNode::Type::MOBILE;

    case BookmarkParentFolder::PermanentFolderType::kManagedNode:
      // There is no specific type for managed nodes.
      return bookmarks::BookmarkNode::Type::FOLDER;
  }
  NOTREACHED();
}
