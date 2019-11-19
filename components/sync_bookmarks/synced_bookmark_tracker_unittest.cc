// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/synced_bookmark_tracker.h"

#include "base/base64.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/entity_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::IsNull;
using testing::NotNull;

namespace sync_bookmarks {

namespace {

// Redefinition of |enum CorruptionReason| in synced_bookmark_tracker.cc to be
// used in tests.
enum class ExpectedCorruptionReason {
  NO_CORRUPTION = 0,
  MISSING_SERVER_ID = 1,
  BOOKMARK_ID_IN_TOMBSTONE = 2,
  MISSING_BOOKMARK_ID = 3,
  COUNT_MISMATCH = 4,
  IDS_MISMATCH = 5,
  kMaxValue = IDS_MISMATCH
};

sync_pb::EntitySpecifics GenerateSpecifics(const std::string& title,
                                           const std::string& url) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_bookmark()->set_title(title);
  specifics.mutable_bookmark()->set_url(url);
  return specifics;
}

std::unique_ptr<sync_pb::EntityMetadata> CreateEntityMetadata(
    const std::string& server_id,
    bool is_deleted) {
  auto metadata = std::make_unique<sync_pb::EntityMetadata>();
  metadata->set_server_id(server_id);
  metadata->set_is_deleted(is_deleted);
  return metadata;
}

TEST(SyncedBookmarkTrackerTest, ShouldGetAssociatedNodes) {
  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  const std::string kSyncId = "SYNC_ID";
  const std::string kTitle = "Title";
  const GURL kUrl("http://www.foo.com");
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const base::Time kCreationTime(base::Time::Now() -
                                 base::TimeDelta::FromSeconds(1));
  const syncer::UniquePosition unique_position =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());

  bookmarks::BookmarkNode node(kId, base::GenerateGUID(), kUrl);
  tracker.Add(kSyncId, &node, kServerVersion, kCreationTime,
              unique_position.ToProto(), specifics);
  const SyncedBookmarkTracker::Entity* entity =
      tracker.GetEntityForSyncId(kSyncId);
  ASSERT_THAT(entity, NotNull());
  EXPECT_THAT(entity->bookmark_node(), Eq(&node));
  EXPECT_THAT(entity->metadata()->server_id(), Eq(kSyncId));
  EXPECT_THAT(entity->metadata()->server_version(), Eq(kServerVersion));
  EXPECT_THAT(entity->metadata()->creation_time(),
              Eq(syncer::TimeToProtoTime(kCreationTime)));
  EXPECT_TRUE(
      syncer::UniquePosition::FromProto(entity->metadata()->unique_position())
          .Equals(unique_position));

  syncer::EntityData data;
  *data.specifics.mutable_bookmark() = specifics.bookmark();
  data.unique_position = unique_position.ToProto();
  EXPECT_TRUE(entity->MatchesDataIgnoringParent(data));
  EXPECT_THAT(tracker.GetEntityForSyncId("unknown id"), IsNull());
}

TEST(SyncedBookmarkTrackerTest, ShouldReturnNullForDisassociatedNodes) {
  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  const std::string kSyncId = "SYNC_ID";
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const base::Time kModificationTime(base::Time::Now() -
                                     base::TimeDelta::FromSeconds(1));
  const sync_pb::UniquePosition unique_position;
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());
  bookmarks::BookmarkNode node(kId, base::GenerateGUID(), GURL());
  tracker.Add(kSyncId, &node, kServerVersion, kModificationTime,
              unique_position, specifics);
  ASSERT_THAT(tracker.GetEntityForSyncId(kSyncId), NotNull());
  tracker.Remove(kSyncId);
  EXPECT_THAT(tracker.GetEntityForSyncId(kSyncId), IsNull());
}

TEST(SyncedBookmarkTrackerTest, ShouldBuildBookmarkModelMetadata) {
  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  const std::string kSyncId = "SYNC_ID";
  const std::string kTitle = "Title";
  const GURL kUrl("http://www.foo.com");
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const base::Time kCreationTime(base::Time::Now() -
                                 base::TimeDelta::FromSeconds(1));
  const syncer::UniquePosition unique_position =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());

  bookmarks::BookmarkNode node(kId, base::GenerateGUID(), kUrl);
  tracker.Add(kSyncId, &node, kServerVersion, kCreationTime,
              unique_position.ToProto(), specifics);

  sync_pb::BookmarkModelMetadata bookmark_model_metadata =
      tracker.BuildBookmarkModelMetadata();

  ASSERT_THAT(bookmark_model_metadata.bookmarks_metadata().size(), Eq(1));
  EXPECT_THAT(
      bookmark_model_metadata.bookmarks_metadata(0).metadata().server_id(),
      Eq(kSyncId));
}

TEST(SyncedBookmarkTrackerTest,
     ShouldRequireCommitRequestWhenSequenceNumberIsIncremented) {
  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  const std::string kSyncId = "SYNC_ID";
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const base::Time kModificationTime(base::Time::Now() -
                                     base::TimeDelta::FromSeconds(1));
  const sync_pb::UniquePosition unique_position;
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());
  bookmarks::BookmarkNode node(kId, base::GenerateGUID(), GURL());
  tracker.Add(kSyncId, &node, kServerVersion, kModificationTime,
              unique_position, specifics);

  EXPECT_THAT(tracker.HasLocalChanges(), Eq(false));
  tracker.IncrementSequenceNumber(kSyncId);
  EXPECT_THAT(tracker.HasLocalChanges(), Eq(true));
  // TODO(crbug.com/516866): Test HasLocalChanges after submitting commit
  // request in a separate test probably.
}

TEST(SyncedBookmarkTrackerTest, ShouldAckSequenceNumber) {
  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  const std::string kSyncId = "SYNC_ID";
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const base::Time kModificationTime(base::Time::Now() -
                                     base::TimeDelta::FromSeconds(1));
  const sync_pb::UniquePosition unique_position;
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());
  bookmarks::BookmarkNode node(kId, base::GenerateGUID(), GURL());
  tracker.Add(kSyncId, &node, kServerVersion, kModificationTime,
              unique_position, specifics);

  // Test simple scenario of ack'ing an incrememented sequence number.
  EXPECT_THAT(tracker.HasLocalChanges(), Eq(false));
  tracker.IncrementSequenceNumber(kSyncId);
  EXPECT_THAT(tracker.HasLocalChanges(), Eq(true));
  tracker.AckSequenceNumber(kSyncId);
  EXPECT_THAT(tracker.HasLocalChanges(), Eq(false));

  // Test ack'ing of a mutliple times incremented sequence number.
  tracker.IncrementSequenceNumber(kSyncId);
  EXPECT_THAT(tracker.HasLocalChanges(), Eq(true));
  tracker.IncrementSequenceNumber(kSyncId);
  tracker.IncrementSequenceNumber(kSyncId);
  EXPECT_THAT(tracker.HasLocalChanges(), Eq(true));
  tracker.AckSequenceNumber(kSyncId);
  EXPECT_THAT(tracker.HasLocalChanges(), Eq(false));
}

TEST(SyncedBookmarkTrackerTest, ShouldUpdateUponCommitResponseWithNewId) {
  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  const std::string kSyncId = "SYNC_ID";
  const std::string kNewSyncId = "NEW_SYNC_ID";
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const int64_t kNewServerVersion = 1001;
  const base::Time kModificationTime(base::Time::Now() -
                                     base::TimeDelta::FromSeconds(1));
  const sync_pb::UniquePosition unique_position;
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());
  bookmarks::BookmarkNode node(kId, base::GenerateGUID(), GURL());
  tracker.Add(kSyncId, &node, kServerVersion, kModificationTime,
              unique_position, specifics);
  ASSERT_THAT(tracker.GetEntityForSyncId(kSyncId), NotNull());
  // Receive a commit response with a changed id.
  tracker.UpdateUponCommitResponse(
      kSyncId, kNewSyncId, /*acked_sequence_number=*/1, kNewServerVersion);
  // Old id shouldn't be there.
  EXPECT_THAT(tracker.GetEntityForSyncId(kSyncId), IsNull());

  const SyncedBookmarkTracker::Entity* entity =
      tracker.GetEntityForSyncId(kNewSyncId);
  ASSERT_THAT(entity, NotNull());
  EXPECT_THAT(entity->metadata()->server_id(), Eq(kNewSyncId));
  EXPECT_THAT(entity->bookmark_node(), Eq(&node));
  EXPECT_THAT(entity->metadata()->server_version(), Eq(kNewServerVersion));
}

TEST(SyncedBookmarkTrackerTest, ShouldUpdateId) {
  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  const std::string kSyncId = "SYNC_ID";
  const std::string kNewSyncId = "NEW_SYNC_ID";
  const int64_t kServerVersion = 1000;
  const base::Time kModificationTime(base::Time::Now() -
                                     base::TimeDelta::FromSeconds(1));
  const sync_pb::UniquePosition unique_position;
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());
  bookmarks::BookmarkNode node(/*id=*/1, base::GenerateGUID(), GURL());
  // Track a sync entity.
  tracker.Add(kSyncId, &node, kServerVersion, kModificationTime,
              unique_position, specifics);

  ASSERT_THAT(tracker.GetEntityForSyncId(kSyncId), NotNull());
  // Update the sync id.
  tracker.UpdateSyncForLocalCreationIfNeeded(kSyncId, kNewSyncId);
  // Old id shouldn't be there.
  EXPECT_THAT(tracker.GetEntityForSyncId(kSyncId), IsNull());

  const SyncedBookmarkTracker::Entity* entity =
      tracker.GetEntityForSyncId(kNewSyncId);
  ASSERT_THAT(entity, NotNull());
  EXPECT_THAT(entity->metadata()->server_id(), Eq(kNewSyncId));
  EXPECT_THAT(entity->bookmark_node(), Eq(&node));
  EXPECT_THAT(entity->metadata()->server_version(), Eq(kServerVersion));
}

TEST(SyncedBookmarkTrackerTest,
     ShouldMaintainTombstoneOrderBetweenCtorAndBuildBookmarkModelMetadata) {
  // Feed a metadata batch of 5 entries to the constructor of the tracker.
  // First 2 are for node, and the last 4 are for tombstones.

  // Server ids.
  const std::string kId0 = "id0";
  const std::string kId1 = "id1";
  const std::string kId2 = "id2";
  const std::string kId3 = "id3";
  const std::string kId4 = "id4";

  const GURL kUrl("http://www.foo.com");
  bookmarks::BookmarkNode node0(/*id=*/0, base::GenerateGUID(), kUrl);
  bookmarks::BookmarkNode node1(/*id=*/1, base::GenerateGUID(), kUrl);

  std::vector<NodeMetadataPair> node_metadata_pairs;
  node_metadata_pairs.emplace_back(
      &node0, CreateEntityMetadata(/*server_id=*/kId0, /*is_deleted=*/false));
  node_metadata_pairs.emplace_back(
      &node1, CreateEntityMetadata(/*server_id=*/kId1, /*is_deleted=*/false));
  node_metadata_pairs.emplace_back(
      nullptr, CreateEntityMetadata(/*server_id=*/kId2, /*is_deleted=*/true));
  node_metadata_pairs.emplace_back(
      nullptr, CreateEntityMetadata(/*server_id=*/kId3, /*is_deleted=*/true));
  node_metadata_pairs.emplace_back(
      nullptr, CreateEntityMetadata(/*server_id=*/kId4, /*is_deleted=*/true));

  SyncedBookmarkTracker tracker(std::move(node_metadata_pairs),
                                std::make_unique<sync_pb::ModelTypeState>());

  sync_pb::BookmarkModelMetadata bookmark_model_metadata =
      tracker.BuildBookmarkModelMetadata();

  // Tombstones should be the last 3 entries in the metadata and in the same
  // order as given to the constructor.
  ASSERT_THAT(bookmark_model_metadata.bookmarks_metadata().size(), Eq(5));
  EXPECT_THAT(
      bookmark_model_metadata.bookmarks_metadata(2).metadata().server_id(),
      Eq(kId2));
  EXPECT_THAT(
      bookmark_model_metadata.bookmarks_metadata(3).metadata().server_id(),
      Eq(kId3));
  EXPECT_THAT(
      bookmark_model_metadata.bookmarks_metadata(4).metadata().server_id(),
      Eq(kId4));
}

TEST(SyncedBookmarkTrackerTest,
     ShouldMaintainOrderOfMarkDeletedCallsWhenBuildBookmarkModelMetadata) {
  // Server ids.
  const std::string kId0 = "id0";
  const std::string kId1 = "id1";
  const std::string kId2 = "id2";
  const std::string kId3 = "id3";
  const std::string kId4 = "id4";

  const GURL kUrl("http://www.foo.com");
  bookmarks::BookmarkNode node0(/*id=*/0, base::GenerateGUID(), kUrl);
  bookmarks::BookmarkNode node1(/*id=*/1, base::GenerateGUID(), kUrl);
  bookmarks::BookmarkNode node2(/*id=*/2, base::GenerateGUID(), kUrl);
  bookmarks::BookmarkNode node3(/*id=*/3, base::GenerateGUID(), kUrl);
  bookmarks::BookmarkNode node4(/*id=*/4, base::GenerateGUID(), kUrl);

  std::vector<NodeMetadataPair> node_metadata_pairs;
  node_metadata_pairs.emplace_back(
      &node0, CreateEntityMetadata(/*server_id=*/kId0, /*is_deleted=*/false));
  node_metadata_pairs.emplace_back(
      &node1, CreateEntityMetadata(/*server_id=*/kId1, /*is_deleted=*/false));
  node_metadata_pairs.emplace_back(
      &node2, CreateEntityMetadata(/*server_id=*/kId2, /*is_deleted=*/false));
  node_metadata_pairs.emplace_back(
      &node3, CreateEntityMetadata(/*server_id=*/kId3, /*is_deleted=*/false));
  node_metadata_pairs.emplace_back(
      &node4, CreateEntityMetadata(/*server_id=*/kId4, /*is_deleted=*/false));

  SyncedBookmarkTracker tracker(std::move(node_metadata_pairs),
                                std::make_unique<sync_pb::ModelTypeState>());

  // Mark entities deleted in that order kId2, kId4, kId1
  tracker.MarkDeleted(kId2);
  tracker.MarkDeleted(kId4);
  tracker.MarkDeleted(kId1);

  sync_pb::BookmarkModelMetadata bookmark_model_metadata =
      tracker.BuildBookmarkModelMetadata();

  // Tombstones should be the last 3 entries in the metadata and in the same as
  // calling MarkDeleted().
  ASSERT_THAT(bookmark_model_metadata.bookmarks_metadata().size(), Eq(5));
  EXPECT_THAT(
      bookmark_model_metadata.bookmarks_metadata(2).metadata().server_id(),
      Eq(kId2));
  EXPECT_THAT(
      bookmark_model_metadata.bookmarks_metadata(3).metadata().server_id(),
      Eq(kId4));
  EXPECT_THAT(
      bookmark_model_metadata.bookmarks_metadata(4).metadata().server_id(),
      Eq(kId1));
}

TEST(SyncedBookmarkTrackerTest,
     ShouldOrderParentUpdatesBeforeChildUpdatesAndDeletionsComeLast) {
  const size_t kMaxEntries = 1000;

  // Construct this structure:
  // bookmark_bar
  //  |- node0
  //    |- node1
  //      |- node2

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node0 = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("node0"));
  const bookmarks::BookmarkNode* node1 = bookmark_model->AddFolder(
      /*parent=*/node0, /*index=*/0, base::UTF8ToUTF16("node1"));
  const bookmarks::BookmarkNode* node2 = bookmark_model->AddFolder(
      /*parent=*/node1, /*index=*/0, base::UTF8ToUTF16("node2"));

  // Server ids.
  const std::string kId0 = "id0";
  const std::string kId1 = "id1";
  const std::string kId2 = "id2";
  const std::string kId3 = "id3";

  // Prepare the metadata with shuffled order.
  std::vector<NodeMetadataPair> node_metadata_pairs;
  node_metadata_pairs.emplace_back(
      node1, CreateEntityMetadata(/*server_id=*/kId1, /*is_deleted=*/false));
  node_metadata_pairs.emplace_back(
      nullptr, CreateEntityMetadata(/*server_id=*/kId3, /*is_deleted=*/true));
  node_metadata_pairs.emplace_back(
      node2, CreateEntityMetadata(/*server_id=*/kId2, /*is_deleted=*/false));
  node_metadata_pairs.emplace_back(
      node0, CreateEntityMetadata(/*server_id=*/kId0, /*is_deleted=*/false));

  SyncedBookmarkTracker tracker(std::move(node_metadata_pairs),
                                std::make_unique<sync_pb::ModelTypeState>());

  // Mark the entities that they have local changes. (in shuffled order just to
  // verify the tracker doesn't simply maintain the order of updates similar to
  // with deletions).
  tracker.IncrementSequenceNumber(kId3);
  tracker.IncrementSequenceNumber(kId1);
  tracker.IncrementSequenceNumber(kId2);
  tracker.IncrementSequenceNumber(kId0);

  std::vector<const SyncedBookmarkTracker::Entity*> entities_with_local_change =
      tracker.GetEntitiesWithLocalChanges(kMaxEntries);

  ASSERT_THAT(entities_with_local_change.size(), Eq(4U));
  // Verify updates are in parent before child order node0 --> node1 --> node2.
  EXPECT_THAT(entities_with_local_change[0]->metadata()->server_id(), Eq(kId0));
  EXPECT_THAT(entities_with_local_change[1]->metadata()->server_id(), Eq(kId1));
  EXPECT_THAT(entities_with_local_change[2]->metadata()->server_id(), Eq(kId2));
  // Verify that deletion is the last entry.
  EXPECT_THAT(entities_with_local_change[3]->metadata()->server_id(), Eq(kId3));
}

TEST(SyncedBookmarkTrackerTest, ShouldMatchModelAndMetadata) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("node0"));

  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.mutable_model_type_state()->set_initial_sync_done(true);
  // Add entries for all the permanent nodes. TestBookmarkClient creates all the
  // 3 permanent nodes.
  sync_pb::BookmarkMetadata* bookmark_metadata =
      model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(model->bookmark_bar_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id("BookmarkBarId");

  bookmark_metadata = model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(model->other_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id("OtherBookmarksId");

  bookmark_metadata = model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(model->mobile_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id("MobileBookmarksId");

  // Add entry for the managed node.
  bookmark_metadata = model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(node->id());
  bookmark_metadata->mutable_metadata()->set_server_id("NodeId");

  // Add a tombstone entry.
  sync_pb::BookmarkMetadata* tombstone =
      model_metadata.add_bookmarks_metadata();
  tombstone->mutable_metadata()->set_server_id("tombstoneId");
  tombstone->mutable_metadata()->set_is_deleted(true);

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(SyncedBookmarkTracker::BookmarkModelMatchesMetadata(
      model.get(), model_metadata));
  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::NO_CORRUPTION, /*count=*/1);
}

TEST(SyncedBookmarkTrackerTest,
     ShouldNotMatchModelAndMetadataIfMissingMobileFolder) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.mutable_model_type_state()->set_initial_sync_done(true);
  // Add entries for all the permanent nodes except for the Mobile bookmarks
  // folder.
  sync_pb::BookmarkMetadata* bookmark_metadata =
      model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(model->bookmark_bar_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id("BookmarkBarId");

  bookmark_metadata = model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(model->other_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id("OtherBookmarksId");

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(SyncedBookmarkTracker::BookmarkModelMatchesMetadata(
      model.get(), model_metadata));
  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::COUNT_MISMATCH, /*count=*/1);
}

TEST(SyncedBookmarkTrackerTest, ShouldNotMatchModelAndCorruptedMetadata) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.mutable_model_type_state()->set_initial_sync_done(true);
  // Add entries for 3 permanent nodes only. TestBookmarkClient creates all the
  // 4 permanent nodes.
  sync_pb::BookmarkMetadata* bookmark_metadata =
      model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(model->bookmark_bar_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id("BookmarkBarId");

  bookmark_metadata = model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(model->mobile_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id("MobileBookmarksId");

  base::HistogramTester histogram_tester;
  // The entry for the Other bookmarks is missing.
  EXPECT_FALSE(SyncedBookmarkTracker::BookmarkModelMatchesMetadata(
      model.get(), model_metadata));
  histogram_tester.ExpectBucketCount(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::COUNT_MISMATCH, /*count=*/1);

  // The entry for the Other bookmarks is missing a server id.
  bookmark_metadata = model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(model->other_node()->id());
  EXPECT_FALSE(SyncedBookmarkTracker::BookmarkModelMatchesMetadata(
      model.get(), model_metadata));

  histogram_tester.ExpectBucketCount(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::MISSING_SERVER_ID, /*count=*/1);

  // The entry for the Other bookmarks is missing a node id.
  bookmark_metadata->clear_id();
  bookmark_metadata->mutable_metadata()->set_server_id("OtherBookmarksId");
  EXPECT_FALSE(SyncedBookmarkTracker::BookmarkModelMatchesMetadata(
      model.get(), model_metadata));
  histogram_tester.ExpectBucketCount(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::MISSING_BOOKMARK_ID, /*count=*/1);

  // The entry for the Other bookmarks is having a wrong node id.
  bookmark_metadata->set_id(model->other_node()->id() + 1000);
  EXPECT_FALSE(SyncedBookmarkTracker::BookmarkModelMatchesMetadata(
      model.get(), model_metadata));
  histogram_tester.ExpectBucketCount(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::IDS_MISMATCH, /*count=*/1);

  // A tombstone shouldn't have a node id.
  sync_pb::BookmarkMetadata* tombstone =
      model_metadata.add_bookmarks_metadata();
  tombstone->mutable_metadata()->set_server_id("tombstoneId");
  tombstone->mutable_metadata()->set_is_deleted(true);
  tombstone->set_id(10);
  EXPECT_FALSE(SyncedBookmarkTracker::BookmarkModelMatchesMetadata(
      model.get(), model_metadata));
  histogram_tester.ExpectBucketCount(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::BOOKMARK_ID_IN_TOMBSTONE,
      /*count=*/1);
}

TEST(SyncedBookmarkTrackerTest,
     ShouldMatchModelWithUnsyncableNodesAndMetadata) {
  // Add a managed node with an arbitrary id 100.
  const int64_t kManagedNodeId = 100;
  auto owned_managed_node = std::make_unique<bookmarks::BookmarkPermanentNode>(
      kManagedNodeId, bookmarks::BookmarkNode::FOLDER);
  bookmarks::BookmarkPermanentNode* managed_node = owned_managed_node.get();
  auto client = std::make_unique<bookmarks::TestBookmarkClient>();
  client->SetManagedNodeToLoad(std::move(owned_managed_node));

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));

  // The model should contain the managed node now.
  ASSERT_THAT(GetBookmarkNodeByID(model.get(), kManagedNodeId),
              Eq(managed_node));

  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.mutable_model_type_state()->set_initial_sync_done(true);
  // Add entries for all the permanent nodes. TestBookmarkClient creates all the
  // 3 permanent nodes.
  sync_pb::BookmarkMetadata* bookmark_metadata =
      model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(model->bookmark_bar_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id("BookmarkBarId");

  bookmark_metadata = model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(model->other_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id("OtherBookmarksId");

  bookmark_metadata = model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(model->mobile_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id("MobileBookmarksId");

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(SyncedBookmarkTracker::BookmarkModelMatchesMetadata(
      model.get(), model_metadata));
  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::NO_CORRUPTION, /*count=*/1);
}

}  // namespace

}  // namespace sync_bookmarks
