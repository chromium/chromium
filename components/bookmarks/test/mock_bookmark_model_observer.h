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

  MOCK_METHOD1(BookmarkModelLoaded, void(bool));

  MOCK_METHOD4(BookmarkNodeMoved,
               void(const BookmarkNode*, size_t, const BookmarkNode*, size_t));

  MOCK_METHOD3(BookmarkNodeAdded, void(const BookmarkNode*, size_t, bool));

  MOCK_METHOD4(OnWillRemoveBookmarks,
               void(const BookmarkNode*,
                    size_t,
                    const BookmarkNode*,
                    const base::Location&));

  MOCK_METHOD5(BookmarkNodeRemoved,
               void(const BookmarkNode*,
                    size_t,
                    const BookmarkNode*,
                    const std::set<GURL>&,
                    const base::Location&));

  MOCK_METHOD1(OnWillChangeBookmarkNode, void(const BookmarkNode* node));

  MOCK_METHOD1(BookmarkNodeChanged, void(const BookmarkNode*));

  MOCK_METHOD1(BookmarkNodeFaviconChanged, void(const BookmarkNode*));

  MOCK_METHOD1(BookmarkNodeChildrenReordered, void(const BookmarkNode*));

  MOCK_METHOD2(BookmarkAllUserNodesRemoved,
               void(const std::set<GURL>&, const base::Location&));
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_TEST_MOCK_BOOKMARK_MODEL_OBSERVER_H_
