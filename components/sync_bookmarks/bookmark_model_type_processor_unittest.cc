// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_type_processor.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/protocol/bookmark_model_metadata.pb.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync_bookmarks/switches.h"
#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"
#include "components/undo/bookmark_undo_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_bookmarks {

namespace {

using base::ASCIIToUTF16;
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::NiceMock;
using testing::NotNull;
using testing::UnorderedElementsAre;

const char kBookmarkBarTag[] = "bookmark_bar";
const char kOtherBookmarksTag[] = "other_bookmarks";
const char kMobileBookmarksTag[] = "synced_bookmarks";
const char kBookmarkBarId[] = "bookmark_bar_id";
const char kOtherBookmarksId[] = "other_bookmarks_id";
const char kMobileBookmarksId[] = "mobile_bookmarks_id";
const char kBookmarksRootId[] = "root_id";
const char kCacheGuid[] = "generated_id";

struct BookmarkInfo {
  std::string server_id;
  std::string title;
  std::string url;  // empty for folders.
  std::string parent_id;
  std::string server_tag;
};

MATCHER_P(CommitRequestDataMatchesGuid, guid, "") {
  const syncer::CommitRequestData* data = arg.get();
  return data != nullptr && data->entity != nullptr &&
         data->entity->specifics.bookmark().guid() == guid.AsLowercaseString();
}

MATCHER_P(TrackedEntityCorrespondsToBookmarkNode, bookmark_node, "") {
  const SyncedBookmarkTrackerEntity* entity = arg;
  return entity->bookmark_node() == bookmark_node;
}

syncer::UpdateResponseData CreateUpdateResponseData(
    const BookmarkInfo& bookmark_info,
    const syncer::UniquePosition& unique_position,
    int response_version,
    const base::GUID& guid) {
  syncer::EntityData data;
  data.id = bookmark_info.server_id;
  data.legacy_parent_id = bookmark_info.parent_id;
  data.server_defined_unique_tag = bookmark_info.server_tag;
  data.originator_client_item_id = guid.AsLowercaseString();

  sync_pb::BookmarkSpecifics* bookmark_specifics =
      data.specifics.mutable_bookmark();
  bookmark_specifics->set_guid(guid.AsLowercaseString());
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
                                  base::GUID::GenerateRandomV4());
}

sync_pb::ModelTypeState CreateDummyModelTypeState() {
  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_cache_guid(kCacheGuid);
  model_type_state.set_initial_sync_done(true);
  return model_type_state;
}

// |node| must not be nullptr.
sync_pb::BookmarkMetadata CreateNodeMetadata(
    const bookmarks::BookmarkNode* node,
    const std::string& server_id) {
  sync_pb::BookmarkMetadata bookmark_metadata;
  bookmark_metadata.set_id(node->id());
  bookmark_metadata.mutable_metadata()->set_server_id(server_id);
  bookmark_metadata.mutable_metadata()->set_client_tag_hash(
      syncer::ClientTagHash::FromUnhashed(syncer::BOOKMARKS,
                                          node->guid().AsLowercaseString())
          .value());
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
    const bookmarks::BookmarkModel* bookmark_model) {
  sync_pb::BookmarkModelMetadata model_metadata;
  *model_metadata.mutable_model_type_state() = CreateDummyModelTypeState();

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

void AssertState(const BookmarkModelTypeProcessor* processor,
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

// TODO(crbug.com/516866): Replace with a simpler implementation (e.g. by
// simulating loading from metadata) instead of receiving remote updates.
// Inititalizes the processor with the bookmarks in |bookmarks|.
void InitWithSyncedBookmarks(const std::vector<BookmarkInfo>& bookmarks,
                             BookmarkModelTypeProcessor* processor) {
  syncer::UpdateResponseDataList updates;
  syncer::UniquePosition pos = syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());
  // Add update for the permanent folders "Bookmarks bar", "Other Bookmarks" and
  // "Mobile Bookmarks".
  updates.push_back(
      CreateUpdateResponseData({kBookmarkBarId, std::string(), std::string(),
                                kBookmarksRootId, kBookmarkBarTag},
                               pos, /*response_version=*/0));
  updates.push_back(
      CreateUpdateResponseData({kOtherBookmarksId, std::string(), std::string(),
                                kBookmarksRootId, kOtherBookmarksTag},
                               pos, /*response_version=*/0));
  updates.push_back(CreateUpdateResponseData(
      {kMobileBookmarksId, std::string(), std::string(), kBookmarksRootId,
       kMobileBookmarksTag},
      pos, /*response_version=*/0));
  for (BookmarkInfo bookmark : bookmarks) {
    pos = syncer::UniquePosition::After(pos,
                                        syncer::UniquePosition::RandomSuffix());
    updates.push_back(
        CreateUpdateResponseData(bookmark, pos, /*response_version=*/0));
  }
  processor->OnUpdateReceived(CreateDummyModelTypeState(), std::move(updates));
  AssertState(processor, bookmarks);
}

class MockCommitQueue : public syncer::CommitQueue {
 public:
  MOCK_METHOD(void, NudgeForCommit, (), (override));
};

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

class BookmarkModelTypeProcessorTest : public testing::Test {
 public:
  BookmarkModelTypeProcessorTest()
      : processor_(std::make_unique<BookmarkModelTypeProcessor>(
            &bookmark_undo_service_)),
        bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()) {
    // TODO(crbug.com/516866): This class assumes model is loaded and sync has
    // started before running tests. We should test other variations (i.e. model
    // isn't loaded yet and/or sync didn't start yet).
    processor_->SetFaviconService(&favicon_service_);
  }

  void SimulateModelReadyToSync() {
    processor_->ModelReadyToSync(
        /*metadata_str=*/std::string(), schedule_save_closure_.Get(),
        bookmark_model_.get());
  }

  void SimulateOnSyncStarting() {
    syncer::DataTypeActivationRequest request;
    request.cache_guid = kCacheGuid;
    processor_->OnSyncStarting(request, base::DoNothing());
  }

  void SimulateConnectSync() {
    processor_->ConnectSync(
        std::make_unique<ProxyCommitQueue>(&mock_commit_queue_));
  }

  // Simulate browser restart.
  void ResetModelTypeProcessor() {
    processor_ =
        std::make_unique<BookmarkModelTypeProcessor>(&bookmark_undo_service_);
    processor_->SetFaviconService(&favicon_service_);
  }

  void DestroyBookmarkModel() { bookmark_model_.reset(); }

  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }
  bookmarks::TestBookmarkClient* bookmark_client() {
    return static_cast<bookmarks::TestBookmarkClient*>(
        bookmark_model_->client());
  }
  BookmarkUndoService* bookmark_undo_service() {
    return &bookmark_undo_service_;
  }
  favicon::FaviconService* favicon_service() { return &favicon_service_; }
  MockCommitQueue* mock_commit_queue() { return &mock_commit_queue_; }
  BookmarkModelTypeProcessor* processor() { return processor_.get(); }
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

 private:
  base::test::TaskEnvironment task_environment_;
  NiceMock<base::MockCallback<base::RepeatingClosure>> schedule_save_closure_;
  BookmarkUndoService bookmark_undo_service_;
  NiceMock<favicon::MockFaviconService> favicon_service_;
  NiceMock<MockCommitQueue> mock_commit_queue_;
  std::unique_ptr<BookmarkModelTypeProcessor> processor_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

TEST_F(BookmarkModelTypeProcessorTest, ShouldDoInitialMerge) {
  const syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  SimulateModelReadyToSync();
  SimulateOnSyncStarting();

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

  ASSERT_THAT(processor()->GetTrackerForTest(), IsNull());

  base::HistogramTester histogram_tester;
  processor()->OnUpdateReceived(CreateDummyModelTypeState(),
                                std::move(updates));
  EXPECT_THAT(processor()->GetTrackerForTest(), NotNull());

  histogram_tester.ExpectUniqueSample(
      "Sync.ModelTypeInitialUpdateReceived",
      /*sample=*/syncer::ModelTypeHistogramValue(syncer::BOOKMARKS),
      /*expected_count=*/3);
}

TEST_F(BookmarkModelTypeProcessorTest, ShouldUpdateModelAfterRemoteCreation) {
  SimulateModelReadyToSync();
  SimulateOnSyncStarting();
  InitWithSyncedBookmarks(/*bookmarks=*/{}, processor());

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

  processor()->OnUpdateReceived(CreateDummyModelTypeState(),
                                std::move(updates));

  ASSERT_THAT(bookmark_bar->children().front().get(), NotNull());
  EXPECT_THAT(bookmark_bar->children().front()->GetTitle(),
              Eq(ASCIIToUTF16(kTitle)));
  EXPECT_THAT(bookmark_bar->children().front()->url(), Eq(GURL(kUrl)));
}

TEST_F(BookmarkModelTypeProcessorTest, ShouldUpdateModelAfterRemoteUpdate) {
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";
  const syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  std::vector<BookmarkInfo> bookmarks = {
      {kNodeId, kTitle, kUrl, kBookmarkBarId, /*server_tag=*/std::string()}};

  SimulateModelReadyToSync();
  SimulateOnSyncStarting();
  InitWithSyncedBookmarks(bookmarks, processor());

  // Make sure original bookmark exists.
  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node =
      bookmark_bar->children().front().get();
  ASSERT_THAT(bookmark_node, NotNull());
  ASSERT_THAT(bookmark_node->GetTitle(), Eq(ASCIIToUTF16(kTitle)));
  ASSERT_THAT(bookmark_node->url(), Eq(GURL(kUrl)));

  // Process an update for the same bookmark.
  const std::string kNewTitle = "new-title";
  const std::string kNewUrl = "http://www.new-url.com";
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      {kNodeId, kNewTitle, kNewUrl, kBookmarkBarId,
       /*server_tag=*/std::string()},
      kRandomPosition, /*response_version=*/1, bookmark_node->guid()));

  base::HistogramTester histogram_tester;
  processor()->OnUpdateReceived(CreateDummyModelTypeState(),
                                std::move(updates));

  // Check if the bookmark has been updated properly.
  EXPECT_THAT(bookmark_bar->children().front().get(), Eq(bookmark_node));
  EXPECT_THAT(bookmark_node->GetTitle(), Eq(ASCIIToUTF16(kNewTitle)));
  EXPECT_THAT(bookmark_node->url(), Eq(GURL(kNewUrl)));

  histogram_tester.ExpectUniqueSample(
      "Sync.ModelTypeIncrementalUpdateReceived",
      /*sample=*/syncer::ModelTypeHistogramValue(syncer::BOOKMARKS),
      /*expected_count=*/1);
}

TEST_F(
    BookmarkModelTypeProcessorTest,
    ShouldScheduleSaveAfterRemoteUpdateWithOnlyMetadataChangeAndReflections) {
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";
  const syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  std::vector<BookmarkInfo> bookmarks = {
      {kNodeId, kTitle, kUrl, kBookmarkBarId, /*server_tag=*/std::string()}};

  SimulateModelReadyToSync();
  SimulateOnSyncStarting();
  InitWithSyncedBookmarks(bookmarks, processor());

  // Make sure original bookmark exists.
  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node =
      bookmark_bar->children().front().get();
  ASSERT_THAT(bookmark_node, NotNull());

  // Process an update for the same bookmark with the same data.
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      {kNodeId, kTitle, kUrl, kBookmarkBarId,
       /*server_tag=*/std::string()},
      kRandomPosition, /*response_version=*/1, bookmark_node->guid()));
  updates[0].response_version++;

  EXPECT_CALL(*schedule_save_closure(), Run());
  processor()->OnUpdateReceived(CreateDummyModelTypeState(),
                                std::move(updates));
}

TEST_F(BookmarkModelTypeProcessorTest, ShouldDecodeSyncMetadata) {
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

  SimulateModelReadyToSync();
  SimulateOnSyncStarting();

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model());

  // Add an entry for the bookmark node.
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmarknode, kNodeId);

  // Create a new processor and init it with the metadata str.
  BookmarkModelTypeProcessor new_processor(bookmark_undo_service());

  std::string metadata_str;
  model_metadata.SerializeToString(&metadata_str);
  new_processor.ModelReadyToSync(metadata_str, base::DoNothing(),
                                 bookmark_model());

  AssertState(&new_processor, bookmarks);
}

TEST_F(BookmarkModelTypeProcessorTest, ShouldDecodeEncodedSyncMetadata) {
  const std::string kNodeId1 = "node_id1";
  const std::string kTitle1 = "title1";
  const std::string kUrl1 = "http://www.url1.com";

  const std::string kNodeId2 = "node_id2";
  const std::string kTitle2 = "title2";
  const std::string kUrl2 = "http://www.url2.com";

  std::vector<BookmarkInfo> bookmarks = {
      {kNodeId1, kTitle1, kUrl1, kBookmarkBarId, /*server_tag=*/std::string()},
      {kNodeId2, kTitle2, kUrl2, kBookmarkBarId,
       /*server_tag=*/std::string()}};
  SimulateModelReadyToSync();
  SimulateOnSyncStarting();
  InitWithSyncedBookmarks(bookmarks, processor());

  std::string metadata_str = processor()->EncodeSyncMetadata();
  // TODO(crbug.com/516866): Remove this after initial sync done is properly set
  // within the processor.
  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.ParseFromString(metadata_str);
  model_metadata.mutable_model_type_state()->set_initial_sync_done(true);

  // Create a new processor and init it with the same metadata str.
  BookmarkModelTypeProcessor new_processor(bookmark_undo_service());
  model_metadata.SerializeToString(&metadata_str);
  new_processor.ModelReadyToSync(metadata_str, base::DoNothing(),
                                 bookmark_model());

  AssertState(&new_processor, bookmarks);

  // Make sure shutdown doesn't crash.
  DestroyBookmarkModel();
  EXPECT_FALSE(processor()->IsConnectedForTest());
  EXPECT_FALSE(new_processor.IsConnectedForTest());
  EXPECT_THAT(processor()->GetTrackerForTest(), NotNull());
  EXPECT_THAT(new_processor.GetTrackerForTest(), NotNull());
}

TEST_F(BookmarkModelTypeProcessorTest, ShouldDecodeEmptyMetadata) {
  // No save should be scheduled.
  EXPECT_CALL(*schedule_save_closure(), Run()).Times(0);
  SimulateModelReadyToSync();
  EXPECT_THAT(processor()->GetTrackerForTest(), IsNull());
}

TEST_F(BookmarkModelTypeProcessorTest,
       ShouldIgnoreNonEmptyMetadataWhileSyncNotDone) {
  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.mutable_model_type_state()->set_initial_sync_done(false);
  // Add entries to the metadata.
  sync_pb::BookmarkMetadata* bookmark_metadata =
      model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(bookmark_model()->bookmark_bar_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id(kBookmarkBarId);

  // Create a new processor and init it with the metadata str.
  BookmarkModelTypeProcessor new_processor(bookmark_undo_service());

  // A save should be scheduled.
  NiceMock<base::MockCallback<base::RepeatingClosure>>
      new_schedule_save_closure;
  EXPECT_CALL(new_schedule_save_closure, Run());

  std::string metadata_str;
  model_metadata.SerializeToString(&metadata_str);
  new_processor.ModelReadyToSync(metadata_str, new_schedule_save_closure.Get(),
                                 bookmark_model());
  // Metadata are corrupted, so no tracker should have been created.
  EXPECT_THAT(new_processor.GetTrackerForTest(), IsNull());
}

TEST_F(BookmarkModelTypeProcessorTest,
       ShouldIgnoreMetadataNotMatchingTheModel) {
  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.mutable_model_type_state()->set_initial_sync_done(true);
  // Add entries for only the bookmark bar. However, the TestBookmarkClient will
  // create all the 3 permanent nodes.
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmark_model()->bookmark_bar_node(),
                         /*server_id=*/kBookmarkBarId);

  // Create a new processor and init it with the metadata str.
  BookmarkModelTypeProcessor new_processor(bookmark_undo_service());

  // A save should be scheduled.
  NiceMock<base::MockCallback<base::RepeatingClosure>>
      new_schedule_save_closure;
  EXPECT_CALL(new_schedule_save_closure, Run());

  std::string metadata_str;
  model_metadata.SerializeToString(&metadata_str);
  new_processor.ModelReadyToSync(metadata_str, new_schedule_save_closure.Get(),
                                 bookmark_model());

  // Metadata are corrupted, so no tracker should have been created.
  EXPECT_THAT(new_processor.GetTrackerForTest(), IsNull());
}

// Verifies that the model type state stored in the tracker gets
// updated upon handling remote updates by assigning a new encryption
// key name.
TEST_F(BookmarkModelTypeProcessorTest,
       ShouldUpdateModelTypeStateUponHandlingRemoteUpdates) {
  SimulateModelReadyToSync();
  SimulateOnSyncStarting();
  // Initialize the process to make sure the tracker has been created.
  InitWithSyncedBookmarks({}, processor());
  const SyncedBookmarkTracker* tracker = processor()->GetTrackerForTest();
  // The encryption key name should be empty.
  ASSERT_TRUE(tracker->model_type_state().encryption_key_name().empty());

  // Build a model type state with an encryption key name.
  const std::string kEncryptionKeyName = "new_encryption_key_name";
  sync_pb::ModelTypeState model_type_state(CreateDummyModelTypeState());
  model_type_state.set_encryption_key_name(kEncryptionKeyName);

  // Push empty updates list to the processor together with the updated model
  // type state.
  syncer::UpdateResponseDataList empty_updates_list;
  processor()->OnUpdateReceived(model_type_state,
                                std::move(empty_updates_list));

  // The model type state inside the tracker should have been updated, and
  // carries the new encryption key name.
  EXPECT_THAT(tracker->model_type_state().encryption_key_name(),
              Eq(kEncryptionKeyName));
}

// This tests that when the encryption key changes, but the received entities
// are already encrypted with the up-to-date encryption key, no recommit is
// needed.
TEST_F(BookmarkModelTypeProcessorTest,
       ShouldNotRecommitEntitiesWhenEncryptionIsUpToDate) {
  SimulateModelReadyToSync();
  SimulateOnSyncStarting();
  SimulateConnectSync();
  // Initialize the process to make sure the tracker has been created.
  InitWithSyncedBookmarks({}, processor());
  const SyncedBookmarkTracker* tracker = processor()->GetTrackerForTest();
  // The encryption key name should be empty.
  ASSERT_TRUE(tracker->model_type_state().encryption_key_name().empty());

  // Build a model type state with an encryption key name.
  const std::string kEncryptionKeyName = "new_encryption_key_name";
  sync_pb::ModelTypeState model_type_state(CreateDummyModelTypeState());
  model_type_state.set_encryption_key_name(kEncryptionKeyName);

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
  processor()->OnUpdateReceived(model_type_state, std::move(updates));

  // The bookmarks shouldn't be marked for committing.
  ASSERT_THAT(tracker->GetEntityForSyncId(kNodeId), NotNull());
  EXPECT_THAT(tracker->GetEntityForSyncId(kNodeId)->IsUnsynced(), Eq(false));
}

// Verifies that the processor doesn't crash if sync is stopped before receiving
// remote updates or tracking metadata.
TEST_F(BookmarkModelTypeProcessorTest, ShouldStopBeforeReceivingRemoteUpdates) {
  SimulateModelReadyToSync();
  SimulateOnSyncStarting();
  ASSERT_THAT(processor()->GetTrackerForTest(), IsNull());
  processor()->OnSyncStopping(syncer::CLEAR_METADATA);
  EXPECT_THAT(processor()->GetTrackerForTest(), IsNull());
}

TEST_F(BookmarkModelTypeProcessorTest, ShouldStopAfterReceivingRemoteUpdates) {
  SimulateModelReadyToSync();
  SimulateOnSyncStarting();
  // Initialize the process to make sure the tracker has been created.
  InitWithSyncedBookmarks({}, processor());
  ASSERT_THAT(processor()->GetTrackerForTest(), NotNull());
  processor()->OnSyncStopping(syncer::CLEAR_METADATA);
  EXPECT_THAT(processor()->GetTrackerForTest(), IsNull());
}

TEST_F(BookmarkModelTypeProcessorTest,
       ShouldReportNoCountersWhenModelIsNotLoaded) {
  SimulateOnSyncStarting();
  ASSERT_THAT(processor()->GetTrackerForTest(), IsNull());
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

TEST_F(BookmarkModelTypeProcessorTest,
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
              ElementsAre(CommitRequestDataMatchesGuid(node->guid())));
}

TEST_F(BookmarkModelTypeProcessorTest,
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
                  unsynced_entities[1]->bookmark_node()->guid())));

  // |unsynced_entities[0]| has been excluded from the result above because the
  // favicon isn't loaded, but the loading process should have started now (see
  // BookmarkLocalChangesBuilder::BuildCommitRequests()).
  EXPECT_TRUE(unsynced_entities[0]->bookmark_node()->is_favicon_loading());
}

TEST_F(BookmarkModelTypeProcessorTest, ShouldReuploadLegacyBookmarksOnStart) {
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";

  std::vector<BookmarkInfo> bookmarks = {
      {kNodeId, kTitle, kUrl, kBookmarkBarId, /*server_tag=*/std::string()}};

  SimulateModelReadyToSync();
  SimulateOnSyncStarting();
  SimulateConnectSync();
  InitWithSyncedBookmarks(bookmarks, processor());

  sync_pb::BookmarkModelMetadata model_metadata =
      processor()->GetTrackerForTest()->BuildBookmarkModelMetadata();
  model_metadata.clear_bookmarks_hierarchy_fields_reuploaded();
  ASSERT_FALSE(processor()->GetTrackerForTest()->HasLocalChanges());

  // Simulate browser restart, enable sync reupload and initialize the processor
  // again.
  ResetModelTypeProcessor();

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(switches::kSyncReuploadBookmarks);

  std::string metadata_str;
  model_metadata.SerializeToString(&metadata_str);
  processor()->ModelReadyToSync(metadata_str, base::DoNothing(),
                                bookmark_model());
  SimulateOnSyncStarting();
  SimulateConnectSync();

  ASSERT_THAT(processor()->GetTrackerForTest(), NotNull());
  const SyncedBookmarkTrackerEntity* entity =
      processor()->GetTrackerForTest()->GetEntityForSyncId(kNodeId);
  ASSERT_THAT(entity, NotNull());

  // Entity should be synced before until first update is received.
  ASSERT_FALSE(entity->IsUnsynced());
  ASSERT_FALSE(processor()
                   ->GetTrackerForTest()
                   ->BuildBookmarkModelMetadata()
                   .bookmarks_hierarchy_fields_reuploaded());

  // Synchronize with the server and get any updates.
  EXPECT_CALL(*mock_commit_queue(), NudgeForCommit());
  processor()->OnUpdateReceived(CreateDummyModelTypeState(),
                                syncer::UpdateResponseDataList());

  // Check that all entities are unsynced now and metadata is marked as
  // reuploaded.
  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_TRUE(processor()
                  ->GetTrackerForTest()
                  ->BuildBookmarkModelMetadata()
                  .bookmarks_hierarchy_fields_reuploaded());
}

}  // namespace

}  // namespace sync_bookmarks
