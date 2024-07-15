// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNDO_BOOKMARK_UNDO_SERVICE_H_
#define COMPONENTS_UNDO_BOOKMARK_UNDO_SERVICE_H_

#include <map>

#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/undo/undo_manager.h"

// BookmarkUndoService --------------------------------------------------------

// BookmarkUndoService is owned by the profile, and is responsible for observing
// BookmarkModel changes in order to provide an undo for those changes.
class BookmarkUndoService : public bookmarks::BaseBookmarkModelObserver,
                            public KeyedService {
 public:
  BookmarkUndoService();

  BookmarkUndoService(const BookmarkUndoService&) = delete;
  BookmarkUndoService& operator=(const BookmarkUndoService&) = delete;

  ~BookmarkUndoService() override;

  // Starts the BookmarkUndoService and register it as a BookmarkModelObserver.
  // Calling this method is optional, but the service will be inactive until it
  // is called at least once. Must be called at most once.
  void StartObservingBookmarkModel(bookmarks::BookmarkModel* model);

  UndoManager* undo_manager() { return &undo_manager_; }

  // KeyedService:
  void Shutdown() override;

  // Pushes an undo operation to the stack that allows restoring a deleted
  // bookmark. As opposed to other operations, which reach this service via
  // BookmarkModelObserver, removals are special-cased to be able to transfer
  // ownership of the removed node. Both `parent` and `node` must belong to
  // the BookmarkModel previously passed via StartObservingBookmarkModel().
  void AddUndoEntryForRemovedNode(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      std::unique_ptr<bookmarks::BookmarkNode> node);

 private:
  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override {}
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void OnWillChangeBookmarkNode(const bookmarks::BookmarkNode* node) override;
  void OnWillReorderBookmarkNode(const bookmarks::BookmarkNode* node) override;
  void GroupedBookmarkChangesBeginning() override;
  void GroupedBookmarkChangesEnded() override;

  UndoManager undo_manager_;
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      scoped_observation_{this};
};

#endif  // COMPONENTS_UNDO_BOOKMARK_UNDO_SERVICE_H_
