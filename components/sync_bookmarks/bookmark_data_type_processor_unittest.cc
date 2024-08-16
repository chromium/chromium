// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_data_type_processor.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/features.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/protocol/bookmark_model_metadata.pb.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/test/mock_commit_queue.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/switches.h"
#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"
#include "components/sync_bookmarks/test_bookmark_model_view.h"
#include "components/undo/bookmark_undo_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/tree_node_iterator.h"

namespace sync_bookmarks {

namespace {

using base::ASCIIToUTF16;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::NiceMock;
using testing::NotNull;
using testing::Pointer;
using testing::SizeIs;
using testing::UnorderedElementsAre;

const char kBookmarkBarTag[] = "bookmark_bar";
const char kOtherBookmarksTag[] = "other_bookmarks";
const char kMobileBookmarksTag[] = "synced_bookmarks";
const char kBookmarkBarId[] = "bookmark_bar_id";
const char kOtherBookmarksId[] = "other_bookmarks_id";
const char kMobileBookmarksId[] = "mobile_bookmarks_id";
const char kBookmarksRootId[] = "root_id";
const char kCacheGuid[] = "generated_id";
const char kPersistentDataTypeConfigurationTimeMetricName[] =
    "Sync.DataTypeConfigurationTime.Persistent.BOOKMARK";

struct BookmarkInfo {
  std::string server_id;
  std::string title;
  std::string url;  // empty for folders.
  std::string parent_id;
  std::string server_tag;
};

MATCHER_P(CommitRequestDataMatchesGuid, uuid, "") {
  const syncer::CommitRequestData* data = arg.get();
  return data != nullptr && data->entity != nullptr &&
         data->entity->specifics.bookmark().guid() == uuid.AsLowercaseString();
}

MATCHER_P(TrackedEntityCorrespondsToBookmarkNode, bookmark_node, "") {
  const SyncedBookmarkTrackerEntity* entity = arg;
  return entity->bookmark_node() == bookmark_node;
}

syncer::UpdateResponseData CreateUpdateResponseData(
    const BookmarkInfo& bookmark_info,
    const syncer::UniquePosition& unique_position,
    int response_version,
    const base::Uuid& uuid) {
  syncer::EntityData data;
  data.id = bookmark_info.server_id;
  data.legacy_parent_id = bookmark_info.parent_id;
  data.server_defined_unique_tag = bookmark_info.server_tag;
  data.originator_client_item_id = uuid.AsLowercaseString();

  sync_pb::BookmarkSpecifics* bookmark_specifics =
      data.specifics.mutable_bookmark();
  bookmark_specifics->set_guid(uuid.AsLowercaseString());
  bookmark_specifics->set_legacy_canonicalized_title(bookmark_info.title);
  bookmark_specifics->set_full_title(bookmark_info.title);
  *bookmark_specifics->mutable_unique_position() = unique_position.ToProto();

  if (bookmark_info.url.empty()) {
    bookmark_specifics->set_type(sync_pb::BookmarkSpecifics::FOLDER);
  } else {
    bookmark_specifics->set_type(sync_pb::BookmarkSpecifics::URL);
    bookmark_specifics->set_url(bookmark_info.url);
  }

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  response_data.response_version = response_version;
  return response_data;
}

syncer::UpdateResponseData CreateUpdateResponseData(
    const BookmarkInfo& bookmark_info,
    const syncer::UniquePosition& unique_position,
    int response_version) {
  return CreateUpdateResponseData(bookmark_info, unique_position,
                                  response_version,
                                  base::Uuid::GenerateRandomV4());
}

sync_pb::DataTypeState CreateDataTypeState() {
  sync_pb::DataTypeState data_type_state;
  data_type_state.set_cache_guid(kCacheGuid);
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  return data_type_state;
}

// |node| must not be nullptr.
sync_pb::BookmarkMetadata CreateNodeMetadata(
    const bookmarks::BookmarkNode* node,
    const std::string& server_id,
    const syncer::UniquePosition& unique_position =
        syncer::UniquePosition::InitialPosition(
            syncer::UniquePosition::RandomSuffix())) {
  sync_pb::BookmarkMetadata bookmark_metadata;
  bookmark_metadata.set_id(node->id());
  bookmark_metadata.mutable_metadata()->set_server_id(server_id);
  bookmark_metadata.mutable_metadata()->set_client_tag_hash(
      syncer::ClientTagHash::FromUnhashed(syncer::BOOKMARKS,
                                          node->uuid().AsLowercaseString())
          .value());
  *bookmark_metadata.mutable_metadata()->mutable_unique_position() =
      unique_position.ToProto();
  // Required by SyncedBookmarkTracker during validation of local metadata.
  if (!node->is_folder()) {
    bookmark_metadata.mutable_metadata()->set_bookmark_favicon_hash(123);
  }
  return bookmark_metadata;
}

// Same as above but marks the node as unsynced (pending commit). |node| must
// not be nullptr.
sync_pb::BookmarkMetadata CreateUnsyncedNodeMetadata(
    const bookmarks::BookmarkNode* node,
    const std::string& server_id) {
  sync_pb::BookmarkMetadata bookmark_metadata =
      CreateNodeMetadata(node, server_id);
  // Mark the entity as unsynced.
  bookmark_metadata.mutable_metadata()->set_sequence_number(2);
  bookmark_metadata.mutable_metadata()->set_acked_sequence_number(1);
  return bookmark_metadata;
}

sync_pb::BookmarkModelMetadata CreateMetadataForPermanentNodes(
    const BookmarkModelView* bookmark_model) {
  sync_pb::BookmarkModelMetadata model_metadata;
  *model_metadata.mutable_data_type_state() = CreateDataTypeState();

  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmark_model->bookmark_bar_node(),
                         /*server_id=*/kBookmarkBarId);
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmark_model->mobile_node(),
                         /*server_id=*/kMobileBookmarksId);
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmark_model->other_node(),
                         /*server_id=*/kOtherBookmarksId);

  return model_metadata;
}

syncer::UpdateResponseDataList CreateUpdateResponseDataListForPermanentNodes() {
  const syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates;
  // Add update for the permanent folders.
  updates.push_back(
      CreateUpdateResponseData({kBookmarkBarId, std::string(), std::string(),
                                kBookmarksRootId, kBookmarkBarTag},
                               kRandomPosition, /*response_version=*/0));
  updates.push_back(
      CreateUpdateResponseData({kOtherBookmarksId, std::string(), std::string(),
                                kBookmarksRootId, kOtherBookmarksTag},
                               kRandomPosition, /*response_version=*/0));
  updates.push_back(CreateUpdateResponseData(
      {kMobileBookmarksId, std::string(), std::string(), kBookmarksRootId,
       kMobileBookmarksTag},
      kRandomPosition, /*response_version=*/0));

  return updates;
}

void AssertState(const BookmarkDataTypeProcessor* processor,
                 const std::vector<BookmarkInfo>& bookmarks) {
  const SyncedBookmarkTracker* tracker = processor->GetTrackerForTest();
  ASSERT_THAT(tracker, NotNull());

  // Make sure the tracker contains all bookmarks in |bookmarks| + the
  // 3 permanent nodes.
  ASSERT_THAT(tracker->TrackedEntitiesCountForTest(), Eq(bookmarks.size() + 3));

  for (BookmarkInfo bookmark : bookmarks) {
    const SyncedBookmarkTrackerEntity* entity =
        tracker->GetEntityForSyncId(bookmark.server_id);
    ASSERT_THAT(entity, NotNull());
    const bookmarks::BookmarkNode* node = entity->bookmark_node();
    ASSERT_THAT(node->GetTitle(), Eq(ASCIIToUTF16(bookmark.title)));
    ASSERT_THAT(node->url(), Eq(GURL(bookmark.url)));
    const SyncedBookmarkTrackerEntity* parent_entity =
        tracker->GetEntityForSyncId(bookmark.parent_id);
    ASSERT_THAT(node->parent(), Eq(parent_entity->bookmark_node()));
  }
}

class ProxyCommitQueue : public syncer::CommitQueue {
 public:
  explicit ProxyCommitQueue(CommitQueue* commit_queue)
      : commit_queue_(commit_queue) {
    DCHECK(commit_queue_);
  }

  void NudgeForCommit() override { commit_queue_->NudgeForCommit(); }

 private:
  raw_ptr<CommitQueue> commit_queue_ = nullptr;
};

class BookmarkDataTypeProcessorTest : public testing::Test {
 public:
  BookmarkDataTypeProcessorTest()
      : bookmark_model_(std::make_unique<TestBookmarkModelView>()),
        processor_(std::make_unique<BookmarkDataTypeProcessor>(
            &bookmark_undo_service_,
            syncer::WipeModelUponSyncDisabledBehavior::kNever)) {
    processor_->SetFaviconService(&favicon_service_);
  }

  // Initialized the processor with bookmarks from the existing model and always
  // initializes permanent folders.
  void SimulateModelReadyToSyncWithInitialSyncDone() {
    sync_pb::BookmarkModelMetadata model_metadata =
        CreateMetadataForPermanentNodes(bookmark_model_.get());
    DCHECK_EQ(model_metadata.data_type_state().initial_sync_state(),
              sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

    // By default, set that bookmarks are reuploaded to avoid reupload logic.
    model_metadata.set_bookmarks_hierarchy_fields_reuploaded(true);

    // Rely on the order of iterating over the tree: left child is always
    // handled before the current one. In this case increasing unique position
    // will always represent the right order.
    syncer::UniquePosition next_unique_position =
        syncer::UniquePosition::InitialPosition(
            syncer::UniquePosition::RandomSuffix());
    ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
        bookmark_model_->root_node());
    while (iterator.has_next()) {
      const bookmarks::BookmarkNode* node = iterator.Next();
      if (node->is_permanent_node()) {
        // Permanent nodes have been added explicitly.
        continue;
      }

      *model_metadata.add_bookmarks_metadata() = CreateNodeMetadata(
          node, /*server_id=*/"id_" + node->uuid().AsLowercaseString(),
          next_unique_position);
      next_unique_position = syncer::UniquePosition::After(
          next_unique_position, syncer::UniquePosition::RandomSuffix());
    }
    processor_->ModelReadyToSync(model_metadata.SerializeAsString(),
                                 schedule_save_closure_.Get(),
                                 bookmark_model_.get());
    ASSERT_TRUE(processor()->IsTrackingMetadata());
  }

  void SimulateModelReadyToSyncWithoutLocalMetadata() {
    processor_->ModelReadyToSync(
        /*metadata_str=*/std::string(), schedule_save_closure_.Get(),
        bookmark_model_.get());
  }

  void SimulateOnSyncStarting(const std::string& cache_guid = kCacheGuid) {
    syncer::DataTypeActivationRequest request;
    request.cache_guid = cache_guid;
    request.error_handler = error_handler_.Get();
    processor_->OnSyncStarting(request, base::DoNothing());
  }

  void SimulateConnectSync() {
    processor_->ConnectSync(
        std::make_unique<ProxyCommitQueue>(&mock_commit_queue_));
  }

  // Simulate browser restart.
  void ResetDataTypeProcessor(
      syncer::WipeModelUponSyncDisabledBehavior
          wipe_model_upon_sync_disabled_behavior =
              syncer::WipeModelUponSyncDisabledBehavior::kNever) {
    processor_ = std::make_unique<BookmarkDataTypeProcessor>(
        &bookmark_undo_service_, wipe_model_upon_sync_disabled_behavior);
    processor_->SetFaviconService(&favicon_service_);
  }

  void DestroyBookmarkModel() { bookmark_model_.reset(); }

  TestBookmarkModelView* bookmark_model() { return bookmark_model_.get(); }
  bookmarks::TestBookmarkClient* bookmark_client() {
    return bookmark_model_->underlying_client();
  }
  BookmarkUndoService* bookmark_undo_service() {
    return &bookmark_undo_service_;
  }
  favicon::FaviconService* favicon_service() { return &favicon_service_; }
  syncer::MockCommitQueue* mock_commit_queue() { return &mock_commit_queue_; }
  BookmarkDataTypeProcessor* processor() { return processor_.get(); }
  base::MockCallback<base::RepeatingClosure>* schedule_save_closure() {
    return &schedule_save_closure_;
  }

  syncer::CommitRequestDataList GetLocalChangesFromProcessor(
      size_t max_entries) {
    base::MockOnceCallback<void(syncer::CommitRequestDataList &&)> callback;
    syncer::CommitRequestDataList local_changes;
    // Destruction of the mock upon return will verify that Run() was indeed
    // invoked.
    EXPECT_CALL(callback, Run).WillOnce(MoveArg(&local_changes));
    processor_->GetLocalChanges(max_entries, callback.Get());
    return local_changes;
  }

  base::MockRepeatingCallback<void(const syncer::ModelError&)>*
  error_handler() {
    return &error_handler_;
  }

  sync_pb::DataTypeState::Invalidation BuildInvalidation(
      int64_t version,
      const std::string& payload) {
    sync_pb::DataTypeState::Invalidation inv;
    inv.set_version(version);
    inv.set_hint(payload);
    return inv;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  NiceMock<base::MockCallback<base::RepeatingClosure>> schedule_save_closure_;
  NiceMock<base::MockRepeatingCallback<void(const syncer::ModelError&)>>
      error_handler_;
  BookmarkUndoService bookmark_undo_service_;
  NiceMock<favicon::MockFaviconService> favicon_service_;
  NiceMock<syncer::MockCommitQueue> mock_commit_queue_;
  std::unique_ptr<TestBookmarkModelView> bookmark_model_;
  // `processor_` might hold a raw_ptr to `bookmark_model_`. It should be
  // destroyed first to avoid holding a briefly dangling pointer.
  std::unique_ptr<BookmarkDataTypeProcessor> processor_;
};

TEST_F(BookmarkDataTypeProcessorTest, ShouldDoInitialMergeWithZeroBookmarks) {
  SimulateModelReadyToSyncWithoutLocalMetadata();
  SimulateOnSyncStarting();
  SimulateConnectSync();

  syncer::UpdateResponseDataList updates =
      CreateUpdateResponseDataListForPermanentNodes();

  ASSERT_FALSE(processor()->IsTrackingMetadata());

  base::HistogramTester histogram_tester;
  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);
  EXPECT_TRUE(processor()->IsTrackingMetadata());
  EXPECT_THAT(bookmark_model()->bookmark_bar_node()->children(), IsEmpty());

  histogram_tester.ExpectTotalCount(
      kPersistentDataTypeConfigurationTimeMetricName,
      /*count=*/1);
}

TEST_F(BookmarkDataTypeProcessorTest, ShouldDoInitialMergeWithOneBookmark) {
  SimulateModelReadyToSyncWithoutLocalMetadata();
  SimulateOnSyncStarting();
  SimulateConnectSync();

  syncer::UpdateResponseDataList updates =
      CreateUpdateResponseDataListForPermanentNodes();

  // Add one regular bookmark.
  updates.push_back(CreateUpdateResponseData(
      {"id1", "title1", "http://foo.com", kBookmarkBarId,
       /*server_tag=*/std::string()},
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix()),
      /*response_version=*/0));

  ASSERT_FALSE(processor()->IsTrackingMetadata());

  base::HistogramTester histogram_tester;
  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);
  EXPECT_TRUE(processor()->IsTrackingMetadata());
  EXPECT_THAT(bookmark_model()->bookmark_bar_node()->children(), SizeIs(1));

  histogram_tester.ExpectTotalCount(
      kPersistentDataTypeConfigurationTimeMetricName,
      /*count=*/1);
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldFailInitialMergeIfServerPermanentNodeMissing) {
  SimulateModelReadyToSyncWithoutLocalMetadata();
  SimulateOnSyncStarting();
  SimulateConnectSync();

  syncer::UpdateResponseDataList updates =
      CreateUpdateResponseDataListForPermanentNodes();

  // Remove one of the permanent nodes.
  updates.pop_back();

  // Add one regular bookmark.
  updates.push_back(CreateUpdateResponseData(
      {"id1", "title1", "http://foo.com", kBookmarkBarId,
       /*server_tag=*/std::string()},
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix()),
      /*response_version=*/0));

  ASSERT_FALSE(processor()->IsTrackingMetadata());
  ASSERT_TRUE(processor()->IsConnectedForTest());

  // Expect failure when doing initial merge.
  EXPECT_CALL(*error_handler(), Run);

  base::HistogramTester histogram_tester;
  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);

  EXPECT_FALSE(processor()->IsTrackingMetadata());
  EXPECT_FALSE(processor()->IsConnectedForTest());

  // Not an actual requirement but it documents current behavior.
  EXPECT_THAT(bookmark_model()->bookmark_bar_node()->children(), SizeIs(1));

  histogram_tester.ExpectTotalCount(
      kPersistentDataTypeConfigurationTimeMetricName,
      /*count=*/0);
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldFailInitialMergeAndAvoidPartialDataIfServerPermanentNodeMissing) {
  ResetDataTypeProcessor(syncer::WipeModelUponSyncDisabledBehavior::kAlways);
  SimulateModelReadyToSyncWithoutLocalMetadata();
  SimulateOnSyncStarting();
  SimulateConnectSync();

  syncer::UpdateResponseDataList updates =
      CreateUpdateResponseDataListForPermanentNodes();

  // Remove one of the permanent nodes.
  updates.pop_back();

  // Add one regular bookmark.
  updates.push_back(CreateUpdateResponseData(
      {"id1", "title1", "http://foo.com", kBookmarkBarId,
       /*server_tag=*/std::string()},
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix()),
      /*response_version=*/0));

  ASSERT_FALSE(processor()->IsTrackingMetadata());
  ASSERT_TRUE(processor()->IsConnectedForTest());

  // Expect failure when doing initial merge.
  EXPECT_CALL(*error_handler(), Run);

  base::HistogramTester histogram_tester;
  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);

  EXPECT_FALSE(processor()->IsTrackingMetadata());
  EXPECT_FALSE(processor()->IsConnectedForTest());

  // Avoid exposing part of the tree to the user. When using
  // `syncer::WipeModelUponSyncDisabledBehavior::kAlways`, reverting to the
  // pre-merge state means clearing all data.
  EXPECT_THAT(bookmark_model()->bookmark_bar_node()->children(), IsEmpty());

  histogram_tester.ExpectTotalCount(
      kPersistentDataTypeConfigurationTimeMetricName,
      /*count=*/0);
}

TEST_F(BookmarkDataTypeProcessorTest, ShouldUpdateModelAfterRemoteCreation) {
  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateOnSyncStarting();

  // Add update for another node under the bookmarks bar.
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";
  const syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates;
  updates.push_back(
      CreateUpdateResponseData({kNodeId, kTitle, kUrl, kBookmarkBarId,
                                /*server_tag=*/std::string()},
                               kRandomPosition, /*response_version=*/0));

  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model()->bookmark_bar_node();
  ASSERT_TRUE(bookmark_bar->children().empty());

  base::HistogramTester histogram_tester;
  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);

  ASSERT_THAT(bookmark_bar->children().front().get(), NotNull());
  EXPECT_THAT(bookmark_bar->children().front()->GetTitle(),
              Eq(ASCIIToUTF16(kTitle)));
  EXPECT_THAT(bookmark_bar->children().front()->url(), Eq(GURL(kUrl)));

  // Incremental updates to not contribute to Sync.DataTypeConfigurationTime.
  histogram_tester.ExpectTotalCount(
      kPersistentDataTypeConfigurationTimeMetricName,
      /*count=*/0);
}

TEST_F(BookmarkDataTypeProcessorTest, ShouldUpdateModelAfterRemoteUpdate) {
  const std::string kTitle = "title";
  const GURL kUrl("http://www.url.com");
  const syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node = bookmark_model()->AddURL(
      bookmark_bar, /*index=*/0, base::UTF8ToUTF16(kTitle), kUrl);
  SimulateOnSyncStarting();
  SimulateModelReadyToSyncWithInitialSyncDone();

  const SyncedBookmarkTrackerEntity* entity =
      processor()->GetTrackerForTest()->GetEntityForBookmarkNode(bookmark_node);
  ASSERT_THAT(entity, NotNull());

  // Process an update for the same bookmark.
  const std::string kNewTitle = "new-title";
  const std::string kNewUrl = "http://www.new-url.com";
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      {entity->metadata().server_id(), kNewTitle, kNewUrl, kBookmarkBarId,
       /*server_tag=*/std::string()},
      kRandomPosition, /*response_version=*/1, bookmark_node->uuid()));

  base::HistogramTester histogram_tester;
  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);

  // Check if the bookmark has been updated properly.
  EXPECT_THAT(bookmark_bar->children().front().get(), Eq(bookmark_node));
  EXPECT_THAT(bookmark_node->GetTitle(), Eq(ASCIIToUTF16(kNewTitle)));
  EXPECT_THAT(bookmark_node->url(), Eq(GURL(kNewUrl)));
}

TEST_F(
    BookmarkDataTypeProcessorTest,
    ShouldScheduleSaveAfterRemoteUpdateWithOnlyMetadataChangeAndReflections) {
  const std::string kTitle = "title";
  const GURL kUrl("http://www.url.com");
  const syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node = bookmark_model()->AddURL(
      bookmark_bar, /*index=*/0, base::UTF8ToUTF16(kTitle), kUrl);
  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateOnSyncStarting();

  const SyncedBookmarkTrackerEntity* entity =
      processor()->GetTrackerForTest()->GetEntityForBookmarkNode(bookmark_node);
  ASSERT_THAT(entity, NotNull());

  // Process an update for the same bookmark with the same data.
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      {entity->metadata().server_id(), kTitle, kUrl.spec(), kBookmarkBarId,
       /*server_tag=*/std::string()},
      kRandomPosition, /*response_version=*/1, bookmark_node->uuid()));
  updates[0].response_version++;

  EXPECT_CALL(*schedule_save_closure(), Run());
  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);
}

TEST_F(BookmarkDataTypeProcessorTest, ShouldDecodeSyncMetadata) {
  const std::string kNodeId = "node_id1";
  const std::string kTitle = "title1";
  const std::string kUrl = "http://www.url1.com";

  std::vector<BookmarkInfo> bookmarks = {
      {kNodeId, kTitle, kUrl, kBookmarkBarId, /*server_tag=*/std::string()}};

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmarknode = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));

  SimulateModelReadyToSyncWithoutLocalMetadata();
  SimulateOnSyncStarting();

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model());

  // Add an entry for the bookmark node.
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmarknode, kNodeId);

  // Create a new processor and init it with the metadata str.
  BookmarkDataTypeProcessor new_processor(
      bookmark_undo_service(),
      syncer::WipeModelUponSyncDisabledBehavior::kNever);

  std::string metadata_str;
  model_metadata.SerializeToString(&metadata_str);
  new_processor.ModelReadyToSync(metadata_str, base::DoNothing(),
                                 bookmark_model());

  AssertState(&new_processor, bookmarks);
}

TEST_F(BookmarkDataTypeProcessorTest, ShouldDecodeEncodedSyncMetadata) {
  const std::string kNodeId1 = "node_id1";
  const std::string kTitle1 = "title1";
  const GURL kUrl1("http://www.url1.com");

  const std::string kNodeId2 = "node_id2";
  const std::string kTitle2 = "title2";
  const GURL kUrl2("http://www.url2.com");

  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model()->bookmark_bar_node();
  bookmark_model()->AddURL(bookmark_bar, /*index=*/0,
                           base::UTF8ToUTF16(kTitle1), kUrl1);
  bookmark_model()->AddURL(bookmark_bar, /*index=*/1,
                           base::UTF8ToUTF16(kTitle2), kUrl2);
  SimulateOnSyncStarting();
  SimulateModelReadyToSyncWithInitialSyncDone();

  // Create a new processor and init it with the same metadata str.
  BookmarkDataTypeProcessor new_processor(
      bookmark_undo_service(),
      syncer::WipeModelUponSyncDisabledBehavior::kNever);
  new_processor.ModelReadyToSync(processor()->EncodeSyncMetadata(),
                                 base::DoNothing(), bookmark_model());

  new_processor.GetTrackerForTest()->CheckAllNodesTracked(bookmark_model());

  // Make sure shutdown doesn't crash.
  DestroyBookmarkModel();
  EXPECT_FALSE(processor()->IsConnectedForTest());
  EXPECT_FALSE(new_processor.IsConnectedForTest());
  EXPECT_TRUE(processor()->IsTrackingMetadata());
  EXPECT_TRUE(new_processor.IsTrackingMetadata());
}

TEST_F(BookmarkDataTypeProcessorTest, ShouldDecodeEmptyMetadata) {
  // No save should be scheduled.
  EXPECT_CALL(*schedule_save_closure(), Run()).Times(0);
  SimulateModelReadyToSyncWithoutLocalMetadata();
  EXPECT_FALSE(processor()->IsTrackingMetadata());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldIgnoreNonEmptyMetadataWhileSyncNotDone) {
  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.mutable_data_type_state()->set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
  // Add entries to the metadata.
  sync_pb::BookmarkMetadata* bookmark_metadata =
      model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(bookmark_model()->bookmark_bar_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id(kBookmarkBarId);

  // Create a new processor and init it with the metadata str.
  BookmarkDataTypeProcessor new_processor(
      bookmark_undo_service(),
      syncer::WipeModelUponSyncDisabledBehavior::kNever);

  // A save should be scheduled.
  NiceMock<base::MockCallback<base::RepeatingClosure>>
      new_schedule_save_closure;
  EXPECT_CALL(new_schedule_save_closure, Run());

  std::string metadata_str;
  model_metadata.SerializeToString(&metadata_str);
  new_processor.ModelReadyToSync(metadata_str, new_schedule_save_closure.Get(),
                                 bookmark_model());
  // Metadata are corrupted, so no tracker should have been created.
  EXPECT_FALSE(new_processor.IsTrackingMetadata());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldIgnoreMetadataNotMatchingTheModel) {
  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.mutable_data_type_state()->set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  // Add entries for only the bookmark bar. However, the TestBookmarkClient will
  // create all the 3 permanent nodes.
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmark_model()->bookmark_bar_node(),
                         /*server_id=*/kBookmarkBarId);

  // Create a new processor and init it with the metadata str.
  BookmarkDataTypeProcessor new_processor(
      bookmark_undo_service(),
      syncer::WipeModelUponSyncDisabledBehavior::kNever);

  // A save should be scheduled.
  NiceMock<base::MockCallback<base::RepeatingClosure>>
      new_schedule_save_closure;
  EXPECT_CALL(new_schedule_save_closure, Run());

  std::string metadata_str;
  model_metadata.SerializeToString(&metadata_str);
  new_processor.ModelReadyToSync(metadata_str, new_schedule_save_closure.Get(),
                                 bookmark_model());

  // Metadata are corrupted, so no tracker should have been created.
  EXPECT_FALSE(new_processor.IsTrackingMetadata());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldIgnoreMetadataIfCacheGuidMismatch) {
  SimulateModelReadyToSyncWithInitialSyncDone();
  ASSERT_TRUE(processor()->IsTrackingMetadata());
  SimulateOnSyncStarting("unexpected_cache_guid");
  EXPECT_FALSE(processor()->IsTrackingMetadata());
}

// Verifies that the data type state stored in the tracker gets
// updated upon handling remote updates by assigning a new encryption
// key name.
TEST_F(BookmarkDataTypeProcessorTest,
       ShouldUpdateDataTypeStateUponHandlingRemoteUpdates) {
  // Initialize the process to make sure the tracker has been created.
  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateOnSyncStarting();
  const SyncedBookmarkTracker* tracker = processor()->GetTrackerForTest();
  // The encryption key name should be empty.
  ASSERT_TRUE(tracker->data_type_state().encryption_key_name().empty());

  // Build a data type state with an encryption key name.
  const std::string kEncryptionKeyName = "new_encryption_key_name";
  sync_pb::DataTypeState data_type_state(CreateDataTypeState());
  data_type_state.set_encryption_key_name(kEncryptionKeyName);

  // Push empty updates list to the processor together with the updated model
  // type state.
  syncer::UpdateResponseDataList empty_updates_list;
  processor()->OnUpdateReceived(data_type_state, std::move(empty_updates_list),
                                /*gc_directive=*/std::nullopt);

  // The data type state inside the tracker should have been updated, and
  // carries the new encryption key name.
  EXPECT_THAT(tracker->data_type_state().encryption_key_name(),
              Eq(kEncryptionKeyName));
}

// Verifies that the data type state stored in the tracker gets
// updated upon handling remote updates by replacing new pending invalidations.
TEST_F(BookmarkDataTypeProcessorTest,
       ShouldUpdateDataTypeStateUponHandlingInvalidations) {
  // Initialize the process to make sure the tracker has been created.
  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateOnSyncStarting();
  const SyncedBookmarkTracker* tracker = processor()->GetTrackerForTest();

  // Build invalidations.
  sync_pb::DataTypeState::Invalidation inv_1 =
      BuildInvalidation(1, "bm_hint_1");
  sync_pb::DataTypeState::Invalidation inv_2 =
      BuildInvalidation(2, "bm_hint_2");
  EXPECT_CALL(*schedule_save_closure(), Run());

  processor()->StorePendingInvalidations({inv_1, inv_2});

  // The data type state inside the tracker should have been updated, and
  // carries the new invalidations.
  EXPECT_EQ(2, tracker->data_type_state().invalidations_size());

  EXPECT_EQ(inv_1.hint(), tracker->data_type_state().invalidations(0).hint());
  EXPECT_EQ(inv_1.version(),
            tracker->data_type_state().invalidations(0).version());

  EXPECT_EQ(inv_2.hint(), tracker->data_type_state().invalidations(1).hint());
  EXPECT_EQ(inv_2.version(),
            tracker->data_type_state().invalidations(1).version());
}

// This tests that when the encryption key changes, but the received entities
// are already encrypted with the up-to-date encryption key, no recommit is
// needed.
TEST_F(BookmarkDataTypeProcessorTest,
       ShouldNotRecommitEntitiesWhenEncryptionIsUpToDate) {
  // Initialize the process to make sure the tracker has been created.
  SimulateOnSyncStarting();
  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateConnectSync();
  const SyncedBookmarkTracker* tracker = processor()->GetTrackerForTest();
  // The encryption key name should be empty.
  ASSERT_TRUE(tracker->data_type_state().encryption_key_name().empty());

  // Build a data type state with an encryption key name.
  const std::string kEncryptionKeyName = "new_encryption_key_name";
  sync_pb::DataTypeState data_type_state(CreateDataTypeState());
  data_type_state.set_encryption_key_name(kEncryptionKeyName);

  // Push an update that is encrypted with the new encryption key.
  const std::string kNodeId = "node_id";
  syncer::UpdateResponseData response_data = CreateUpdateResponseData(
      {kNodeId, "title", "http://www.url.com", /*parent_id=*/kBookmarkBarId,
       /*server_tag=*/std::string()},
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix()),
      /*response_version=*/0);
  response_data.encryption_key_name = kEncryptionKeyName;

  EXPECT_CALL(*mock_commit_queue(), NudgeForCommit()).Times(0);
  syncer::UpdateResponseDataList updates;
  updates.push_back(std::move(response_data));
  processor()->OnUpdateReceived(data_type_state, std::move(updates),
                                /*gc_directive=*/std::nullopt);

  // The bookmarks shouldn't be marked for committing.
  ASSERT_THAT(tracker->GetEntityForSyncId(kNodeId), NotNull());
  EXPECT_THAT(tracker->GetEntityForSyncId(kNodeId)->IsUnsynced(), Eq(false));
}

// Verifies that the processor doesn't crash if sync is stopped before receiving
// remote updates or tracking metadata.
TEST_F(BookmarkDataTypeProcessorTest, ShouldStopBeforeReceivingRemoteUpdates) {
  SimulateModelReadyToSyncWithoutLocalMetadata();
  SimulateOnSyncStarting();
  ASSERT_FALSE(processor()->IsTrackingMetadata());
  processor()->OnSyncStopping(syncer::CLEAR_METADATA);
  EXPECT_FALSE(processor()->IsTrackingMetadata());
}

TEST_F(BookmarkDataTypeProcessorTest, ShouldStopAfterReceivingRemoteUpdates) {
  // Initialize the process to make sure the tracker has been created.
  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateOnSyncStarting();
  ASSERT_TRUE(processor()->IsTrackingMetadata());
  processor()->OnSyncStopping(syncer::CLEAR_METADATA);
  EXPECT_FALSE(processor()->IsTrackingMetadata());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldReportNoCountersWhenModelIsNotLoaded) {
  SimulateOnSyncStarting();
  ASSERT_FALSE(processor()->IsTrackingMetadata());
  syncer::TypeEntitiesCount count(syncer::BOOKMARKS);
  // Assign an arbitrary non-zero number of entities to be able to check that
  // actually a 0 has been written to it later.
  count.non_tombstone_entities = 1000;
  processor()->GetTypeEntitiesCountForDebugging(base::BindLambdaForTesting(
      [&](const syncer::TypeEntitiesCount& returned_count) {
        count = returned_count;
      }));
  EXPECT_EQ(0, count.non_tombstone_entities);
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldNotCommitEntitiesWithoutLoadedFavicons) {
  const std::string kNodeId = "node_id1";
  const std::string kTitle = "title1";
  const std::string kUrl = "http://www.url1.com";
  const std::string kIconUrl = "http://www.url1.com/favicon";

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));

  // Add an entry for the bookmark node that is unsynced.
  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model());
  *model_metadata.add_bookmarks_metadata() =
      CreateUnsyncedNodeMetadata(node, kNodeId);

  SimulateOnSyncStarting();
  processor()->ModelReadyToSync(model_metadata.SerializeAsString(),
                                schedule_save_closure()->Get(),
                                bookmark_model());

  ASSERT_FALSE(bookmark_client()->HasFaviconLoadTasks());
  EXPECT_THAT(GetLocalChangesFromProcessor(/*max_entries=*/10), IsEmpty());
  EXPECT_TRUE(node->is_favicon_loading());

  bookmark_client()->SimulateFaviconLoaded(GURL(kUrl), GURL(kIconUrl),
                                           gfx::Image());
  ASSERT_TRUE(node->is_favicon_loaded());
  EXPECT_THAT(GetLocalChangesFromProcessor(/*max_entries=*/10),
              ElementsAre(CommitRequestDataMatchesGuid(node->uuid())));
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldCommitEntitiesWhileOtherFaviconsLoading) {
  const std::string kNodeId1 = "node_id1";
  const std::string kNodeId2 = "node_id2";
  const std::string kTitle = "title";
  const std::string kUrl1 = "http://www.url1.com";
  const std::string kUrl2 = "http://www.url2.com";
  const std::string kIconUrl = "http://www.url.com/favicon";

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* node1 = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl1));
  const bookmarks::BookmarkNode* node2 = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/1, base::UTF8ToUTF16(kTitle),
      GURL(kUrl2));

  // Add entries for the two bookmark nodes and mark them as unsynced.
  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model());
  *model_metadata.add_bookmarks_metadata() =
      CreateUnsyncedNodeMetadata(node1, kNodeId1);
  *model_metadata.add_bookmarks_metadata() =
      CreateUnsyncedNodeMetadata(node2, kNodeId2);

  SimulateOnSyncStarting();
  processor()->ModelReadyToSync(model_metadata.SerializeAsString(),
                                schedule_save_closure()->Get(),
                                bookmark_model());

  // The goal of this test is to mimic the case where one bookmark (the first
  // one listed by SyncedBookmarkTracker::GetEntitiesWithLocalChanges()) has no
  // loaded favicon, while the second one does. The precise order is not known
  // in advance (in the current implementation, it depends on the iteration
  // order for raw pointers in an unordered_set) which means the test needs to
  // pass for both cases.
  const std::vector<const SyncedBookmarkTrackerEntity*> unsynced_entities =
      processor()->GetTrackerForTest()->GetEntitiesWithLocalChanges();
  ASSERT_THAT(
      unsynced_entities,
      UnorderedElementsAre(TrackedEntityCorrespondsToBookmarkNode(node1),
                           TrackedEntityCorrespondsToBookmarkNode(node2)));

  // Force a favicon load for the second listed entity, but leave the first
  // without loaded favicon.
  bookmark_model()->GetFavicon(unsynced_entities[1]->bookmark_node());
  bookmark_client()->SimulateFaviconLoaded(
      unsynced_entities[1]->bookmark_node()->url(), GURL(kIconUrl),
      gfx::Image());
  ASSERT_TRUE(unsynced_entities[1]->bookmark_node()->is_favicon_loaded());
  ASSERT_FALSE(unsynced_entities[0]->bookmark_node()->is_favicon_loaded());
  ASSERT_FALSE(unsynced_entities[0]->bookmark_node()->is_favicon_loading());

  EXPECT_THAT(GetLocalChangesFromProcessor(/*max_entries=*/1),
              ElementsAre(CommitRequestDataMatchesGuid(
                  unsynced_entities[1]->bookmark_node()->uuid())));

  // |unsynced_entities[0]| has been excluded from the result above because the
  // favicon isn't loaded, but the loading process should have started now (see
  // BookmarkLocalChangesBuilder::BuildCommitRequests()).
  EXPECT_TRUE(unsynced_entities[0]->bookmark_node()->is_favicon_loading());
}

TEST_F(BookmarkDataTypeProcessorTest, ShouldReuploadLegacyBookmarksOnStart) {
  const std::string kTitle = "title";
  const GURL kUrl("http://www.url.com");

  const bookmarks::BookmarkNode* node =
      bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(),
                               /*index=*/0, base::UTF8ToUTF16(kTitle), kUrl);

  SimulateOnSyncStarting();
  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateConnectSync();

  ASSERT_THAT(processor()->GetTrackerForTest()->GetEntityForBookmarkNode(node),
              NotNull());
  const std::string server_id = processor()
                                    ->GetTrackerForTest()
                                    ->GetEntityForBookmarkNode(node)
                                    ->metadata()
                                    .server_id();

  sync_pb::BookmarkModelMetadata model_metadata =
      processor()->GetTrackerForTest()->BuildBookmarkModelMetadata();
  model_metadata.clear_bookmarks_hierarchy_fields_reuploaded();
  ASSERT_FALSE(processor()->GetTrackerForTest()->HasLocalChanges());

  // Simulate browser restart, enable sync reupload and initialize the processor
  // again.
  ResetDataTypeProcessor();

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(switches::kSyncReuploadBookmarks);

  std::string metadata_str;
  model_metadata.SerializeToString(&metadata_str);
  processor()->ModelReadyToSync(metadata_str, base::DoNothing(),
                                bookmark_model());
  SimulateOnSyncStarting();
  SimulateConnectSync();

  ASSERT_TRUE(processor()->IsTrackingMetadata());
  const SyncedBookmarkTrackerEntity* entity =
      processor()->GetTrackerForTest()->GetEntityForSyncId(server_id);
  ASSERT_THAT(entity, NotNull());

  // Entity should be synced before until first update is received.
  ASSERT_FALSE(entity->IsUnsynced());
  ASSERT_FALSE(processor()
                   ->GetTrackerForTest()
                   ->BuildBookmarkModelMetadata()
                   .bookmarks_hierarchy_fields_reuploaded());

  // Synchronize with the server and get any updates.
  EXPECT_CALL(*mock_commit_queue(), NudgeForCommit());
  processor()->OnUpdateReceived(CreateDataTypeState(),
                                syncer::UpdateResponseDataList(),
                                /*gc_directive=*/std::nullopt);

  // Check that all entities are unsynced now and metadata is marked as
  // reuploaded.
  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_TRUE(processor()
                  ->GetTrackerForTest()
                  ->BuildBookmarkModelMetadata()
                  .bookmarks_hierarchy_fields_reuploaded());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldReportErrorIfIncrementalLocalCreationCrossesMaxCountLimit) {
  // Set a limit of 3 bookmarks, i.e. limit it to the 3 permanent nodes.
  processor()->SetMaxBookmarksTillSyncEnabledForTest(3);

  // Expect failure when adding new bookmark.
  EXPECT_CALL(*error_handler(), Run);

  SimulateOnSyncStarting();
  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateConnectSync();

  const std::string kNodeId = "node_id1";
  const std::string kTitle = "title1";
  const std::string kUrl = "http://www.url1.com";
  const std::string kIconUrl = "http://www.url1.com/favicon";

  ASSERT_TRUE(processor()->IsConnectedForTest());
  // Add a new bookmark to exceed the limit.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));

  EXPECT_FALSE(processor()->IsConnectedForTest());
  // Expect tracking to still be enabled.
  EXPECT_TRUE(processor()->IsTrackingMetadata());
}

TEST_F(
    BookmarkDataTypeProcessorTest,
    ShouldReportErrorIfBookmarksCountExceedsLimitOnStartupWhenMetadataMatchesModel) {
  // Set a limit of 3 bookmarks, i.e. limit it to the 3 permanent nodes.
  processor()->SetMaxBookmarksTillSyncEnabledForTest(3);

  // Expect error twice. First, when new bookmark is added. Next after restart.
  EXPECT_CALL(*error_handler(), Run).Times(2);

  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateOnSyncStarting();
  SimulateConnectSync();

  const std::string kNodeId = "node_id1";
  const std::string kTitle = "title1";
  const std::string kUrl = "http://www.url1.com";
  const std::string kIconUrl = "http://www.url1.com/favicon";

  // Add a new bookmark to exceed the limit.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmarknode = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model());

  // Add an entry for the bookmark node.
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmarknode, kNodeId);

  // Save metadata for init after restart.
  std::string metadata_str;
  model_metadata.SerializeToString(&metadata_str);

  // Simulate browser restart.
  ResetDataTypeProcessor();
  processor()->SetMaxBookmarksTillSyncEnabledForTest(3);
  processor()->ModelReadyToSync(metadata_str, schedule_save_closure()->Get(),
                                bookmark_model());
  // Metadata matches model, so tracker should be not null.
  EXPECT_TRUE(processor()->IsTrackingMetadata());
  // Should invoke error_handler::Run and schedule_save_closure::Run.
  SimulateOnSyncStarting();

  // Expect tracking to still be enabled.
  EXPECT_TRUE(processor()->IsTrackingMetadata());
}

TEST_F(
    BookmarkDataTypeProcessorTest,
    ShouldReportErrorIfBookmarksCountExceedsLimitOnStartupWhenMetadataDoesNotMatchModel) {
  // Set a limit of 3 bookmarks, i.e. limit it to the 3 permanent nodes.
  processor()->SetMaxBookmarksTillSyncEnabledForTest(3);

  // Expect error twice. First, when new bookmark is added. Next after restart.
  EXPECT_CALL(*error_handler(), Run).Times(2);

  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateOnSyncStarting();
  SimulateConnectSync();

  const std::string kNodeId = "node_id1";
  const std::string kTitle = "title1";
  const std::string kUrl = "http://www.url1.com";
  const std::string kIconUrl = "http://www.url1.com/favicon";

  // Add a new bookmark to exceed the limit.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));

  // Simulate browser restart.
  ResetDataTypeProcessor();
  processor()->SetMaxBookmarksTillSyncEnabledForTest(3);
  SimulateModelReadyToSyncWithoutLocalMetadata();
  // Metadata does not match model, so tracker should be null.
  EXPECT_FALSE(processor()->IsTrackingMetadata());
  // Should invoke error_handler::Run and schedule_save_closure::Run.
  SimulateOnSyncStarting();
}

TEST_F(
    BookmarkDataTypeProcessorTest,
    BookmarkModelShouldWorkNormallyEvenAfterSyncReportedErrorDueToMaxLimitCrossed) {
  // Ensure that bookmarks model works normally even after sync reports error
  // when max count limit is crossed.

  // Set a limit of 3 bookmarks, i.e. limit it to the 3 permanent nodes.
  processor()->SetMaxBookmarksTillSyncEnabledForTest(3);

  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateOnSyncStarting();
  SimulateConnectSync();

  const std::string kNodeId1 = "node_id1";
  const std::string kTitle1 = "title1";
  const std::string kUrl1 = "http://www.url1.com";

  // Add a new bookmark to exceed the limit.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmarknode1 = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle1),
      GURL(kUrl1));

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model());

  // Add an entry for the bookmark node.
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmarknode1, kNodeId1);

  // Add another bookmark.
  const std::string kNodeId2 = "node_id2";
  const std::string kTitle2 = "title2";
  const std::string kUrl2 = "http://www.url2.com";

  const bookmarks::BookmarkNode* bookmarknode2 = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle2),
      GURL(kUrl2));

  // Add an entry for the bookmark node.
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmarknode2, kNodeId2);

  // Save metadata for init after restart.
  std::string metadata_str;
  model_metadata.SerializeToString(&metadata_str);

  // Simulate browser restart.
  ResetDataTypeProcessor();
  processor()->SetMaxBookmarksTillSyncEnabledForTest(3);
  processor()->ModelReadyToSync(metadata_str, base::DoNothing(),
                                bookmark_model());
  // Should lead to error_handler::Run.
  SimulateOnSyncStarting();

  // The second bookmark should have been added anyway.
  EXPECT_EQ(bookmark_model()->bookmark_bar_node()->children().size(), 2u);
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldReportErrorIfBookmarksCountExceedsLimitAfterInitialUpdate) {
  // Set a limit of 4 bookmarks: 3 permanent nodes and 1 additional node which
  // is different from the remote.
  processor()->SetMaxBookmarksTillSyncEnabledForTest(4);

  const std::string kTitle1 = "title1";
  const std::string kUrl1 = "http://www.url1.com";

  // Set up a preexisting bookmark under other node.
  const bookmarks::BookmarkNode* other_node = bookmark_model()->other_node();
  bookmark_model()->AddURL(
      /*parent=*/other_node, /*index=*/0, base::UTF8ToUTF16(kTitle1),
      GURL(kUrl1));

  // Expect failure after initial update is merged.
  bool error_reported = false;
  EXPECT_CALL(*error_handler(), Run).Times(1).WillRepeatedly([&]() {
    error_reported = true;
  });

  SimulateModelReadyToSyncWithoutLocalMetadata();
  SimulateOnSyncStarting();
  SimulateConnectSync();

  const syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates =
      CreateUpdateResponseDataListForPermanentNodes();

  // Add update for another node under the bookmarks bar.
  const std::string kNodeId2 = "node_id2";
  const std::string kTitle2 = "title2";
  const std::string kUrl2 = "http://www.url2.com";

  updates.push_back(
      CreateUpdateResponseData({kNodeId2, kTitle2, kUrl2, kBookmarkBarId,
                                /*server_tag=*/std::string()},
                               kRandomPosition, /*response_version=*/0));

  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model()->bookmark_bar_node();

  // Ensures that OnInitialUpdateReceived will be called.
  ASSERT_FALSE(processor()->IsTrackingMetadata());
  ASSERT_TRUE(bookmark_bar->children().empty());
  ASSERT_TRUE(processor()->IsConnectedForTest());

  ASSERT_FALSE(error_reported);
  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);
  EXPECT_TRUE(error_reported);
  EXPECT_FALSE(processor()->IsConnectedForTest());
  // New bookmark gets added though. Note that this is as per the current
  // behaviour but is not a requirement.
  EXPECT_FALSE(bookmark_bar->children().empty());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldReportErrorIfBookmarksCountExceedsLimitAfterIncrementalUpdate) {
  // Set a limit of 3 bookmarks, i.e. limit it to the 3 permanent nodes.
  processor()->SetMaxBookmarksTillSyncEnabledForTest(3);

  // Expect failure after initial update is merged.
  bool error_reported = false;
  EXPECT_CALL(*error_handler(), Run).Times(1).WillRepeatedly([&]() {
    error_reported = true;
  });

  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateOnSyncStarting();
  SimulateConnectSync();

  const syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates;
  // Add update for another node under the bookmarks bar.
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";

  updates.push_back(
      CreateUpdateResponseData({kNodeId, kTitle, kUrl, kBookmarkBarId,
                                /*server_tag=*/std::string()},
                               kRandomPosition, /*response_version=*/0));

  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model()->bookmark_bar_node();

  // Ensures that path for incremental updates will be called.
  ASSERT_TRUE(processor()->IsTrackingMetadata());
  ASSERT_TRUE(bookmark_bar->children().empty());
  ASSERT_TRUE(processor()->IsConnectedForTest());

  ASSERT_FALSE(error_reported);
  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);
  EXPECT_TRUE(error_reported);
  EXPECT_FALSE(processor()->IsConnectedForTest());
  EXPECT_TRUE(processor()->IsTrackingMetadata());
  // New bookmark gets added though. Note that this is as per the current
  // behaviour but is not a requirement.
  EXPECT_FALSE(bookmark_bar->children().empty());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldReportErrorIfInitialUpdatesCrossMaxCountLimit) {
  // Set a limit of 3 bookmarks, i.e. limit it to the 3 permanent nodes.
  processor()->SetMaxBookmarksTillSyncEnabledForTest(3);

  // Expect failure when initial update of count 4 is received.
  bool error_reported = false;
  EXPECT_CALL(*error_handler(), Run).Times(1).WillRepeatedly([&]() {
    error_reported = true;
  });
  EXPECT_CALL(*mock_commit_queue(), NudgeForCommit()).Times(0);

  SimulateModelReadyToSyncWithoutLocalMetadata();
  SimulateOnSyncStarting();
  SimulateConnectSync();

  const syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates =
      CreateUpdateResponseDataListForPermanentNodes();

  // Entry for the root folder. The server may or may not send a root node, but
  // the current implementation still handles it.
  updates.push_back(CreateUpdateResponseData(
      {kBookmarksRootId, std::string(), std::string(), std::string(),
       syncer::DataTypeToProtocolRootTag(syncer::BOOKMARKS)},
      kRandomPosition, /*response_version=*/0));

  // Add update for another node under the bookmarks bar.
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";

  updates.push_back(
      CreateUpdateResponseData({kNodeId, kTitle, kUrl, kBookmarkBarId,
                                /*server_tag=*/std::string()},
                               kRandomPosition, /*response_version=*/0));

  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model()->bookmark_bar_node();

  // Ensures that OnInitialUpdateReceived will be called.
  ASSERT_FALSE(processor()->IsTrackingMetadata());
  ASSERT_TRUE(bookmark_bar->children().empty());
  ASSERT_TRUE(processor()->IsConnectedForTest());

  ASSERT_FALSE(error_reported);
  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);
  EXPECT_TRUE(error_reported);
  EXPECT_FALSE(processor()->IsConnectedForTest());
  // Tracker should remain null and bookmark model unchanged.
  EXPECT_FALSE(processor()->IsTrackingMetadata());
  EXPECT_TRUE(bookmark_bar->children().empty());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldSaveRemoteUpdatesCountExceedingLimitResultDuringInitialMerge) {
  // Set a limit of 3 bookmarks, i.e. limit it to the 3 permanent nodes.
  processor()->SetMaxBookmarksTillSyncEnabledForTest(3);

  SimulateModelReadyToSyncWithoutLocalMetadata();
  SimulateOnSyncStarting();
  SimulateConnectSync();

  const syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates =
      CreateUpdateResponseDataListForPermanentNodes();

  // Entry for the root folder. The server may or may not send a root node, but
  // the current implementation still handles it.
  updates.push_back(CreateUpdateResponseData(
      {kBookmarksRootId, std::string(), std::string(), std::string(),
       syncer::DataTypeToProtocolRootTag(syncer::BOOKMARKS)},
      kRandomPosition, /*response_version=*/0));

  // Add update for another node under the bookmarks bar.
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";

  updates.push_back(
      CreateUpdateResponseData({kNodeId, kTitle, kUrl, kBookmarkBarId,
                                /*server_tag=*/std::string()},
                               kRandomPosition, /*response_version=*/0));

  // Ensures that OnInitialUpdateReceived will be called.
  ASSERT_FALSE(processor()->IsTrackingMetadata());
  ASSERT_TRUE(processor()->IsConnectedForTest());

  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);

  ASSERT_FALSE(processor()->IsTrackingMetadata());
  ASSERT_FALSE(processor()->IsConnectedForTest());

  // Metadata should contain the relevant field.
  sync_pb::BookmarkModelMetadata model_metadata;
  std::string metadata_str = processor()->EncodeSyncMetadata();
  ASSERT_FALSE(metadata_str.empty());
  ASSERT_TRUE(model_metadata.ParseFromString(metadata_str));
  EXPECT_TRUE(
      model_metadata.last_initial_merge_remote_updates_exceeded_limit());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldReportErrorIfRemoteBookmarksCountExceededLimitOnLastTry) {
  // Set a limit of 3 bookmarks, i.e. limit it to the 3 permanent nodes.
  processor()->SetMaxBookmarksTillSyncEnabledForTest(3);

  // Expect failure when initial update of count 4 is received.
  bool error_reported = false;
  EXPECT_CALL(*error_handler(), Run).Times(2).WillRepeatedly([&]() {
    error_reported = true;
  });

  SimulateModelReadyToSyncWithoutLocalMetadata();
  SimulateOnSyncStarting();
  SimulateConnectSync();

  const syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates =
      CreateUpdateResponseDataListForPermanentNodes();

  // Entry for the root folder. The server may or may not send a root node, but
  // the current implementation still handles it.
  updates.push_back(CreateUpdateResponseData(
      {kBookmarksRootId, std::string(), std::string(), std::string(),
       syncer::DataTypeToProtocolRootTag(syncer::BOOKMARKS)},
      kRandomPosition, /*response_version=*/0));

  // Add update for another node under the bookmarks bar.
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";

  updates.push_back(
      CreateUpdateResponseData({kNodeId, kTitle, kUrl, kBookmarkBarId,
                                /*server_tag=*/std::string()},
                               kRandomPosition, /*response_version=*/0));

  // Ensures that OnInitialUpdateReceived will be called.
  ASSERT_FALSE(processor()->IsTrackingMetadata());

  ASSERT_FALSE(error_reported);
  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);
  ASSERT_TRUE(error_reported);
  ASSERT_FALSE(processor()->IsTrackingMetadata());
  ASSERT_FALSE(processor()->IsConnectedForTest());

  sync_pb::BookmarkModelMetadata model_metadata;
  std::string metadata_str = processor()->EncodeSyncMetadata();
  ASSERT_FALSE(metadata_str.empty());
  ASSERT_TRUE(model_metadata.ParseFromString(metadata_str));
  ASSERT_TRUE(
      model_metadata.last_initial_merge_remote_updates_exceeded_limit());

  ResetDataTypeProcessor();
  // Expect failure.
  error_reported = false;
  processor()->ModelReadyToSync(metadata_str, schedule_save_closure()->Get(),
                                bookmark_model());
  SimulateOnSyncStarting();

  EXPECT_TRUE(error_reported);
  // Tracker would not be initialised.
  EXPECT_FALSE(processor()->IsTrackingMetadata());

  // Metadata remains unchanged on this failure.
  metadata_str = processor()->EncodeSyncMetadata();
  ASSERT_FALSE(metadata_str.empty());
  ASSERT_TRUE(model_metadata.ParseFromString(metadata_str));
  EXPECT_TRUE(
      model_metadata.last_initial_merge_remote_updates_exceeded_limit());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldPersistRemoteBookmarksCountExceedingLimitAcrossBrowserRestarts) {
  // Set a limit of 3 bookmarks, i.e. limit it to the 3 permanent nodes.
  processor()->SetMaxBookmarksTillSyncEnabledForTest(3);

  // Expect failure when initial update of count 4 is received.
  bool error_reported = false;
  EXPECT_CALL(*error_handler(), Run).Times(3).WillRepeatedly([&]() {
    error_reported = true;
  });

  SimulateModelReadyToSyncWithoutLocalMetadata();
  SimulateOnSyncStarting();

  const syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates =
      CreateUpdateResponseDataListForPermanentNodes();

  // Entry for the root folder. The server may or may not send a root node, but
  // the current implementation still handles it.
  updates.push_back(CreateUpdateResponseData(
      {kBookmarksRootId, std::string(), std::string(), std::string(),
       syncer::DataTypeToProtocolRootTag(syncer::BOOKMARKS)},
      kRandomPosition, /*response_version=*/0));

  // Add update for another node under the bookmarks bar.
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";

  updates.push_back(
      CreateUpdateResponseData({kNodeId, kTitle, kUrl, kBookmarkBarId,
                                /*server_tag=*/std::string()},
                               kRandomPosition, /*response_version=*/0));

  // Ensures that OnInitialUpdateReceived will be called.
  ASSERT_FALSE(processor()->IsTrackingMetadata());

  ASSERT_FALSE(error_reported);
  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);
  ASSERT_TRUE(error_reported);

  ASSERT_FALSE(processor()->IsTrackingMetadata());

  sync_pb::BookmarkModelMetadata model_metadata;
  std::string metadata_str = processor()->EncodeSyncMetadata();
  ASSERT_FALSE(metadata_str.empty());
  ASSERT_TRUE(model_metadata.ParseFromString(metadata_str));
  ASSERT_TRUE(
      model_metadata.last_initial_merge_remote_updates_exceeded_limit());

  // Simulate browser restart.
  ResetDataTypeProcessor();
  // Expect failure.
  error_reported = false;
  processor()->ModelReadyToSync(metadata_str, schedule_save_closure()->Get(),
                                bookmark_model());
  SimulateOnSyncStarting();
  EXPECT_TRUE(error_reported);
  // Tracker would not be initialised.
  EXPECT_FALSE(processor()->IsTrackingMetadata());

  // Metadata remains unchanged on this failure.
  metadata_str = processor()->EncodeSyncMetadata();
  ASSERT_FALSE(metadata_str.empty());
  ASSERT_TRUE(model_metadata.ParseFromString(metadata_str));
  ASSERT_TRUE(
      model_metadata.last_initial_merge_remote_updates_exceeded_limit());

  // Simulate browser restart again.
  ResetDataTypeProcessor();
  // Expect failure.
  error_reported = false;
  processor()->ModelReadyToSync(metadata_str, schedule_save_closure()->Get(),
                                bookmark_model());
  SimulateOnSyncStarting();
  EXPECT_TRUE(error_reported);
  // Tracker would not be initialised.
  EXPECT_FALSE(processor()->IsTrackingMetadata());

  // Metadata remains unchanged on this failure as well.
  metadata_str = processor()->EncodeSyncMetadata();
  ASSERT_FALSE(metadata_str.empty());
  ASSERT_TRUE(model_metadata.ParseFromString(metadata_str));
  EXPECT_TRUE(
      model_metadata.last_initial_merge_remote_updates_exceeded_limit());
}

TEST_F(BookmarkDataTypeProcessorTest, ShouldClearMetadataIfStopped) {
  SimulateModelReadyToSyncWithInitialSyncDone();
  processor()->OnSyncStopping(syncer::KEEP_METADATA);
  ASSERT_TRUE(processor()->IsTrackingMetadata());

  base::HistogramTester histogram_tester;

  // Expect saving empty metadata upon call to ClearMetadataIfStopped().
  EXPECT_CALL(*schedule_save_closure(), Run);

  processor()->ClearMetadataIfStopped();
  // Should clear the tracker even if already stopped.
  EXPECT_FALSE(processor()->IsTrackingMetadata());
  // Expect an entry to the histogram.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 1);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 1);
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldClearMetadataIfStoppedUponModelReadyToSync) {
  ASSERT_FALSE(processor()->IsTrackingMetadata());

  base::HistogramTester histogram_tester;

  // Expect no call to save metadata before ModelReadyToSync().
  EXPECT_CALL(*schedule_save_closure(), Run).Times(0);
  // Call ClearMetadataIfStopped() before ModelReadyToSync(). This should set
  // the flag for a pending clearing of metadata.
  processor()->ClearMetadataIfStopped();
  // Nothing recorded to the histograms yet.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.DelayedClear", 0);

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model());
  // Expect saving empty metadata from ModelReadyToSync() while processing the
  // pending clearing of metadata.
  EXPECT_CALL(*schedule_save_closure(), Run);
  // ModelReadyToSync() should take into account the pending metadata clearing
  // flag and clear the metadata.
  processor()->ModelReadyToSync(model_metadata.SerializeAsString(),
                                schedule_save_closure()->Get(),
                                bookmark_model());
  // Tracker should have not been set.
  EXPECT_FALSE(processor()->IsTrackingMetadata());
  // Expect recording of the delayed clear.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 1);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.DelayedClear", 1);
}

TEST_F(BookmarkDataTypeProcessorTest, ShouldNotClearMetadataIfNotStopped) {
  // Initialize and start the processor with some metadata.
  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateOnSyncStarting();
  ASSERT_TRUE(processor()->IsTrackingMetadata());

  base::HistogramTester histogram_tester;

  processor()->ClearMetadataIfStopped();

  // Should NOT have cleared the metadata since the processor is not stopped.
  EXPECT_TRUE(processor()->IsTrackingMetadata());
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 0);
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldNotClearMetadataIfStoppedIfPreviouslyStoppedWithClearMetadata) {
  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateOnSyncStarting();
  processor()->OnSyncStopping(syncer::CLEAR_METADATA);
  ASSERT_FALSE(processor()->IsTrackingMetadata());

  // Expect no call to save metadata upon ClearMetadataIfStopped().
  EXPECT_CALL(*schedule_save_closure(), Run).Times(0);

  base::HistogramTester histogram_tester;

  processor()->ClearMetadataIfStopped();
  // Expect no entry to the histogram.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 0);
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldWipeBookmarksRepeatedlyIfStoppedWithClearMetadata) {
  ResetDataTypeProcessor(syncer::WipeModelUponSyncDisabledBehavior::kAlways);

  const GURL kUrl("http://www.example.com");
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"foo", kUrl);
  const bookmarks::BookmarkNode* folder = bookmark_model()->AddFolder(
      bookmark_model()->mobile_node(), /*index=*/0, u"folder");
  bookmark_model()->AddURL(folder, /*index=*/0, u"bar", kUrl);

  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateOnSyncStarting();
  ASSERT_FALSE(bookmark_model()
                   ->underlying_model()
                   ->HasNoUserCreatedBookmarksOrFolders());

  processor()->OnSyncStopping(syncer::CLEAR_METADATA);
  EXPECT_TRUE(bookmark_model()
                  ->underlying_model()
                  ->HasNoUserCreatedBookmarksOrFolders());

  // If the process is repeated, the result should be the same (bookmarks
  // deleted once again). This requires doing initial sync again.
  SimulateOnSyncStarting();
  processor()->OnUpdateReceived(CreateDataTypeState(),
                                CreateUpdateResponseDataListForPermanentNodes(),
                                /*gc_directive=*/std::nullopt);
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"foo", kUrl);
  ASSERT_TRUE(processor()->IsTrackingMetadata());
  ASSERT_FALSE(bookmark_model()
                   ->underlying_model()
                   ->HasNoUserCreatedBookmarksOrFolders());

  processor()->OnSyncStopping(syncer::CLEAR_METADATA);
  EXPECT_TRUE(bookmark_model()
                  ->underlying_model()
                  ->HasNoUserCreatedBookmarksOrFolders());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldWipeBookmarksOnceIfStoppedWithClearMetadata) {
  ResetDataTypeProcessor(
      syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata);

  const GURL kUrl("http://www.example.com");
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"foo", kUrl);
  const bookmarks::BookmarkNode* folder = bookmark_model()->AddFolder(
      bookmark_model()->mobile_node(), /*index=*/0, u"folder");
  bookmark_model()->AddURL(folder, /*index=*/0, u"bar", kUrl);

  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateOnSyncStarting();
  ASSERT_FALSE(bookmark_model()
                   ->underlying_model()
                   ->HasNoUserCreatedBookmarksOrFolders());

  processor()->OnSyncStopping(syncer::CLEAR_METADATA);
  EXPECT_TRUE(bookmark_model()
                  ->underlying_model()
                  ->HasNoUserCreatedBookmarksOrFolders());

  // If the process is repeated, the deletion should not happen.
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"foo", kUrl);
  SimulateOnSyncStarting();
  ASSERT_FALSE(bookmark_model()
                   ->underlying_model()
                   ->HasNoUserCreatedBookmarksOrFolders());
  processor()->OnSyncStopping(syncer::CLEAR_METADATA);
  EXPECT_FALSE(bookmark_model()
                   ->underlying_model()
                   ->HasNoUserCreatedBookmarksOrFolders());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldNotWipeBookmarksIfStoppedWithClearMetadataWithoutInitialSyncDone) {
  ResetDataTypeProcessor(
      syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata);

  const GURL kUrl("http://www.example.com");
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"foo", kUrl);
  const bookmarks::BookmarkNode* folder = bookmark_model()->AddFolder(
      bookmark_model()->mobile_node(), /*index=*/0, u"folder");
  bookmark_model()->AddURL(folder, /*index=*/0, u"bar", kUrl);

  SimulateModelReadyToSyncWithoutLocalMetadata();
  ASSERT_FALSE(bookmark_model()
                   ->underlying_model()
                   ->HasNoUserCreatedBookmarksOrFolders());

  processor()->OnSyncStopping(syncer::CLEAR_METADATA);
  EXPECT_FALSE(bookmark_model()
                   ->underlying_model()
                   ->HasNoUserCreatedBookmarksOrFolders());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldNotWipeBookmarksIfStoppedWithClearMetadataIfInitialSyncDoneLater) {
  ResetDataTypeProcessor(
      syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata);

  const GURL kUrl("http://www.example.com");
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"foo", kUrl);
  const bookmarks::BookmarkNode* folder = bookmark_model()->AddFolder(
      bookmark_model()->mobile_node(), /*index=*/0, u"folder");
  bookmark_model()->AddURL(folder, /*index=*/0, u"bar", kUrl);

  SimulateModelReadyToSyncWithoutLocalMetadata();
  ASSERT_FALSE(bookmark_model()
                   ->underlying_model()
                   ->HasNoUserCreatedBookmarksOrFolders());

  // In most cases, because of how SyncServiceImpl behaves, OnSyncStopping()
  // would be called upon startup. To be extra safe, BookmarkDataTypeProcessor
  // does not rely on this assumption, so this test verifies that bookmarks
  // shouldn't be cleared if sync was initially off (upon startup), then turned
  // on, then turned off again.
  SimulateOnSyncStarting();
  processor()->OnUpdateReceived(CreateDataTypeState(),
                                CreateUpdateResponseDataListForPermanentNodes(),
                                /*gc_directive=*/std::nullopt);
  EXPECT_TRUE(processor()->IsTrackingMetadata());

  processor()->OnSyncStopping(syncer::CLEAR_METADATA);
  EXPECT_FALSE(bookmark_model()
                   ->underlying_model()
                   ->HasNoUserCreatedBookmarksOrFolders());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldNotWipeBookmarksIfStoppedWithKeepMetadata) {
  ResetDataTypeProcessor(syncer::WipeModelUponSyncDisabledBehavior::kAlways);

  const GURL kUrl("http://www.example.com");
  const bookmarks::BookmarkNode* node = bookmark_model()->AddURL(
      bookmark_model()->mobile_node(), /*index=*/0, u"foo", kUrl);

  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateOnSyncStarting();

  processor()->OnSyncStopping(syncer::KEEP_METADATA);
  EXPECT_THAT(bookmark_model()->mobile_node()->children(),
              ElementsAre(Pointer(Eq(node))));
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldNotClearMetadataIfStoppedWithoutMetadataInitially) {
  SimulateModelReadyToSyncWithoutLocalMetadata();
  ASSERT_FALSE(processor()->IsTrackingMetadata());

  base::HistogramTester histogram_tester;

  // Call ClearMetadataIfStopped() without a prior call to OnSyncStopping().
  processor()->ClearMetadataIfStopped();

  // Expect no call to save metadata upon ClearMetadataIfStopped().
  EXPECT_CALL(*schedule_save_closure(), Run).Times(0);
  // Expect no entry to the histogram.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 0);
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldNotClearMetadataIfStoppedUponModelReadyToSyncWithoutMetadata) {
  base::HistogramTester histogram_tester;

  // Expect no call to save metadata.
  EXPECT_CALL(*schedule_save_closure(), Run).Times(0);
  // Call ClearMetadataIfStopped() before ModelReadyToSync(). This should set
  // the flag for a pending clearing of metadata.
  processor()->ClearMetadataIfStopped();

  SimulateModelReadyToSyncWithoutLocalMetadata();
  ASSERT_FALSE(processor()->IsTrackingMetadata());

  // Nothing recorded to the histograms.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.DelayedClear", 0);
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldWipeBookmarksIfMetadataClearedWhileStopped) {
  ResetDataTypeProcessor(syncer::WipeModelUponSyncDisabledBehavior::kAlways);
  SimulateModelReadyToSyncWithInitialSyncDone();
  processor()->OnSyncStopping(syncer::KEEP_METADATA);

  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"foo", GURL("http://www.example.com"));

  ASSERT_TRUE(processor()->IsTrackingMetadata());
  ASSERT_FALSE(bookmark_model()
                   ->underlying_model()
                   ->HasNoUserCreatedBookmarksOrFolders());

  base::HistogramTester histogram_tester;

  // Expect saving empty metadata upon call to ClearMetadataIfStopped().
  EXPECT_CALL(*schedule_save_closure(), Run);

  processor()->ClearMetadataIfStopped();
  // Should clear the tracker even if already stopped.
  EXPECT_FALSE(processor()->IsTrackingMetadata());
  // Expect an entry to the histogram.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 1);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 1);

  // Local bookmarks should have been deleted.
  EXPECT_TRUE(bookmark_model()
                  ->underlying_model()
                  ->HasNoUserCreatedBookmarksOrFolders());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldWipeBookmarksIfMetadataClearedWhileStoppedUponModelReadyToSync) {
  ResetDataTypeProcessor(syncer::WipeModelUponSyncDisabledBehavior::kAlways);

  base::HistogramTester histogram_tester;

  // Expect no call to save metadata before ModelReadyToSync().
  EXPECT_CALL(*schedule_save_closure(), Run).Times(0);
  // Call ClearMetadataIfStopped() before ModelReadyToSync(). This should set
  // the flag for a pending clearing of metadata.
  processor()->ClearMetadataIfStopped();
  // Nothing recorded to the histograms yet.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.DelayedClear", 0);

  // Mimic some bookmarks being loaded as part of startup.
  const bookmarks::BookmarkNode* bookmarknode = bookmark_model()->AddURL(
      bookmark_model()->bookmark_bar_node(), /*index=*/0, u"foo",
      GURL("http://www.example.com"));

  ASSERT_FALSE(processor()->IsTrackingMetadata());
  ASSERT_FALSE(bookmark_model()
                   ->underlying_model()
                   ->HasNoUserCreatedBookmarksOrFolders());

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model());
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmarknode, "node_id1");

  // Expect saving empty metadata from ModelReadyToSync() while processing the
  // pending clearing of metadata.
  EXPECT_CALL(*schedule_save_closure(), Run);
  // ModelReadyToSync() should take into account the pending metadata clearing
  // flag and clear the metadata.
  processor()->ModelReadyToSync(model_metadata.SerializeAsString(),
                                schedule_save_closure()->Get(),
                                bookmark_model());
  // Tracker should have not been set.
  EXPECT_FALSE(processor()->IsTrackingMetadata());
  // Expect recording of the delayed clear.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 1);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.DelayedClear", 1);

  // Local bookmarks should have been deleted.
  EXPECT_TRUE(bookmark_model()
                  ->underlying_model()
                  ->HasNoUserCreatedBookmarksOrFolders());
}

TEST_F(BookmarkDataTypeProcessorTest, ShouldWipeBookmarksIfCacheGuidMismatch) {
  ResetDataTypeProcessor(syncer::WipeModelUponSyncDisabledBehavior::kAlways);
  SimulateModelReadyToSyncWithInitialSyncDone();
  ASSERT_TRUE(processor()->IsTrackingMetadata());
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"foo", GURL("http://www.example.com"));

  ASSERT_FALSE(bookmark_model()
                   ->underlying_model()
                   ->HasNoUserCreatedBookmarksOrFolders());

  SimulateOnSyncStarting("unexpected_cache_guid");

  EXPECT_FALSE(processor()->IsTrackingMetadata());

  // Local bookmarks should have been deleted.
  EXPECT_TRUE(bookmark_model()
                  ->underlying_model()
                  ->HasNoUserCreatedBookmarksOrFolders());
}

TEST_F(BookmarkDataTypeProcessorTest,
       ShouldRecordNumUnsyncedEntitiesOnModelReady) {
  {
    base::HistogramTester histogram_tester;
    SimulateModelReadyToSyncWithInitialSyncDone();
    // There are no local unsynced entities.
    histogram_tester.ExpectUniqueSample(
        "Sync.DataTypeNumUnsyncedEntitiesOnModelReady.BOOKMARK", /*sample=*/0,
        /*expected_bucket_count=*/1);
  }

  // Add a bookmark, but don't sync it.
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(),
                           /*index=*/0, u"Title", GURL("http://www.url.com"));
  sync_pb::BookmarkModelMetadata model_metadata =
      processor()->GetTrackerForTest()->BuildBookmarkModelMetadata();

  // Simulate the browser restart.
  ResetDataTypeProcessor();

  {
    base::HistogramTester histogram_tester;

    std::string metadata_str;
    model_metadata.SerializeToString(&metadata_str);
    processor()->ModelReadyToSync(metadata_str, base::DoNothing(),
                                  bookmark_model());

    // Bookmark added above is unsynced.
    histogram_tester.ExpectUniqueSample(
        "Sync.DataTypeNumUnsyncedEntitiesOnModelReady.BOOKMARK", /*sample=*/1,
        /*expected_bucket_count=*/1);
  }
}

}  // namespace

}  // namespace sync_bookmarks
