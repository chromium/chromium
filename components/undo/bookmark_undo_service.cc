// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/undo/bookmark_undo_service.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/macros.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_undo_provider.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/strings/grit/components_strings.h"
#include "components/undo/undo_operation.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::BookmarkNodeData;
using bookmarks::BookmarkUndoProvider;

namespace {

// BookmarkUndoOperation ------------------------------------------------------

// Base class for all bookmark related UndoOperations that facilitates access to
// the BookmarkUndoService.
class BookmarkUndoOperation : public UndoOperation {
 public:
  explicit BookmarkUndoOperation(BookmarkModel* bookmark_model)
      : bookmark_model_(bookmark_model) {}
  ~BookmarkUndoOperation() override {}

  BookmarkModel* bookmark_model() { return bookmark_model_; }

 private:
  BookmarkModel* bookmark_model_;
};

// BookmarkAddOperation -------------------------------------------------------

// Handles the undo of the insertion of a bookmark or folder.
class BookmarkAddOperation : public BookmarkUndoOperation {
 public:
  BookmarkAddOperation(BookmarkModel* bookmark_model,
                       const BookmarkNode* parent,
                       size_t index);
  ~BookmarkAddOperation() override {}

  // UndoOperation:
  void Undo() override;
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

 private:
  int64_t parent_id_;
  const size_t index_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAddOperation);
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

  model->Remove(parent->children()[index_].get());
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
                          BookmarkUndoProvider* undo_provider,
                          const BookmarkNode* parent,
                          size_t index,
                          std::unique_ptr<BookmarkNode> node);
  ~BookmarkRemoveOperation() override;

  // UndoOperation:
  void Undo() override;
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

 private:
  BookmarkUndoProvider* undo_provider_;
  const int64_t parent_node_id_;
  const size_t index_;
  std::unique_ptr<BookmarkNode> node_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkRemoveOperation);
};

BookmarkRemoveOperation::BookmarkRemoveOperation(
    BookmarkModel* model,
    BookmarkUndoProvider* undo_provider,
    const BookmarkNode* parent,
    size_t index,
    std::unique_ptr<BookmarkNode> node)
    : BookmarkUndoOperation(model),
      undo_provider_(undo_provider),
      parent_node_id_(parent->id()),
      index_(index),
      node_(std::move(node)) {}

BookmarkRemoveOperation::~BookmarkRemoveOperation() {
}

void BookmarkRemoveOperation::Undo() {
  DCHECK(node_);

  const BookmarkNode* parent = bookmarks::GetBookmarkNodeByID(
      bookmark_model(), parent_node_id_);
  DCHECK(parent);

  undo_provider_->RestoreRemovedNode(parent, index_, std::move(node_));
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
  ~BookmarkEditOperation() override {}

  // UndoOperation:
  void Undo() override;
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

 private:
  int64_t node_id_;
  BookmarkNodeData original_bookmark_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkEditOperation);
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

  model->SetTitle(node, original_bookmark_.elements[0].title);
  if (original_bookmark_.elements[0].is_url)
    model->SetURL(node, original_bookmark_.elements[0].url);
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
  ~BookmarkMoveOperation() override {}
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

  // UndoOperation:
  void Undo() override;

 private:
  int64_t old_parent_id_;
  int64_t new_parent_id_;
  size_t old_index_;
  size_t new_index_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkMoveOperation);
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
  ~BookmarkReorderOperation() override;

  // UndoOperation:
  void Undo() override;
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

 private:
  int64_t parent_id_;
  std::vector<int64_t> ordered_bookmarks_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkReorderOperation);
};

BookmarkReorderOperation::BookmarkReorderOperation(
    BookmarkModel* bookmark_model,
    const BookmarkNode* parent)
    : BookmarkUndoOperation(bookmark_model),
      parent_id_(parent->id()) {
  ordered_bookmarks_.resize(parent->children().size());
  std::transform(parent->children().cbegin(), parent->children().cend(),
                 ordered_bookmarks_.begin(),
                 [](const auto& child) { return child->id(); });
}

BookmarkReorderOperation::~BookmarkReorderOperation() {
}

void BookmarkReorderOperation::Undo() {
  BookmarkModel* model = bookmark_model();
  const BookmarkNode* parent =
      bookmarks::GetBookmarkNodeByID(model, parent_id_);
  DCHECK(parent);

  std::vector<const BookmarkNode*> ordered_nodes;
  for (size_t i = 0; i < ordered_bookmarks_.size(); ++i) {
    ordered_nodes.push_back(
        bookmarks::GetBookmarkNodeByID(model, ordered_bookmarks_[i]));
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

BookmarkUndoService::BookmarkUndoService() : model_(nullptr) {}

BookmarkUndoService::~BookmarkUndoService() {
}

void BookmarkUndoService::Start(BookmarkModel* model) {
  DCHECK(!model_);
  model_ = model;
  scoped_observer_.Add(model);
  model->SetUndoDelegate(this);
}

void BookmarkUndoService::Shutdown() {
  DCHECK(model_);
  scoped_observer_.RemoveAll();
  model_->SetUndoDelegate(nullptr);
}

void BookmarkUndoService::BookmarkModelLoaded(BookmarkModel* model,
                                              bool ids_reassigned) {
  undo_manager_.RemoveAllOperations();
}

void BookmarkUndoService::BookmarkModelBeingDeleted(BookmarkModel* model) {
  undo_manager_.RemoveAllOperations();
}

void BookmarkUndoService::BookmarkNodeMoved(BookmarkModel* model,
                                            const BookmarkNode* old_parent,
                                            size_t old_index,
                                            const BookmarkNode* new_parent,
                                            size_t new_index) {
  std::unique_ptr<UndoOperation> op(new BookmarkMoveOperation(
      model, old_parent, old_index, new_parent, new_index));
  undo_manager()->AddUndoOperation(std::move(op));
}

void BookmarkUndoService::BookmarkNodeAdded(BookmarkModel* model,
                                            const BookmarkNode* parent,
                                            size_t index) {
  std::unique_ptr<UndoOperation> op(
      new BookmarkAddOperation(model, parent, index));
  undo_manager()->AddUndoOperation(std::move(op));
}

void BookmarkUndoService::OnWillChangeBookmarkNode(BookmarkModel* model,
                                                   const BookmarkNode* node) {
  std::unique_ptr<UndoOperation> op(new BookmarkEditOperation(model, node));
  undo_manager()->AddUndoOperation(std::move(op));
}

void BookmarkUndoService::OnWillReorderBookmarkNode(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  std::unique_ptr<UndoOperation> op(new BookmarkReorderOperation(model, node));
  undo_manager()->AddUndoOperation(std::move(op));
}

void BookmarkUndoService::GroupedBookmarkChangesBeginning(
    BookmarkModel* model) {
  undo_manager()->StartGroupingActions();
}

void BookmarkUndoService::GroupedBookmarkChangesEnded(BookmarkModel* model) {
  undo_manager()->EndGroupingActions();
}

void BookmarkUndoService::SetUndoProvider(BookmarkUndoProvider* undo_provider) {
  undo_provider_ = undo_provider;
}

void BookmarkUndoService::OnBookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    size_t index,
    std::unique_ptr<BookmarkNode> node) {
  DCHECK(undo_provider_);
  std::unique_ptr<UndoOperation> op(new BookmarkRemoveOperation(
      model, undo_provider_, parent, index, std::move(node)));
  undo_manager()->AddUndoOperation(std::move(op));
}
