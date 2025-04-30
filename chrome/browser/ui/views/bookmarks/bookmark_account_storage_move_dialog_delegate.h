// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_ACCOUNT_STORAGE_MOVE_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_ACCOUNT_STORAGE_MOVE_DIALOG_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_observer.h"
#include "chrome/browser/ui/browser.h"
#include "ui/base/models/dialog_model.h"

namespace bookmarks {
class BookmarkNode;
}

// Delegate that handles bookmark service updates for the bookmark account
// storage move dialog.
class BookmarkAccountStorageMoveDialogDelegate
    : public ui::DialogModelDelegate,
      public BookmarkMergedSurfaceServiceObserver {
 public:
  explicit BookmarkAccountStorageMoveDialogDelegate(
      Browser* browser,
      const bookmarks::BookmarkNode* source,
      const bookmarks::BookmarkNode* destination);
  ~BookmarkAccountStorageMoveDialogDelegate() override;

  void CloseDialog();

  // BookmarkMergedSurfaceServiceObserver:
  void BookmarkMergedSurfaceServiceLoaded() override {}
  void BookmarkMergedSurfaceServiceBeingDeleted() override {}
  void BookmarkNodeAdded(const BookmarkParentFolder& parent,
                         size_t index) override {}
  void BookmarkNodesRemoved(
      const BookmarkParentFolder& parent,
      const base::flat_set<const bookmarks::BookmarkNode*>& nodes) override;
  void BookmarkNodeMoved(const BookmarkParentFolder& old_parent,
                         size_t old_index,
                         const BookmarkParentFolder& new_parent,
                         size_t new_index) override {}
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkParentFolderChildrenReordered(
      const BookmarkParentFolder& folder) override {}
  void BookmarkAllUserNodesRemoved() override {}

 private:
  raw_ptr<Browser> browser_;
  raw_ptr<BookmarkMergedSurfaceService> bookmark_service_;
  const raw_ptr<const bookmarks::BookmarkNode> dialog_source_node_;
  const raw_ptr<const bookmarks::BookmarkNode> dialog_destination_node_;

  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_ACCOUNT_STORAGE_MOVE_DIALOG_DELEGATE_H_
