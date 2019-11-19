// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNDO_BOOKMARK_UNDO_SERVICE_H_
#define COMPONENTS_UNDO_BOOKMARK_UNDO_SERVICE_H_

#include <map>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_undo_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/undo/undo_manager.h"

// BookmarkUndoService --------------------------------------------------------

// BookmarkUndoService is owned by the profile, and is responsible for observing
// BookmarkModel changes in order to provide an undo for those changes.
class BookmarkUndoService : public bookmarks::BaseBookmarkModelObserver,
                            public bookmarks::BookmarkUndoDelegate,
                            public KeyedService {
 public:
  BookmarkUndoService();
  ~BookmarkUndoService() override;

  // Starts the BookmarkUndoService and register it as a BookmarkModelObserver.
  // Calling this method is optional, but the service will be inactive until it
  // is called.
  void Start(bookmarks::BookmarkModel* model);

  UndoManager* undo_manager() { return &undo_manager_; }

  // KeyedService:
  void Shutdown() override;

 private:
  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override {}
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;
  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         size_t index) override;
  void OnWillChangeBookmarkNode(bookmarks::BookmarkModel* model,
                                const bookmarks::BookmarkNode* node) override;
  void OnWillReorderBookmarkNode(bookmarks::BookmarkModel* model,
                                 const bookmarks::BookmarkNode* node) override;
  void GroupedBookmarkChangesBeginning(
      bookmarks::BookmarkModel* model) override;
  void GroupedBookmarkChangesEnded(bookmarks::BookmarkModel* model) override;

  // bookmarks::BookmarkUndoDelegate:
  void SetUndoProvider(bookmarks::BookmarkUndoProvider* undo_provider) override;
  void OnBookmarkNodeRemoved(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* parent,
      size_t index,
      std::unique_ptr<bookmarks::BookmarkNode> node) override;

  bookmarks::BookmarkModel* model_;
  bookmarks::BookmarkUndoProvider* undo_provider_;
  UndoManager undo_manager_;
  ScopedObserver<bookmarks::BookmarkModel, bookmarks::BookmarkModelObserver>
      scoped_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(BookmarkUndoService);
};

#endif  // COMPONENTS_UNDO_BOOKMARK_UNDO_SERVICE_H_
