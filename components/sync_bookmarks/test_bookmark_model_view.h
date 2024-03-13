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

class TestBookmarkModelView : public BookmarkModelView {
 public:
  enum ViewType {
    kLocalOrSyncableNodes,
    kAccountNodes,
  };

  explicit TestBookmarkModelView(
      ViewType type = ViewType::kLocalOrSyncableNodes);
  TestBookmarkModelView(
      ViewType type,
      std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client);
  ~TestBookmarkModelView() override;

  bookmarks::BookmarkModel* underlying_model() {
    return BookmarkModelView::underlying_model();
  }
  bookmarks::TestBookmarkClient* underlying_client() {
    return static_cast<bookmarks::TestBookmarkClient*>(
        bookmark_model_->client());
  }

  // BookmarkModelView overrides.
  const bookmarks::BookmarkNode* bookmark_bar_node() const override;
  const bookmarks::BookmarkNode* other_node() const override;
  const bookmarks::BookmarkNode* mobile_node() const override;
  void EnsurePermanentNodesExist() override;
  void RemoveAllSyncableNodes() override;
  const bookmarks::BookmarkNode* GetNodeByUuid(
      const base::Uuid& uuid) const override;

  // Convenience overloads with default argument values, used often in tests.
  const bookmarks::BookmarkNode* AddFolder(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const bookmarks::BookmarkNode::MetaInfoMap* meta_info = nullptr,
      std::optional<base::Time> creation_time = std::nullopt,
      std::optional<base::Uuid> uuid = std::nullopt) {
    return BookmarkModelView::AddFolder(parent, index, title, meta_info,
                                        creation_time, uuid);
  }

  const bookmarks::BookmarkNode* AddURL(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url,
      const bookmarks::BookmarkNode::MetaInfoMap* meta_info = nullptr,
      std::optional<base::Time> creation_time = std::nullopt,
      std::optional<base::Uuid> uuid = std::nullopt) {
    return BookmarkModelView::AddURL(parent, index, title, url, meta_info,
                                     creation_time, uuid);
  }

 private:
  // Constructor overload needed to enforce construction order.
  TestBookmarkModelView(
      ViewType type,
      std::unique_ptr<bookmarks::BookmarkModel> bookmark_model);

  const std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;

  // A wrapped view is used to avoid templates and still allow the constructor
  // to choose which precise BookmarkModelView subclass should be used.
  const std::unique_ptr<BookmarkModelView> wrapped_view_;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_TEST_BOOKMARK_MODEL_VIEW_H_
