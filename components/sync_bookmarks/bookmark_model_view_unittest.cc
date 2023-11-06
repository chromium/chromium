// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_view.h"

#include <memory>
#include <utility>

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_bookmarks {

namespace {

using testing::Eq;
using testing::NotNull;

TEST(BookmarkModelViewTest, ShouldExposeLocalOrSyncablePermanentFolders) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  ASSERT_THAT(model->bookmark_bar_node(), NotNull());
  ASSERT_THAT(model->other_node(), NotNull());
  ASSERT_THAT(model->mobile_node(), NotNull());

  BookmarkModelViewUsingLocalOrSyncableNodes view(model.get());

  EXPECT_THAT(view.bookmark_bar_node(), Eq(model->bookmark_bar_node()));
  EXPECT_THAT(view.other_node(), Eq(model->other_node()));
  EXPECT_THAT(view.mobile_node(), Eq(model->mobile_node()));
}

TEST(BookmarkModelViewTest, ShouldIdentifySyncableNodes) {
  auto client = std::make_unique<bookmarks::TestBookmarkClient>();
  bookmarks::BookmarkNode* managed_node = client->EnableManagedNode();

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));

  ASSERT_THAT(model->bookmark_bar_node(), NotNull());
  ASSERT_THAT(model->other_node(), NotNull());
  ASSERT_THAT(model->mobile_node(), NotNull());

  // Create some nodes to exercise the predicate on non-permanent folders.
  const bookmarks::BookmarkNode* folder1 = model->AddFolder(
      /*parent=*/model->bookmark_bar_node(), /*index=*/0, u"Title 1");
  const bookmarks::BookmarkNode* nested_folder1 = model->AddFolder(
      /*parent=*/folder1, /*index=*/0, u"Title 3");
  const bookmarks::BookmarkNode* managed_folder = model->AddFolder(
      /*parent=*/managed_node, /*index=*/0, u"Managed Title");

  BookmarkModelViewUsingLocalOrSyncableNodes view(model.get());

  EXPECT_TRUE(view.IsNodeSyncable(model->bookmark_bar_node()));
  EXPECT_TRUE(view.IsNodeSyncable(model->other_node()));
  EXPECT_TRUE(view.IsNodeSyncable(model->mobile_node()));
  EXPECT_TRUE(view.IsNodeSyncable(folder1));
  EXPECT_TRUE(view.IsNodeSyncable(nested_folder1));

  // Permanent nodes should be excluded.
  EXPECT_FALSE(view.IsNodeSyncable(managed_node));
  EXPECT_FALSE(view.IsNodeSyncable(managed_folder));
}

}  // namespace

}  // namespace sync_bookmarks
