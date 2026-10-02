// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmark_uuid_mapper.h"

#include <string>
#include <string_view>

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

TEST_F(BookmarkUuidMapperTest, StandardUniqueNodeMapsToDerivedUuid) {
  const base::Uuid shared_native_uuid =
      base::Uuid::ParseLowercase(kSharedNativeUuidStr);
  BookmarkUuidMapper mapper;
  bookmarks::BookmarkNode node(/*id=*/1, shared_native_uuid,
                               GURL("https://example.com"));

  base::Uuid api_uuid = mapper.GetUuidFor(&node, BookmarkStorage::kLocal);
  EXPECT_TRUE(api_uuid.is_valid());
  EXPECT_NE(api_uuid, shared_native_uuid);

  // Subsequent lookups return the same API UUID
  EXPECT_EQ(mapper.GetUuidFor(&node, BookmarkStorage::kLocal), api_uuid);
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
  EXPECT_EQ(mapper.GetUuidFor(&node, BookmarkStorage::kLocal), custom_override);
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(custom_override), 2);
}

// Nodes in the same storage shouldn't share a native UUID, but if they do, the
// mapper must still hand out distinct API UUIDs.
TEST_F(BookmarkUuidMapperTest, NodesWithSameUuidInSameStorageStayUnique) {
  const base::Uuid shared_native_uuid =
      base::Uuid::ParseLowercase(kSharedNativeUuidStr);
  BookmarkUuidMapper mapper;
  bookmarks::BookmarkNode node_1(/*id=*/10, shared_native_uuid,
                                 GURL("https://node1.com"));
  bookmarks::BookmarkNode node_2(/*id=*/20, shared_native_uuid,
                                 GURL("https://node2.com"));

  base::Uuid uuid1 = mapper.GetUuidFor(&node_1, BookmarkStorage::kLocal);
  base::Uuid uuid2 = mapper.GetUuidFor(&node_2, BookmarkStorage::kLocal);

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

  base::Uuid api_uuid1 = mapper.GetUuidFor(&node_1, BookmarkStorage::kLocal);
  base::Uuid api_uuid2 = mapper.GetUuidFor(&node_2, BookmarkStorage::kLocal);

  EXPECT_TRUE(api_uuid1.is_valid());
  EXPECT_TRUE(api_uuid2.is_valid());
  EXPECT_NE(api_uuid1, api_uuid2);
  EXPECT_EQ(mapper.GetUuidFor(&node_1, BookmarkStorage::kLocal), api_uuid1);
  EXPECT_EQ(mapper.GetUuidFor(&node_2, BookmarkStorage::kLocal), api_uuid2);
  EXPECT_TRUE(mapper.HasOverrideFor(&node_1));
  EXPECT_TRUE(mapper.HasOverrideFor(&node_2));
}

TEST_F(BookmarkUuidMapperTest, NodeRemovalClearsMapping) {
  const base::Uuid shared_native_uuid =
      base::Uuid::ParseLowercase(kSharedNativeUuidStr);
  BookmarkUuidMapper mapper;
  bookmarks::BookmarkNode node(/*id=*/50, shared_native_uuid,
                               GURL("https://example.com"));

  base::Uuid api_uuid = mapper.GetUuidFor(&node, BookmarkStorage::kLocal);
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

  base::Uuid api_folder_uuid =
      mapper.GetUuidFor(&folder, BookmarkStorage::kLocal);
  base::Uuid api_child_uuid =
      mapper.GetUuidFor(child_ptr, BookmarkStorage::kLocal);
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

  base::Uuid api_uuid = mapper.GetUuidFor(&node, BookmarkStorage::kLocal);
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

  base::Uuid perm1_api_uuid =
      mapper.GetUuidFor(&permanent_node1, BookmarkStorage::kLocal);
  base::Uuid perm2_api_uuid =
      mapper.GetUuidFor(&permanent_node2, BookmarkStorage::kLocal);
  base::Uuid user_api_uuid =
      mapper.GetUuidFor(&user_node, BookmarkStorage::kLocal);

  EXPECT_TRUE(mapper.HasOverrideFor(&permanent_node1));
  EXPECT_TRUE(mapper.HasOverrideFor(&permanent_node2));
  EXPECT_TRUE(mapper.HasOverrideFor(&user_node));

  mapper.ClearAllExcept({&permanent_node1, &permanent_node2});

  // Permanent node mappings are preserved with the exact same API UUIDs.
  EXPECT_TRUE(mapper.HasOverrideFor(&permanent_node1));
  EXPECT_TRUE(mapper.HasOverrideFor(&permanent_node2));
  EXPECT_EQ(mapper.GetUuidFor(&permanent_node1, BookmarkStorage::kLocal),
            perm1_api_uuid);
  EXPECT_EQ(mapper.GetUuidFor(&permanent_node2, BookmarkStorage::kLocal),
            perm2_api_uuid);
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(perm1_api_uuid), 1);
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(perm2_api_uuid), 2);

  // User node mapping is discarded.
  EXPECT_FALSE(mapper.HasOverrideFor(&user_node));
  EXPECT_EQ(mapper.MaybeGetIdFromUuidOverride(user_api_uuid), std::nullopt);
}

TEST_F(BookmarkUuidMapperTest, DerivedUuidIsVersion5) {
  BookmarkUuidMapper mapper;
  bookmarks::BookmarkNode node(/*id=*/1,
                               base::Uuid::ParseLowercase(kSharedNativeUuidStr),
                               GURL("https://example.com"));

  // xxxxxxxx-xxxx-5xxx-yxxx-xxxxxxxxxxxx, with y in [8, 9, a, b].
  const std::string api_uuid =
      mapper.GetUuidFor(&node, BookmarkStorage::kLocal).AsLowercaseString();
  EXPECT_EQ(api_uuid[14], '5');
  EXPECT_NE(std::string_view("89ab").find(api_uuid[19]), std::string::npos);
}

// Simulates a browser restart: a new mapper must produce the same API UUID.
TEST_F(BookmarkUuidMapperTest, DerivedUuidIsStableAcrossMappers) {
  bookmarks::BookmarkNode node(/*id=*/1,
                               base::Uuid::ParseLowercase(kSharedNativeUuidStr),
                               GURL("https://example.com"));

  BookmarkUuidMapper mapper1;
  BookmarkUuidMapper mapper2;
  EXPECT_EQ(mapper1.GetUuidFor(&node, BookmarkStorage::kLocal),
            mapper2.GetUuidFor(&node, BookmarkStorage::kLocal));
}

// Model ids can change across sessions (e.g. account storage is re-downloaded
// after sign-out and sign-in), so they must not affect the API UUID.
TEST_F(BookmarkUuidMapperTest, DerivedUuidIgnoresModelId) {
  const base::Uuid native_uuid =
      base::Uuid::ParseLowercase(kSharedNativeUuidStr);
  bookmarks::BookmarkNode before(/*id=*/1, native_uuid,
                                 GURL("https://example.com"));
  bookmarks::BookmarkNode after(/*id=*/42, native_uuid,
                                GURL("https://example.com"));

  BookmarkUuidMapper mapper1;
  BookmarkUuidMapper mapper2;
  EXPECT_EQ(mapper1.GetUuidFor(&before, BookmarkStorage::kLocal),
            mapper2.GetUuidFor(&after, BookmarkStorage::kLocal));
}

// Local and account storage may contain nodes with the same native UUID (e.g.
// the permanent folders, which use hard-coded UUIDs).
TEST_F(BookmarkUuidMapperTest, SameUuidInDifferentStoragesGetDistinctUuids) {
  const base::Uuid native_uuid =
      base::Uuid::ParseLowercase(kSharedNativeUuidStr);
  bookmarks::BookmarkNode local_node(/*id=*/1, native_uuid, GURL());
  bookmarks::BookmarkNode account_node(/*id=*/2, native_uuid, GURL());

  BookmarkUuidMapper mapper1;
  const base::Uuid local_api_uuid =
      mapper1.GetUuidFor(&local_node, BookmarkStorage::kLocal);
  const base::Uuid account_api_uuid =
      mapper1.GetUuidFor(&account_node, BookmarkStorage::kAccount);
  EXPECT_NE(local_api_uuid, account_api_uuid);

  // Both remain stable in a new mapper.
  BookmarkUuidMapper mapper2;
  EXPECT_EQ(mapper2.GetUuidFor(&local_node, BookmarkStorage::kLocal),
            local_api_uuid);
  EXPECT_EQ(mapper2.GetUuidFor(&account_node, BookmarkStorage::kAccount),
            account_api_uuid);
}

}  // namespace

}  // namespace bookmarks_api
