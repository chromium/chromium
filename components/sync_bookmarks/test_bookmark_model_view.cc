// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/test_bookmark_model_view.h"

#include <utility>

#include "base/notreached.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"

namespace sync_bookmarks {

namespace {

std::unique_ptr<BookmarkModelView> CreateWrappedView(
    bookmarks::BookmarkModel* model,
    TestBookmarkModelView::ViewType type) {
  CHECK(model);

  switch (type) {
    case TestBookmarkModelView::ViewType::kLocalOrSyncableNodes:
      return std::make_unique<BookmarkModelViewUsingLocalOrSyncableNodes>(
          model);
    case TestBookmarkModelView::ViewType::kAccountNodes:
      return std::make_unique<BookmarkModelViewUsingAccountNodes>(model);
  }

  NOTREACHED();
}

}  // namespace

TestBookmarkModelView::TestBookmarkModelView(ViewType type)
    : TestBookmarkModelView(type,
                            std::make_unique<bookmarks::TestBookmarkClient>()) {
}

TestBookmarkModelView::TestBookmarkModelView(
    ViewType type,
    std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client)
    : TestBookmarkModelView(
          type,
          bookmarks::TestBookmarkClient::CreateModelWithClient(
              std::move(bookmark_client))) {}

TestBookmarkModelView::~TestBookmarkModelView() = default;

TestBookmarkModelView::TestBookmarkModelView(
    ViewType type,
    std::unique_ptr<bookmarks::BookmarkModel> bookmark_model)
    : BookmarkModelView(bookmark_model.get()),
      bookmark_model_(std::move(bookmark_model)),
      wrapped_view_(CreateWrappedView(bookmark_model_.get(), type)) {
  CHECK(bookmark_model_);
}

const bookmarks::BookmarkNode* TestBookmarkModelView::bookmark_bar_node()
    const {
  return wrapped_view_->bookmark_bar_node();
}

const bookmarks::BookmarkNode* TestBookmarkModelView::other_node() const {
  return wrapped_view_->other_node();
}

const bookmarks::BookmarkNode* TestBookmarkModelView::mobile_node() const {
  return wrapped_view_->mobile_node();
}

void TestBookmarkModelView::EnsurePermanentNodesExist() {
  wrapped_view_->EnsurePermanentNodesExist();
}

void TestBookmarkModelView::RemoveAllSyncableNodes() {
  wrapped_view_->RemoveAllSyncableNodes();
}

const bookmarks::BookmarkNode* TestBookmarkModelView::GetNodeByUuid(
    const base::Uuid& uuid) const {
  return wrapped_view_->GetNodeByUuid(uuid);
}

}  // namespace sync_bookmarks
