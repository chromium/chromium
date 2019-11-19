// Copyright 2014 The Chromium Authors. All rights reserved.
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

  MOCK_METHOD2(BookmarkModelLoaded, void(BookmarkModel*, bool));

  MOCK_METHOD5(BookmarkNodeMoved,
               void(BookmarkModel*,
                    const BookmarkNode*,
                    size_t,
                    const BookmarkNode*,
                    size_t));

  MOCK_METHOD3(BookmarkNodeAdded,
               void(BookmarkModel*, const BookmarkNode*, size_t));

  MOCK_METHOD5(BookmarkNodeRemoved,
               void(BookmarkModel*,
                    const BookmarkNode*,
                    size_t,
                    const BookmarkNode*,
                    const std::set<GURL>&));

  MOCK_METHOD2(BookmarkNodeChanged, void(BookmarkModel*, const BookmarkNode*));

  MOCK_METHOD2(BookmarkNodeFaviconChanged, void(BookmarkModel*,
                                                const BookmarkNode*));

  MOCK_METHOD2(BookmarkNodeChildrenReordered, void(BookmarkModel*,
                                                   const BookmarkNode*));

  MOCK_METHOD2(BookmarkAllUserNodesRemoved, void(BookmarkModel*,
                                                 const std::set<GURL>&));
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_TEST_MOCK_BOOKMARK_MODEL_OBSERVER_H_
