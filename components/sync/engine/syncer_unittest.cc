// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/syncer.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/active_devices_invalidation_info.h"
#include "components/sync/engine/backoff_delay_provider.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/cycle/sync_cycle_context.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/forwarding_data_type_processor.h"
#include "components/sync/engine/net/server_connection_manager.h"
#include "components/sync/engine/nigori/keystore_keys_handler.h"
#include "components/sync/engine/sync_scheduler_impl.h"
#include "components/sync/engine/syncer_proto_util.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/client_commands.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/fake_sync_encryption_handler.h"
#include "components/sync/test/mock_connection_manager.h"
#include "components/sync/test/mock_data_type_processor.h"
#include "components/sync/test/mock_debug_info_getter.h"
#include "components/sync/test/mock_nudge_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

sync_pb::EntitySpecifics MakeSpecifics(DataType data_type) {
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(data_type, &specifics);
  return specifics;
}

sync_pb::EntitySpecifics MakeBookmarkSpecificsToCommit() {
  sync_pb::EntitySpecifics specifics = MakeSpecifics(BOOKMARKS);
  // The worker DCHECKs for the validity of the |type| and |unique_position|
  // fields for outgoing commits.
  specifics.mutable_bookmark()->set_type(sync_pb::BookmarkSpecifics::URL);
  *specifics.mutable_bookmark()->mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();
  return specifics;
}

}  // namespace

// Syncer unit tests. Unfortunately a lot of these tests
// are outdated and need to be reworked and updated.
class SyncerTest : public testing::Test,
                   public SyncCycle::Delegate,
                   public SyncEngineEventListener {
 public:
  SyncerTest() = default;

  SyncerTest(const SyncerTest&) = delete;
  SyncerTest& operator=(const SyncerTest&) = delete;

  // SyncCycle::Delegate implementation.
  void OnThrottled(const base::TimeDelta& throttle_duration) override {
    FAIL() << "Should not get silenced.";
  }

  void OnTypesThrottled(DataTypeSet types,
                        const base::TimeDelta& throttle_duration) override {
    scheduler_->OnTypesThrottled(types, throttle_duration);
  }

  void OnTypesBackedOff(DataTypeSet types) override {
    scheduler_->OnTypesBackedOff(types);
  }

  bool IsAnyThrottleOrBackoff() override { return false; }

  void OnReceivedPollIntervalUpdate(
      const base::TimeDelta& new_interval) override {
    last_poll_interval_received_ = new_interval;
  }

  void OnReceivedCustomNudgeDelays(
      const std::map<DataType, base::TimeDelta>& delay_map) override {
    auto iter = delay_map.find(BOOKMARKS);
    if (iter != delay_map.end() && iter->second.is_positive()) {
      last_bookmarks_commit_delay_ = iter->second;
    }
  }

  void OnReceivedGuRetryDelay(const base::TimeDelta& delay) override {}
  void OnReceivedMigrationRequest(DataTypeSet types) override {}
  void OnReceivedQuotaParamsForExtensionTypes(
      std::optional<int> max_tokens,
      std::optional<base::TimeDelta> refill_interval,
      std::optional<base::TimeDelta> depleted_quota_nudge_delay) override {}
  void OnProtocolEvent(const ProtocolEvent& event) override {}
  void OnSyncProtocolError(const SyncProtocolError& error) override {}

  void OnSyncCycleEvent(const SyncCycleEvent& event) override {
    DVLOG(1) << "HandleSyncEngineEvent in unittest " << event.what_happened;
  }

  void OnActionableProtocolError(const SyncProtocolError& error) override {}
  void OnRetryTimeChanged(base::Time retry_time) override {}
  void OnThrottledTypesChanged(DataTypeSet throttled_types) override {}
  void OnBackedOffTypesChanged(DataTypeSet backed_off_types) override {}
  void OnMigrationRequested(DataTypeSet types) override {}

  void ResetCycle() {
    cycle_ = std::make_unique<SyncCycle>(context_.get(), this);
  }

  bool SyncShareNudge() {
    ResetCycle();

    // Pretend we've seen a local change, to make the nudge_tracker look normal.
    nudge_tracker_.RecordLocalChange(BOOKMARKS, false);

    return syncer_->NormalSyncShare(context_->GetConnectedTypes(),
                                    &nudge_tracker_, cycle_.get());
  }

  bool SyncShareConfigure() {
    return SyncShareConfigureTypes(context_->GetConnectedTypes());
  }

  bool SyncShareConfigureTypes(DataTypeSet types) {
    ResetCycle();
    return syncer_->ConfigureSyncShare(
        types, sync_pb::SyncEnums::RECONFIGURATION, cycle_.get());
  }

  void SetUp() override {
    mock_server_ = std::make_unique<MockConnectionManager>();
    debug_info_getter_ = std::make_unique<MockDebugInfoGetter>();
    std::vector<SyncEngineEventListener*> listeners;
    listeners.push_back(this);

    data_type_registry_ = std::make_unique<DataTypeRegistry>(
        &mock_nudge_handler_, &cancelation_signal_, &encryption_handler_);

    EnableDatatype(BOOKMARKS);
    EnableDatatype(EXTENSIONS);
    EnableDatatype(NIGORI);
    EnableDatatype(PREFERENCES);

    context_ = std::make_unique<SyncCycleContext>(
        mock_server_.get(), extensions_activity_.get(), listeners,
        debug_info_getter_.get(), data_type_registry_.get(), local_cache_guid(),
        mock_server_->store_birthday(), "fake_bag_of_chips",
        /*poll_interval=*/base::Minutes(30));
    auto syncer = std::make_unique<Syncer>(&cancelation_signal_);
    // The syncer is destroyed with the scheduler that owns it.
    syncer_ = syncer.get();
    scheduler_ = std::make_unique<SyncSchedulerImpl>(
        "TestSyncScheduler", BackoffDelayProvider::FromDefaults(),
        context_.get(), std::move(syncer), false);

    mock_server_->SetKeystoreKey("encryption_key");
  }

  void TearDown() override {
    mock_server_.reset();
    scheduler_.reset();
  }

  const std::string local_cache_guid() { return "lD16ebCGCZh+zkiZ68gWDw=="; }

  const std::string foreign_cache_guid() { return "kqyg7097kro6GSUod+GSg=="; }

  MockDataTypeProcessor* GetProcessor(DataType data_type) {
    return &mock_data_type_processors_[data_type];
  }

  std::unique_ptr<DataTypeActivationResponse> MakeFakeActivationResponse(
      DataType data_type) {
    auto response = std::make_unique<DataTypeActivationResponse>();
    response->data_type_state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
    response->data_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromDataType(data_type));
    response->type_processor =
        std::make_unique<ForwardingDataTypeProcessor>(GetProcessor(data_type));
    return response;
  }

  void EnableDatatype(DataType data_type) {
    enabled_datatypes_.Put(data_type);
    data_type_registry_->ConnectDataType(data_type,
                                         MakeFakeActivationResponse(data_type));
    mock_server_->ExpectGetUpdatesRequestTypes(enabled_datatypes_);
  }

  void DisableDatatype(DataType data_type) {
    enabled_datatypes_.Remove(data_type);
    data_type_registry_->DisconnectDataType(data_type);
    mock_server_->ExpectGetUpdatesRequestTypes(enabled_datatypes_);
  }

  // Configures SyncCycleContext and NudgeTracker so Syncer won't call
  // GetUpdates prior to Commit. This method can be used to ensure a Commit is
  // not preceeded by GetUpdates.
  void ConfigureNoGetUpdatesRequired() {
    nudge_tracker_.OnInvalidationsEnabled();
    nudge_tracker_.RecordSuccessfulSyncCycleIfNotBlocked(DataTypeSet::All());

    ASSERT_FALSE(nudge_tracker_.IsGetUpdatesRequired(DataTypeSet::All()));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  FakeSyncEncryptionHandler encryption_handler_;
  scoped_refptr<ExtensionsActivity> extensions_activity_ =
      new ExtensionsActivity;
  std::unique_ptr<MockConnectionManager> mock_server_;
  CancelationSignal cancelation_signal_;
  std::map<DataType, MockDataTypeProcessor> mock_data_type_processors_;

  raw_ptr<Syncer, DanglingUntriaged> syncer_ = nullptr;

  std::unique_ptr<SyncCycle> cycle_;
  MockNudgeHandler mock_nudge_handler_;
  std::unique_ptr<DataTypeRegistry> data_type_registry_;
  std::unique_ptr<SyncSchedulerImpl> scheduler_;
  std::unique_ptr<SyncCycleContext> context_;
  base::TimeDelta last_poll_interval_received_;
  base::TimeDelta last_bookmarks_commit_delay_;
  int last_client_invalidation_hint_buffer_size_ = 10;

  DataTypeSet enabled_datatypes_;
  NudgeTracker nudge_tracker_;
  std::unique_ptr<MockDebugInfoGetter> debug_info_getter_;
};

TEST_F(SyncerTest, CommitFiltersThrottledEntries) {
  const DataTypeSet throttled_types = {BOOKMARKS};

  GetProcessor(BOOKMARKS)->AppendCommitRequest(
      ClientTagHash::FromHashed("tag1"), MakeBookmarkSpecificsToCommit(),
      "id1");

  // Sync without enabling bookmarks.
  mock_server_->ExpectGetUpdatesRequestTypes(
      Difference(context_->GetConnectedTypes(), throttled_types));
  ResetCycle();
  syncer_->NormalSyncShare(
      Difference(context_->GetConnectedTypes(), throttled_types),
      &nudge_tracker_, cycle_.get());

  // Nothing should have been committed as bookmarks is throttled.
  EXPECT_EQ(0, GetProcessor(BOOKMARKS)->GetLocalChangesCallCount());

  // Sync again with bookmarks enabled.
  mock_server_->ExpectGetUpdatesRequestTypes(context_->GetConnectedTypes());
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, GetProcessor(BOOKMARKS)->GetLocalChangesCallCount());
}

TEST_F(SyncerTest, GetUpdatesPartialThrottled) {
  const sync_pb::EntitySpecifics bookmark = MakeSpecifics(BOOKMARKS);
  const sync_pb::EntitySpecifics pref = MakeSpecifics(PREFERENCES);

  // Normal sync, all the data types should get synced.
  mock_server_->AddUpdateSpecifics("1", "0", "A", 10, 10, true, bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics("2", "1", "B", 10, 10, false, bookmark,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateSpecifics("3", "1", "C", 10, 10, false, bookmark,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateSpecifics("4", "0", "D", 10, 10, false, pref);

  EXPECT_TRUE(SyncShareNudge());
  // Initial state. Everything is normal.
  ASSERT_EQ(1U, GetProcessor(BOOKMARKS)->GetNumUpdateResponses());
  ASSERT_EQ(3U, GetProcessor(BOOKMARKS)->GetNthUpdateResponse(0).size());
  ASSERT_EQ(1U, GetProcessor(PREFERENCES)->GetNumUpdateResponses());
  ASSERT_EQ(1U, GetProcessor(PREFERENCES)->GetNthUpdateResponse(0).size());

  // Set BOOKMARKS throttled but PREFERENCES not,
  // then BOOKMARKS should not get synced but PREFERENCES should.
  DataTypeSet throttled_types = {BOOKMARKS};
  mock_server_->set_throttling(true);
  mock_server_->SetPartialFailureTypes(throttled_types);

  mock_server_->AddUpdateSpecifics("1", "0", "E", 20, 20, true, bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics("2", "1", "F", 20, 20, false, bookmark,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateSpecifics("3", "1", "G", 20, 20, false, bookmark,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateSpecifics("4", "0", "H", 20, 20, false, pref);
  EXPECT_TRUE(SyncShareNudge());

  // PREFERENCES continues to work normally (not throttled).
  ASSERT_EQ(2U, GetProcessor(PREFERENCES)->GetNumUpdateResponses());
  // BOOKMARKS throttled.
  EXPECT_EQ(1U, GetProcessor(BOOKMARKS)->GetNumUpdateResponses());

  // Unthrottled BOOKMARKS, then BOOKMARKS should get synced now.
  mock_server_->set_throttling(false);

  mock_server_->AddUpdateSpecifics("1", "0", "E", 30, 30, true, bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics("2", "1", "F", 30, 30, false, bookmark,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateSpecifics("3", "1", "G", 30, 30, false, bookmark,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateSpecifics("4", "0", "H", 30, 30, false, pref);
  EXPECT_TRUE(SyncShareNudge());
  // BOOKMARKS unthrottled.
  EXPECT_EQ(2U, GetProcessor(BOOKMARKS)->GetNumUpdateResponses());
}

TEST_F(SyncerTest, GetUpdatesPartialFailure) {
  const sync_pb::EntitySpecifics bookmark = MakeSpecifics(BOOKMARKS);
  const sync_pb::EntitySpecifics pref = MakeSpecifics(PREFERENCES);

  // Normal sync, all the data types should get synced.
  mock_server_->AddUpdateSpecifics("1", "0", "A", 10, 10, true, bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics("2", "1", "B", 10, 10, false, bookmark,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateSpecifics("3", "1", "C", 10, 10, false, bookmark,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateSpecifics("4", "0", "D", 10, 10, false, pref);

  EXPECT_TRUE(SyncShareNudge());
  // Initial state. Everything is normal.
  ASSERT_EQ(1U, GetProcessor(BOOKMARKS)->GetNumUpdateResponses());
  ASSERT_EQ(3U, GetProcessor(BOOKMARKS)->GetNthUpdateResponse(0).size());
  ASSERT_EQ(1U, GetProcessor(PREFERENCES)->GetNumUpdateResponses());
  ASSERT_EQ(1U, GetProcessor(PREFERENCES)->GetNthUpdateResponse(0).size());

  // Set BOOKMARKS failure but PREFERENCES not,
  // then BOOKMARKS should not get synced but PREFERENCES should.
  DataTypeSet failed_types = {BOOKMARKS};
  mock_server_->set_partial_failure(true);
  mock_server_->SetPartialFailureTypes(failed_types);

  mock_server_->AddUpdateSpecifics("1", "0", "E", 20, 20, true, bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics("2", "1", "F", 20, 20, false, bookmark,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateSpecifics("3", "1", "G", 20, 20, false, bookmark,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateSpecifics("4", "0", "H", 20, 20, false, pref);
  EXPECT_TRUE(SyncShareNudge());

  // PREFERENCES continues to work normally (not throttled).
  ASSERT_EQ(2U, GetProcessor(PREFERENCES)->GetNumUpdateResponses());
  // BOOKMARKS failed.
  EXPECT_EQ(1U, GetProcessor(BOOKMARKS)->GetNumUpdateResponses());

  // Set BOOKMARKS not partial failed, then BOOKMARKS should get synced now.
  mock_server_->set_partial_failure(false);

  mock_server_->AddUpdateSpecifics("1", "0", "E", 30, 30, true, bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics("2", "1", "F", 30, 30, false, bookmark,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateSpecifics("3", "1", "G", 30, 30, false, bookmark,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateSpecifics("4", "0", "H", 30, 30, false, pref);
  EXPECT_TRUE(SyncShareNudge());
  // BOOKMARKS not failed.
  EXPECT_EQ(2U, GetProcessor(BOOKMARKS)->GetNumUpdateResponses());
}

TEST_F(SyncerTest, TestSimpleCommit) {
  const std::string kSyncId1 = "id1";
  const std::string kSyncId2 = "id2";

  GetProcessor(PREFERENCES)
      ->AppendCommitRequest(ClientTagHash::FromHashed("tag1"),
                            MakeSpecifics(PREFERENCES), kSyncId1);
  GetProcessor(PREFERENCES)
      ->AppendCommitRequest(ClientTagHash::FromHashed("tag2"),
                            MakeSpecifics(PREFERENCES), kSyncId2);

  EXPECT_TRUE(SyncShareNudge());
  EXPECT_THAT(mock_server_->committed_ids(),
              UnorderedElementsAre(kSyncId1, kSyncId2));
}

TEST_F(SyncerTest, TestSimpleGetUpdates) {
  std::string id = "some_id";
  std::string parent_id = "0";
  std::string name = "in_root";
  int64_t version = 10;
  int64_t timestamp = 10;
  mock_server_->AddUpdateDirectory(id, parent_id, name, version, timestamp,
                                   foreign_cache_guid(), "-1");

  EXPECT_TRUE(SyncShareNudge());

  ASSERT_EQ(1U, GetProcessor(BOOKMARKS)->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> updates_list =
      GetProcessor(BOOKMARKS)->GetNthUpdateResponse(0);
  EXPECT_EQ(1U, updates_list.size());

  const UpdateResponseData& update = *updates_list.back();
  const EntityData& entity = update.entity;

  EXPECT_EQ(id, entity.id);
  EXPECT_EQ(version, update.response_version);
  // Creation time hardcoded in MockConnectionManager::AddUpdateMeta().
  EXPECT_EQ(ProtoTimeToTime(1), entity.creation_time);
  EXPECT_EQ(ProtoTimeToTime(timestamp), entity.modification_time);
  EXPECT_EQ(name, entity.name);
  EXPECT_FALSE(entity.is_deleted());
}

// Committing more than kDefaultMaxCommitBatchSize items requires that
// we post more than one commit command to the server.  This test makes
// sure that scenario works as expected.
TEST_F(SyncerTest, CommitManyItemsInOneGo_Success) {
  int num_batches = 3;
  int items_to_commit = kDefaultMaxCommitBatchSize * num_batches;

  for (int i = 0; i < items_to_commit; i++) {
    GetProcessor(PREFERENCES)
        ->AppendCommitRequest(
            ClientTagHash::FromHashed(base::StringPrintf("tag%d", i)),
            MakeSpecifics(PREFERENCES));
  }

  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(static_cast<size_t>(num_batches),
            mock_server_->commit_messages().size());

  ASSERT_EQ(static_cast<size_t>(num_batches),
            GetProcessor(PREFERENCES)->GetNumCommitResponses());
  EXPECT_EQ(static_cast<size_t>(kDefaultMaxCommitBatchSize),
            GetProcessor(PREFERENCES)->GetNthCommitResponse(0).size());
  EXPECT_EQ(static_cast<size_t>(kDefaultMaxCommitBatchSize),
            GetProcessor(PREFERENCES)->GetNthCommitResponse(1).size());
  EXPECT_EQ(static_cast<size_t>(kDefaultMaxCommitBatchSize),
            GetProcessor(PREFERENCES)->GetNthCommitResponse(2).size());
}

// Test that a single failure to contact the server will cause us to exit the
// commit loop immediately.
TEST_F(SyncerTest, CommitManyItemsInOneGo_PostBufferFail) {
  int num_batches = 3;
  int items_to_commit = kDefaultMaxCommitBatchSize * num_batches;

  for (int i = 0; i < items_to_commit; i++) {
    GetProcessor(PREFERENCES)
        ->AppendCommitRequest(
            ClientTagHash::FromHashed(base::StringPrintf("tag%d", i)),
            MakeSpecifics(PREFERENCES));
  }

  // The second commit should fail.  It will be preceded by one successful
  // GetUpdate and one succesful commit.
  mock_server_->FailNthPostBufferToPathCall(3);
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(SyncShareNudge());

  EXPECT_EQ(1U, mock_server_->commit_messages().size());
  ASSERT_EQ(
      cycle_->status_controller().model_neutral_state().commit_result.type(),
      SyncerError::Type::kHttpError);

  // Since the second batch fails, the third one should not even be gathered.
  EXPECT_EQ(2, GetProcessor(PREFERENCES)->GetLocalChangesCallCount());

  histogram_tester.ExpectBucketCount("Sync.CommitResponse.PREFERENCE",
                                     SyncerErrorValueForUma::kSyncServerError,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Sync.CommitResponse",
                                     SyncerErrorValueForUma::kSyncServerError,
                                     /*expected_count=*/1);
}

// Test that a single conflict response from the server will cause us to exit
// the commit loop immediately.
TEST_F(SyncerTest, CommitManyItemsInOneGo_CommitConflict) {
  int num_batches = 2;
  int items_to_commit = kDefaultMaxCommitBatchSize * num_batches;

  for (int i = 0; i < items_to_commit; i++) {
    GetProcessor(PREFERENCES)
        ->AppendCommitRequest(
            ClientTagHash::FromHashed(base::StringPrintf("tag%d", i)),
            MakeSpecifics(PREFERENCES));
  }

  // Return a CONFLICT response for the first item.
  mock_server_->set_conflict_n_commits(1);
  EXPECT_FALSE(SyncShareNudge());

  // We should stop looping at the first sign of trouble.
  EXPECT_EQ(1U, mock_server_->commit_messages().size());
  EXPECT_EQ(1, GetProcessor(PREFERENCES)->GetLocalChangesCallCount());
}

// Tests that sending debug info events works.
TEST_F(SyncerTest, SendDebugInfoEventsOnGetUpdates_HappyCase) {
  debug_info_getter_->AddDebugEvent();
  debug_info_getter_->AddDebugEvent();

  EXPECT_TRUE(SyncShareNudge());

  // Verify we received one GetUpdates request with two debug info events.
  EXPECT_EQ(1U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(2, mock_server_->last_request().debug_info().events_size());

  EXPECT_TRUE(SyncShareNudge());

  // See that we received another GetUpdates request, but that it contains no
  // debug info events.
  EXPECT_EQ(2U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(0, mock_server_->last_request().debug_info().events_size());

  debug_info_getter_->AddDebugEvent();

  EXPECT_TRUE(SyncShareNudge());

  // See that we received another GetUpdates request and it contains one debug
  // info event.
  EXPECT_EQ(3U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(1, mock_server_->last_request().debug_info().events_size());
}

// Tests that debug info events are dropped on server error.
TEST_F(SyncerTest, SendDebugInfoEventsOnGetUpdates_PostFailsDontDrop) {
  debug_info_getter_->AddDebugEvent();
  debug_info_getter_->AddDebugEvent();

  mock_server_->FailNextPostBufferToPathCall();
  EXPECT_FALSE(SyncShareNudge());

  // Verify we attempted to send one GetUpdates request with two debug info
  // events.
  EXPECT_EQ(1U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(2, mock_server_->last_request().debug_info().events_size());

  EXPECT_TRUE(SyncShareNudge());

  // See that the client resent the two debug info events.
  EXPECT_EQ(2U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(2, mock_server_->last_request().debug_info().events_size());

  // The previous send was successful so this next one shouldn't generate any
  // debug info events.
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(3U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(0, mock_server_->last_request().debug_info().events_size());
}

// Tests that commit failure with conflict will trigger GetUpdates for next
// cycle of sync
TEST_F(SyncerTest, CommitFailureWithConflict) {
  ConfigureNoGetUpdatesRequired();

  GetProcessor(PREFERENCES)
      ->AppendCommitRequest(ClientTagHash::FromHashed("tag1"),
                            MakeSpecifics(PREFERENCES), "id1");

  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired(DataTypeSet::All()));

  EXPECT_TRUE(SyncShareNudge());
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired(DataTypeSet::All()));

  GetProcessor(PREFERENCES)
      ->AppendCommitRequest(ClientTagHash::FromHashed("tag1"),
                            MakeSpecifics(PREFERENCES), "id1");

  mock_server_->set_conflict_n_commits(1);
  EXPECT_FALSE(SyncShareNudge());
  EXPECT_TRUE(nudge_tracker_.IsGetUpdatesRequired(DataTypeSet::All()));

  nudge_tracker_.RecordSuccessfulSyncCycleIfNotBlocked(DataTypeSet::All());
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired(DataTypeSet::All()));
}

// Tests that sending debug info events on Commit works.
TEST_F(SyncerTest, SendDebugInfoEventsOnCommit_HappyCase) {
  // Make sure GetUpdate isn't call as it would "steal" debug info events before
  // Commit has a chance to send them.
  ConfigureNoGetUpdatesRequired();

  // Generate a debug info event and trigger a commit.
  debug_info_getter_->AddDebugEvent();
  GetProcessor(PREFERENCES)
      ->AppendCommitRequest(ClientTagHash::FromHashed("tag1"),
                            MakeSpecifics(PREFERENCES), "id1");
  EXPECT_TRUE(SyncShareNudge());

  // Verify that the last request received is a Commit and that it contains a
  // debug info event.
  EXPECT_EQ(1U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(1, mock_server_->last_request().debug_info().events_size());

  // Generate another commit, but no debug info event.
  GetProcessor(PREFERENCES)
      ->AppendCommitRequest(ClientTagHash::FromHashed("tag2"),
                            MakeSpecifics(PREFERENCES), "id2");
  EXPECT_TRUE(SyncShareNudge());

  // See that it was received and contains no debug info events.
  EXPECT_EQ(2U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(0, mock_server_->last_request().debug_info().events_size());
}

// Tests that debug info events are not dropped on server error.
TEST_F(SyncerTest, SendDebugInfoEventsOnCommit_PostFailsDontDrop) {
  // Make sure GetUpdate isn't call as it would "steal" debug info events before
  // Commit has a chance to send them.
  ConfigureNoGetUpdatesRequired();

  mock_server_->FailNextPostBufferToPathCall();

  // Generate a debug info event and trigger a commit.
  debug_info_getter_->AddDebugEvent();
  GetProcessor(PREFERENCES)
      ->AppendCommitRequest(ClientTagHash::FromHashed("tag1"),
                            MakeSpecifics(PREFERENCES), "id1");
  EXPECT_FALSE(SyncShareNudge());

  // Verify that the last request sent is a Commit and that it contains a debug
  // info event.
  EXPECT_EQ(1U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(1, mock_server_->last_request().debug_info().events_size());

  // Try again. Because of how MockDataTypeProcessor works, commit data needs
  // to be provided again.
  GetProcessor(PREFERENCES)
      ->AppendCommitRequest(ClientTagHash::FromHashed("tag1"),
                            MakeSpecifics(PREFERENCES), "id1");
  EXPECT_TRUE(SyncShareNudge());

  // Verify that we've received another Commit and that it contains a debug info
  // event (just like the previous one).
  EXPECT_EQ(2U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(1, mock_server_->last_request().debug_info().events_size());

  // Generate another commit and try again.
  GetProcessor(PREFERENCES)
      ->AppendCommitRequest(ClientTagHash::FromHashed("tag2"),
                            MakeSpecifics(PREFERENCES), "id2");
  EXPECT_TRUE(SyncShareNudge());

  // See that it was received and contains no debug info events.
  EXPECT_EQ(3U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(0, mock_server_->last_request().debug_info().events_size());
}

TEST_F(SyncerTest, TestClientCommandDuringUpdate) {
  using sync_pb::ClientCommand;

  auto command = std::make_unique<ClientCommand>();
  command->set_set_sync_poll_interval(8);
  command->set_set_sync_long_poll_interval(800);
  sync_pb::CustomNudgeDelay* bookmark_delay =
      command->add_custom_nudge_delays();
  bookmark_delay->set_datatype_id(
      GetSpecificsFieldNumberFromDataType(BOOKMARKS));
  bookmark_delay->set_delay_ms(950);
  mock_server_->AddUpdateDirectory("1", "0", "in_root", 1, 1,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetGUClientCommand(std::move(command));
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(base::Seconds(8), last_poll_interval_received_);
  EXPECT_EQ(base::Milliseconds(950), last_bookmarks_commit_delay_);

  command = std::make_unique<ClientCommand>();
  command->set_set_sync_poll_interval(180);
  command->set_set_sync_long_poll_interval(190);
  bookmark_delay = command->add_custom_nudge_delays();
  bookmark_delay->set_datatype_id(
      GetSpecificsFieldNumberFromDataType(BOOKMARKS));
  bookmark_delay->set_delay_ms(1050);
  mock_server_->AddUpdateDirectory("1", "0", "in_root", 1, 1,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetGUClientCommand(std::move(command));
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(base::Seconds(180), last_poll_interval_received_);
  EXPECT_EQ(base::Milliseconds(1050), last_bookmarks_commit_delay_);
}

TEST_F(SyncerTest, TestClientCommandDuringCommit) {
  using sync_pb::ClientCommand;

  auto command = std::make_unique<ClientCommand>();
  command->set_set_sync_poll_interval(8);
  command->set_set_sync_long_poll_interval(800);
  sync_pb::CustomNudgeDelay* bookmark_delay =
      command->add_custom_nudge_delays();
  bookmark_delay->set_datatype_id(
      GetSpecificsFieldNumberFromDataType(BOOKMARKS));
  bookmark_delay->set_delay_ms(950);
  GetProcessor(BOOKMARKS)->AppendCommitRequest(
      ClientTagHash::FromHashed("tag1"), MakeBookmarkSpecificsToCommit(),
      "id1");
  mock_server_->SetCommitClientCommand(std::move(command));
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(base::Seconds(8), last_poll_interval_received_);
  EXPECT_EQ(base::Milliseconds(950), last_bookmarks_commit_delay_);

  command = std::make_unique<ClientCommand>();
  command->set_set_sync_poll_interval(180);
  command->set_set_sync_long_poll_interval(190);
  bookmark_delay = command->add_custom_nudge_delays();
  bookmark_delay->set_datatype_id(
      GetSpecificsFieldNumberFromDataType(BOOKMARKS));
  bookmark_delay->set_delay_ms(1050);
  GetProcessor(BOOKMARKS)->AppendCommitRequest(
      ClientTagHash::FromHashed("tag2"), MakeBookmarkSpecificsToCommit(),
      "id2");
  mock_server_->SetCommitClientCommand(std::move(command));
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(base::Seconds(180), last_poll_interval_received_);
  EXPECT_EQ(base::Milliseconds(1050), last_bookmarks_commit_delay_);
}

TEST_F(SyncerTest, ShouldPopulateSingleClientFlag) {
  GetProcessor(BOOKMARKS)->AppendCommitRequest(
      ClientTagHash::FromHashed("tag1"), MakeBookmarkSpecificsToCommit(),
      "id1");

  // No other devices are interested in bookmarks.
  context_->set_active_devices_invalidation_info(
      ActiveDevicesInvalidationInfo::Create(
          /*all_fcm_registration_tokens=*/{},
          /*all_interested_data_types=*/{PREFERENCES},
          /*fcm_token_and_interested_data_types=*/{},
          /*old_invalidations_interested_data_types=*/{}));
  ASSERT_TRUE(SyncShareNudge());
  EXPECT_TRUE(
      mock_server_->last_request().commit().config_params().single_client());
  EXPECT_TRUE(mock_server_->last_request()
                  .commit()
                  .config_params()
                  .single_client_with_standalone_invalidations());
  EXPECT_TRUE(mock_server_->last_request()
                  .commit()
                  .config_params()
                  .single_client_with_old_invalidations());
}

TEST_F(SyncerTest,
       ShouldPopulateSingleClientFlagForStandaloneInvalidationsOnly) {
  GetProcessor(BOOKMARKS)->AppendCommitRequest(
      ClientTagHash::FromHashed("tag1"), MakeBookmarkSpecificsToCommit(),
      "id1");

  // No other devices with standalone invalidations are interested in bookmarks.
  context_->set_active_devices_invalidation_info(
      ActiveDevicesInvalidationInfo::Create(
          /*all_fcm_registration_tokens=*/{"token_1"},
          /*all_interested_data_types=*/{BOOKMARKS, PREFERENCES},
          /*fcm_token_and_interested_data_types=*/
          {{"token_1", {PREFERENCES}}},
          /*old_invalidations_interested_data_types=*/{BOOKMARKS}));
  ASSERT_TRUE(SyncShareNudge());
  EXPECT_FALSE(
      mock_server_->last_request().commit().config_params().single_client());
  EXPECT_TRUE(mock_server_->last_request()
                  .commit()
                  .config_params()
                  .single_client_with_standalone_invalidations());
  EXPECT_FALSE(mock_server_->last_request()
                   .commit()
                   .config_params()
                   .single_client_with_old_invalidations());
}

TEST_F(SyncerTest, ShouldPopulateSingleClientForOldInvalidations) {
  GetProcessor(BOOKMARKS)->AppendCommitRequest(
      ClientTagHash::FromHashed("tag1"), MakeBookmarkSpecificsToCommit(),
      "id1");

  // No other devices without standalone invalidations are interested in
  // bookmarks.
  context_->set_active_devices_invalidation_info(
      ActiveDevicesInvalidationInfo::Create(
          /*all_fcm_registration_tokens=*/{"token_1"},
          /*all_interested_data_types=*/{BOOKMARKS, PREFERENCES},
          /*fcm_token_and_interested_data_types=*/
          {{"token_1", {BOOKMARKS, PREFERENCES}}},
          /*old_invalidations_interested_data_types=*/{PREFERENCES}));
  ASSERT_TRUE(SyncShareNudge());
  EXPECT_FALSE(
      mock_server_->last_request().commit().config_params().single_client());
  EXPECT_FALSE(mock_server_->last_request()
                   .commit()
                   .config_params()
                   .single_client_with_standalone_invalidations());
  EXPECT_TRUE(mock_server_->last_request()
                  .commit()
                  .config_params()
                  .single_client_with_old_invalidations());
}

TEST_F(SyncerTest, ShouldPopulateFcmRegistrationTokens) {
  GetProcessor(BOOKMARKS)->AppendCommitRequest(
      ClientTagHash::FromHashed("tag1"), MakeBookmarkSpecificsToCommit(),
      "id1");

  context_->set_active_devices_invalidation_info(
      ActiveDevicesInvalidationInfo::Create(
          {"token"}, /*all_interested_data_types=*/{BOOKMARKS},
          /*fcm_token_and_interested_data_types=*/{{"token", {BOOKMARKS}}},
          /*old_invalidations_interested_data_types=*/{}));
  ASSERT_TRUE(SyncShareNudge());
  EXPECT_FALSE(
      mock_server_->last_request().commit().config_params().single_client());
  EXPECT_FALSE(mock_server_->last_request()
                   .commit()
                   .config_params()
                   .single_client_with_standalone_invalidations());
  EXPECT_THAT(mock_server_->last_sent_commit()
                  .config_params()
                  .devices_fcm_registration_tokens(),
              ElementsAre("token"));
  EXPECT_THAT(mock_server_->last_sent_commit()
                  .config_params()
                  .fcm_registration_tokens_for_interested_clients(),
              ElementsAre("token"));
}

TEST_F(SyncerTest, ShouldPopulateFcmRegistrationTokensForInterestedTypesOnly) {
  GetProcessor(BOOKMARKS)->AppendCommitRequest(
      ClientTagHash::FromHashed("tag1"), MakeBookmarkSpecificsToCommit(),
      "id1");

  context_->set_active_devices_invalidation_info(
      ActiveDevicesInvalidationInfo::Create(
          {"token_1", "token_2"}, /*all_interested_data_types=*/{BOOKMARKS},
          /*fcm_token_and_interested_data_types=*/
          {{"token_1", {BOOKMARKS}}, {"token_2", {PREFERENCES}}},
          /*old_invalidations_interested_data_types=*/{}));
  ASSERT_TRUE(SyncShareNudge());
  EXPECT_FALSE(
      mock_server_->last_request().commit().config_params().single_client());
  EXPECT_FALSE(mock_server_->last_request()
                   .commit()
                   .config_params()
                   .single_client_with_standalone_invalidations());
  EXPECT_THAT(mock_server_->last_sent_commit()
                  .config_params()
                  .devices_fcm_registration_tokens(),
              ElementsAre("token_1", "token_2"));
  EXPECT_THAT(mock_server_->last_sent_commit()
                  .config_params()
                  .fcm_registration_tokens_for_interested_clients(),
              ElementsAre("token_1"));
}

TEST_F(SyncerTest, ShouldNotPopulateTooManyFcmRegistrationTokens) {
  std::map<std::string, DataTypeSet> fcm_token_and_interested_data_types;
  for (size_t i = 0; i < 7; ++i) {
    fcm_token_and_interested_data_types["token_" + base::NumberToString(i)] = {
        BOOKMARKS};
  }
  GetProcessor(BOOKMARKS)->AppendCommitRequest(
      ClientTagHash::FromHashed("tag1"), MakeBookmarkSpecificsToCommit(),
      "id1");

  context_->set_active_devices_invalidation_info(
      ActiveDevicesInvalidationInfo::Create(
          {}, /*all_interested_data_types=*/{BOOKMARKS},
          std::move(fcm_token_and_interested_data_types),
          /*old_invalidations_interested_data_types=*/{}));
  ASSERT_TRUE(SyncShareNudge());
  EXPECT_FALSE(
      mock_server_->last_request().commit().config_params().single_client());
  EXPECT_FALSE(mock_server_->last_request()
                   .commit()
                   .config_params()
                   .single_client_with_standalone_invalidations());
  EXPECT_THAT(mock_server_->last_sent_commit()
                  .config_params()
                  .devices_fcm_registration_tokens(),
              IsEmpty());
  EXPECT_THAT(mock_server_->last_sent_commit()
                  .config_params()
                  .fcm_registration_tokens_for_interested_clients(),
              IsEmpty());
}

TEST_F(SyncerTest,
       ShouldNotPopulateOptimizationFlagsIfDeviceInfoRecentlyUpdated) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(
      kSkipInvalidationOptimizationsWhenDeviceInfoUpdated);

  EnableDatatype(DEVICE_INFO);
  mock_server_->AddUpdateSpecifics("id", /*parent_id=*/"", "name",
                                   /*version=*/1, /*sync_ts=*/10,
                                   /*is_dir=*/false, /*specifics=*/
                                   MakeSpecifics(DEVICE_INFO));
  GetProcessor(BOOKMARKS)->AppendCommitRequest(
      ClientTagHash::FromHashed("tag1"), MakeBookmarkSpecificsToCommit(),
      "id1");

  // No other devices are interested in bookmarks.
  context_->set_active_devices_invalidation_info(
      ActiveDevicesInvalidationInfo::Create(
          {"token"}, /*all_interested_data_types=*/{PREFERENCES},
          /*fcm_token_and_interested_data_types=*/{{"token", {PREFERENCES}}},
          /*old_invalidations_interested_data_types=*/{}));
  ASSERT_TRUE(SyncShareNudge());

  // All invalidation info should be ignored due to DeviceInfo update.
  EXPECT_FALSE(
      mock_server_->last_request().commit().config_params().single_client());
  EXPECT_FALSE(mock_server_->last_request()
                   .commit()
                   .config_params()
                   .single_client_with_standalone_invalidations());
  EXPECT_TRUE(mock_server_->last_sent_commit()
                  .config_params()
                  .devices_fcm_registration_tokens()
                  .empty());
  EXPECT_TRUE(mock_server_->last_sent_commit()
                  .config_params()
                  .fcm_registration_tokens_for_interested_clients()
                  .empty());
}

TEST_F(SyncerTest, ClientTagServerCreatedUpdatesWork) {
  mock_server_->AddUpdateDirectory("1", "0", "permitem1", 1, 10,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateClientTag("clienttag");

  EXPECT_TRUE(SyncShareNudge());

  ASSERT_EQ(1U, GetProcessor(BOOKMARKS)->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> updates_list =
      GetProcessor(BOOKMARKS)->GetNthUpdateResponse(0);
  EXPECT_EQ(1U, updates_list.size());

  const UpdateResponseData& update = *updates_list.back();
  const EntityData& entity = update.entity;

  EXPECT_EQ("permitem1", entity.name);
  EXPECT_EQ(ClientTagHash::FromHashed("clienttag"), entity.client_tag_hash);
  EXPECT_FALSE(entity.is_deleted());
}

TEST_F(SyncerTest, GetUpdatesSetsRequestedTypes) {
  // The expectations of this test happen in the MockConnectionManager's
  // GetUpdates handler.  EnableDatatype sets the expectation value from our
  // set of enabled/disabled datatypes.
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());

  EnableDatatype(AUTOFILL);
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());

  DisableDatatype(BOOKMARKS);
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());

  DisableDatatype(AUTOFILL);
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());

  DisableDatatype(PREFERENCES);
  EnableDatatype(AUTOFILL);
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
}

// A typical scenario: server and client each have one update for the other.
// This is the "happy path" alternative to UpdateFailsThenDontCommit.
TEST_F(SyncerTest, UpdateThenCommit) {
  std::string to_receive = "some_id1";
  std::string to_commit = "some_id2";
  std::string parent_id = "0";
  mock_server_->AddUpdateDirectory(to_receive, parent_id, "x", 1, 10,
                                   foreign_cache_guid(), "-1");
  GetProcessor(BOOKMARKS)->AppendCommitRequest(
      ClientTagHash::FromHashed("tag1"), MakeBookmarkSpecificsToCommit(),
      to_commit);

  EXPECT_TRUE(SyncShareNudge());

  // The sync cycle should have included a GetUpdate, then a commit.
  EXPECT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_THAT(mock_server_->committed_ids(), UnorderedElementsAre(to_commit));

  // The update should have been received.
  ASSERT_EQ(1U, GetProcessor(BOOKMARKS)->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> updates_list =
      GetProcessor(BOOKMARKS)->GetNthUpdateResponse(0);
  ASSERT_EQ(1U, updates_list.size());
  EXPECT_EQ(to_receive, updates_list[0]->entity.id);
}

// Same as above, but this time we fail to download updates.
// We should not attempt to commit anything unless we successfully downloaded
// updates, otherwise we risk causing a server-side conflict.
TEST_F(SyncerTest, UpdateFailsThenDontCommit) {
  std::string to_receive = "some_id1";
  std::string to_commit = "some_id2";
  std::string parent_id = "0";
  mock_server_->AddUpdateDirectory(to_receive, parent_id, "x", 1, 10,
                                   foreign_cache_guid(), "-1");
  GetProcessor(BOOKMARKS)->AppendCommitRequest(
      ClientTagHash::FromHashed("tag1"), MakeBookmarkSpecificsToCommit(),
      to_commit);

  mock_server_->FailNextPostBufferToPathCall();
  EXPECT_FALSE(SyncShareNudge());

  // We did not receive this update.
  EXPECT_EQ(0U, GetProcessor(BOOKMARKS)->GetNumUpdateResponses());

  // No commit should have been sent.
  EXPECT_FALSE(mock_server_->last_request().has_commit());
  EXPECT_THAT(mock_server_->committed_ids(), IsEmpty());

  // Inform the Mock we won't be fetching all updates.
  mock_server_->ClearUpdatesQueue();
}

// Downloads two updates successfully.
// This is the "happy path" alternative to ConfigureFailsDontApplyUpdates.
TEST_F(SyncerTest, ConfigureDownloadsTwoBatchesSuccess) {
  // Construct the first GetUpdates response.
  mock_server_->AddUpdatePref("id1", "", "one", 1, 10);
  mock_server_->SetChangesRemaining(1);
  mock_server_->NextUpdateBatch();

  // Construct the second GetUpdates response.
  mock_server_->AddUpdatePref("id2", "", "two", 2, 20);

  ASSERT_EQ(0U, GetProcessor(PREFERENCES)->GetNumUpdateResponses());

  SyncShareConfigure();

  // The type should have received the initial updates.
  EXPECT_EQ(1U, GetProcessor(PREFERENCES)->GetNumUpdateResponses());
}

// Same as the above case, but this time the second batch fails to download.
TEST_F(SyncerTest, ConfigureFailsDontApplyUpdates) {
  // The scenario: we have two batches of updates with one update each.  A
  // normal confgure step would download all the updates one batch at a time and
  // apply them.  This configure will succeed in downloading the first batch
  // then fail when downloading the second.
  mock_server_->FailNthPostBufferToPathCall(2);

  // Construct the first GetUpdates response.
  mock_server_->AddUpdatePref("id1", "", "one", 1, 10);
  mock_server_->SetChangesRemaining(1);
  mock_server_->NextUpdateBatch();

  // Construct the second GetUpdates response.
  mock_server_->AddUpdatePref("id2", "", "two", 2, 20);

  ASSERT_EQ(0U, GetProcessor(PREFERENCES)->GetNumUpdateResponses());

  SyncShareConfigure();

  // The processor should not have received the initial sync data.
  EXPECT_EQ(0U, GetProcessor(PREFERENCES)->GetNumUpdateResponses());

  // One update remains undownloaded.
  mock_server_->ClearUpdatesQueue();
}

// Tests that if type is not registered with DataTypeRegistry (e.g. because
// type's LoadModels failed), Syncer::ConfigureSyncShare runs without triggering
// DCHECK.
TEST_F(SyncerTest, ConfigureFailedUnregisteredType) {
  // Simulate type being unregistered before configuration by including type
  // that isn't registered with DataTypeRegistry.
  SyncShareConfigureTypes({APPS});

  // No explicit verification, DCHECK shouldn't have been triggered.
}

TEST_F(SyncerTest, GetKeySuccess) {
  KeystoreKeysHandler* keystore_keys_handler =
      data_type_registry_->keystore_keys_handler();
  EXPECT_TRUE(keystore_keys_handler->NeedKeystoreKey());

  SyncShareConfigure();

  EXPECT_FALSE(cycle_->status_controller().last_get_key_failed());
  EXPECT_FALSE(keystore_keys_handler->NeedKeystoreKey());
}

TEST_F(SyncerTest, GetKeyEmpty) {
  KeystoreKeysHandler* keystore_keys_handler =
      data_type_registry_->keystore_keys_handler();
  EXPECT_TRUE(keystore_keys_handler->NeedKeystoreKey());

  mock_server_->SetKeystoreKey(std::string());
  SyncShareConfigure();

  EXPECT_TRUE(cycle_->status_controller().last_get_key_failed());
  EXPECT_TRUE(keystore_keys_handler->NeedKeystoreKey());
}

// Verify that commit only types are never requested in GetUpdates, but still
// make it into the commit messages. Additionally, make sure failing GU types
// are correctly removed before commit.
TEST_F(SyncerTest, CommitOnlyTypes) {
  mock_server_->set_partial_failure(true);
  mock_server_->SetPartialFailureTypes({PREFERENCES});

  EnableDatatype(USER_EVENTS);

  GetProcessor(PREFERENCES)
      ->AppendCommitRequest(ClientTagHash::FromHashed("tag1"),
                            MakeSpecifics(PREFERENCES), "id1");
  GetProcessor(EXTENSIONS)
      ->AppendCommitRequest(ClientTagHash::FromHashed("tag2"),
                            MakeSpecifics(EXTENSIONS), "id2");
  GetProcessor(USER_EVENTS)
      ->AppendCommitRequest(ClientTagHash::FromHashed("tag3"),
                            MakeSpecifics(USER_EVENTS), "id3");

  EXPECT_TRUE(SyncShareNudge());

  ASSERT_EQ(2U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->requests()[0].has_get_updates());
  // MockConnectionManager will ensure USER_EVENTS was not included in the GU.
  EXPECT_EQ(
      4, mock_server_->requests()[0].get_updates().from_progress_marker_size());

  ASSERT_TRUE(mock_server_->requests()[1].has_commit());
  const sync_pb::CommitMessage commit = mock_server_->requests()[1].commit();
  EXPECT_EQ(2, commit.entries_size());
  EXPECT_TRUE(commit.entries(0).specifics().has_extension());
  EXPECT_TRUE(commit.entries(1).specifics().has_user_event());
}

enum {
  TEST_PARAM_BOOKMARK_ENABLE_BIT,
  TEST_PARAM_AUTOFILL_ENABLE_BIT,
  TEST_PARAM_BIT_COUNT
};

class MixedResult : public SyncerTest,
                    public ::testing::WithParamInterface<int> {
 protected:
  bool ShouldFailBookmarkCommit() {
    return (GetParam() & (1 << TEST_PARAM_BOOKMARK_ENABLE_BIT)) == 0;
  }
  bool ShouldFailAutofillCommit() {
    return (GetParam() & (1 << TEST_PARAM_AUTOFILL_ENABLE_BIT)) == 0;
  }
};

INSTANTIATE_TEST_SUITE_P(ExtensionsActivity,
                         MixedResult,
                         testing::Range(0, 1 << TEST_PARAM_BIT_COUNT));

TEST_P(MixedResult, ExtensionsActivity) {
  GetProcessor(PREFERENCES)
      ->AppendCommitRequest(ClientTagHash::FromHashed("pref1"),
                            MakeSpecifics(PREFERENCES), "prefid1");

  GetProcessor(BOOKMARKS)->AppendCommitRequest(
      ClientTagHash::FromHashed("bookmark1"), MakeBookmarkSpecificsToCommit(),
      "bookmarkid2");

  if (ShouldFailBookmarkCommit()) {
    mock_server_->SetTransientErrorId("bookmarkid2");
  }

  if (ShouldFailAutofillCommit()) {
    mock_server_->SetTransientErrorId("prefid1");
  }

  // Put some extensions activity records into the monitor.
  {
    ExtensionsActivity::Records records;
    records["ABC"].extension_id = "ABC";
    records["ABC"].bookmark_write_count = 2049U;
    records["xyz"].extension_id = "xyz";
    records["xyz"].bookmark_write_count = 4U;
    context_->extensions_activity()->PutRecords(records);
  }

  EXPECT_EQ(!ShouldFailBookmarkCommit() && !ShouldFailAutofillCommit(),
            SyncShareNudge());

  ExtensionsActivity::Records final_monitor_records;
  context_->extensions_activity()->GetAndClearRecords(&final_monitor_records);
  if (ShouldFailBookmarkCommit()) {
    ASSERT_EQ(2U, final_monitor_records.size())
        << "Should restore records after unsuccessful bookmark commit.";
    EXPECT_EQ("ABC", final_monitor_records["ABC"].extension_id);
    EXPECT_EQ("xyz", final_monitor_records["xyz"].extension_id);
    EXPECT_EQ(2049U, final_monitor_records["ABC"].bookmark_write_count);
    EXPECT_EQ(4U, final_monitor_records["xyz"].bookmark_write_count);
  } else {
    EXPECT_TRUE(final_monitor_records.empty())
        << "Should not restore records after successful bookmark commit.";
  }
}

}  // namespace syncer
