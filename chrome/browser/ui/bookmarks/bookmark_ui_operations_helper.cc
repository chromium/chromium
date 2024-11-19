// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_ui_operations_helper.h"

#include "base/files/file_path.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"

using ::bookmarks::BookmarkModel;
using ::bookmarks::BookmarkNode;
using ::bookmarks::BookmarkNodeData;
using ::chrome::BookmarkReorderDropTarget;
using ::ui::mojom::DragOperation;

namespace {

// Returns the URL from the clipboard. If there is no URL an empty URL is
// returned.
GURL GetUrlFromClipboard(bool notify_if_restricted) {
  std::u16string url_text;
  ui::DataTransferEndpoint data_dst =
      ui::DataTransferEndpoint(ui::EndpointType::kDefault,
                               {.notify_if_restricted = notify_if_restricted});
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst, &url_text);
  return GURL(url_text);
}

// Updates `title` such that `url` and `title` pair are unique among the
// children of `parent`.
void MakeTitleUnique(const BookmarkModel* model,
                     const BookmarkNode* parent,
                     const GURL& url,
                     std::u16string* title) {
  std::unordered_set<std::u16string> titles;
  std::u16string original_title_lower = base::i18n::ToLower(*title);
  for (const auto& node : parent->children()) {
    if (node->is_url() && (url == node->url()) &&
        base::StartsWith(base::i18n::ToLower(node->GetTitle()),
                         original_title_lower, base::CompareCase::SENSITIVE)) {
      titles.insert(node->GetTitle());
    }
  }

  if (titles.find(*title) == titles.end()) {
    return;
  }

  for (size_t i = 0; i < titles.size(); i++) {
    const std::u16string new_title(*title +
                                   base::ASCIIToUTF16(base::StringPrintf(
                                       " (%lu)", (unsigned long)(i + 1))));
    if (titles.find(new_title) == titles.end()) {
      *title = new_title;
      return;
    }
  }
  NOTREACHED();
}

}  // namespace

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
    CopyBookmarkNodeData(data, index);
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
    CopyBookmarkNodeData(data, index);
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

void BookmarkUIOperationsHelperNonMergedSurfaces::PasteFromClipboard(
    size_t index) {
  if (!parent_) {
    return;
  }

  BookmarkNodeData bookmark_data;
  if (!bookmark_data.ReadFromClipboard(ui::ClipboardBuffer::kCopyPaste)) {
    GURL url = GetUrlFromClipboard(/*notify_if_restricted=*/true);
    if (!url.is_valid()) {
      return;
    }
    BookmarkNode node(/*id=*/0, base::Uuid::GenerateRandomV4(), url);
    node.SetTitle(base::ASCIIToUTF16(url.spec()));
    bookmark_data = BookmarkNodeData(&node);
  }
  CHECK_LE(index, parent_->children().size());
  if (bookmark_data.size() == 1 &&
      model_->IsBookmarked(bookmark_data.elements[0].url)) {
    MakeTitleUnique(model_, parent_, bookmark_data.elements[0].url,
                    &bookmark_data.elements[0].title);
  }

  CopyBookmarkNodeData(bookmark_data, index);
}

bookmarks::BookmarkModel* BookmarkUIOperationsHelperNonMergedSurfaces::model() {
  return model_;
}

void BookmarkUIOperationsHelperNonMergedSurfaces::CopyBookmarkNodeData(
    const bookmarks::BookmarkNodeData& data,
    size_t index_to_add_at) {
  bookmarks::ScopedGroupBookmarkActions group_drops(model_);
  bookmarks::CloneBookmarkNode(model_, data.elements, parent_, index_to_add_at,
                               /*reset_node_times=*/true);
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
    size_t index_to_add_at) {
  CHECK_EQ(data.size(), 1u);
  merged_surface_service_->CopyBookmarkNodeDataElement(
      data.elements[0], *parent_, index_to_add_at);
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
