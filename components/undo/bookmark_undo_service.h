// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNDO_BOOKMARK_UNDO_SERVICE_H_
#define COMPONENTS_UNDO_BOOKMARK_UNDO_SERVICE_H_

#include <map>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
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
  // is called at least once. Can be called multiple times.
  void StartObservingBookmarkModel(bookmarks::BookmarkModel* model);

  UndoManager* undo_manager() { return &undo_manager_; }

  // KeyedService:
  void Shutdown() override;

  // Pushes an undo operation to the stack that allows restoring a deleted
  // bookmark. As opposed to other operations, which reach this service via
  // BookmarkModelObserver, removals are special-cased to be able to transfer
  // ownership of the removed node.
  void AddUndoEntryForRemovedNode(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* parent,
      size_t index,
      std::unique_ptr<bookmarks::BookmarkNode> node);

 private:
  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override {}
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;
  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void OnWillChangeBookmarkNode(bookmarks::BookmarkModel* model,
                                const bookmarks::BookmarkNode* node) override;
  void OnWillReorderBookmarkNode(bookmarks::BookmarkModel* model,
                                 const bookmarks::BookmarkNode* node) override;
  void GroupedBookmarkChangesBeginning(
      bookmarks::BookmarkModel* model) override;
  void GroupedBookmarkChangesEnded(bookmarks::BookmarkModel* model) override;

  UndoManager undo_manager_;
  base::flat_set<raw_ptr<bookmarks::BookmarkModel>> observed_models_;
  base::ScopedMultiSourceObservation<bookmarks::BookmarkModel,
                                     bookmarks::BookmarkModelObserver>
      scoped_observations_{this};
};

#endif  // COMPONENTS_UNDO_BOOKMARK_UNDO_SERVICE_H_
