// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/undo/bookmark_undo_service.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_undo_provider.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "components/undo/undo_operation.h"

namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::BookmarkNodeData;
using bookmarks::BookmarkUndoProvider;

// BookmarkUndoOperation ------------------------------------------------------

// Base class for all bookmark related UndoOperations that facilitates access to
// the BookmarkUndoService.
class BookmarkUndoOperation : public UndoOperation {
 public:
  explicit BookmarkUndoOperation(BookmarkModel* bookmark_model)
      : bookmark_model_(bookmark_model) {}
  ~BookmarkUndoOperation() override = default;

  BookmarkModel* bookmark_model() { return bookmark_model_; }

 private:
  raw_ptr<BookmarkModel, DanglingUntriaged> bookmark_model_;
};

// BookmarkAddOperation -------------------------------------------------------

// Handles the undo of the insertion of a bookmark or folder.
class BookmarkAddOperation : public BookmarkUndoOperation {
 public:
  BookmarkAddOperation(BookmarkModel* bookmark_model,
                       const BookmarkNode* parent,
                       size_t index);

  BookmarkAddOperation(const BookmarkAddOperation&) = delete;
  BookmarkAddOperation& operator=(const BookmarkAddOperation&) = delete;

  ~BookmarkAddOperation() override = default;

  // UndoOperation:
  void Undo() override;
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

 private:
  int64_t parent_id_;
  const size_t index_;
};

BookmarkAddOperation::BookmarkAddOperation(BookmarkModel* bookmark_model,
                                           const BookmarkNode* parent,
                                           size_t index)
    : BookmarkUndoOperation(bookmark_model),
      parent_id_(parent->id()),
      index_(index) {}

void BookmarkAddOperation::Undo() {
  BookmarkModel* model = bookmark_model();
  const BookmarkNode* parent =
      bookmarks::GetBookmarkNodeByID(model, parent_id_);
  DCHECK(parent);

  model->Remove(parent->children()[index_].get(),
                bookmarks::metrics::BookmarkEditSource::kUser, FROM_HERE);
}

int BookmarkAddOperation::GetUndoLabelId() const {
  return IDS_BOOKMARK_BAR_UNDO_ADD;
}

int BookmarkAddOperation::GetRedoLabelId() const {
  return IDS_BOOKMARK_BAR_REDO_DELETE;
}

// BookmarkRemoveOperation ----------------------------------------------------

// Handles the undo of the deletion of a bookmark node. For a bookmark folder,
// the information for all descendant bookmark nodes is maintained.
//
// The BookmarkModel allows only single bookmark node to be removed.
class BookmarkRemoveOperation : public BookmarkUndoOperation {
 public:
  BookmarkRemoveOperation(BookmarkModel* model,
                          const BookmarkNode* parent,
                          size_t index,
                          std::unique_ptr<BookmarkNode> node);

  BookmarkRemoveOperation(const BookmarkRemoveOperation&) = delete;
  BookmarkRemoveOperation& operator=(const BookmarkRemoveOperation&) = delete;

  ~BookmarkRemoveOperation() override;

  // UndoOperation:
  void Undo() override;
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

 private:
  const int64_t parent_node_id_;
  const size_t index_;
  std::unique_ptr<BookmarkNode> node_;
};

BookmarkRemoveOperation::BookmarkRemoveOperation(
    BookmarkModel* model,
    const BookmarkNode* parent,
    size_t index,
    std::unique_ptr<BookmarkNode> node)
    : BookmarkUndoOperation(model),
      parent_node_id_(parent->id()),
      index_(index),
      node_(std::move(node)) {}

BookmarkRemoveOperation::~BookmarkRemoveOperation() = default;

void BookmarkRemoveOperation::Undo() {
  DCHECK(node_);
  const BookmarkNode* parent = bookmarks::GetBookmarkNodeByID(
      bookmark_model(), parent_node_id_);
  DCHECK(parent);
  static_cast<BookmarkUndoProvider*>(bookmark_model())
      ->RestoreRemovedNode(parent, index_, std::move(node_));
}

int BookmarkRemoveOperation::GetUndoLabelId() const {
  return IDS_BOOKMARK_BAR_UNDO_DELETE;
}

int BookmarkRemoveOperation::GetRedoLabelId() const {
  return IDS_BOOKMARK_BAR_REDO_ADD;
}

// BookmarkEditOperation ------------------------------------------------------

// Handles the undo of the modification of a bookmark node.
class BookmarkEditOperation : public BookmarkUndoOperation {
 public:
  BookmarkEditOperation(BookmarkModel* bookmark_model,
                        const BookmarkNode* node);

  BookmarkEditOperation(const BookmarkEditOperation&) = delete;
  BookmarkEditOperation& operator=(const BookmarkEditOperation&) = delete;

  ~BookmarkEditOperation() override = default;

  // UndoOperation:
  void Undo() override;
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

 private:
  int64_t node_id_;
  BookmarkNodeData original_bookmark_;
};

BookmarkEditOperation::BookmarkEditOperation(
    BookmarkModel* bookmark_model,
    const BookmarkNode* node)
    : BookmarkUndoOperation(bookmark_model),
      node_id_(node->id()),
      original_bookmark_(node) {
}

void BookmarkEditOperation::Undo() {
  DCHECK(original_bookmark_.is_valid());
  BookmarkModel* model = bookmark_model();
  const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(model, node_id_);
  DCHECK(node);

  model->SetTitle(node, original_bookmark_.elements[0].title,
                  bookmarks::metrics::BookmarkEditSource::kOther);
  if (original_bookmark_.elements[0].is_url)
    model->SetURL(node, original_bookmark_.elements[0].url,
                  bookmarks::metrics::BookmarkEditSource::kOther);
}

int BookmarkEditOperation::GetUndoLabelId() const {
  return IDS_BOOKMARK_BAR_UNDO_EDIT;
}

int BookmarkEditOperation::GetRedoLabelId() const {
  return IDS_BOOKMARK_BAR_REDO_EDIT;
}

// BookmarkMoveOperation ------------------------------------------------------

// Handles the undo of a bookmark being moved to a new location.
class BookmarkMoveOperation : public BookmarkUndoOperation {
 public:
  BookmarkMoveOperation(BookmarkModel* bookmark_model,
                        const BookmarkNode* old_parent,
                        size_t old_index,
                        const BookmarkNode* new_parent,
                        size_t new_index);

  BookmarkMoveOperation(const BookmarkMoveOperation&) = delete;
  BookmarkMoveOperation& operator=(const BookmarkMoveOperation&) = delete;

  ~BookmarkMoveOperation() override = default;
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

  // UndoOperation:
  void Undo() override;

 private:
  int64_t old_parent_id_;
  int64_t new_parent_id_;
  size_t old_index_;
  size_t new_index_;
};

BookmarkMoveOperation::BookmarkMoveOperation(BookmarkModel* bookmark_model,
                                             const BookmarkNode* old_parent,
                                             size_t old_index,
                                             const BookmarkNode* new_parent,
                                             size_t new_index)
    : BookmarkUndoOperation(bookmark_model),
      old_parent_id_(old_parent->id()),
      new_parent_id_(new_parent->id()),
      old_index_(old_index),
      new_index_(new_index) {}

void BookmarkMoveOperation::Undo() {
  BookmarkModel* model = bookmark_model();
  const BookmarkNode* old_parent =
      bookmarks::GetBookmarkNodeByID(model, old_parent_id_);
  const BookmarkNode* new_parent =
      bookmarks::GetBookmarkNodeByID(model, new_parent_id_);
  DCHECK(old_parent);
  DCHECK(new_parent);

  const BookmarkNode* node = new_parent->children()[new_index_].get();
  size_t destination_index = old_index_;

  // If the bookmark was moved up within the same parent then the destination
  // index needs to be incremented since the old index did not account for the
  // moved bookmark.
  if (old_parent == new_parent && new_index_ < old_index_)
    ++destination_index;

  model->Move(node, old_parent, destination_index);
}

int BookmarkMoveOperation::GetUndoLabelId() const {
  return IDS_BOOKMARK_BAR_UNDO_MOVE;
}

int BookmarkMoveOperation::GetRedoLabelId() const {
  return IDS_BOOKMARK_BAR_REDO_MOVE;
}

// BookmarkReorderOperation ---------------------------------------------------

// Handle the undo of reordering of bookmarks that can happen as a result of
// sorting a bookmark folder by name or the undo of that operation.  The change
// of order is not recursive so only the order of the immediate children of the
// folder need to be restored.
class BookmarkReorderOperation : public BookmarkUndoOperation {
 public:
  BookmarkReorderOperation(BookmarkModel* bookmark_model,
                           const BookmarkNode* parent);

  BookmarkReorderOperation(const BookmarkReorderOperation&) = delete;
  BookmarkReorderOperation& operator=(const BookmarkReorderOperation&) = delete;

  ~BookmarkReorderOperation() override;

  // UndoOperation:
  void Undo() override;
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

 private:
  int64_t parent_id_;
  std::vector<int64_t> ordered_bookmarks_;
};

BookmarkReorderOperation::BookmarkReorderOperation(
    BookmarkModel* bookmark_model,
    const BookmarkNode* parent)
    : BookmarkUndoOperation(bookmark_model),
      parent_id_(parent->id()) {
  ordered_bookmarks_.resize(parent->children().size());
  base::ranges::transform(parent->children(), ordered_bookmarks_.begin(),
                          &BookmarkNode::id);
}

BookmarkReorderOperation::~BookmarkReorderOperation() = default;

void BookmarkReorderOperation::Undo() {
  BookmarkModel* model = bookmark_model();
  const BookmarkNode* parent =
      bookmarks::GetBookmarkNodeByID(model, parent_id_);
  DCHECK(parent);

  std::vector<const BookmarkNode*> ordered_nodes;
  for (int64_t ordered_bookmark : ordered_bookmarks_) {
    ordered_nodes.push_back(
        bookmarks::GetBookmarkNodeByID(model, ordered_bookmark));
  }

  model->ReorderChildren(parent, ordered_nodes);
}

int BookmarkReorderOperation::GetUndoLabelId() const {
  return IDS_BOOKMARK_BAR_UNDO_REORDER;
}

int BookmarkReorderOperation::GetRedoLabelId() const {
  return IDS_BOOKMARK_BAR_REDO_REORDER;
}

}  // namespace

// BookmarkUndoService --------------------------------------------------------

BookmarkUndoService::BookmarkUndoService() = default;
BookmarkUndoService::~BookmarkUndoService() = default;

void BookmarkUndoService::StartObservingBookmarkModel(BookmarkModel* model) {
  CHECK(!scoped_observation_.IsObserving());
  scoped_observation_.Observe(model);
}

void BookmarkUndoService::Shutdown() {
  // After `RemoveAllObservations` call below - this instance won't be notified
  // of `BookmarkModel` destruction. Undo operations keep a pointer to
  // `BookmarkModel` - delete them to avoid dangling pointers.
  undo_manager_.RemoveAllOperations();
  scoped_observation_.Reset();
}

void BookmarkUndoService::BookmarkModelBeingDeleted() {
  // Delete all undo operations to avoid dangling pointers to `BookmarkModel`
  // that is getting destroyed. `BookmarkModel` is a `KeyedService`, so it is
  // destroyed during shutdown along with other services - other `BookmarkModel`
  // objects and `BookmarkUndoService` itself will be destroyed soon.
  Shutdown();
}

void BookmarkUndoService::BookmarkNodeMoved(const BookmarkNode* old_parent,
                                            size_t old_index,
                                            const BookmarkNode* new_parent,
                                            size_t new_index) {
  BookmarkModel* model = scoped_observation_.GetSource();
  std::unique_ptr<UndoOperation> op(new BookmarkMoveOperation(
      model, old_parent, old_index, new_parent, new_index));
  undo_manager()->AddUndoOperation(std::move(op));
}

void BookmarkUndoService::BookmarkNodeAdded(const BookmarkNode* parent,
                                            size_t index,
                                            bool added_by_user) {
  BookmarkModel* model = scoped_observation_.GetSource();
  std::unique_ptr<UndoOperation> op(
      new BookmarkAddOperation(model, parent, index));
  undo_manager()->AddUndoOperation(std::move(op));
}

void BookmarkUndoService::OnWillChangeBookmarkNode(const BookmarkNode* node) {
  BookmarkModel* model = scoped_observation_.GetSource();
  std::unique_ptr<UndoOperation> op(new BookmarkEditOperation(model, node));
  undo_manager()->AddUndoOperation(std::move(op));
}

void BookmarkUndoService::OnWillReorderBookmarkNode(const BookmarkNode* node) {
  BookmarkModel* model = scoped_observation_.GetSource();
  std::unique_ptr<UndoOperation> op(new BookmarkReorderOperation(model, node));
  undo_manager()->AddUndoOperation(std::move(op));
}

void BookmarkUndoService::GroupedBookmarkChangesBeginning() {
  undo_manager()->StartGroupingActions();
}

void BookmarkUndoService::GroupedBookmarkChangesEnded() {
  undo_manager()->EndGroupingActions();
}

void BookmarkUndoService::AddUndoEntryForRemovedNode(
    const BookmarkNode* parent,
    size_t index,
    std::unique_ptr<BookmarkNode> node) {
  BookmarkModel* model = scoped_observation_.GetSource();
  CHECK(model);

  // `model` is guaranteed to outlive `BookmarkRemoveOperation`, since all undo
  // operations are deleted whenever `BookmarkModelBeingDeleted` or `Shutdown`
  // are invoked.
  std::unique_ptr<UndoOperation> op(
      new BookmarkRemoveOperation(model, parent, index, std::move(node)));
  undo_manager()->AddUndoOperation(std::move(op));
}
