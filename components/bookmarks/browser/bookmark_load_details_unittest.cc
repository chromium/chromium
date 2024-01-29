// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_load_details.h"

#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks {

namespace {

const BookmarkNode* FindNodeByUuid(const UuidIndex& index,
                                   const base::Uuid& uuid) {
  auto it = index.find(uuid);
  return it == index.end() ? nullptr : *it;
}

TEST(BookmarkLoadDetails, CreateEmpty) {
  BookmarkLoadDetails details;

  // Local-or-syncable nodes exist by default.
  EXPECT_NE(nullptr, details.bb_node());
  EXPECT_NE(nullptr, details.other_folder_node());
  EXPECT_NE(nullptr, details.mobile_folder_node());

  // Account nodes should not exist.
  EXPECT_EQ(nullptr, details.account_bb_node());
  EXPECT_EQ(nullptr, details.account_other_folder_node());
  EXPECT_EQ(nullptr, details.account_mobile_folder_node());

  EXPECT_EQ(3u, details.RootNodeForTest()->children().size());
}

TEST(BookmarkLoadDetailsTest, AddAccountPermanentNodes) {
  BookmarkLoadDetails details;

  ASSERT_EQ(nullptr, details.account_bb_node());
  ASSERT_EQ(nullptr, details.account_other_folder_node());
  ASSERT_EQ(nullptr, details.account_mobile_folder_node());

  details.AddAccountPermanentNodes(
      BookmarkPermanentNode::CreateBookmarkBar(/*id=*/100),
      BookmarkPermanentNode::CreateOtherBookmarks(/*id=*/200),
      BookmarkPermanentNode::CreateMobileBookmarks(/*id=*/300));

  EXPECT_NE(nullptr, details.account_bb_node());
  EXPECT_NE(nullptr, details.account_other_folder_node());
  EXPECT_NE(nullptr, details.account_mobile_folder_node());

  EXPECT_EQ(6u, details.RootNodeForTest()->children().size());
}

TEST(BookmarkLoadDetailsTest, CreateIndicesWithoutAccountNodes) {
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid3 = base::Uuid::GenerateRandomV4();

  BookmarkLoadDetails details;

  ASSERT_NE(nullptr, details.bb_node());
  ASSERT_NE(nullptr, details.other_folder_node());
  ASSERT_NE(nullptr, details.mobile_folder_node());

  BookmarkNode* node1 = details.bb_node()->Add(
      std::make_unique<BookmarkNode>(/*id=*/100, kUuid1, GURL()));
  BookmarkNode* node2 = details.other_folder_node()->Add(
      std::make_unique<BookmarkNode>(/*id=*/200, kUuid2, GURL()));
  BookmarkNode* node3 = details.mobile_folder_node()->Add(
      std::make_unique<BookmarkNode>(/*id=*/300, kUuid3, GURL()));

  details.CreateIndices();

  const UuidIndex local_or_syncable_uuid_index =
      details.owned_local_or_syncable_uuid_index();
  const UuidIndex account_uuid_index = details.owned_account_uuid_index();

  EXPECT_TRUE(account_uuid_index.empty());

  // Permanent folders should be in the index.
  EXPECT_EQ(FindNodeByUuid(local_or_syncable_uuid_index,
                           base::Uuid::ParseLowercase(kBookmarkBarNodeUuid)),
            details.bb_node());
  EXPECT_EQ(FindNodeByUuid(local_or_syncable_uuid_index,
                           base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid)),
            details.other_folder_node());
  EXPECT_EQ(
      FindNodeByUuid(local_or_syncable_uuid_index,
                     base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid)),
      details.mobile_folder_node());

  // The root node should be present.
  EXPECT_EQ(FindNodeByUuid(local_or_syncable_uuid_index,
                           base::Uuid::ParseLowercase(kRootNodeUuid)),
            details.RootNodeForTest());

  // The three user-created nodes should also be present.
  EXPECT_EQ(FindNodeByUuid(local_or_syncable_uuid_index, kUuid1), node1);
  EXPECT_EQ(FindNodeByUuid(local_or_syncable_uuid_index, kUuid2), node2);
  EXPECT_EQ(FindNodeByUuid(local_or_syncable_uuid_index, kUuid3), node3);

  // Besides the nodes listed above, there should be nothing else.
  EXPECT_EQ(7u, local_or_syncable_uuid_index.size());
}

TEST(BookmarkLoadDetailsTest, CreateIndicesWithAccountNodes) {
  const base::Uuid kLocalUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kLocalUuid2 = base::Uuid::GenerateRandomV4();
  const base::Uuid kLocalUuid3 = base::Uuid::GenerateRandomV4();
  const base::Uuid kLocalUuid4 = base::Uuid::GenerateRandomV4();
  const base::Uuid kAccountUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kAccountUuid2 = base::Uuid::GenerateRandomV4();
  const base::Uuid kAccountUuid3 = base::Uuid::GenerateRandomV4();
  const base::Uuid kAccountUuid4 = base::Uuid::GenerateRandomV4();

  BookmarkLoadDetails details;
  details.AddAccountPermanentNodes(
      BookmarkPermanentNode::CreateBookmarkBar(/*id=*/100),
      BookmarkPermanentNode::CreateOtherBookmarks(/*id=*/200),
      BookmarkPermanentNode::CreateMobileBookmarks(/*id=*/300));

  ASSERT_NE(nullptr, details.bb_node());
  ASSERT_NE(nullptr, details.other_folder_node());
  ASSERT_NE(nullptr, details.mobile_folder_node());
  ASSERT_NE(nullptr, details.account_bb_node());
  ASSERT_NE(nullptr, details.account_other_folder_node());
  ASSERT_NE(nullptr, details.account_mobile_folder_node());

  BookmarkNode* local_node1 = details.bb_node()->Add(
      std::make_unique<BookmarkNode>(/*id=*/400, kLocalUuid1, GURL()));
  BookmarkNode* local_node2 = details.other_folder_node()->Add(
      std::make_unique<BookmarkNode>(/*id=*/500, kLocalUuid2, GURL()));
  BookmarkNode* local_node3 = details.mobile_folder_node()->Add(
      std::make_unique<BookmarkNode>(/*id=*/600, kLocalUuid3, GURL()));
  BookmarkNode* local_node4 = local_node3->Add(
      std::make_unique<BookmarkNode>(/*id=*/700, kLocalUuid4, GURL()));

  BookmarkNode* account_node1 = details.account_bb_node()->Add(
      std::make_unique<BookmarkNode>(/*id=*/800, kAccountUuid1, GURL()));
  BookmarkNode* account_node2 = details.account_other_folder_node()->Add(
      std::make_unique<BookmarkNode>(/*id=*/900, kAccountUuid2, GURL()));
  BookmarkNode* account_node3 = details.account_mobile_folder_node()->Add(
      std::make_unique<BookmarkNode>(/*id=*/1000, kAccountUuid3, GURL()));
  BookmarkNode* account_node4 = account_node3->Add(
      std::make_unique<BookmarkNode>(/*id=*/1100, kAccountUuid4, GURL()));

  details.CreateIndices();

  const UuidIndex local_or_syncable_uuid_index =
      details.owned_local_or_syncable_uuid_index();
  const UuidIndex account_uuid_index = details.owned_account_uuid_index();

  // Permanent folders should be present in the local-or-syncable index.
  EXPECT_EQ(FindNodeByUuid(local_or_syncable_uuid_index,
                           base::Uuid::ParseLowercase(kBookmarkBarNodeUuid)),
            details.bb_node());
  EXPECT_EQ(FindNodeByUuid(local_or_syncable_uuid_index,
                           base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid)),
            details.other_folder_node());
  EXPECT_EQ(
      FindNodeByUuid(local_or_syncable_uuid_index,
                     base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid)),
      details.mobile_folder_node());

  // Permanent folders should also be present in the account index.
  EXPECT_EQ(FindNodeByUuid(account_uuid_index,
                           base::Uuid::ParseLowercase(kBookmarkBarNodeUuid)),
            details.account_bb_node());
  EXPECT_EQ(FindNodeByUuid(account_uuid_index,
                           base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid)),
            details.account_other_folder_node());
  EXPECT_EQ(FindNodeByUuid(account_uuid_index, base::Uuid::ParseLowercase(
                                                   kMobileBookmarksNodeUuid)),
            details.account_mobile_folder_node());

  // The root node should be present only in the local-or-syncable index.
  EXPECT_EQ(FindNodeByUuid(local_or_syncable_uuid_index,
                           base::Uuid::ParseLowercase(kRootNodeUuid)),
            details.RootNodeForTest());
  EXPECT_EQ(FindNodeByUuid(account_uuid_index,
                           base::Uuid::ParseLowercase(kRootNodeUuid)),
            nullptr);

  // The three user-created nodes should also be present.
  EXPECT_EQ(FindNodeByUuid(local_or_syncable_uuid_index, kLocalUuid1),
            local_node1);
  EXPECT_EQ(FindNodeByUuid(local_or_syncable_uuid_index, kLocalUuid2),
            local_node2);
  EXPECT_EQ(FindNodeByUuid(local_or_syncable_uuid_index, kLocalUuid3),
            local_node3);
  EXPECT_EQ(FindNodeByUuid(local_or_syncable_uuid_index, kLocalUuid4),
            local_node4);
  EXPECT_EQ(FindNodeByUuid(account_uuid_index, kAccountUuid1), account_node1);
  EXPECT_EQ(FindNodeByUuid(account_uuid_index, kAccountUuid2), account_node2);
  EXPECT_EQ(FindNodeByUuid(account_uuid_index, kAccountUuid3), account_node3);
  EXPECT_EQ(FindNodeByUuid(account_uuid_index, kAccountUuid4), account_node4);

  // Besides the nodes listed above, there should be nothing else.
  EXPECT_EQ(8u, local_or_syncable_uuid_index.size());
  EXPECT_EQ(7u, account_uuid_index.size());
}

}  // namespace

}  // namespace bookmarks
