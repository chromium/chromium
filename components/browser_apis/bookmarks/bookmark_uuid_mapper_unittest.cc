// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmark_uuid_mapper.h"

#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace bookmarks_api {

namespace {

constexpr char kSharedNativeUuidStr[] = "aa7db4f9-ffd4-4268-9094-39e1be377c9d";

class BookmarkUuidMapperTest : public testing::Test {
 public:
  BookmarkUuidMapperTest() = default;
  ~BookmarkUuidMapperTest() override = default;
};

TEST_F(BookmarkUuidMapperTest, BookmarkIdTupleBasics) {
  const base::Uuid uuid = base::Uuid::ParseLowercase(kSharedNativeUuidStr);
  BookmarkIdTuple tuple1(uuid, 42);
  EXPECT_EQ(tuple1.uuid(), uuid);
  EXPECT_EQ(tuple1.id(), 42);

  BookmarkIdTuple tuple2(uuid, 42);
  EXPECT_EQ(tuple1, tuple2);

  BookmarkIdTuple tuple3(uuid, 43);
  EXPECT_NE(tuple1, tuple3);
  EXPECT_LT(tuple1, tuple3);

  BookmarkIdTupleHash hash;
  EXPECT_EQ(hash(tuple1), hash(tuple2));

  bookmarks::BookmarkNode node(/*id=*/99, uuid, GURL("https://example.com"));
  BookmarkIdTuple tuple_from_node(&node);
  EXPECT_EQ(tuple_from_node.id(), 99);
  EXPECT_EQ(tuple_from_node.uuid(), uuid);
}

TEST_F(BookmarkUuidMapperTest, StandardUniqueNodeMapsToRandomV4Uuid) {
  const base::Uuid shared_native_uuid =
      base::Uuid::ParseLowercase(kSharedNativeUuidStr);
  BookmarkUuidMapper mapper;
  bookmarks::BookmarkNode node(/*id=*/1, shared_native_uuid,
                               GURL("https://example.com"));

  base::Uuid api_uuid = mapper.GetUuidFor(&node);
  EXPECT_TRUE(api_uuid.is_valid());
  EXPECT_NE(api_uuid, shared_native_uuid);

  // Subsequent lookups return the same API UUID
  EXPECT_EQ(mapper.GetUuidFor(&node), api_uuid);
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(api_uuid), 1);

  std::optional<BookmarkIdTuple> model_id = mapper.MaybeGetModelId(api_uuid);
  ASSERT_TRUE(model_id.has_value());
  EXPECT_EQ(model_id->id(), 1);
  EXPECT_EQ(model_id->uuid(), shared_native_uuid);
}

TEST_F(BookmarkUuidMapperTest, ExplicitOverrideTakesPrecedence) {
  const base::Uuid shared_native_uuid =
      base::Uuid::ParseLowercase(kSharedNativeUuidStr);
  BookmarkUuidMapper mapper;
  bookmarks::BookmarkNode node(/*id=*/2, shared_native_uuid,
                               GURL("https://example.com"));
  base::Uuid custom_override = base::Uuid::GenerateRandomV4();

  mapper.SetUuidOverride(&node, custom_override);
  EXPECT_TRUE(mapper.HasOverrideFor(&node));
  EXPECT_EQ(mapper.GetUuidFor(&node), custom_override);
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(custom_override), 2);
}

TEST_F(BookmarkUuidMapperTest, MultipleNodesGetUniqueRandomUuids) {
  const base::Uuid shared_native_uuid =
      base::Uuid::ParseLowercase(kSharedNativeUuidStr);
  BookmarkUuidMapper mapper;
  bookmarks::BookmarkNode node_1(/*id=*/10, shared_native_uuid,
                                 GURL("https://node1.com"));
  bookmarks::BookmarkNode node_2(/*id=*/20, shared_native_uuid,
                                 GURL("https://node2.com"));

  base::Uuid uuid1 = mapper.GetUuidFor(&node_1);
  base::Uuid uuid2 = mapper.GetUuidFor(&node_2);

  EXPECT_TRUE(uuid1.is_valid());
  EXPECT_TRUE(uuid2.is_valid());
  EXPECT_NE(uuid1, shared_native_uuid);
  EXPECT_NE(uuid2, shared_native_uuid);
  EXPECT_NE(uuid1, uuid2);

  // Reverse lookups resolve accurately to their respective model tuples
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(uuid1), 10);
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(uuid2), 20);

  std::optional<BookmarkIdTuple> tuple1 = mapper.MaybeGetModelId(uuid1);
  ASSERT_TRUE(tuple1.has_value());
  EXPECT_EQ(tuple1->id(), 10);
  EXPECT_EQ(tuple1->uuid(), shared_native_uuid);

  std::optional<BookmarkIdTuple> tuple2 = mapper.MaybeGetModelId(uuid2);
  ASSERT_TRUE(tuple2.has_value());
  EXPECT_EQ(tuple2->id(), 20);
  EXPECT_EQ(tuple2->uuid(), shared_native_uuid);
}

TEST_F(BookmarkUuidMapperTest, SyntheticNodesWithSameIdHaveSeparateMappings) {
  const base::Uuid uuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid uuid2 = base::Uuid::GenerateRandomV4();
  BookmarkUuidMapper mapper;
  bookmarks::BookmarkNode node_1(/*id=*/0, uuid1, GURL("https://node1.com"));
  bookmarks::BookmarkNode node_2(/*id=*/0, uuid2, GURL("https://node2.com"));

  base::Uuid api_uuid1 = mapper.GetUuidFor(&node_1);
  base::Uuid api_uuid2 = mapper.GetUuidFor(&node_2);

  EXPECT_TRUE(api_uuid1.is_valid());
  EXPECT_TRUE(api_uuid2.is_valid());
  EXPECT_NE(api_uuid1, api_uuid2);
  EXPECT_EQ(mapper.GetUuidFor(&node_1), api_uuid1);
  EXPECT_EQ(mapper.GetUuidFor(&node_2), api_uuid2);
  EXPECT_TRUE(mapper.HasOverrideFor(&node_1));
  EXPECT_TRUE(mapper.HasOverrideFor(&node_2));
}

TEST_F(BookmarkUuidMapperTest, NodeRemovalClearsMapping) {
  const base::Uuid shared_native_uuid =
      base::Uuid::ParseLowercase(kSharedNativeUuidStr);
  BookmarkUuidMapper mapper;
  bookmarks::BookmarkNode node(/*id=*/50, shared_native_uuid,
                               GURL("https://example.com"));

  base::Uuid api_uuid = mapper.GetUuidFor(&node);
  EXPECT_TRUE(api_uuid.is_valid());

  mapper.RemoveNode(&node);
  EXPECT_FALSE(mapper.HasOverrideFor(&node));
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(api_uuid), std::nullopt);
  EXPECT_EQ(mapper.MaybeGetModelId(api_uuid), std::nullopt);
}

TEST_F(BookmarkUuidMapperTest, SubtreeNodeRemovalClearsMapping) {
  const base::Uuid uuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid uuid2 = base::Uuid::GenerateRandomV4();
  BookmarkUuidMapper mapper;
  bookmarks::BookmarkNode folder(/*id=*/10, uuid1, GURL());
  auto child = std::make_unique<bookmarks::BookmarkNode>(
      /*id=*/11, uuid2, GURL("https://example.com"));
  bookmarks::BookmarkNode* child_ptr = child.get();
  folder.Add(std::move(child));

  base::Uuid api_folder_uuid = mapper.GetUuidFor(&folder);
  base::Uuid api_child_uuid = mapper.GetUuidFor(child_ptr);
  EXPECT_TRUE(mapper.HasOverrideFor(&folder));
  EXPECT_TRUE(mapper.HasOverrideFor(child_ptr));

  mapper.RemoveNode(&folder);
  EXPECT_FALSE(mapper.HasOverrideFor(&folder));
  EXPECT_FALSE(mapper.HasOverrideFor(child_ptr));
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(api_folder_uuid), std::nullopt);
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(api_child_uuid), std::nullopt);
}

TEST_F(BookmarkUuidMapperTest, ClearClearsAllMappings) {
  const base::Uuid uuid1 = base::Uuid::GenerateRandomV4();
  BookmarkUuidMapper mapper;
  bookmarks::BookmarkNode node(/*id=*/100, uuid1, GURL("https://example.com"));

  base::Uuid api_uuid = mapper.GetUuidFor(&node);
  EXPECT_TRUE(mapper.HasOverrideFor(&node));

  mapper.Clear();
  EXPECT_FALSE(mapper.HasOverrideFor(&node));
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(api_uuid), std::nullopt);
}

TEST_F(BookmarkUuidMapperTest,
       ClearAllExceptPreservesSpecifiedAndDiscardsOthers) {
  const base::Uuid uuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid uuid2 = base::Uuid::GenerateRandomV4();
  const base::Uuid uuid3 = base::Uuid::GenerateRandomV4();
  BookmarkUuidMapper mapper;
  bookmarks::BookmarkNode permanent_node1(/*id=*/1, uuid1, GURL());
  bookmarks::BookmarkNode permanent_node2(/*id=*/2, uuid2, GURL());
  bookmarks::BookmarkNode user_node(/*id=*/10, uuid3,
                                    GURL("https://example.com"));

  base::Uuid perm1_api_uuid = mapper.GetUuidFor(&permanent_node1);
  base::Uuid perm2_api_uuid = mapper.GetUuidFor(&permanent_node2);
  base::Uuid user_api_uuid = mapper.GetUuidFor(&user_node);

  EXPECT_TRUE(mapper.HasOverrideFor(&permanent_node1));
  EXPECT_TRUE(mapper.HasOverrideFor(&permanent_node2));
  EXPECT_TRUE(mapper.HasOverrideFor(&user_node));

  mapper.ClearAllExcept({&permanent_node1, &permanent_node2});

  // Permanent node mappings are preserved with the exact same API UUIDs.
  EXPECT_TRUE(mapper.HasOverrideFor(&permanent_node1));
  EXPECT_TRUE(mapper.HasOverrideFor(&permanent_node2));
  EXPECT_EQ(mapper.GetUuidFor(&permanent_node1), perm1_api_uuid);
  EXPECT_EQ(mapper.GetUuidFor(&permanent_node2), perm2_api_uuid);
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(perm1_api_uuid), 1);
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(perm2_api_uuid), 2);

  // User node mapping is discarded.
  EXPECT_FALSE(mapper.HasOverrideFor(&user_node));
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(user_api_uuid), std::nullopt);
}

}  // namespace

}  // namespace bookmarks_api
