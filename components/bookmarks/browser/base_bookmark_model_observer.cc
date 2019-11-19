// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/base_bookmark_model_observer.h"

namespace bookmarks {

void BaseBookmarkModelObserver::BookmarkModelLoaded(BookmarkModel* model,
                                                    bool ids_reassigned) {}

void BaseBookmarkModelObserver::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeMoved(
    BookmarkModel* model,
    const BookmarkNode* old_parent,
    size_t old_index,
    const BookmarkNode* new_parent,
    size_t new_index) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeAdded(BookmarkModel* model,
                                                  const BookmarkNode* parent,
                                                  size_t index) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    size_t old_index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeChanged(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeFaviconChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
}

void BaseBookmarkModelObserver::BookmarkNodeChildrenReordered(
    BookmarkModel* model,
    const BookmarkNode* node) {
  BookmarkModelChanged();
}

}  // namespace bookmarks
