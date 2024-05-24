// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_TEST_MOCK_BOOKMARK_MODEL_OBSERVER_H_
#define COMPONENTS_BOOKMARKS_TEST_MOCK_BOOKMARK_MODEL_OBSERVER_H_

#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace bookmarks {

class MockBookmarkModelObserver : public BookmarkModelObserver {
 public:
  MockBookmarkModelObserver();
  ~MockBookmarkModelObserver() override;

  MOCK_METHOD(void, BookmarkModelLoaded, (bool));

  MOCK_METHOD(void, BookmarkModelBeingDeleted, ());

  MOCK_METHOD(void,
              BookmarkNodeMoved,
              (const BookmarkNode*, size_t, const BookmarkNode*, size_t));

  MOCK_METHOD(void, BookmarkNodeAdded, (const BookmarkNode*, size_t, bool));

  MOCK_METHOD(void,
              OnWillRemoveBookmarks,
              (const BookmarkNode*,
               size_t,
               const BookmarkNode*,
               const base::Location&));

  MOCK_METHOD(void,
              BookmarkNodeRemoved,
              (const BookmarkNode*,
               size_t,
               const BookmarkNode*,
               const std::set<GURL>&,
               const base::Location&));

  MOCK_METHOD(void, OnWillChangeBookmarkNode, (const BookmarkNode* node));

  MOCK_METHOD(void, BookmarkNodeChanged, (const BookmarkNode*));

  MOCK_METHOD(void, BookmarkNodeFaviconChanged, (const BookmarkNode*));

  MOCK_METHOD(void, BookmarkNodeChildrenReordered, (const BookmarkNode*));

  MOCK_METHOD(void,
              BookmarkAllUserNodesRemoved,
              (const std::set<GURL>&, const base::Location&));
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_TEST_MOCK_BOOKMARK_MODEL_OBSERVER_H_
