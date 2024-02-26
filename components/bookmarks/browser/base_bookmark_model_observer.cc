// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/base_bookmark_model_observer.h"

namespace bookmarks {

void BaseBookmarkModelObserver::BookmarkModelLoaded(bool ids_reassigned) {}

void BaseBookmarkModelObserver::BookmarkModelBeingDeleted() {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeMoved(
    const BookmarkNode* old_parent,
    size_t old_index,
    const BookmarkNode* new_parent,
    size_t new_index) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeAdded(const BookmarkNode* parent,
                                                  size_t index,
                                                  bool added_by_user) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeRemoved(
    const BookmarkNode* parent,
    size_t old_index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeChanged(const BookmarkNode* node) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeFaviconChanged(
    const BookmarkNode* node) {}

void BaseBookmarkModelObserver::BookmarkNodeChildrenReordered(
    const BookmarkNode* node) {
  BookmarkModelChanged();
}

}  // namespace bookmarks
