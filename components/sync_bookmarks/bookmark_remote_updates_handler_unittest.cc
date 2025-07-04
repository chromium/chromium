// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_remote_updates_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/protocol/bookmark_model_metadata.pb.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync_bookmarks/bookmark_model_merger.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/switches.h"
#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"
#include "components/sync_bookmarks/test_bookmark_model_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using testing::_;
using testing::AnyOf;
using testing::ElementsAre;
using testing::Eq;
using testing::IsNull;
using testing::NotNull;

namespace sync_bookmarks {

namespace {

// The parent tag for children of the root entity. Entities with this parent are
// referred to as top level entities.
const char kBookmarkBarId[] = "bookmark_bar_id";
const char kBookmarkBarTag[] = "bookmark_bar";
const char kMobileBookmarksId[] = "synced_bookmarks_id";
const char kMobileBookmarksTag[] = "synced_bookmarks";
const char kOtherBookmarksId[] = "other_bookmarks_id";
const char kOtherBookmarksTag[] = "other_bookmarks";

// Fork of enum RemoteBookmarkUpdateError.
enum class ExpectedRemoteBookmarkUpdateError {
  kConflictingTypes = 0,
  kInvalidSpecifics = 1,
  // kDeprecatedInvalidUniquePosition = 2,
  // kDeprecatedPermanentNodeCreationAfterMerge = 3,
  kMissingParentEntity = 4,
  kMissingParentNode = 5,
  kMissingParentEntityInConflict = 6,
  kMissingParentNodeInConflict = 7,
  kCreationFailure = 8,
  kUnexpectedGuid = 9,
  kParentNotFolder = 10,
  kGuidChangedForTrackedServerId = 11,
  kTrackedServerIdWithoutServerTagMatchesPermanentNode = 12,
  kMaxValue = kTrackedServerIdWithoutServerTagMatchesPermanentNode,
};

syncer::UniquePosition RandomUniquePosition() {
  return syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());
}

// Returns a sync ID mimic-ing what a real server could return, which means it
// generally opaque for the client but deterministic given |guid|, because the
// sync ID is roughly a hashed GUID, at least in normal circumnstances where the
// GUID is used either as client tag hash or as originator client item ID.
std::string GetFakeServerIdFromGUID(const base::Uuid& guid) {
  // For convenience in tests, |guid| may refer to permanent nodes too,
  // and yet the returned sync ID will honor the sync ID constants for permanent
  // nodes.
  if (guid.AsLowercaseString() == bookmarks::kBookmarkBarNodeUuid) {
    return kBookmarkBarId;
  }
  if (guid.AsLowercaseString() == bookmarks::kOtherBookmarksNodeUuid) {
    return kOtherBookmarksId;
  }
  if (guid.AsLowercaseString() == bookmarks::kMobileBookmarksNodeUuid) {
    return kMobileBookmarksId;
  }
  return base::StrCat({"server_id_for_", guid.AsLowercaseString()});
}

// |node| must not be nullptr.
sync_pb::BookmarkMetadata CreateNodeMetadata(
    const bookmarks::BookmarkNode* node,
    const syncer::UniquePosition& unique_position) {
  DCHECK(node);
  sync_pb::BookmarkMetadata bookmark_metadata;
  bookmark_metadata.set_id(node->id());
  bookmark_metadata.mutable_metadata()->set_server_id(
      GetFakeServerIdFromGUID(node->uuid()));
  bookmark_metadata.mutable_metadata()->set_client_tag_hash(
      syncer::ClientTagHash::FromUnhashed(syncer::BOOKMARKS,
                                          node->uuid().AsLowercaseString())
          .value());
  *bookmark_metadata.mutable_metadata()->mutable_unique_position() =
      unique_position.ToProto();
  return bookmark_metadata;
}

// |node| must not be nullptr.
sync_pb::BookmarkMetadata CreatePermanentNodeMetadata(
    const bookmarks::BookmarkNode* node,
    const std::string& server_id) {
  sync_pb::BookmarkMetadata bookmark_metadata;
  bookmark_metadata.set_id(node->id());
  bookmark_metadata.mutable_metadata()->set_server_id(server_id);
  return bookmark_metadata;
}

sync_pb::BookmarkModelMetadata CreateMetadataForPermanentNodes(
    const BookmarkModelView* bookmark_model) {
  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.mutable_data_type_state()->set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  model_metadata.set_bookmarks_hierarchy_fields_reuploaded(true);

  *model_metadata.add_bookmarks_metadata() =
      CreatePermanentNodeMetadata(bookmark_model->bookmark_bar_node(),
                                  /*server_id=*/kBookmarkBarId);
  *model_metadata.add_bookmarks_metadata() =
      CreatePermanentNodeMetadata(bookmark_model->mobile_node(),
                                  /*server_id=*/kMobileBookmarksId);
  *model_metadata.add_bookmarks_metadata() =
      CreatePermanentNodeMetadata(bookmark_model->other_node(),
                                  /*server_id=*/kOtherBookmarksId);

  return model_metadata;
}

syncer::UpdateResponseData CreateTombstoneResponseData(const base::Uuid& guid,
                                                       int version) {
  syncer::EntityData data;
  data.id = GetFakeServerIdFromGUID(guid);

  // EntityData is considered a deletion if its specifics hasn't been set.
  DCHECK(data.is_deleted());

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  response_data.response_version = version;

  return response_data;
}

syncer::UpdateResponseData CreateUpdateResponseData(
    const base::Uuid& guid,
    const base::Uuid& parent_guid,
    const std::string& title,
    int version,
    const syncer::UniquePosition& unique_position) {
  syncer::UpdateResponseData response_data =
      CreateTombstoneResponseData(guid, version);

  response_data.entity.originator_client_item_id = guid.AsLowercaseString();

  sync_pb::BookmarkSpecifics* bookmark_specifics =
      response_data.entity.specifics.mutable_bookmark();
  bookmark_specifics->set_guid(guid.AsLowercaseString());
  bookmark_specifics->set_parent_guid(parent_guid.AsLowercaseString());
  bookmark_specifics->set_legacy_canonicalized_title(title);
  bookmark_specifics->set_full_title(title);
  bookmark_specifics->set_type(sync_pb::BookmarkSpecifics::FOLDER);
  *bookmark_specifics->mutable_unique_position() = unique_position.ToProto();

  DCHECK(!response_data.entity.is_deleted());
  return response_data;
}

// Overload that assign a random position. Should only be used when the title,
// version and position are irrelevant.
syncer::UpdateResponseData CreateUpdateResponseData(
    const base::Uuid& guid,
    const base::Uuid& parent_guid) {
  return CreateUpdateResponseData(
      guid, parent_guid, base::StrCat({"Title for ", guid.AsLowercaseString()}),
      /*version=*/0, RandomUniquePosition());
}

syncer::UpdateResponseData CreateBookmarkRootUpdateData() {
  syncer::EntityData data;
  data.id = syncer::DataTypeToProtocolRootTag(syncer::BOOKMARKS);
  data.server_defined_unique_tag =
      syncer::DataTypeToProtocolRootTag(syncer::BOOKMARKS);

  data.specifics.mutable_bookmark();

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;
  return response_data;
}

syncer::UpdateResponseData CreatePermanentFolderUpdateData(
    const std::string& id,
    const std::string& tag) {
  syncer::EntityData data;
  data.id = id;
  data.server_defined_unique_tag = tag;

  data.specifics.mutable_bookmark();

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;
  return response_data;
}

syncer::UpdateResponseDataList CreatePermanentFoldersUpdateData() {
  syncer::UpdateResponseDataList updates;
  updates.push_back(
      CreatePermanentFolderUpdateData(kBookmarkBarId, kBookmarkBarTag));
  updates.push_back(
      CreatePermanentFolderUpdateData(kOtherBookmarksId, kOtherBookmarksTag));
  updates.push_back(
      CreatePermanentFolderUpdateData(kMobileBookmarksId, kMobileBookmarksTag));
  return updates;
}

class BookmarkRemoteUpdatesHandlerWithInitialMergeTest : public testing::Test {
 public:
  BookmarkRemoteUpdatesHandlerWithInitialMergeTest()
      : tracker_(SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState())),
        updates_handler_(&bookmark_model_, &favicon_service_, tracker_.get()) {
    BookmarkModelMerger(CreatePermanentFoldersUpdateData(), &bookmark_model_,
                        &favicon_service_, tracker_.get())
        .Merge();
  }

  BookmarkModelView* bookmark_model() { return &bookmark_model_; }
  SyncedBookmarkTracker* tracker() { return tracker_.get(); }
  favicon::MockFaviconService* favicon_service() { return &favicon_service_; }
  BookmarkRemoteUpdatesHandler* updates_handler() { return &updates_handler_; }

  const base::Uuid kBookmarkBarGuid =
      base::Uuid::ParseLowercase(bookmarks::kBookmarkBarNodeUuid);

 private:
  TestBookmarkModelView bookmark_model_;
  std::unique_ptr<SyncedBookmarkTracker> tracker_;
  testing::NiceMock<favicon::MockFaviconService> favicon_service_;
  BookmarkRemoteUpdatesHandler updates_handler_;
};

TEST(BookmarkRemoteUpdatesHandlerReorderUpdatesTest, ShouldIgnoreRootNode) {
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkRootUpdateData());
  std::vector<const syncer::UpdateResponseData*> ordered_updates =
      BookmarkRemoteUpdatesHandler::ReorderValidUpdatesForTest(&updates);
  // Root node update should be filtered out.
  EXPECT_THAT(ordered_updates.size(), Eq(0U));
}

TEST(BookmarkRemoteUpdatesHandlerReorderUpdatesTest,
     ShouldIgnorePermanentNodes) {
  syncer::UpdateResponseDataList updates = CreatePermanentFoldersUpdateData();
  std::vector<const syncer::UpdateResponseData*> ordered_updates =
      BookmarkRemoteUpdatesHandler::ReorderValidUpdatesForTest(&updates);
  // Root node update should be filtered out.
  EXPECT_THAT(ordered_updates.size(), Eq(0U));
}

TEST(BookmarkRemoteUpdatesHandlerReorderUpdatesTest,
     ShouldIgnoreInvalidSpecifics) {
  const std::string kTitle = "title";
  const syncer::UniquePosition kPosition = RandomUniquePosition();
  const base::Uuid kBookmarkBarGuid =
      base::Uuid::ParseLowercase(bookmarks::kBookmarkBarNodeUuid);

  syncer::UpdateResponseDataList updates;

  // Create update with an invalid GUID.
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/base::Uuid(),
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/0,
      /*unique_position=*/kPosition));

  base::HistogramTester histogram_tester;
  std::vector<const syncer::UpdateResponseData*> ordered_updates =
      BookmarkRemoteUpdatesHandler::ReorderValidUpdatesForTest(&updates);

  // The update should be filtered out.
  EXPECT_THAT(ordered_updates.size(), Eq(0U));

  histogram_tester.ExpectBucketCount(
      "Sync.ProblematicServerSideBookmarks",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kInvalidSpecifics,
      /*expected_count=*/1);
}

TEST(BookmarkRemoteUpdatesHandlerReorderUpdatesTest,
     ShouldReorderParentsUpdateBeforeChildrenAndBothBeforeDeletions) {
  const base::Uuid kBookmarkBarGuid =
      base::Uuid::ParseLowercase(bookmarks::kBookmarkBarNodeUuid);

  // Prepare creation updates to build this structure:
  // bookmark_bar
  //  |- node0
  //    |- node1
  //      |- node2
  // and another sub hierarchy under node3 that won't receive any update.
  // node4
  //  |- node5
  // and a deletion for node6.

  // Constuct the updates list to have deletion first, and then all creations in
  // reverse shuffled order (from child to parent).

  std::vector<base::Uuid> guids;
  for (int i = 0; i < 7; i++) {
    // Use non-random GUIDs to produce a deterministic test outcome, since the
    // precise sync IDs can change the final order in ways that don't matter.
    guids.push_back(base::Uuid::ParseLowercase(
        base::StringPrintf("00000000-0000-4000-a000-00000000000%d", i)));
  }

  // Construct updates list
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateTombstoneResponseData(/*guid=*/guids[6],
                                                /*version=*/1));
  updates.push_back(CreateUpdateResponseData(/*guid=*/guids[5],
                                             /*parent_guid=*/guids[4]));
  updates.push_back(CreateUpdateResponseData(/*guid=*/guids[2],
                                             /*parent_guid=*/guids[1]));
  updates.push_back(CreateUpdateResponseData(/*guid=*/guids[1],
                                             /*parent_guid=*/guids[0]));
  updates.push_back(CreateUpdateResponseData(/*guid=*/guids[4],
                                             /*parent_guid=*/guids[3]));
  updates.push_back(CreateUpdateResponseData(/*guid=*/guids[0],
                                             /*parent_guid=*/kBookmarkBarGuid));

  std::vector<const syncer::UpdateResponseData*> ordered_updates =
      BookmarkRemoteUpdatesHandler::ReorderValidUpdatesForTest(&updates);

  std::vector<std::string> ordered_update_sync_ids;
  for (const syncer::UpdateResponseData* update : ordered_updates) {
    ordered_update_sync_ids.push_back(update->entity.id);
  }

  // Updates should be ordered such that parent node update comes first, and
  // deletions come last. The ordering requirements are within substrees only,
  // since it doesn't matter whether node1 comes before or after node4, so there
  // are two acceptable outcomes:
  // A) node0 --> node1 --> node2 --> node4 --> node5 --> node6
  // B) node4 --> node5 --> node0 --> node1 --> node2 --> node6
  EXPECT_THAT(ordered_update_sync_ids,
              AnyOf(ElementsAre(GetFakeServerIdFromGUID(guids[0]),
                                GetFakeServerIdFromGUID(guids[1]),
                                GetFakeServerIdFromGUID(guids[2]),
                                GetFakeServerIdFromGUID(guids[4]),
                                GetFakeServerIdFromGUID(guids[5]),
                                GetFakeServerIdFromGUID(guids[6])),
                    ElementsAre(GetFakeServerIdFromGUID(guids[4]),
                                GetFakeServerIdFromGUID(guids[5]),
                                GetFakeServerIdFromGUID(guids[0]),
                                GetFakeServerIdFromGUID(guids[1]),
                                GetFakeServerIdFromGUID(guids[2]),
                                GetFakeServerIdFromGUID(guids[6]))));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldProcessRandomlyOrderedCreations) {
  // Prepare creation updates to construct this structure:
  // bookmark_bar
  //  |- node0
  //    |- node1
  //      |- node2

  const base::Uuid kGuid0 = base::Uuid::GenerateRandomV4();
  const base::Uuid kGuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kGuid2 = base::Uuid::GenerateRandomV4();

  // Constuct the updates list to have creations randomly ordered.
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(/*guid=*/kGuid2,
                                             /*parent_guid=*/kGuid1));
  updates.push_back(CreateUpdateResponseData(/*guid=*/kGuid0,
                                             /*parent_guid=*/kBookmarkBarGuid));
  updates.push_back(CreateUpdateResponseData(/*guid=*/kGuid1,
                                             /*parent_guid=*/kGuid0));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // All nodes should be tracked including the "bookmark bar", "other
  // bookmarks" node and "mobile bookmarks".
  EXPECT_THAT(tracker()->TrackedEntitiesCountForTest(), Eq(6U));

  // All nodes should have been added to the model.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  EXPECT_THAT(bookmark_bar_node->children().front()->uuid(), Eq(kGuid0));
  ASSERT_THAT(bookmark_bar_node->children().front()->children().size(), Eq(1u));
  const bookmarks::BookmarkNode* grandchild =
      bookmark_bar_node->children().front()->children().front().get();
  EXPECT_THAT(grandchild->uuid(), Eq(kGuid1));
  ASSERT_THAT(grandchild->children().size(), Eq(1u));
  EXPECT_THAT(grandchild->children().front()->uuid(), Eq(kGuid2));
  EXPECT_THAT(grandchild->children().front()->children().size(), Eq(0u));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldLogFreshnessToUma) {
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(/*guid=*/kGuid,
                                             /*parent_guid=*/kBookmarkBarGuid));

  base::HistogramTester histogram_tester;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  histogram_tester.ExpectTotalCount(
      "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.BOOKMARK", 1);

  // Process the same update again, which should be ignored because the version
  // hasn't increased.
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  histogram_tester.ExpectTotalCount(
      "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.BOOKMARK", 1);

  // Increase version and process again; should log freshness.
  ++updates[0].response_version;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  histogram_tester.ExpectTotalCount(
      "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.BOOKMARK", 2);

  // Process remote deletion; should log freshness.
  updates[0] =
      CreateTombstoneResponseData(/*guid=*/kGuid,
                                  /*version=*/updates[0].response_version + 1);
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  histogram_tester.ExpectTotalCount(
      "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.BOOKMARK", 3);

  // Process another (redundant) deletion for the same entity; should not log.
  ++updates[0].response_version;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  histogram_tester.ExpectTotalCount(
      "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.BOOKMARK", 3);
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldProcessRandomlyOrderedDeletions) {
  // Prepare creation updates to construct this structure:
  // bookmark_bar
  //  |- node0
  //    |- node1
  //      |- node2

  const base::Uuid kGuid0 = base::Uuid::GenerateRandomV4();
  const base::Uuid kGuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kGuid2 = base::Uuid::GenerateRandomV4();

  // Construct the updates list to create that structure
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(/*guid=*/kGuid0,
                                             /*parent_guid=*/kBookmarkBarGuid));
  updates.push_back(CreateUpdateResponseData(/*guid=*/kGuid1,
                                             /*parent_guid=*/kGuid0));
  updates.push_back(CreateUpdateResponseData(/*guid=*/kGuid2,
                                             /*parent_guid=*/kGuid1));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // All nodes should be tracked including the "bookmark bar", "other
  // bookmarks" node and "mobile bookmarks".
  ASSERT_THAT(tracker()->TrackedEntitiesCountForTest(), Eq(6U));

  // Construct the updates list to have random deletions order.
  updates.clear();
  updates.push_back(CreateTombstoneResponseData(/*guid=*/kGuid1,
                                                /*version=*/1));
  updates.push_back(CreateTombstoneResponseData(/*guid=*/kGuid0,
                                                /*version=*/1));
  updates.push_back(CreateTombstoneResponseData(/*guid=*/kGuid2,
                                                /*version=*/1));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // |tracker| should have only permanent nodes now.
  EXPECT_THAT(tracker()->TrackedEntitiesCountForTest(), Eq(3U));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldProcessDeletionWithServerIdOnly) {
  const base::Uuid kGuid0 = base::Uuid::GenerateRandomV4();

  // Construct the updates list to create that structure
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(/*guid=*/kGuid0,
                                             /*parent_guid=*/kBookmarkBarGuid));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // The node should be tracked including the "bookmark bar", "other
  // bookmarks" node and "mobile bookmarks".
  ASSERT_THAT(tracker()->TrackedEntitiesCountForTest(), Eq(4U));

  // Construct the updates list with one minimalistic deletion (server ID only).
  updates.clear();
  updates.emplace_back();
  updates.back().entity.id = GetFakeServerIdFromGUID(kGuid0);
  updates.back().response_version = 1;
  ASSERT_TRUE(updates.back().entity.is_deleted());

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // |tracker| should have only permanent nodes now.
  EXPECT_THAT(tracker()->TrackedEntitiesCountForTest(), Eq(3U));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldIgnoreRemoteCreationWithInvalidGuidInSpecifics) {
  const std::string kTitle = "title";
  const syncer::UniquePosition kPosition = RandomUniquePosition();

  syncer::UpdateResponseDataList updates;

  // Create update with an invalid GUID.
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/base::Uuid(),
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/0,
      /*unique_position=*/kPosition));

  base::HistogramTester histogram_tester;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  EXPECT_THAT(tracker()->GetEntityForSyncId(updates[0].entity.id), IsNull());

  histogram_tester.ExpectBucketCount(
      "Sync.ProblematicServerSideBookmarks",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kInvalidSpecifics,
      /*expected_count=*/1);
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldIgnoreRemoteCreationWithUnexpectedGuidInSpecifics) {
  const base::Uuid kOriginalGuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kGuidInSpecifics = base::Uuid::GenerateRandomV4();
  const std::string kTitle = "title";
  const syncer::UniquePosition kPosition = RandomUniquePosition();

  syncer::UpdateResponseDataList updates;

  // Create update with empty GUID.
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kOriginalGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/0,
      /*unique_position=*/kPosition));

  // Override the GUID in specifics to mimic a mismatch with respecto to the
  // client tag hash.
  updates.back().entity.specifics.mutable_bookmark()->set_guid(
      kGuidInSpecifics.AsLowercaseString());

  base::HistogramTester histogram_tester;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  EXPECT_THAT(tracker()->GetEntityForUuid(kOriginalGuid), IsNull());
  EXPECT_THAT(tracker()->GetEntityForUuid(kGuidInSpecifics), IsNull());

  histogram_tester.ExpectBucketCount(
      "Sync.ProblematicServerSideBookmarks",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kUnexpectedGuid,
      /*expected_count=*/1);
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldIgnoreMisbehavingServerWithRemoteGuidUpdate) {
  const std::string kTitle = "title";
  const base::Uuid kOldGuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kNewGuid = base::Uuid::GenerateRandomV4();
  const syncer::UniquePosition kPosition = RandomUniquePosition();

  syncer::UpdateResponseDataList updates;

  // Create update with GUID.
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kOldGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/0,
      /*unique_position=*/kPosition));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  ASSERT_THAT(tracker()->GetEntityForUuid(kOldGuid), NotNull());
  ASSERT_THAT(tracker()->GetEntityForUuid(kNewGuid), IsNull());

  // Push an update for the same entity with a new GUID. Note that this is a
  // protocol violation, because |originator_client_item_id| cannot have changed
  // for a given server ID.
  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kNewGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/1,
      /*unique_position=*/RandomUniquePosition()));
  updates[0].entity.id = GetFakeServerIdFromGUID(kOldGuid);

  base::HistogramTester histogram_tester;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  EXPECT_THAT(tracker()->GetEntityForUuid(kOldGuid), NotNull());
  EXPECT_THAT(tracker()->GetEntityForUuid(kNewGuid), IsNull());

  // The GUID should not have been updated.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  EXPECT_THAT(bookmark_bar_node->children().front()->uuid(), Eq(kOldGuid));

  histogram_tester.ExpectBucketCount(
      "Sync.ProblematicServerSideBookmarks",
      /*sample=*/
      ExpectedRemoteBookmarkUpdateError::kGuidChangedForTrackedServerId,
      /*expected_count=*/1);
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldIgnoreMisbehavingServerWithPermanentNodeUpdateWithoutServerTag) {
  ASSERT_THAT(tracker()->GetEntityForSyncId(kBookmarkBarId), NotNull());

  // Push an update for a permanent entity, but without a unique server tag.
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/bookmark_model()->bookmark_bar_node()->uuid(),
      /*parent_guid=*/base::Uuid::GenerateRandomV4(),
      /*title=*/"title",
      /*version=*/1,
      /*unique_position=*/RandomUniquePosition()));

  base::HistogramTester histogram_tester;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  histogram_tester.ExpectBucketCount(
      "Sync.ProblematicServerSideBookmarks",
      /*sample=*/
      ExpectedRemoteBookmarkUpdateError::
          kTrackedServerIdWithoutServerTagMatchesPermanentNode,
      /*expected_count=*/1);
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldPositionRemoteCreations) {
  // Prepare creation updates to construct this structure:
  // bookmark_bar
  //  |- node0
  //  |- node1
  //  |- node2

  const base::Uuid kGuid0 = base::Uuid::GenerateRandomV4();
  const base::Uuid kGuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kGuid2 = base::Uuid::GenerateRandomV4();

  const std::string kTitle0 = "title 0";
  const std::string kTitle1 = "title 1";
  const std::string kTitle2 = "title 2";

  syncer::UniquePosition pos0 = syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());
  syncer::UniquePosition pos1 = syncer::UniquePosition::After(
      pos0, syncer::UniquePosition::RandomSuffix());
  syncer::UniquePosition pos2 = syncer::UniquePosition::After(
      pos1, syncer::UniquePosition::RandomSuffix());

  // Constuct the updates list to have creations randomly ordered.
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(/*guid=*/kGuid2,
                                             /*parent_guid=*/kBookmarkBarGuid,
                                             /*title=*/kTitle2,
                                             /*version=*/0,
                                             /*unique_position=*/pos2));
  updates.push_back(CreateUpdateResponseData(/*guid=*/kGuid0,
                                             /*parent_guid=*/kBookmarkBarGuid,
                                             /*title=*/kTitle0,
                                             /*version=*/0,
                                             /*unique_position=*/pos0));
  updates.push_back(CreateUpdateResponseData(/*guid=*/kGuid1,
                                             /*parent_guid=*/kBookmarkBarGuid,
                                             /*title=*/kTitle1,
                                             /*version=*/0,
                                             /*unique_position=*/pos1));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // All nodes should have been added to the model in the correct order.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(3u));
  EXPECT_THAT(bookmark_bar_node->children()[0]->uuid(), Eq(kGuid0));
  EXPECT_THAT(bookmark_bar_node->children()[1]->uuid(), Eq(kGuid1));
  EXPECT_THAT(bookmark_bar_node->children()[2]->uuid(), Eq(kGuid2));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldPositionRemoteMovesToTheLeft) {
  // Start with structure:
  // bookmark_bar
  //  |- node0
  //  |- node1
  //  |- node2
  //  |- node3
  //  |- node4

  std::vector<std::string> ids;
  std::vector<base::Uuid> guids;
  std::vector<syncer::UniquePosition> positions;

  syncer::UniquePosition position = syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());
  syncer::UpdateResponseDataList updates;
  for (int i = 0; i < 5; i++) {
    ids.push_back("node" + base::NumberToString(i));
    guids.push_back(base::Uuid::GenerateRandomV4());
    position = syncer::UniquePosition::After(
        position, syncer::UniquePosition::RandomSuffix());
    positions.push_back(position);
    updates.push_back(
        CreateUpdateResponseData(/*guid=*/guids[i],
                                 /*parent_guid=*/kBookmarkBarGuid,
                                 /*title=*/"Title",
                                 /*version=*/0,
                                 /*unique_position=*/positions[i]));
  }

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(5u));

  // Change it to this structure by moving node3 after node1.
  // bookmark_bar
  //  |- node0
  //  |- node1
  //  |- node3
  //  |- node2
  //  |- node4

  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/guids[3],
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/"Title",
      /*version=*/1,
      /*unique_position=*/
      syncer::UniquePosition::Between(positions[1], positions[2],
                                      syncer::UniquePosition::RandomSuffix())));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // Model should have been updated.
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(5u));
  EXPECT_THAT(bookmark_bar_node->children()[2]->uuid(), Eq(guids[3]));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldPositionRemoteMovesToTheRight) {
  // Start with structure:
  // bookmark_bar
  //  |- node0
  //  |- node1
  //  |- node2
  //  |- node3
  //  |- node4

  std::vector<std::string> ids;
  std::vector<base::Uuid> guids;
  std::vector<syncer::UniquePosition> positions;

  syncer::UniquePosition position = syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());
  syncer::UpdateResponseDataList updates;
  for (int i = 0; i < 5; i++) {
    ids.push_back("node" + base::NumberToString(i));
    guids.push_back(base::Uuid::GenerateRandomV4());
    position = syncer::UniquePosition::After(
        position, syncer::UniquePosition::RandomSuffix());
    positions.push_back(position);
    updates.push_back(
        CreateUpdateResponseData(/*guid=*/guids[i],
                                 /*parent_guid=*/kBookmarkBarGuid,
                                 /*title=*/ids[i],
                                 /*version=*/0,
                                 /*unique_position=*/positions[i]));
  }

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(5u));

  // Change it to this structure by moving node1 after node3.
  // bookmark_bar
  //  |- node0
  //  |- node2
  //  |- node3
  //  |- node1
  //  |- node4

  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/guids[1],
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/ids[1],
      /*version=*/1,
      /*unique_position=*/
      syncer::UniquePosition::Between(positions[3], positions[4],
                                      syncer::UniquePosition::RandomSuffix())));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // Model should have been updated.
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(5u));
  EXPECT_THAT(bookmark_bar_node->children()[3]->uuid(), Eq(guids[1]));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldPositionRemoteReparenting) {
  // Start with structure:
  // bookmark_bar
  //  |- node0
  //  |- node1
  //  |- node2
  //  |- node3
  //  |- node4

  std::vector<std::string> ids;
  std::vector<base::Uuid> guids;
  std::vector<syncer::UniquePosition> positions;

  syncer::UniquePosition position = syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());
  syncer::UpdateResponseDataList updates;
  for (int i = 0; i < 5; i++) {
    ids.push_back("node" + base::NumberToString(i));
    guids.push_back(base::Uuid::GenerateRandomV4());
    position = syncer::UniquePosition::After(
        position, syncer::UniquePosition::RandomSuffix());
    positions.push_back(position);
    updates.push_back(
        CreateUpdateResponseData(/*guid=*/guids[i],
                                 /*parent_guid=*/kBookmarkBarGuid,
                                 /*title=*/"Title",
                                 /*version=*/0,
                                 /*unique_position=*/positions[i]));
  }

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(5u));

  // Change it to this structure by moving node4 under node1.
  // bookmark_bar
  //  |- node0
  //  |- node1
  //    |- node4
  //  |- node2
  //  |- node3

  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/guids[4],
      /*parent_guid=*/guids[1],
      /*title=*/"Title",
      /*version=*/1,
      /*unique_position=*/RandomUniquePosition()));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // Model should have been updated.
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(4u));
  ASSERT_THAT(bookmark_bar_node->children()[1]->children().size(), Eq(1u));
  EXPECT_THAT(bookmark_bar_node->children()[1]->children()[0]->uuid(),
              Eq(guids[4]));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldIgnoreNodeIfMissingParentNode) {
  // Prepare creation updates to construct this structure:
  // bookmark_bar

  const base::Uuid kMissingParentGuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kChildGuid = base::Uuid::GenerateRandomV4();
  const std::string kChildId = "child_id";
  const std::string kTitle = "Title";
  const GURL kUrl("http://www.url.com");

  syncer::UpdateResponseDataList updates;
  updates.push_back(
      CreateUpdateResponseData(/*guid=*/kChildGuid,
                               /*parent_guid=*/kMissingParentGuid));

  base::HistogramTester histogram_tester;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  EXPECT_EQ(bookmark_bar_node->children().size(), 0U);
  histogram_tester.ExpectBucketCount(
      "Sync.ProblematicServerSideBookmarks",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kMissingParentNode,
      /*expected_count=*/1);

  EXPECT_THAT(tracker()->GetNumIgnoredUpdatesDueToMissingParentForTest(),
              Eq(1));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldIgnoreNodeIfParentIsNotFolder) {
  // Prepare creation updates to construct this structure:
  // bookmark_bar
  //  |- node0 (is_folder=false)
  //    |- node1

  const base::Uuid kParentGuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kChildGuid = base::Uuid::GenerateRandomV4();
  const std::string kTitle = "Title";
  const GURL kUrl("http://www.url.com");

  syncer::UpdateResponseDataList updates;
  syncer::EntityData data;
  data.id = GetFakeServerIdFromGUID(kParentGuid);
  sync_pb::BookmarkSpecifics* bookmark_specifics =
      data.specifics.mutable_bookmark();
  bookmark_specifics->set_guid(kParentGuid.AsLowercaseString());
  bookmark_specifics->set_parent_guid(bookmarks::kBookmarkBarNodeUuid);
  bookmark_specifics->set_legacy_canonicalized_title(kTitle);
  bookmark_specifics->set_url(kUrl.spec());
  bookmark_specifics->set_type(sync_pb::BookmarkSpecifics::URL);
  *bookmark_specifics->mutable_unique_position() =
      RandomUniquePosition().ToProto();
  data.originator_client_item_id = bookmark_specifics->guid();

  ASSERT_TRUE(IsValidBookmarkSpecifics(*bookmark_specifics));

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;

  updates.push_back(std::move(response_data));

  updates.push_back(CreateUpdateResponseData(/*guid=*/kChildGuid,
                                             /*parent_guid=*/kParentGuid));

  base::HistogramTester histogram_tester;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_EQ(bookmark_bar_node->children().size(), 1U);
  EXPECT_TRUE(bookmark_bar_node->children()[0]->children().empty());
  histogram_tester.ExpectBucketCount(
      "Sync.ProblematicServerSideBookmarks",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kParentNotFolder,
      /*expected_count=*/1);
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldMergeFaviconUponRemoteCreationsWithFavicon) {
  // Prepare creation updates to construct this structure:
  // bookmark_bar
  //  |- node0

  const std::string kTitle = "Title";
  const GURL kUrl("http://www.url.com");
  const GURL kIconUrl("http://www.icon-url.com");

  syncer::UpdateResponseDataList updates;
  syncer::EntityData data;
  data.id = "server_id";
  sync_pb::BookmarkSpecifics* bookmark_specifics =
      data.specifics.mutable_bookmark();
  bookmark_specifics->set_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  bookmark_specifics->set_parent_guid(bookmarks::kBookmarkBarNodeUuid);
  // Use the server id as the title for simplicity.
  bookmark_specifics->set_legacy_canonicalized_title(kTitle);
  bookmark_specifics->set_url(kUrl.spec());
  bookmark_specifics->set_icon_url(kIconUrl.spec());
  bookmark_specifics->set_favicon("PNG");
  bookmark_specifics->set_type(sync_pb::BookmarkSpecifics::URL);
  *bookmark_specifics->mutable_unique_position() =
      RandomUniquePosition().ToProto();
  data.originator_client_item_id = bookmark_specifics->guid();

  ASSERT_TRUE(IsValidBookmarkSpecifics(*bookmark_specifics));

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;

  updates.push_back(std::move(response_data));

  EXPECT_CALL(*favicon_service(),
              AddPageNoVisitForBookmark(kUrl, base::UTF8ToUTF16(kTitle)));
  EXPECT_CALL(*favicon_service(), MergeFavicon(kUrl, kIconUrl, _, _, _));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldDeleteFaviconUponRemoteCreationsWithoutFavicon) {
  // Prepare creation updates to construct this structure:
  // bookmark_bar
  //  |- node0

  const std::string kTitle = "Title";
  const GURL kUrl("http://www.url.com");

  syncer::UpdateResponseDataList updates;
  syncer::EntityData data;
  data.id = "server_id";
  sync_pb::BookmarkSpecifics* bookmark_specifics =
      data.specifics.mutable_bookmark();
  bookmark_specifics->set_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  bookmark_specifics->set_parent_guid(bookmarks::kBookmarkBarNodeUuid);
  // Use the server id as the title for simplicity.
  bookmark_specifics->set_legacy_canonicalized_title(kTitle);
  bookmark_specifics->set_url(kUrl.spec());
  bookmark_specifics->set_type(sync_pb::BookmarkSpecifics::URL);
  *bookmark_specifics->mutable_unique_position() =
      RandomUniquePosition().ToProto();
  data.originator_client_item_id = bookmark_specifics->guid();

  ASSERT_TRUE(IsValidBookmarkSpecifics(*bookmark_specifics));

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;

  updates.push_back(std::move(response_data));

  EXPECT_CALL(*favicon_service(),
              DeleteFaviconMappings(ElementsAre(kUrl),
                                    favicon_base::IconType::kFavicon));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
}

// This tests the case when a local creation is successfully committed to the
// server but the commit respone isn't received for some reason. Further updates
// to that entity should update the sync id in the tracker.
TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldUpdateSyncIdWhenRecevingUpdateForNewlyCreatedLocalNode) {
  const std::string kCacheGuid = "generated_id";
  const base::Uuid kBookmarkGuid = base::Uuid::GenerateRandomV4();
  const std::string kOriginatorClientItemId = kBookmarkGuid.AsLowercaseString();
  const std::string kSyncId = "server_id";
  const int64_t kServerVersion = 1000;
  const base::Time kModificationTime(base::Time::Now() - base::Seconds(1));

  sync_pb::DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bookmark_specifics = specifics.mutable_bookmark();
  bookmark_specifics->set_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  bookmark_specifics->set_parent_guid(bookmarks::kBookmarkBarNodeUuid);
  bookmark_specifics->set_legacy_canonicalized_title("Title");
  bookmark_specifics->set_type(sync_pb::BookmarkSpecifics::FOLDER);
  *bookmark_specifics->mutable_unique_position() =
      RandomUniquePosition().ToProto();

  ASSERT_TRUE(IsValidBookmarkSpecifics(*bookmark_specifics));

  const bookmarks::BookmarkNode* node = bookmark_model()->AddFolder(
      /*parent=*/bookmark_model()->bookmark_bar_node(),
      /*index=*/0, u"title", /*meta_info=*/nullptr,
      /*creation_time=*/std::nullopt, kBookmarkGuid);

  // Track a sync entity (similar to what happens after a local creation). The
  // |originator_client_item_id| is used a temp sync id and mark the entity that
  // it needs to be committed..
  const SyncedBookmarkTrackerEntity* entity =
      tracker()->Add(node, /*sync_id=*/kOriginatorClientItemId,
                     /*server_version=*/0, kModificationTime, specifics);
  tracker()->IncrementSequenceNumber(entity);

  ASSERT_THAT(tracker()->GetEntityForSyncId(kOriginatorClientItemId),
              Eq(entity));

  // Now receive an update with the actual server id.
  syncer::UpdateResponseDataList updates;
  syncer::EntityData data;
  data.id = kSyncId;
  data.originator_client_item_id = kOriginatorClientItemId;
  // Set the other required fields.
  data.specifics = specifics;
  data.specifics.mutable_bookmark()->set_guid(kOriginatorClientItemId);
  bookmark_specifics->set_type(sync_pb::BookmarkSpecifics::FOLDER);
  *bookmark_specifics->mutable_unique_position() =
      RandomUniquePosition().ToProto();

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = kServerVersion;
  updates.push_back(std::move(response_data));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // The sync id in the tracker should have been updated.
  EXPECT_THAT(tracker()->GetEntityForSyncId(kOriginatorClientItemId), IsNull());
  EXPECT_THAT(tracker()->GetEntityForSyncId(kSyncId), Eq(entity));
  EXPECT_THAT(entity->metadata().server_id(), Eq(kSyncId));
  EXPECT_THAT(entity->bookmark_node(), Eq(node));
}

// Same as above for bookmarks created with client tags.
TEST_F(
    BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
    ShouldUpdateSyncIdWhenRecevingUpdateForNewlyCreatedLocalNodeWithClientTag) {
  const base::Uuid kBookmarkGuid = base::Uuid::GenerateRandomV4();
  const std::string kSyncId = "server_id";
  const int64_t kServerVersion = 1000;
  const base::Time kModificationTime(base::Time::Now() - base::Seconds(1));

  sync_pb::DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bookmark_specifics = specifics.mutable_bookmark();
  bookmark_specifics->set_guid(kBookmarkGuid.AsLowercaseString());
  bookmark_specifics->set_parent_guid(
      bookmark_model()->bookmark_bar_node()->uuid().AsLowercaseString());
  bookmark_specifics->set_legacy_canonicalized_title("Title");
  bookmark_specifics->set_type(sync_pb::BookmarkSpecifics::FOLDER);
  *bookmark_specifics->mutable_unique_position() =
      RandomUniquePosition().ToProto();

  ASSERT_TRUE(IsValidBookmarkSpecifics(*bookmark_specifics));

  const bookmarks::BookmarkNode* node = bookmark_model()->AddFolder(
      /*parent=*/bookmark_model()->bookmark_bar_node(),
      /*index=*/0, u"title", /*meta_info=*/nullptr,
      /*creation_time=*/std::nullopt, kBookmarkGuid);
  // Track a sync entity (similar to what happens after a local creation).
  const SyncedBookmarkTrackerEntity* entity =
      tracker()->Add(node, /*sync_id=*/kSyncId, /*server_version=*/0,
                     kModificationTime, specifics);
  tracker()->IncrementSequenceNumber(entity);

  ASSERT_THAT(tracker()->GetEntityForSyncId(kSyncId), Eq(entity));

  // Now receive an update with the actual server id.
  syncer::UpdateResponseDataList updates;
  syncer::EntityData data;
  data.id = kSyncId;
  data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
      syncer::BOOKMARKS, kBookmarkGuid.AsLowercaseString());
  // Set the other required fields.
  data.specifics = specifics;
  data.specifics.mutable_bookmark()->set_guid(
      kBookmarkGuid.AsLowercaseString());
  bookmark_specifics->set_type(sync_pb::BookmarkSpecifics::FOLDER);
  *bookmark_specifics->mutable_unique_position() =
      RandomUniquePosition().ToProto();
  ASSERT_TRUE(IsValidBookmarkSpecifics(data.specifics.bookmark()));

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = kServerVersion;
  updates.push_back(std::move(response_data));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // The sync id in the tracker should have been updated.
  EXPECT_THAT(tracker()->GetEntityForSyncId(kBookmarkGuid.AsLowercaseString()),
              IsNull());
  EXPECT_THAT(tracker()->GetEntityForSyncId(kSyncId), Eq(entity));
  EXPECT_THAT(entity->metadata().server_id(), Eq(kSyncId));
  EXPECT_THAT(entity->bookmark_node(), Eq(node));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldRecommitWhenEncryptionIsOutOfDate) {
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();

  sync_pb::DataTypeState data_type_state;
  data_type_state.set_encryption_key_name("encryption_key_name");
  tracker()->set_data_type_state(data_type_state);

  syncer::UpdateResponseDataList updates;
  syncer::UpdateResponseData response_data =
      CreateUpdateResponseData(/*guid=*/kGuid,
                               /*parent_guid=*/kBookmarkBarGuid);
  response_data.encryption_key_name = "out_of_date_encryption_key_name";
  updates.push_back(std::move(response_data));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  ASSERT_THAT(tracker()->GetEntityForUuid(kGuid), NotNull());
  EXPECT_THAT(tracker()->GetEntityForUuid(kGuid)->IsUnsynced(), Eq(true));
}

// Tests that recommit will be initiated in case when there is a local tombstone
// and server's update has out of date encryption.
TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldRecommitWhenEncryptionIsOutOfDateOnConflict) {
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();

  sync_pb::DataTypeState data_type_state;
  data_type_state.set_encryption_key_name("encryption_key_name");
  tracker()->set_data_type_state(data_type_state);

  // Create a new node and remove it locally.
  syncer::UpdateResponseDataList updates;
  syncer::UpdateResponseData response_data =
      CreateUpdateResponseData(/*guid=*/kGuid,
                               /*parent_guid=*/kBookmarkBarGuid,
                               /*title=*/"title",
                               /*version=*/0,
                               /*unique_position=*/RandomUniquePosition());
  response_data.encryption_key_name = "encryption_key_name";
  updates.push_back(std::move(response_data));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  const SyncedBookmarkTrackerEntity* entity =
      tracker()->GetEntityForUuid(kGuid);
  ASSERT_THAT(entity, NotNull());
  ASSERT_THAT(entity->bookmark_node(), NotNull());
  ASSERT_THAT(entity->bookmark_node()->uuid(), Eq(kGuid));

  auto* node = entity->bookmark_node();
  tracker()->MarkDeleted(entity, FROM_HERE);
  tracker()->IncrementSequenceNumber(entity);
  bookmark_model()->Remove(node, FROM_HERE);

  // Process an update with outdated encryption. This should cause a conflict
  // and the remote version must be applied. Local tombstone entity will be
  // removed during processing conflict.
  updates.clear();
  response_data =
      CreateUpdateResponseData(/*guid=*/kGuid,
                               /*parent_guid=*/kBookmarkBarGuid,
                               /*title=*/"title",
                               /*version=*/1,
                               /*unique_position=*/RandomUniquePosition());
  response_data.encryption_key_name = "out_of_date_encryption_key_name";
  updates.push_back(std::move(response_data));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  // |entity| may be deleted here while processing update during conflict
  // resolution.
  entity = tracker()->GetEntityForUuid(kGuid);
  ASSERT_THAT(entity, NotNull());
  EXPECT_THAT(entity->IsUnsynced(), Eq(true));
  EXPECT_THAT(entity->bookmark_node(), NotNull());
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldRecommitWhenGotNewEncryptionRequirements) {
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(/*guid=*/kGuid,
                                             /*parent_guid=*/kBookmarkBarGuid));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  ASSERT_THAT(tracker()->GetEntityForUuid(kGuid), NotNull());
  EXPECT_THAT(tracker()->GetEntityForUuid(kGuid)->IsUnsynced(), Eq(false));

  updates_handler()->Process(syncer::UpdateResponseDataList(),
                             /*got_new_encryption_requirements=*/true);
  EXPECT_THAT(tracker()->GetEntityForUuid(kGuid)->IsUnsynced(), Eq(true));
  // Permanent nodes shouldn't be committed. They are only created on the server
  // and synced down.
  EXPECT_THAT(tracker()->GetEntityForSyncId(kBookmarkBarId)->IsUnsynced(),
              Eq(false));
}

TEST_F(
    BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
    ShouldNotRecommitWhenEncryptionKeyNameMistmatchWithConflictWithDeletions) {
  sync_pb::DataTypeState data_type_state;
  data_type_state.set_encryption_key_name("encryption_key_name");
  tracker()->set_data_type_state(data_type_state);

  // Create the bookmark with same encryption key name.
  const std::string kTitle = "title";
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();

  syncer::UpdateResponseDataList updates;
  syncer::UpdateResponseData response_data =
      CreateUpdateResponseData(/*guid=*/kGuid,
                               /*parent_guid=*/kBookmarkBarGuid);
  response_data.encryption_key_name = "encryption_key_name";
  updates.push_back(std::move(response_data));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  // The bookmark has been added and tracked.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  const SyncedBookmarkTrackerEntity* entity =
      tracker()->GetEntityForUuid(kGuid);
  ASSERT_THAT(entity, NotNull());

  // Mark the entity as deleted locally.
  tracker()->MarkDeleted(entity, FROM_HERE);
  tracker()->IncrementSequenceNumber(entity);
  ASSERT_THAT(tracker()->GetEntityForUuid(kGuid)->IsUnsynced(), Eq(true));

  // Remove the bookmark from the local bookmark model.
  bookmark_model()->Remove(bookmark_bar_node->children().front().get(),
                           FROM_HERE);
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(0u));

  // Push a remote deletion for the same entity with an out of date encryption
  // key name.
  updates.clear();
  syncer::UpdateResponseData response_data2 =
      CreateTombstoneResponseData(/*guid=*/kGuid, /*version=*/1);
  response_data2.encryption_key_name = "out_of_date_encryption_key_name";
  // Increment the server version to make sure the update isn't discarded as
  // reflection.
  response_data2.response_version++;
  updates.push_back(std::move(response_data2));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // There should have been conflict, and it should have been resolved by
  // removing local entity since both changes are deletions.
  EXPECT_THAT(tracker()->GetEntityForUuid(kGuid), IsNull());
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldNotRecommitUptoDateEntitiesWhenGotNewEncryptionRequirements) {
  const base::Uuid kGuid0 = base::Uuid::GenerateRandomV4();

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(/*guid=*/kGuid0,
                                             /*parent_guid=*/kBookmarkBarGuid));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  ASSERT_THAT(tracker()->GetEntityForUuid(kGuid0), NotNull());
  EXPECT_THAT(tracker()->GetEntityForUuid(kGuid0)->IsUnsynced(), Eq(false));

  // Push another update to for the same entity.
  syncer::UpdateResponseData response_data =
      CreateUpdateResponseData(/*guid=*/kGuid0,
                               /*parent_guid=*/kBookmarkBarGuid);

  // Increment the server version to make sure the update isn't discarded as
  // reflection.
  response_data.response_version++;
  syncer::UpdateResponseDataList new_updates;
  new_updates.push_back(std::move(response_data));
  updates_handler()->Process(new_updates,
                             /*got_new_encryption_requirements=*/true);
  EXPECT_THAT(tracker()->GetEntityForUuid(kGuid0)->IsUnsynced(), Eq(false));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldResolveConflictBetweenLocalAndRemoteDeletionsByMatchingThem) {
  const std::string kTitle = "title";
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();

  syncer::UpdateResponseDataList updates;

  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/0,
      /*unique_position=*/RandomUniquePosition()));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  const SyncedBookmarkTrackerEntity* entity =
      tracker()->GetEntityForUuid(kGuid);
  ASSERT_THAT(entity, NotNull());
  ASSERT_THAT(entity->IsUnsynced(), Eq(false));
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));

  // Mark the entity as deleted locally.
  tracker()->MarkDeleted(entity, FROM_HERE);
  tracker()->IncrementSequenceNumber(entity);
  ASSERT_THAT(entity->IsUnsynced(), Eq(true));

  // Remove the bookmark from the local bookmark model.
  bookmark_model()->Remove(bookmark_bar_node->children().front().get(),
                           FROM_HERE);
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(0u));

  // Push a remote deletion for the same entity.
  updates.clear();
  updates.push_back(CreateTombstoneResponseData(
      /*guid=*/kGuid,
      /*version=*/1));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // There should have been conflict, and it should have been resolved by
  // removing local entity since both changes are deletions.
  EXPECT_THAT(tracker()->GetEntityForUuid(kGuid), IsNull());
  // Make sure the bookmark hasn't been resurrected.
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(0u));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldResolveConflictBetweenLocalUpdateAndRemoteDeletionWithLocal) {
  const std::string kTitle = "title";
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();

  syncer::UpdateResponseDataList updates;

  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/0,
      /*unique_position=*/RandomUniquePosition()));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  const SyncedBookmarkTrackerEntity* entity =
      tracker()->GetEntityForUuid(kGuid);
  ASSERT_THAT(entity, NotNull());
  ASSERT_THAT(entity->IsUnsynced(), Eq(false));

  // Mark the entity as modified locally.
  tracker()->IncrementSequenceNumber(entity);
  ASSERT_THAT(entity->IsUnsynced(), Eq(true));

  // Push a remote deletion for the same entity.
  updates.clear();
  updates.push_back(CreateTombstoneResponseData(
      /*guid=*/kGuid,
      /*version=*/1));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // There should have been conflict, and it should have been resolved with the
  // local version that will be committed later.
  ASSERT_THAT(tracker()->GetEntityForUuid(kGuid), Eq(entity));
  EXPECT_THAT(entity->IsUnsynced(), Eq(true));
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(1u));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldResolveConflictBetweenLocalDeletionAndRemoteUpdateByRemote) {
  const std::string kTitle = "title";
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();

  syncer::UpdateResponseDataList updates;

  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/0,
      /*unique_position=*/RandomUniquePosition()));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  const SyncedBookmarkTrackerEntity* entity =
      tracker()->GetEntityForUuid(kGuid);
  ASSERT_THAT(entity, NotNull());
  ASSERT_THAT(entity->bookmark_node(), NotNull());
  ASSERT_THAT(entity->bookmark_node()->uuid(), Eq(kGuid));
  ASSERT_THAT(entity->IsUnsynced(), Eq(false));

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));

  // Mark the entity as deleted locally.
  tracker()->MarkDeleted(entity, FROM_HERE);
  tracker()->IncrementSequenceNumber(entity);
  ASSERT_THAT(entity->IsUnsynced(), Eq(true));

  // Remove the bookmark from the local bookmark model.
  bookmark_model()->Remove(bookmark_bar_node->children().front().get(),
                           FROM_HERE);
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(0u));

  // Push an update for the same entity.
  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/1,
      /*unique_position=*/RandomUniquePosition()));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // There should have been conflict, and it should have been resolved with the
  // remote version. The implementation may or may not reuse |entity|, so let's
  // look it up again.
  entity = tracker()->GetEntityForUuid(kGuid);
  ASSERT_THAT(entity, NotNull());
  EXPECT_THAT(entity->IsUnsynced(), Eq(false));
  EXPECT_THAT(entity->metadata().is_deleted(), Eq(false));

  // The bookmark should have been resurrected.
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(1u));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldResolveConflictBetweenLocalAndRemoteUpdatesWithMatchingThem) {
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();
  const std::string kTitle = "title";

  syncer::UpdateResponseDataList updates;

  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/0,
      /*unique_position=*/RandomUniquePosition()));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  const SyncedBookmarkTrackerEntity* entity =
      tracker()->GetEntityForUuid(kGuid);
  ASSERT_THAT(entity, NotNull());
  ASSERT_THAT(entity->IsUnsynced(), Eq(false));

  // Mark the entity as modified locally.
  tracker()->IncrementSequenceNumber(entity);
  ASSERT_THAT(entity->IsUnsynced(), Eq(true));

  // Push an update for the same entity with the same information.
  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/1,
      /*unique_position=*/RandomUniquePosition()));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // There should have been conflict but both local and remote updates should
  // match. The conflict should have been resolved.
  ASSERT_THAT(tracker()->GetEntityForUuid(kGuid), Eq(entity));
  EXPECT_THAT(entity->IsUnsynced(), Eq(false));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldResolveConflictBetweenLocalAndRemoteUpdatesWithRemote) {
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();
  const std::string kTitle = "title";
  const std::string kNewRemoteTitle = "remote title";

  syncer::UpdateResponseDataList updates;

  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/0,
      /*unique_position=*/RandomUniquePosition()));

  BookmarkRemoteUpdatesHandler updates_handler(bookmark_model(),
                                               favicon_service(), tracker());
  updates_handler.Process(updates, /*got_new_encryption_requirements=*/false);

  const SyncedBookmarkTrackerEntity* entity =
      tracker()->GetEntityForUuid(kGuid);
  ASSERT_THAT(entity, NotNull());
  ASSERT_THAT(entity->IsUnsynced(), Eq(false));

  // Mark the entity as modified locally.
  tracker()->IncrementSequenceNumber(entity);
  ASSERT_THAT(entity->IsUnsynced(), Eq(true));

  // Push an update for the same entity with a new title.
  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kNewRemoteTitle,
      /*version=*/1,
      /*unique_position=*/RandomUniquePosition()));

  updates_handler.Process(updates, /*got_new_encryption_requirements=*/false);

  // There should have been conflict, and it should have been resolved with the
  // remote version.
  ASSERT_THAT(tracker()->GetEntityForUuid(kGuid), Eq(entity));
  EXPECT_THAT(entity->IsUnsynced(), Eq(false));
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  EXPECT_THAT(bookmark_bar_node->children().front()->GetTitle(),
              Eq(ASCIIToUTF16(kNewRemoteTitle)));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldReuploadOnEmptyUniquePositionOnUpdateWithSameSpecifics) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(switches::kSyncReuploadBookmarks);

  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();

  syncer::UpdateResponseDataList updates;
  updates.push_back(
      CreateUpdateResponseData(/*guid=*/kGuid,
                               /*parent_guid=*/kBookmarkBarGuid,
                               /*title=*/"Title",
                               /*version=*/0,
                               /*unique_position=*/RandomUniquePosition()));
  ASSERT_TRUE(updates.back().entity.specifics.bookmark().has_full_title());

  BookmarkRemoteUpdatesHandler(bookmark_model(), favicon_service(), tracker())
      .Process(updates,
               /*got_new_encryption_requirements=*/false);

  const SyncedBookmarkTrackerEntity* entity =
      tracker()->GetEntityForUuid(kGuid);
  ASSERT_THAT(entity, NotNull());
  ASSERT_FALSE(entity->IsUnsynced());

  // Mimic the case when there is another update but without |unique_position|
  // in the original specifics.
  updates.back().entity.is_bookmark_unique_position_in_specifics_preprocessed =
      true;
  updates.back().response_version++;
  BookmarkRemoteUpdatesHandler(bookmark_model(), favicon_service(), tracker())
      .Process(updates,
               /*got_new_encryption_requirements=*/false);

  EXPECT_TRUE(entity->IsUnsynced());
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldIncrementSequenceNumberOnConflict) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(switches::kSyncReuploadBookmarks);
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();
  const std::string kTitle = "title";
  const std::string kNewTitle = "New title";

  // Create a local bookmark with unsynced state (it should be unsynced due to
  // enabled reupload).
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/0,
      /*unique_position=*/RandomUniquePosition()));
  updates.back().entity.is_bookmark_unique_position_in_specifics_preprocessed =
      true;

  {
    BookmarkRemoteUpdatesHandler updates_handler(bookmark_model(),
                                                 favicon_service(), tracker());
    updates_handler.Process(updates, /*got_new_encryption_requirements=*/false);
  }

  const SyncedBookmarkTrackerEntity* entity =
      tracker()->GetEntityForUuid(kGuid);
  ASSERT_THAT(entity, NotNull());
  ASSERT_TRUE(entity->IsUnsynced());

  // Check that the |entity| is applied an incoming update (by verifying that
  // the node's title has been changed) and that it will be reuploaded after
  // conflict resolution.
  updates.back()
      .entity.specifics.mutable_bookmark()
      ->set_legacy_canonicalized_title(kNewTitle);
  updates.back().entity.specifics.mutable_bookmark()->set_full_title(kNewTitle);
  updates.back().response_version++;
  {
    BookmarkRemoteUpdatesHandler updates_handler(bookmark_model(),
                                                 favicon_service(), tracker());
    updates_handler.Process(updates, /*got_new_encryption_requirements=*/false);
  }
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  const bookmarks::BookmarkNode* node =
      bookmark_bar_node->children().front().get();
  EXPECT_EQ(base::UTF16ToUTF8(node->GetTitle()), kNewTitle);
  EXPECT_TRUE(entity->IsUnsynced());

  // Same as above but with the same title in specifics (the local entity
  // contains the same specifics as the incoming update).
  updates.back().response_version++;
  {
    BookmarkRemoteUpdatesHandler updates_handler(bookmark_model(),
                                                 favicon_service(), tracker());
    updates_handler.Process(updates, /*got_new_encryption_requirements=*/false);
  }
  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  EXPECT_EQ(bookmark_bar_node->children().front().get(), node);
  EXPECT_EQ(base::UTF16ToUTF8(node->GetTitle()), kNewTitle);
  EXPECT_TRUE(entity->IsUnsynced());
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldIncrementSequenceNumberOnUpdate) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(switches::kSyncReuploadBookmarks);
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();
  const std::string kTitle = "title";
  const std::string kRemoteTitle = "New title";

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/0,
      /*unique_position=*/RandomUniquePosition()));

  {
    BookmarkRemoteUpdatesHandler updates_handler(bookmark_model(),
                                                 favicon_service(), tracker());
    updates_handler.Process(updates, /*got_new_encryption_requirements=*/false);
  }

  const SyncedBookmarkTrackerEntity* entity =
      tracker()->GetEntityForUuid(kGuid);
  ASSERT_THAT(entity, NotNull());
  ASSERT_FALSE(entity->IsUnsynced());

  // Check reupload on update.
  updates.back()
      .entity.specifics.mutable_bookmark()
      ->set_legacy_canonicalized_title(kRemoteTitle);
  updates.back().entity.specifics.mutable_bookmark()->set_full_title(
      kRemoteTitle);
  updates.back().entity.is_bookmark_unique_position_in_specifics_preprocessed =
      true;
  updates.back().response_version++;
  {
    BookmarkRemoteUpdatesHandler updates_handler(bookmark_model(),
                                                 favicon_service(), tracker());
    updates_handler.Process(updates, /*got_new_encryption_requirements=*/false);
  }
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  const bookmarks::BookmarkNode* node =
      bookmark_bar_node->children().front().get();
  EXPECT_EQ(node->GetTitle(), base::UTF8ToUTF16(kRemoteTitle));
  EXPECT_TRUE(entity->IsUnsynced());
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldReuploadBookmarkOnEmptyUniquePosition) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(switches::kSyncReuploadBookmarks);
  const std::string kFolder1Title = "folder1";
  const std::string kFolder2Title = "folder2";

  const base::Uuid kFolder1Guid = base::Uuid::GenerateRandomV4();
  const base::Uuid kFolder2Guid = base::Uuid::GenerateRandomV4();

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kFolder1Guid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kFolder1Title,
      /*version=*/0,
      /*unique_position=*/RandomUniquePosition()));

  // Remove |unique_position| field for the first item only.
  updates.back().entity.is_bookmark_unique_position_in_specifics_preprocessed =
      true;

  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kFolder2Guid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kFolder2Title,
      /*version=*/0,
      /*unique_position=*/RandomUniquePosition()));
  ASSERT_FALSE(
      updates.back()
          .entity.is_bookmark_unique_position_in_specifics_preprocessed);

  updates_handler()->Process(std::move(updates),
                             /*got_new_encryption_requirements=*/false);

  const SyncedBookmarkTrackerEntity* entity1 =
      tracker()->GetEntityForUuid(kFolder1Guid);
  const SyncedBookmarkTrackerEntity* entity2 =
      tracker()->GetEntityForUuid(kFolder2Guid);
  ASSERT_THAT(entity1, NotNull());
  ASSERT_THAT(entity2, NotNull());

  EXPECT_TRUE(entity1->IsUnsynced());
  EXPECT_FALSE(entity2->IsUnsynced());
}

// Tests that the reflection which doesn't have |unique_position| in specifics
// will be reuploaded.
TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldReuploadBookmarkOnEmptyUniquePositionForReflection) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(switches::kSyncReuploadBookmarks);
  const std::string kFolderTitle = "folder";
  const base::Uuid kFolderGuid = base::Uuid::GenerateRandomV4();

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = bookmark_model()->AddFolder(
      bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kFolderTitle),
      /*meta_info=*/nullptr, /*creation_time=*/base::Time::Now(), kFolderGuid);
  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model());
  sync_pb::BookmarkMetadata* node_metadata =
      model_metadata.add_bookmarks_metadata();
  *node_metadata = CreateNodeMetadata(node, RandomUniquePosition());
  const int64_t server_version = node_metadata->metadata().server_version();

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
          bookmark_model(), std::move(model_metadata));
  ASSERT_THAT(tracker, NotNull());
  ASSERT_EQ(4u, tracker->GetAllEntities().size());

  const SyncedBookmarkTrackerEntity* entity =
      tracker->GetEntityForUuid(kFolderGuid);
  ASSERT_THAT(entity, NotNull());
  ASSERT_FALSE(entity->IsUnsynced());

  syncer::UpdateResponseDataList updates;
  // Create an update with the same server version as local entity has. This
  // will simulate processing of reflection.
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kFolderGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kFolderTitle,
      /*version=*/server_version,
      /*unique_position=*/RandomUniquePosition()));

  updates.back().entity.is_bookmark_unique_position_in_specifics_preprocessed =
      true;

  BookmarkRemoteUpdatesHandler updates_handler(
      bookmark_model(), favicon_service(), tracker.get());
  updates_handler.Process(std::move(updates),
                          /*got_new_encryption_requirements=*/false);
  ASSERT_EQ(entity, tracker->GetEntityForUuid(kFolderGuid));

  EXPECT_TRUE(entity->IsUnsynced());
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldProcessDifferentEntitiesWithSameGuid) {
  const std::string kServerId1 = "server_id_1";
  const std::string kServerId2 = "server_id_2";

  const std::string kTitle = "Title";
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();

  // Initialize the model with one node.
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/0,
      /*unique_position=*/RandomUniquePosition()));
  updates[0].entity.id = kServerId1;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  ASSERT_THAT(tracker()->TrackedEntitiesCountForTest(), Eq(4U));
  ASSERT_THAT(tracker()->GetEntityForSyncId(kServerId1), NotNull());
  updates.clear();

  // Create two updates having the same GUID, one is a tombstone for the old
  // |server_id|, another with a new one. The tombstone is processed after the
  // update and should be ignored due to its server version.
  updates.push_back(CreateUpdateResponseData(
      /*guid=*/kGuid,
      /*parent_guid=*/kBookmarkBarGuid,
      /*title=*/kTitle,
      /*version=*/2, RandomUniquePosition()));
  updates[0].entity.id = kServerId2;
  updates.push_back(CreateTombstoneResponseData(
      /*guid=*/kGuid,
      /*version=*/1));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  EXPECT_THAT(tracker()->TrackedEntitiesCountForTest(), Eq(4U));
  EXPECT_THAT(tracker()->GetEntityForSyncId(kServerId1), IsNull());
  const SyncedBookmarkTrackerEntity* entity =
      tracker()->GetEntityForSyncId(kServerId2);
  EXPECT_THAT(entity, NotNull());
  EXPECT_THAT(entity->bookmark_node(), NotNull());
  EXPECT_THAT(entity->bookmark_node()->uuid(), Eq(kGuid));
}

TEST(BookmarkRemoteUpdatesHandlerTest,
     ShouldComputeRightChildNodeIndexForEmptyParent) {
  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::RandomSuffix();
  const syncer::UniquePosition pos1 =
      syncer::UniquePosition::InitialPosition(suffix);

  TestBookmarkModelView bookmark_model;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
          &bookmark_model, CreateMetadataForPermanentNodes(&bookmark_model));

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();

  // Should always return 0 for any UniquePosition in the initial state.
  EXPECT_EQ(0u, BookmarkRemoteUpdatesHandler::ComputeChildNodeIndexForTest(
                    bookmark_bar_node, pos1.ToProto(), tracker.get()));
}

TEST(BookmarkRemoteUpdatesHandlerTest, ShouldComputeRightChildNodeIndex) {
  TestBookmarkModelView bookmark_model;

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::RandomSuffix();

  const syncer::UniquePosition pos1 =
      syncer::UniquePosition::InitialPosition(suffix);
  const syncer::UniquePosition pos2 =
      syncer::UniquePosition::After(pos1, suffix);
  const syncer::UniquePosition pos3 =
      syncer::UniquePosition::After(pos2, suffix);

  // Create 3 nodes using remote update.
  const bookmarks::BookmarkNode* node1 = bookmark_model.AddFolder(
      bookmark_bar_node, /*index=*/0, /*title=*/std::u16string());
  const bookmarks::BookmarkNode* node2 = bookmark_model.AddFolder(
      bookmark_bar_node, /*index=*/1, /*title=*/std::u16string());
  const bookmarks::BookmarkNode* node3 = bookmark_model.AddFolder(
      bookmark_bar_node, /*index=*/2, /*title=*/std::u16string());

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(&bookmark_model);
  *model_metadata.add_bookmarks_metadata() = CreateNodeMetadata(node1, pos1);
  *model_metadata.add_bookmarks_metadata() = CreateNodeMetadata(node2, pos2);
  *model_metadata.add_bookmarks_metadata() = CreateNodeMetadata(node3, pos3);

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
          &bookmark_model, std::move(model_metadata));

  // Check for the same position as existing bookmarks have. In practice this
  // shouldn't happen.
  EXPECT_EQ(1u, BookmarkRemoteUpdatesHandler::ComputeChildNodeIndexForTest(
                    bookmark_bar_node, pos1.ToProto(), tracker.get()));
  EXPECT_EQ(2u, BookmarkRemoteUpdatesHandler::ComputeChildNodeIndexForTest(
                    bookmark_bar_node, pos2.ToProto(), tracker.get()));
  EXPECT_EQ(3u, BookmarkRemoteUpdatesHandler::ComputeChildNodeIndexForTest(
                    bookmark_bar_node, pos3.ToProto(), tracker.get()));

  EXPECT_EQ(0u, BookmarkRemoteUpdatesHandler::ComputeChildNodeIndexForTest(
                    bookmark_bar_node,
                    syncer::UniquePosition::Before(pos1, suffix).ToProto(),
                    tracker.get()));
  EXPECT_EQ(1u, BookmarkRemoteUpdatesHandler::ComputeChildNodeIndexForTest(
                    bookmark_bar_node,
                    syncer::UniquePosition::Between(/*before=*/pos1,
                                                    /*after=*/pos2, suffix)
                        .ToProto(),
                    tracker.get()));
  EXPECT_EQ(2u, BookmarkRemoteUpdatesHandler::ComputeChildNodeIndexForTest(
                    bookmark_bar_node,
                    syncer::UniquePosition::Between(/*before=*/pos2,
                                                    /*after=*/pos3, suffix)
                        .ToProto(),
                    tracker.get()));
  EXPECT_EQ(3u, BookmarkRemoteUpdatesHandler::ComputeChildNodeIndexForTest(
                    bookmark_bar_node,
                    syncer::UniquePosition::After(pos3, suffix).ToProto(),
                    tracker.get()));
}

}  // namespace

}  // namespace sync_bookmarks
