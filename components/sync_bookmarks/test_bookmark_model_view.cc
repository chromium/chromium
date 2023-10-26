// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/test_bookmark_model_view.h"

#include <utility>

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"

namespace sync_bookmarks {

TestBookmarkModelView::TestBookmarkModelView()
    : TestBookmarkModelView(std::make_unique<bookmarks::TestBookmarkClient>()) {
}

TestBookmarkModelView::TestBookmarkModelView(
    std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client)
    : TestBookmarkModelView(
          bookmarks::TestBookmarkClient::CreateModelWithClient(
              std::move(bookmark_client))) {}

TestBookmarkModelView::~TestBookmarkModelView() = default;

TestBookmarkModelView::TestBookmarkModelView(
    std::unique_ptr<bookmarks::BookmarkModel> bookmark_model)
    : BookmarkModelViewUsingLocalOrSyncableNodes(bookmark_model.get()),
      bookmark_model_(std::move(bookmark_model)) {
  CHECK(bookmark_model_);
}

}  // namespace sync_bookmarks
