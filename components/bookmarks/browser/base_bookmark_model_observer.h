// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BASE_BOOKMARK_MODEL_OBSERVER_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BASE_BOOKMARK_MODEL_OBSERVER_H_

#include "components/bookmarks/browser/bookmark_model_observer.h"

namespace bookmarks {

// Base class for a BookmarkModelObserver implementation. All mutations of the
// model funnel into the method BookmarkModelChanged.
class BaseBookmarkModelObserver : public BookmarkModelObserver {
 public:
  BaseBookmarkModelObserver() = default;

  BaseBookmarkModelObserver(const BaseBookmarkModelObserver&) = delete;
  BaseBookmarkModelObserver& operator=(const BaseBookmarkModelObserver&) =
      delete;

  virtual void BookmarkModelChanged() = 0;

  // BookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeMoved(const BookmarkNode* old_parent,
                         size_t old_index,
                         const BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeRemoved(const BookmarkNode* parent,
                           size_t old_index,
                           const BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;
  void BookmarkNodeChanged(const BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(const BookmarkNode* node) override;

 protected:
  ~BaseBookmarkModelObserver() override {}
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BASE_BOOKMARK_MODEL_OBSERVER_H_
