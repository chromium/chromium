// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_ui_operations_helper.h"

#include <cstddef>
#include <unordered_set>

#include "base/files/file_path.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
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

// This traces node up to root, determines if it is a descendant of one of
// selected nodes.
bool HasAncestorInSelectedNodes(
    BookmarkModel* model,
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        selected_nodes,
    const BookmarkNode* node) {
  std::unordered_set selected_nodes_set(selected_nodes.begin(),
                                        selected_nodes.end());
  const BookmarkNode* current_node = node;
  while (current_node != nullptr) {
    // Ancestor of `node` is already is the selected nodes. Copying the ancestor
    // node, will include all descendants.
    if (selected_nodes_set.find(current_node) != selected_nodes_set.end()) {
      return true;
    }
    current_node = current_node->parent();
  }
  return false;
}

}  // namespace

namespace internal {

BookmarkUIOperationsHelper::TargetParent::~TargetParent() = default;

BookmarkUIOperationsHelper::~BookmarkUIOperationsHelper() = default;

ui::mojom::DragOperation BookmarkUIOperationsHelper::DropBookmarks(
    Profile* profile,
    const bookmarks::BookmarkNodeData& data,
    size_t index,
    bool copy,
    chrome::BookmarkReorderDropTarget target,
    Browser* browser) {
  CHECK(target_parent());
  CHECK(!target_parent()->IsManaged());
  if (!data.IsFromProfilePath(profile->GetPath())) {
    RecordBookmarksAdded(profile);
    RecordBookmarkDropped(data, target_parent()->IsPermanentNode(), false);
    // Dropping a folder from different profile. Always accept.
    AddNodesAsCopiesOfNodeData(data, index);
    return DragOperation::kCopy;
  }

  const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>
      dragged_nodes = data.GetNodes(model(), profile->GetPath());
  if (dragged_nodes.empty()) {
    return DragOperation::kNone;
  }

  bool is_reorder =
      !copy && target_parent()->IsDirectChild(dragged_nodes[0]->parent());
  RecordBookmarkDropped(data, target_parent()->IsPermanentNode(), is_reorder);
  if (is_reorder) {
    base::UmaHistogramEnumeration("Bookmarks.ReorderDropTarget", target);
    switch (target_parent()->GetType()) {
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
    AddNodesAsCopiesOfNodeData(data, index);
    return DragOperation::kCopy;
  }

  MoveBookmarkNodeData(data, profile->GetPath(), index, browser);
  return DragOperation::kMove;
}

// static
void BookmarkUIOperationsHelper::CopyToClipboard(
    bookmarks::BookmarkModel* model,
    const std::vector<
        raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>& nodes,
    bookmarks::metrics::BookmarkEditSource source,
    bool is_off_the_record) {
  CopyOrCutToClipboard(model, nodes, /*remove_nodes=*/false, source,
                       is_off_the_record);
}

// static
void BookmarkUIOperationsHelper::CutToClipboard(
    bookmarks::BookmarkModel* model,
    const std::vector<
        raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>& nodes,
    bookmarks::metrics::BookmarkEditSource source,
    bool is_off_the_record) {
  CopyOrCutToClipboard(model, nodes, /*remove_nodes=*/true, source,
                       is_off_the_record);
}

// static
void BookmarkUIOperationsHelper::CopyOrCutToClipboard(
    bookmarks::BookmarkModel* model,
    const std::vector<
        raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>& nodes,
    bool remove_nodes,
    bookmarks::metrics::BookmarkEditSource source,
    bool is_off_the_record) {
  if (nodes.empty()) {
    return;
  }

  // Create array of selected nodes with descendants filtered out.
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> filtered_nodes;
  for (const BookmarkNode* node : nodes) {
    if (!HasAncestorInSelectedNodes(model, nodes, node->parent())) {
      filtered_nodes.push_back(node);
    }
  }
  CHECK(!filtered_nodes.empty());
  BookmarkNodeData(filtered_nodes).WriteToClipboard(is_off_the_record);

  if (remove_nodes) {
    bookmarks::ScopedGroupBookmarkActions group_cut(model);
    for (const bookmarks::BookmarkNode* node : filtered_nodes) {
      model->Remove(node, source, FROM_HERE);
    }
  }
}

bool BookmarkUIOperationsHelper::CanPasteFromClipboard() const {
  if (!target_parent() || target_parent()->IsManaged()) {
    return false;
  }
  return BookmarkNodeData::ClipboardContainsBookmarks() ||
         GetUrlFromClipboard(/*notify_if_restricted=*/false).is_valid();
}

void BookmarkUIOperationsHelper::PasteFromClipboard(size_t index) {
  if (!target_parent()) {
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
  CHECK_LE(index, target_parent()->GetChildrenCount());
  if (bookmark_data.size() == 1 &&
      model()->IsBookmarked(bookmark_data.elements[0].url)) {
    MakeTitleUnique(bookmark_data.elements[0].url,
                    &bookmark_data.elements[0].title);
  }

  AddNodesAsCopiesOfNodeData(bookmark_data, index);
}

void BookmarkUIOperationsHelper::MakeTitleUnique(const GURL& url,
                                                 std::u16string* title) const {
  const TargetParent* parent = target_parent();
  CHECK(parent);
  std::unordered_set<std::u16string> titles;
  std::u16string original_title_lower = base::i18n::ToLower(*title);
  const size_t children_size = parent->GetChildrenCount();
  for (size_t i = 0; i < children_size; i++) {
    const BookmarkNode* node = parent->GetNodeAtIndex(i);
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

}  // namespace internal

// BookmarkUIOperationsHelperNonMergedSurfaces::TargetParent:

// static
std::unique_ptr<BookmarkUIOperationsHelperNonMergedSurfaces::TargetParent>
BookmarkUIOperationsHelperNonMergedSurfaces::TargetParent::CreateTargetParent(
    BookmarkModel* model,
    const BookmarkNode* parent) {
  if (!model || !parent) {
    return nullptr;
  }
  return std::make_unique<TargetParent>(parent,
                                        model->client()->IsNodeManaged(parent));
}

BookmarkUIOperationsHelperNonMergedSurfaces::TargetParent::TargetParent(
    const bookmarks::BookmarkNode* parent,
    bool is_managed)
    : parent_(parent), is_managed_(is_managed) {
  CHECK(parent);
}

BookmarkUIOperationsHelperNonMergedSurfaces::TargetParent::~TargetParent() =
    default;

const bookmarks::BookmarkNode*
BookmarkUIOperationsHelperNonMergedSurfaces::TargetParent::parent_node() const {
  return parent_;
}

bool BookmarkUIOperationsHelperNonMergedSurfaces::TargetParent::IsManaged()
    const {
  return is_managed_;
}

bool BookmarkUIOperationsHelperNonMergedSurfaces::TargetParent::
    IsPermanentNode() const {
  return parent_->is_permanent_node();
}

bool BookmarkUIOperationsHelperNonMergedSurfaces::TargetParent::IsDirectChild(
    const bookmarks::BookmarkNode* node) const {
  return node->parent() == parent_;
}

bookmarks::BookmarkNode::Type
BookmarkUIOperationsHelperNonMergedSurfaces::TargetParent::GetType() const {
  return parent_->type();
}

const bookmarks::BookmarkNode*
BookmarkUIOperationsHelperNonMergedSurfaces::TargetParent::GetNodeAtIndex(
    size_t index) const {
  CHECK_LE(index, GetChildrenCount());
  return parent_->children()[index].get();
}

size_t
BookmarkUIOperationsHelperNonMergedSurfaces::TargetParent::GetChildrenCount()
    const {
  return parent_->children().size();
}

// BookmarkUIOperationsHelperNonMergedSurfaces:

BookmarkUIOperationsHelperNonMergedSurfaces::
    BookmarkUIOperationsHelperNonMergedSurfaces(
        bookmarks::BookmarkModel* model,
        const bookmarks::BookmarkNode* parent)
    : model_(model),
      target_parent_(TargetParent::CreateTargetParent(model, parent)) {
  CHECK(model);
}

BookmarkUIOperationsHelperNonMergedSurfaces::
    ~BookmarkUIOperationsHelperNonMergedSurfaces() = default;

bookmarks::BookmarkModel* BookmarkUIOperationsHelperNonMergedSurfaces::model() {
  return model_;
}

void BookmarkUIOperationsHelperNonMergedSurfaces::AddNodesAsCopiesOfNodeData(
    const bookmarks::BookmarkNodeData& data,
    size_t index_to_add_at) {
  CHECK(parent_node());
  bookmarks::ScopedGroupBookmarkActions group_drops(model_);
  bookmarks::CloneBookmarkNode(model_, data.elements, parent_node(),
                               index_to_add_at,
                               /*reset_node_times=*/true);
}

void BookmarkUIOperationsHelperNonMergedSurfaces::MoveBookmarkNodeData(
    const bookmarks::BookmarkNodeData& data,
    const base::FilePath& profile_path,
    size_t index_to_add_at,
    Browser* browser) {
  CHECK(parent_node());
  const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>
      moved_nodes = data.GetNodes(model_, profile_path);
  CHECK(!moved_nodes.empty());
  for (const auto& moved_node : moved_nodes) {
    CHECK(!model_->client()->IsNodeManaged(moved_node));
    model_->Move(moved_node, parent_node(), index_to_add_at);
    index_to_add_at = parent_node()->GetIndexOf(moved_node).value() + 1;
  }
}

const internal::BookmarkUIOperationsHelper::TargetParent*
BookmarkUIOperationsHelperNonMergedSurfaces::target_parent() const {
  if (!target_parent_) {
    return nullptr;
  }
  return target_parent_.get();
}

const bookmarks::BookmarkNode*
BookmarkUIOperationsHelperNonMergedSurfaces::parent_node() const {
  if (!target_parent_) {
    return nullptr;
  }
  return target_parent_->parent_node();
}

// BookmarkUIOperationsHelperMergedSurfaces::TargetParent:

// static
std::unique_ptr<BookmarkUIOperationsHelperMergedSurfaces::TargetParent>
BookmarkUIOperationsHelperMergedSurfaces::TargetParent::CreateTargetParent(
    BookmarkMergedSurfaceService* merged_surface_service,
    const BookmarkParentFolder* parent) {
  if (!merged_surface_service || !parent) {
    return nullptr;
  }
  return std::make_unique<TargetParent>(merged_surface_service, parent);
}

BookmarkUIOperationsHelperMergedSurfaces::TargetParent::TargetParent(
    BookmarkMergedSurfaceService* merged_surface_service,
    const BookmarkParentFolder* parent)
    : merged_surface_service_(merged_surface_service), parent_(parent) {
  CHECK(parent);
}

BookmarkUIOperationsHelperMergedSurfaces::TargetParent::~TargetParent() =
    default;

const BookmarkParentFolder*
BookmarkUIOperationsHelperMergedSurfaces::TargetParent::parent_folder() const {
  return parent_;
}

bool BookmarkUIOperationsHelperMergedSurfaces::TargetParent::IsManaged() const {
  return merged_surface_service_->IsParentFolderManaged(*parent_);
}

bool BookmarkUIOperationsHelperMergedSurfaces::TargetParent::IsPermanentNode()
    const {
  return !parent_->HoldsNonPermanentFolder();
}

bool BookmarkUIOperationsHelperMergedSurfaces::TargetParent::IsDirectChild(
    const bookmarks::BookmarkNode* node) const {
  return parent_->HasDirectChildNode(node);
}

bookmarks::BookmarkNode::Type
BookmarkUIOperationsHelperMergedSurfaces::TargetParent::GetType() const {
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

const bookmarks::BookmarkNode*
BookmarkUIOperationsHelperMergedSurfaces::TargetParent::GetNodeAtIndex(
    size_t index) const {
  CHECK(parent_);
  CHECK_LE(index, GetChildrenCount());
  return merged_surface_service_->GetNodeAtIndex(*parent_, index);
}

size_t
BookmarkUIOperationsHelperMergedSurfaces::TargetParent::GetChildrenCount()
    const {
  CHECK(parent_);
  return merged_surface_service_->GetChildrenCount(*parent_);
}

// BookmarkUIOperationsHelperMergedSurfaces:

BookmarkUIOperationsHelperMergedSurfaces::
    BookmarkUIOperationsHelperMergedSurfaces(
        BookmarkMergedSurfaceService* merged_surface_service,
        const BookmarkParentFolder* parent)
    : merged_surface_service_(merged_surface_service),
      target_parent_(
          TargetParent::CreateTargetParent(merged_surface_service, parent)) {
  CHECK(merged_surface_service);
}

BookmarkUIOperationsHelperMergedSurfaces::
    ~BookmarkUIOperationsHelperMergedSurfaces() = default;

const BookmarkNode*
BookmarkUIOperationsHelperMergedSurfaces::GetDefaultParentForNonMergedSurfaces()
    const {
  CHECK(parent_folder());
  if (target_parent_->IsManaged()) {
    return merged_surface_service_->GetParentForManagedNode(*parent_folder());
  }

  return merged_surface_service_->GetDefaultParentForNewNodes(*parent_folder());
}

bookmarks::BookmarkModel* BookmarkUIOperationsHelperMergedSurfaces::model() {
  return merged_surface_service_->bookmark_model();
}

void BookmarkUIOperationsHelperMergedSurfaces::AddNodesAsCopiesOfNodeData(
    const bookmarks::BookmarkNodeData& data,
    size_t index_to_add_at) {
  bookmarks::ScopedGroupBookmarkActions group_drops(model());
  merged_surface_service_->AddNodesAsCopiesOfNodeData(
      data.elements, *parent_folder(), index_to_add_at);
}

void BookmarkUIOperationsHelperMergedSurfaces::MoveBookmarkNodeData(
    const bookmarks::BookmarkNodeData& data,
    const base::FilePath& profile_path,
    size_t index_to_add_at,
    Browser* browser) {
  CHECK_GE(data.size(), 1u);
  const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>
      moved_nodes = data.GetNodes(model(), profile_path);
  CHECK(!moved_nodes.empty());

  for (const auto& moved_node : moved_nodes) {
    CHECK(!model()->client()->IsNodeManaged(moved_node));
    merged_surface_service_->Move(moved_node, *parent_folder(), index_to_add_at,
                                  browser);
    index_to_add_at++;
  }
}

const internal::BookmarkUIOperationsHelper::TargetParent*
BookmarkUIOperationsHelperMergedSurfaces::target_parent() const {
  if (!target_parent_) {
    return nullptr;
  }
  return target_parent_.get();
}

const BookmarkParentFolder*
BookmarkUIOperationsHelperMergedSurfaces::parent_folder() const {
  if (!target_parent_) {
    return nullptr;
  }
  return target_parent_->parent_folder();
}
