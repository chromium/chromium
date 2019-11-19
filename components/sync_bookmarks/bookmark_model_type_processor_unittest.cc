// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_type_processor.h"

#include <string>
#include <utility>

#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/undo/bookmark_undo_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using testing::Eq;
using testing::IsNull;
using testing::NiceMock;
using testing::NotNull;

namespace sync_bookmarks {

namespace {

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

std::unique_ptr<syncer::UpdateResponseData> CreateUpdateResponseData(
    const BookmarkInfo& bookmark_info,
    const syncer::UniquePosition& unique_position,
    int response_version,
    const std::string& guid) {
  auto data = std::make_unique<syncer::EntityData>();
  data->id = bookmark_info.server_id;
  data->parent_id = bookmark_info.parent_id;
  data->server_defined_unique_tag = bookmark_info.server_tag;
  data->unique_position = unique_position.ToProto();

  sync_pb::BookmarkSpecifics* bookmark_specifics =
      data->specifics.mutable_bookmark();
  bookmark_specifics->set_guid(guid);
  bookmark_specifics->set_title(bookmark_info.title);
  if (bookmark_info.url.empty()) {
    data->is_folder = true;
  } else {
    bookmark_specifics->set_url(bookmark_info.url);
  }

  auto response_data = std::make_unique<syncer::UpdateResponseData>();
  response_data->entity = std::move(data);
  response_data->response_version = response_version;
  return response_data;
}

std::unique_ptr<syncer::UpdateResponseData> CreateUpdateResponseData(
    const BookmarkInfo& bookmark_info,
    const syncer::UniquePosition& unique_position,
    int response_version) {
  return CreateUpdateResponseData(bookmark_info, unique_position,
                                  response_version, base::GenerateGUID());
}

sync_pb::ModelTypeState CreateDummyModelTypeState() {
  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_cache_guid(kCacheGuid);
  model_type_state.set_initial_sync_done(true);
  return model_type_state;
}

void AssertState(const BookmarkModelTypeProcessor* processor,
                 const std::vector<BookmarkInfo>& bookmarks) {
  const SyncedBookmarkTracker* tracker = processor->GetTrackerForTest();

  // Make sure the tracker contains all bookmarks in |bookmarks| + the
  // 3 permanent nodes.
  ASSERT_THAT(tracker->TrackedEntitiesCountForTest(), Eq(bookmarks.size() + 3));

  for (BookmarkInfo bookmark : bookmarks) {
    const SyncedBookmarkTracker::Entity* entity =
        tracker->GetEntityForSyncId(bookmark.server_id);
    ASSERT_THAT(entity, NotNull());
    const bookmarks::BookmarkNode* node = entity->bookmark_node();
    ASSERT_THAT(node->GetTitle(), Eq(ASCIIToUTF16(bookmark.title)));
    ASSERT_THAT(node->url(), Eq(GURL(bookmark.url)));
    const SyncedBookmarkTracker::Entity* parent_entity =
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

class BookmarkModelTypeProcessorTest : public testing::Test {
 public:
  BookmarkModelTypeProcessorTest()
      : processor_(&bookmark_undo_service_),
        bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()) {
    // TODO(crbug.com/516866): This class assumes model is loaded and sync has
    // started before running tests. We should test other variations (i.e. model
    // isn't loaded yet and/or sync didn't start yet).
    processor_.SetFaviconService(&favicon_service_);
  }

  void SimulateModelReadyToSync() {
    processor_.ModelReadyToSync(
        /*metadata_str=*/std::string(), schedule_save_closure_.Get(),
        bookmark_model_.get());
  }

  void SimulateOnSyncStarting() {
    syncer::DataTypeActivationRequest request;
    request.cache_guid = kCacheGuid;
    processor_.OnSyncStarting(request, base::DoNothing());
  }

  void DestroyBookmarkModel() { bookmark_model_.reset(); }

  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }
  BookmarkUndoService* bookmark_undo_service() {
    return &bookmark_undo_service_;
  }
  favicon::FaviconService* favicon_service() { return &favicon_service_; }
  BookmarkModelTypeProcessor* processor() { return &processor_; }
  base::MockCallback<base::RepeatingClosure>* schedule_save_closure() {
    return &schedule_save_closure_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  NiceMock<base::MockCallback<base::RepeatingClosure>> schedule_save_closure_;
  BookmarkUndoService bookmark_undo_service_;
  NiceMock<favicon::MockFaviconService> favicon_service_;
  BookmarkModelTypeProcessor processor_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

TEST_F(BookmarkModelTypeProcessorTest, ShouldUpdateModelAfterRemoteCreation) {
  SimulateModelReadyToSync();
  SimulateOnSyncStarting();
  syncer::UniquePosition kRandomPosition =
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

  // Add update for another node under the bookmarks bar.
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";
  updates.push_back(
      CreateUpdateResponseData({kNodeId, kTitle, kUrl, kBookmarkBarId,
                                /*server_tag=*/std::string()},
                               kRandomPosition, /*response_version=*/0));

  const bookmarks::BookmarkNode* bookmarkbar =
      bookmark_model()->bookmark_bar_node();
  EXPECT_TRUE(bookmarkbar->children().empty());

  processor()->OnUpdateReceived(CreateDummyModelTypeState(),
                                std::move(updates));

  ASSERT_THAT(bookmarkbar->children().front().get(), NotNull());
  EXPECT_THAT(bookmarkbar->children().front()->GetTitle(),
              Eq(ASCIIToUTF16(kTitle)));
  EXPECT_THAT(bookmarkbar->children().front()->url(), Eq(GURL(kUrl)));
}

TEST_F(BookmarkModelTypeProcessorTest, ShouldUpdateModelAfterRemoteUpdate) {
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";
  syncer::UniquePosition kRandomPosition =
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

  processor()->OnUpdateReceived(CreateDummyModelTypeState(),
                                std::move(updates));

  // Check if the bookmark has been updated properly.
  EXPECT_THAT(bookmark_bar->children().front().get(), Eq(bookmark_node));
  EXPECT_THAT(bookmark_node->GetTitle(), Eq(ASCIIToUTF16(kNewTitle)));
  EXPECT_THAT(bookmark_node->url(), Eq(GURL(kNewUrl)));
}

TEST_F(
    BookmarkModelTypeProcessorTest,
    ShouldScheduleSaveAfterRemoteUpdateWithOnlyMetadataChangeAndReflections) {
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";
  syncer::UniquePosition kRandomPosition =
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
  updates.push_back(
      CreateUpdateResponseData({kNodeId, kTitle, kUrl, kBookmarkBarId,
                                /*server_tag=*/std::string()},
                               kRandomPosition, /*response_version=*/1));
  updates[0]->response_version++;

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
  // TODO(crbug.com/516866): Remove this after initial sync done is properly set
  // within the processor.
  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.mutable_model_type_state()->set_initial_sync_done(true);
  // Add entries for the permanent nodes. TestBookmarkClient adds all of them.
  sync_pb::BookmarkMetadata* bookmark_metadata =
      model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(bookmark_bar_node->id());
  bookmark_metadata->mutable_metadata()->set_server_id(kBookmarkBarId);

  bookmark_metadata = model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(bookmark_model()->other_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id(kOtherBookmarksId);

  bookmark_metadata = model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(bookmark_model()->mobile_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id(kMobileBookmarksId);

  // Add an entry for the bookmark node.
  bookmark_metadata = model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(bookmarknode->id());
  bookmark_metadata->mutable_metadata()->set_server_id(kNodeId);

  // Create a new processor and init it with the metadata str.
  BookmarkModelTypeProcessor new_processor(bookmark_undo_service());

  std::string metadata_str;
  model_metadata.SerializeToString(&metadata_str);
  base::HistogramTester histogram_tester;
  new_processor.ModelReadyToSync(metadata_str, base::DoNothing(),
                                 bookmark_model());
  histogram_tester.ExpectTotalCount("Sync.BookmarksModelReadyToSyncTime",
                                    /*count=*/1);

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

  std::string metadata_str;
  model_metadata.SerializeToString(&metadata_str);
  new_processor.ModelReadyToSync(metadata_str, base::DoNothing(),
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
  sync_pb::BookmarkMetadata* bookmark_metadata =
      model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(bookmark_model()->bookmark_bar_node()->id());
  bookmark_metadata->mutable_metadata()->set_server_id(kBookmarkBarId);

  // Create a new processor and init it with the metadata str.
  BookmarkModelTypeProcessor new_processor(bookmark_undo_service());

  std::string metadata_str;
  model_metadata.SerializeToString(&metadata_str);
  new_processor.ModelReadyToSync(metadata_str, base::DoNothing(),
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
  std::unique_ptr<syncer::UpdateResponseData> response_data =
      CreateUpdateResponseData(
          {kNodeId, "title", "http://www.url.com", /*parent_id=*/kBookmarkBarId,
           /*server_tag=*/std::string()},
          syncer::UniquePosition::InitialPosition(
              syncer::UniquePosition::RandomSuffix()),
          /*response_version=*/0);
  response_data->encryption_key_name = kEncryptionKeyName;

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
  syncer::StatusCounters status_counters;
  // Assign an arbitrary non-zero number to the |num_entries| to be able to
  // check that actually a 0 has been written to it later.
  status_counters.num_entries = 1000;
  processor()->GetStatusCountersForDebugging(
      base::BindLambdaForTesting([&](syncer::ModelType model_type,
                                     const syncer::StatusCounters& counters) {
        status_counters = counters;
      }));
  EXPECT_EQ(0u, status_counters.num_entries);
}

}  // namespace

}  // namespace sync_bookmarks
