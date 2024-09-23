// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_bookmarks {

namespace {

using testing::Eq;
using testing::IsNull;
using testing::NotNull;

class BookmarkModelViewTest : public testing::Test {
 protected:
  BookmarkModelViewTest() {
    // Enable all possible permanent folders to verify how BookmarkModelView
    // does the filtering.
    auto client = std::make_unique<bookmarks::TestBookmarkClient>();
    managed_node_ = client->EnableManagedNode();
    model_ =
        bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));

    model_->CreateAccountPermanentFolders();

    EXPECT_THAT(model_->bookmark_bar_node(), NotNull());
    EXPECT_THAT(model_->other_node(), NotNull());
    EXPECT_THAT(model_->mobile_node(), NotNull());
    EXPECT_THAT(model_->account_bookmark_bar_node(), NotNull());
    EXPECT_THAT(model_->account_other_node(), NotNull());
    EXPECT_THAT(model_->account_mobile_node(), NotNull());
    EXPECT_THAT(managed_node_, NotNull());

    EXPECT_NE(model_->bookmark_bar_node(), model_->account_bookmark_bar_node());
    EXPECT_NE(model_->other_node(), model_->account_other_node());
    EXPECT_NE(model_->mobile_node(), model_->account_mobile_node());
  }

  ~BookmarkModelViewTest() override = default;

  base::test::ScopedFeatureList features_{
      syncer::kSyncEnableBookmarksInTransportMode};
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  raw_ptr<bookmarks::BookmarkNode> managed_node_;
};

TEST_F(BookmarkModelViewTest, ShouldExposeLocalOrSyncablePermanentFolders) {
  BookmarkModelViewUsingLocalOrSyncableNodes view(model_.get());

  EXPECT_THAT(view.bookmark_bar_node(), Eq(model_->bookmark_bar_node()));
  EXPECT_THAT(view.other_node(), Eq(model_->other_node()));
  EXPECT_THAT(view.mobile_node(), Eq(model_->mobile_node()));
}

TEST_F(BookmarkModelViewTest, ShouldExposeAccountPermanentFolders) {
  BookmarkModelViewUsingAccountNodes view(model_.get());

  EXPECT_THAT(view.bookmark_bar_node(),
              Eq(model_->account_bookmark_bar_node()));
  EXPECT_THAT(view.other_node(), Eq(model_->account_other_node()));
  EXPECT_THAT(view.mobile_node(), Eq(model_->account_mobile_node()));
}

TEST_F(BookmarkModelViewTest, ShouldIdentifySyncableNodes) {
  // Create some nodes to exercise the predicate on non-permanent folders.
  const bookmarks::BookmarkNode* folder1 = model_->AddFolder(
      /*parent=*/model_->bookmark_bar_node(), /*index=*/0, u"Title 1");
  const bookmarks::BookmarkNode* folder2 = model_->AddFolder(
      /*parent=*/model_->account_bookmark_bar_node(), /*index=*/0, u"Title 2");
  const bookmarks::BookmarkNode* nested_folder1 = model_->AddFolder(
      /*parent=*/folder1, /*index=*/0, u"Title 3");
  const bookmarks::BookmarkNode* nested_folder2 = model_->AddFolder(
      /*parent=*/folder2, /*index=*/0, u"Title 4");
  const bookmarks::BookmarkNode* managed_folder = model_->AddFolder(
      /*parent=*/managed_node_, /*index=*/0, u"Managed Title");

  BookmarkModelViewUsingLocalOrSyncableNodes view1(model_.get());
  BookmarkModelViewUsingAccountNodes view2(model_.get());

  // In `view1`, which uses local-or-syncable data, only the local-or-syncable
  // permanent folders and their descendants should be considered syncable.
  EXPECT_TRUE(view1.IsNodeSyncable(model_->bookmark_bar_node()));
  EXPECT_TRUE(view1.IsNodeSyncable(model_->other_node()));
  EXPECT_TRUE(view1.IsNodeSyncable(model_->mobile_node()));
  EXPECT_TRUE(view1.IsNodeSyncable(folder1));
  EXPECT_TRUE(view1.IsNodeSyncable(nested_folder1));
  EXPECT_FALSE(view1.IsNodeSyncable(model_->account_bookmark_bar_node()));
  EXPECT_FALSE(view1.IsNodeSyncable(model_->account_other_node()));
  EXPECT_FALSE(view1.IsNodeSyncable(model_->account_mobile_node()));
  EXPECT_FALSE(view1.IsNodeSyncable(folder2));
  EXPECT_FALSE(view1.IsNodeSyncable(nested_folder2));

  // In `view2`, which uses pure account data, only the corresponding
  // permanent folders and their descendants should be considered syncable,
  // which is the exact opposite.
  EXPECT_FALSE(view2.IsNodeSyncable(model_->bookmark_bar_node()));
  EXPECT_FALSE(view2.IsNodeSyncable(model_->other_node()));
  EXPECT_FALSE(view2.IsNodeSyncable(model_->mobile_node()));
  EXPECT_FALSE(view2.IsNodeSyncable(folder1));
  EXPECT_FALSE(view2.IsNodeSyncable(nested_folder1));
  EXPECT_TRUE(view2.IsNodeSyncable(model_->account_bookmark_bar_node()));
  EXPECT_TRUE(view2.IsNodeSyncable(model_->account_other_node()));
  EXPECT_TRUE(view2.IsNodeSyncable(model_->account_mobile_node()));
  EXPECT_TRUE(view2.IsNodeSyncable(folder2));
  EXPECT_TRUE(view2.IsNodeSyncable(nested_folder2));

  // Managed nodes should be excluded in all cases.
  EXPECT_FALSE(view1.IsNodeSyncable(managed_node_));
  EXPECT_FALSE(view1.IsNodeSyncable(managed_folder));
  EXPECT_FALSE(view2.IsNodeSyncable(managed_node_));
  EXPECT_FALSE(view2.IsNodeSyncable(managed_folder));
}

TEST_F(BookmarkModelViewTest, ShouldGetNodeByUuid) {
  const bookmarks::BookmarkNode* folder1 = model_->AddFolder(
      /*parent=*/model_->bookmark_bar_node(), /*index=*/0, u"Title 1");
  const bookmarks::BookmarkNode* folder2 = model_->AddFolder(
      /*parent=*/model_->account_bookmark_bar_node(), /*index=*/0, u"Title 2");

  ASSERT_NE(folder1->uuid(), folder2->uuid());

  BookmarkModelViewUsingLocalOrSyncableNodes view1(model_.get());
  BookmarkModelViewUsingAccountNodes view2(model_.get());

  EXPECT_THAT(view1.GetNodeByUuid(
                  base::Uuid::ParseLowercase(bookmarks::kBookmarkBarNodeUuid)),
              Eq(model_->bookmark_bar_node()));
  EXPECT_THAT(view2.GetNodeByUuid(
                  base::Uuid::ParseLowercase(bookmarks::kBookmarkBarNodeUuid)),
              Eq(model_->account_bookmark_bar_node()));

  // `folder1` should only be exposed in `view1`.
  EXPECT_THAT(view1.GetNodeByUuid(folder1->uuid()), Eq(folder1));
  EXPECT_THAT(view2.GetNodeByUuid(folder1->uuid()), IsNull());

  // `folder2` should only be exposed in `view2`.
  EXPECT_THAT(view1.GetNodeByUuid(folder2->uuid()), IsNull());
  EXPECT_THAT(view2.GetNodeByUuid(folder2->uuid()), Eq(folder2));
}

TEST_F(BookmarkModelViewTest, ShouldRemoveAllLocalOrSyncableNodes) {
  // Add two local bookmarks.
  model_->AddFolder(/*parent=*/model_->bookmark_bar_node(), /*index=*/0,
                    u"Title 1");
  model_->AddFolder(/*parent=*/model_->bookmark_bar_node(), /*index=*/1,
                    u"Title 2");

  // Add two account bookmarks.
  model_->AddFolder(/*parent=*/model_->account_bookmark_bar_node(), /*index=*/0,
                    u"Title 3");
  model_->AddFolder(/*parent=*/model_->account_bookmark_bar_node(), /*index=*/1,
                    u"Title 4");

  ASSERT_THAT(model_->bookmark_bar_node()->children().size(), Eq(2));
  ASSERT_THAT(model_->account_bookmark_bar_node()->children().size(), Eq(2));

  BookmarkModelViewUsingLocalOrSyncableNodes view1(model_.get());
  BookmarkModelViewUsingAccountNodes view2(model_.get());

  view1.RemoveAllSyncableNodes();

  // Only local-or-syncable nodes should have been removed.
  EXPECT_THAT(model_->bookmark_bar_node()->children().size(), Eq(0));
  ASSERT_THAT(model_->account_bookmark_bar_node(), NotNull());
  EXPECT_THAT(model_->account_bookmark_bar_node()->children().size(), Eq(2));
}

TEST_F(BookmarkModelViewTest, ShouldRemoveAllAccountNodes) {
  // Add two local bookmarks.
  model_->AddFolder(/*parent=*/model_->bookmark_bar_node(), /*index=*/0,
                    u"Title 1");
  model_->AddFolder(/*parent=*/model_->bookmark_bar_node(), /*index=*/1,
                    u"Title 2");

  // Add two account bookmarks.
  model_->AddFolder(/*parent=*/model_->account_bookmark_bar_node(), /*index=*/0,
                    u"Title 3");
  model_->AddFolder(/*parent=*/model_->account_bookmark_bar_node(), /*index=*/1,
                    u"Title 4");

  ASSERT_THAT(model_->bookmark_bar_node()->children().size(), Eq(2));
  ASSERT_THAT(model_->account_bookmark_bar_node()->children().size(), Eq(2));

  BookmarkModelViewUsingLocalOrSyncableNodes view1(model_.get());
  BookmarkModelViewUsingAccountNodes view2(model_.get());

  view2.RemoveAllSyncableNodes();

  // Only account nodes should have been removed.
  EXPECT_THAT(model_->bookmark_bar_node()->children().size(), Eq(2));
  EXPECT_THAT(model_->account_bookmark_bar_node(), IsNull());
  EXPECT_THAT(model_->account_mobile_node(), IsNull());
  EXPECT_THAT(model_->account_other_node(), IsNull());
}

}  // namespace

}  // namespace sync_bookmarks
