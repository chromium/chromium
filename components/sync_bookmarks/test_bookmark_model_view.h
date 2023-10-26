// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_TEST_BOOKMARK_MODEL_VIEW_H_
#define COMPONENTS_SYNC_BOOKMARKS_TEST_BOOKMARK_MODEL_VIEW_H_

#include <memory>

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync_bookmarks/bookmark_model_view.h"

namespace bookmarks {
class BookmarkModel;
class TestBookmarkClient;
}  // namespace bookmarks

namespace sync_bookmarks {

class TestBookmarkModelView
    : public BookmarkModelViewUsingLocalOrSyncableNodes {
 public:
  TestBookmarkModelView();
  explicit TestBookmarkModelView(
      std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client);
  ~TestBookmarkModelView() override;

  bookmarks::BookmarkModel* underlying_model() {
    return BookmarkModelView::underlying_model();
  }
  bookmarks::TestBookmarkClient* underlying_client() {
    return static_cast<bookmarks::TestBookmarkClient*>(
        bookmark_model_->client());
  }

  // Convenience overloads with default argument values, used often in tests.
  const bookmarks::BookmarkNode* AddFolder(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const bookmarks::BookmarkNode::MetaInfoMap* meta_info = nullptr,
      absl::optional<base::Time> creation_time = absl::nullopt,
      absl::optional<base::Uuid> uuid = absl::nullopt) {
    return BookmarkModelView::AddFolder(parent, index, title, meta_info,
                                        creation_time, uuid);
  }

  const bookmarks::BookmarkNode* AddURL(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url,
      const bookmarks::BookmarkNode::MetaInfoMap* meta_info = nullptr,
      absl::optional<base::Time> creation_time = absl::nullopt,
      absl::optional<base::Uuid> uuid = absl::nullopt) {
    return BookmarkModelView::AddURL(parent, index, title, url, meta_info,
                                     creation_time, uuid);
  }

 private:
  // Constructor overload needed to enforce construction order.
  explicit TestBookmarkModelView(
      std::unique_ptr<bookmarks::BookmarkModel> bookmark_model);

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_TEST_BOOKMARK_MODEL_VIEW_H_
