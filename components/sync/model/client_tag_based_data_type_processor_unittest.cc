// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/client_tag_based_data_type_processor.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/processor_entity.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync/test/fake_data_type_sync_bridge.h"
#include "components/sync/test/mock_data_type_worker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using base::test::EqualsProto;
using sync_pb::DataTypeState;
using sync_pb::EntityMetadata;
using sync_pb::EntitySpecifics;
using testing::Not;
using testing::NotNull;

const char kDefaultAuthenticatedAccountId[] = "DefaultAccountId";

const char kKey1[] = "key1";
const char kKey2[] = "key2";
const char kKey3[] = "key3";
const char kKey4[] = "key4";
const char kKey5[] = "key5";
const char kValue1[] = "value1";
const char kValue2[] = "value2";
const char kValue3[] = "value3";

const char kCacheGuid[] = "TestCacheGuid";

// Typically used for verification after a delete. The specifics given to the
// worker/processor will not have been initialized and thus empty.
const EntitySpecifics kEmptySpecifics;

ClientTagHash GetHash(DataType type, const std::string& key) {
  return ClientTagHash::FromUnhashed(
      type, FakeDataTypeSyncBridge::ClientTagFromKey(key));
}

ClientTagHash GetPrefHash(const std::string& key) {
  return GetHash(PREFERENCES, key);
}

ClientTagHash GetSharedTabHash(const std::string& key) {
  return GetHash(SHARED_TAB_GROUP_DATA, key);
}

EntitySpecifics GeneratePrefSpecifics(const std::string& key,
                                      const std::string& value) {
  EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(key);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

EntitySpecifics GenerateUserEventSpecifics(int64_t event_time_usec,
                                           int64_t navigation_id) {
  EntitySpecifics specifics;
  specifics.mutable_user_event()->set_event_time_usec(event_time_usec);
  specifics.mutable_user_event()->set_navigation_id(navigation_id);
  return specifics;
}

EntitySpecifics GenerateSharedTabSpecifics(
    std::string guid,
    sync_pb::UniquePosition unique_position) {
  EntitySpecifics specifics;
  specifics.mutable_shared_tab_group_data()->set_guid(std::move(guid));
  *specifics.mutable_shared_tab_group_data()
       ->mutable_tab()
       ->mutable_unique_position() = std::move(unique_position);
  return specifics;
}

std::unique_ptr<EntityData> GenerateEntityData(
    DataType type,
    const std::string& key,
    const EntitySpecifics& specifics) {
  std::unique_ptr<EntityData> entity_data = std::make_unique<EntityData>();
  entity_data->client_tag_hash = GetHash(type, key);
  entity_data->specifics = specifics;
  entity_data->name = key;
  return entity_data;
}

std::unique_ptr<EntityData> GeneratePrefEntityData(const std::string& key,
                                                   const std::string& value) {
  return GenerateEntityData(PREFERENCES, key,
                            GeneratePrefSpecifics(key, value));
}

std::unique_ptr<EntityData> GenerateSharedTabGroupEntityData(
    const std::string& guid,
    sync_pb::UniquePosition unique_position) {
  return GenerateEntityData(
      SHARED_TAB_GROUP_DATA, guid,
      GenerateSharedTabSpecifics(guid, std::move(unique_position)));
}

EntitySpecifics WritePrefItem(FakeDataTypeSyncBridge* bridge,
                              const std::string& key,
                              const std::string& value) {
  std::unique_ptr<EntityData> entity_data = GeneratePrefEntityData(key, value);
  EntitySpecifics specifics_copy = entity_data->specifics;
  bridge->WriteItem(key, std::move(entity_data));
  return specifics_copy;
}

const std::string& GetPrefValue(const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_preference());
  return entity_data.specifics.preference().value();
}

EntitySpecifics WriteUserEventItem(FakeDataTypeSyncBridge* bridge,
                                   int64_t event_time,
                                   int64_t navigation_id) {
  std::string key = base::NumberToString(event_time);
  EntitySpecifics specifics =
      GenerateUserEventSpecifics(event_time, navigation_id);
  bridge->WriteItem(key, GenerateEntityData(USER_EVENTS, key, specifics));
  return specifics;
}

void CaptureCommitRequest(CommitRequestDataList* dst,
                          CommitRequestDataList&& src) {
  *dst = std::move(src);
}

void CaptureTypeEntitiesCount(TypeEntitiesCount* dst,
                              const TypeEntitiesCount& count) {
  *dst = count;
}

sync_pb::UniquePosition ExtractUniquePositionFromSharedTab(
    const sync_pb::EntitySpecifics& specifics) {
  return specifics.shared_tab_group_data().tab().unique_position();
}

class TestDataTypeSyncBridge : public FakeDataTypeSyncBridge {
 public:
  TestDataTypeSyncBridge(DataType data_type, bool supports_incremental_updates)
      : FakeDataTypeSyncBridge(
            data_type,
            std::make_unique<ClientTagBasedDataTypeProcessor>(
                data_type,
                /*dump_stack=*/base::RepeatingClosure())),
        supports_incremental_updates_(supports_incremental_updates) {}

  TestDataTypeSyncBridge(std::unique_ptr<TestDataTypeSyncBridge> other,
                         DataType data_type,
                         bool supports_clear_all)
      : TestDataTypeSyncBridge(data_type, supports_clear_all) {
    std::swap(db_, other->db_);
  }

  ~TestDataTypeSyncBridge() override = default;

  void OnSyncStarting(
      const syncer::DataTypeActivationRequest& request) override {
    FakeDataTypeSyncBridge::OnSyncStarting(request);
    sync_started_ = true;
  }

  void ApplyDisableSyncChanges(std::unique_ptr<MetadataChangeList>
                                   delete_metadata_change_list) override {
    sync_started_ = false;
    FakeDataTypeSyncBridge::ApplyDisableSyncChanges(
        std::move(delete_metadata_change_list));
  }

  void OnSyncPaused() override {
    sync_started_ = false;
    FakeDataTypeSyncBridge::OnSyncPaused();
  }

  std::string GetStorageKey(const EntityData& entity_data) override {
    get_storage_key_call_count_++;
    return FakeDataTypeSyncBridge::GetStorageKey(entity_data);
  }

  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override {
    if (entity_specifics.has_preference()) {
      sync_pb::EntitySpecifics trimmed_specifics = entity_specifics;
      trimmed_specifics.mutable_preference()->clear_value();
      return trimmed_specifics;
    }
    return FakeDataTypeSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
        entity_specifics);
  }

  void SetInitialSyncState(
      sync_pb::DataTypeState::InitialSyncState initial_sync_state) {
    DataTypeState data_type_state(db().data_type_state());
    data_type_state.set_initial_sync_state(initial_sync_state);
    data_type_state.set_cache_guid(kCacheGuid);
    data_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromDataType(type()));
    data_type_state.set_authenticated_account_id(
        kDefaultAuthenticatedAccountId);
    db_->set_data_type_state(data_type_state);
  }

  int merge_call_count() const { return merge_call_count_; }
  int apply_call_count() const { return apply_call_count_; }
  int get_storage_key_call_count() const { return get_storage_key_call_count_; }
  int commit_failures_count() const { return commit_failures_count_; }

  bool sync_started() const { return sync_started_; }

  // FakeDataTypeSyncBridge overrides.

  bool SupportsIncrementalUpdates() const override {
    return supports_incremental_updates_;
  }

  std::optional<ModelError> MergeFullSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_data) override {
    merge_call_count_++;
    if (!SupportsIncrementalUpdates()) {
      // If the bridge does not support incremental updates, it should clear
      // local data in MergeFullSyncData.
      db_->ClearAllData();
    }
    return FakeDataTypeSyncBridge::MergeFullSyncData(
        std::move(metadata_change_list), std::move(entity_data));
  }
  std::optional<ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override {
    apply_call_count_++;
    return FakeDataTypeSyncBridge::ApplyIncrementalSyncChanges(
        std::move(metadata_change_list), std::move(entity_changes));
  }

  void OnCommitAttemptErrors(
      const FailedCommitResponseDataList& error_response_list) override {
    if (!on_commit_attempt_errors_callback_.is_null()) {
      std::move(on_commit_attempt_errors_callback_).Run(error_response_list);
    }
  }

  CommitAttemptFailedBehavior OnCommitAttemptFailed(
      syncer::SyncCommitError commit_error) override {
    commit_failures_count_++;
    return commit_attempt_failed_behaviour_;
  }

  void SetOnCommitAttemptErrorsCallback(
      base::OnceCallback<void(const FailedCommitResponseDataList&)> callback) {
    on_commit_attempt_errors_callback_ = std::move(callback);
  }

  void EnableRetriesOnCommitFailure() {
    commit_attempt_failed_behaviour_ =
        CommitAttemptFailedBehavior::kShouldRetryOnNextCycle;
  }

 private:
  const bool supports_incremental_updates_;

  bool sync_started_ = false;

  // The number of times MergeFullSyncData has been called.
  int merge_call_count_ = 0;
  int apply_call_count_ = 0;
  int get_storage_key_call_count_ = 0;
  int commit_failures_count_ = 0;

  CommitAttemptFailedBehavior commit_attempt_failed_behaviour_ =
      CommitAttemptFailedBehavior::kDontRetryOnNextCycle;

  base::OnceCallback<void(const FailedCommitResponseDataList&)>
      on_commit_attempt_errors_callback_;
};

}  // namespace

// Tests the various functionality of ClientTagBasedDataTypeProcessor.
//
// The processor sits between the bridge (implemented by this test class) and
// the worker, which is represented by a MockDataTypeWorker. This test suite
// exercises the initialization flows (whether initial sync is done, performing
// the initial merge, etc) as well as normal functionality:
//
// - Initialization before the initial sync and merge correctly performs a merge
//   and initializes the metadata in storage.
// - Initialization after the initial sync correctly loads metadata and queues
//   any pending commits.
// - Put and Delete calls from the bridge result in the correct metadata in
//   storage and the correct commit requests on the worker side.
// - Updates and commit responses from the worker correctly affect data and
//   metadata in storage on the bridge side.
class ClientTagBasedDataTypeProcessorTest : public ::testing::Test {
 public:
  ClientTagBasedDataTypeProcessorTest() = default;
  ~ClientTagBasedDataTypeProcessorTest() override { CheckPostConditions(); }

  void SetUp() override {
    bridge_ = std::make_unique<TestDataTypeSyncBridge>(
        GetDataType(), SupportsIncrementalUpdates());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void InitializeToMetadataLoaded(
      sync_pb::DataTypeState::InitialSyncState initial_sync_state =
          sync_pb::DataTypeState::INITIAL_SYNC_DONE) {
    if (initial_sync_state !=
        sync_pb::DataTypeState::INITIAL_SYNC_STATE_UNSPECIFIED) {
      bridge()->SetInitialSyncState(initial_sync_state);
    }
    ModelReadyToSync();
  }

  // Initialize to a "ready-to-commit" state.
  void InitializeToReadyState() {
    InitializeToMetadataLoaded();
    OnSyncStarting();
  }

  void ModelReadyToSync() {
    type_processor()->ModelReadyToSync(db()->CreateMetadataBatch());
    WaitForStartCallbackIfNeeded();
  }

  void OnSyncStarting(const std::string& authenticated_account_id =
                          kDefaultAuthenticatedAccountId,
                      const std::string& cache_guid = kCacheGuid,
                      SyncMode sync_mode = SyncMode::kFull) {
    DataTypeActivationRequest request;
    request.error_handler =
        base::BindRepeating(&ClientTagBasedDataTypeProcessorTest::ErrorReceived,
                            base::Unretained(this));
    request.cache_guid = cache_guid;
    request.authenticated_account_id =
        CoreAccountId::FromString(authenticated_account_id);
    request.sync_mode = sync_mode;
    request.configuration_start_time = base::Time::Now();

    error_reported_ = false;

    // |run_loop_| may exist here if OnSyncStarting is called without resetting
    // state. But it is safe to remove it.
    ASSERT_TRUE(!run_loop_ || !run_loop_->running());
    run_loop_ = std::make_unique<base::RunLoop>();
    type_processor()->OnSyncStarting(
        request,
        base::BindOnce(&ClientTagBasedDataTypeProcessorTest::OnReadyToConnect,
                       base::Unretained(this)));
    WaitForStartCallbackIfNeeded();
  }

  void DisconnectSync() {
    type_processor()->DisconnectSync();
    worker_ = nullptr;
  }

  // Writes data for |key| and simulates a commit response for it.
  EntitySpecifics WriteItemAndAck(const std::string& key,
                                  const std::string& value) {
    EntitySpecifics specifics = WritePrefItem(bridge(), key, value);
    base::RunLoop().RunUntilIdle();
    worker()->VerifyPendingCommits({{GetPrefHash(key)}});
    worker()->AckOnePendingCommit();
    EXPECT_EQ(0U, worker()->GetNumPendingCommits());
    return specifics;
  }

  void WriteItemAndAck(const std::string& key,
                       std::unique_ptr<EntityData> entity_data) {
    bridge()->WriteItem(key, std::move(entity_data));
    worker()->VerifyPendingCommits({{GetPrefHash(key)}});
    worker()->AckOnePendingCommit();
    EXPECT_EQ(0U, worker()->GetNumPendingCommits());
    return;
  }

  const ProcessorEntity* GetEntityForStorageKey(
      const std::string& storage_key) {
    return type_processor()->entity_tracker_->GetEntityForStorageKey(
        storage_key);
  }

  void ResetState(bool keep_db) {
    bridge_ = keep_db ? std::make_unique<TestDataTypeSyncBridge>(
                            std::move(bridge_), GetDataType(),
                            SupportsIncrementalUpdates())
                      : std::make_unique<TestDataTypeSyncBridge>(
                            GetDataType(), SupportsIncrementalUpdates());
    worker_ = nullptr;
    run_loop_.reset();
    CheckPostConditions();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  virtual DataType GetDataType() { return PREFERENCES; }

  virtual bool SupportsIncrementalUpdates() { return true; }

  // Wipes existing DB and simulates a pending update of a server-known item.
  EntitySpecifics ResetStateWriteItem(const std::string& name,
                                      const std::string& value) {
    ResetState(false);
    InitializeToReadyState();
    EXPECT_EQ(0U, ProcessorEntityCount());
    WriteItemAndAck(name, "acked-value");
    EntitySpecifics specifics = WritePrefItem(bridge(), name, value);
    EXPECT_EQ(1U, ProcessorEntityCount());
    ResetState(true);
    return specifics;
  }

  // Wipes existing DB and simulates a pending deletion of a server-known item.
  void ResetStateDeleteItem(const std::string& name, const std::string& value) {
    ResetState(false);
    InitializeToReadyState();
    EXPECT_EQ(0U, ProcessorEntityCount());
    WriteItemAndAck(name, value);
    EXPECT_EQ(1U, ProcessorEntityCount());
    bridge()->DeleteItem(name);
    EXPECT_EQ(1U, ProcessorEntityCount());
    ResetState(true);
  }

  // Return the number of entities the processor has metadata for.
  size_t ProcessorEntityCount() const {
    if (type_processor()->entity_tracker_) {
      return type_processor()->entity_tracker_->size();
    }
    return 0;
  }

  // Expect to receive an error from the processor.
  void ExpectError(ClientTagBasedDataTypeProcessor::ErrorSite error_site) {
    EXPECT_FALSE(expect_error_);
    expect_error_ = error_site;
  }

  TestDataTypeSyncBridge* bridge() const { return bridge_.get(); }

  FakeDataTypeSyncBridge::Store* db() const { return bridge()->mutable_db(); }

  MockDataTypeWorker* worker() const { return worker_.get(); }

  ClientTagBasedDataTypeProcessor* type_processor() const {
    return static_cast<ClientTagBasedDataTypeProcessor*>(
        bridge()->change_processor());
  }

  sync_pb::DataTypeState::Invalidation BuildInvalidation(
      int64_t version,
      const std::string& payload) {
    sync_pb::DataTypeState::Invalidation inv;
    inv.set_version(version);
    inv.set_hint(payload);
    return inv;
  }

 protected:
  void CheckPostConditions() { EXPECT_FALSE(expect_error_); }

  void OnReadyToConnect(std::unique_ptr<DataTypeActivationResponse> context) {
    worker_ =
        MockDataTypeWorker::CreateWorkerAndConnectSync(std::move(context));

    // The processor uses DataTypeProcessorProxy, which requires processing
    // tasks to complete.
    task_environment_.RunUntilIdle();

    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void ErrorReceived(const ModelError& error) {
    EXPECT_TRUE(expect_error_);
    histogram_tester_->ExpectBucketCount("Sync.DataTypeErrorSite.PREFERENCE",
                                         *expect_error_, /*count=*/1);
    expect_error_ = std::nullopt;
    error_reported_ = true;
    // Do not expect for a start callback anymore.
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  sync_pb::DataTypeState data_type_state() {
    return worker_ ? worker_->data_type_state() : sync_pb::DataTypeState();
  }

  void WaitForStartCallbackIfNeeded() {
    if (!type_processor()->IsModelReadyToSyncForTest() ||
        !bridge()->sync_started()) {
      return;
    }
    ASSERT_TRUE(run_loop_);
    run_loop_->Run();
  }

  bool error_reported() const { return error_reported_; }

 private:
  // This sets SequencedTaskRunner::CurrentDefaultHandle on the current thread,
  // which the type processor will pick up as the sync task runner.
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<TestDataTypeSyncBridge> bridge_;

  // This run loop is used to wait for OnReadyToConnect is called.
  std::unique_ptr<base::RunLoop> run_loop_;

  // The current mock queue.
  std::unique_ptr<MockDataTypeWorker> worker_;

  // Whether to expect an error from the processor (and from which site).
  std::optional<ClientTagBasedDataTypeProcessor::ErrorSite> expect_error_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  bool error_reported_ = false;
};

TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldExposeNewlyTrackedAccountId) {
  ModelReadyToSync();
  ASSERT_EQ("", type_processor()->TrackedAccountId());
  OnSyncStarting();
  worker()->UpdateFromServer();
  EXPECT_EQ(kDefaultAuthenticatedAccountId,
            type_processor()->TrackedAccountId());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldExposePreviouslyTrackedAccountId) {
  std::unique_ptr<MetadataBatch> metadata_batch = db()->CreateMetadataBatch();
  sync_pb::DataTypeState data_type_state(metadata_batch->GetDataTypeState());
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  data_type_state.set_cache_guid(kCacheGuid);
  data_type_state.set_authenticated_account_id("PersistedAccountId");
  data_type_state.mutable_progress_marker()->set_data_type_id(
      GetSpecificsFieldNumberFromDataType(GetDataType()));
  metadata_batch->SetDataTypeState(data_type_state);
  type_processor()->ModelReadyToSync(std::move(metadata_batch));

  // Even prior to starting sync, the account ID should already be tracked.
  EXPECT_EQ("PersistedAccountId", type_processor()->TrackedAccountId());

  // If sync gets started, the account should still be tracked.
  OnSyncStarting("PersistedAccountId");
  EXPECT_EQ("PersistedAccountId", type_processor()->TrackedAccountId());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldExposeNewlyTrackedAccountIdIfChanged) {
  std::unique_ptr<MetadataBatch> metadata_batch = db()->CreateMetadataBatch();
  sync_pb::DataTypeState data_type_state(metadata_batch->GetDataTypeState());
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  data_type_state.set_cache_guid(kCacheGuid);
  data_type_state.set_authenticated_account_id("PersistedAccountId");
  data_type_state.mutable_progress_marker()->set_data_type_id(
      GetSpecificsFieldNumberFromDataType(GetDataType()));
  metadata_batch->SetDataTypeState(data_type_state);
  type_processor()->ModelReadyToSync(std::move(metadata_batch));

  // Even prior to starting sync, the account ID should already be tracked.
  ASSERT_EQ("PersistedAccountId", type_processor()->TrackedAccountId());

  // If sync gets started, the new account should be tracked.
  OnSyncStarting("NewAccountId");
  EXPECT_TRUE(type_processor()->IsTrackingMetadata());
  EXPECT_EQ("NewAccountId", type_processor()->TrackedAccountId());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldExposeNewlyAddedInvalidations) {
  // Populate the bridge's metadata with some non-empty values for us to later
  // check that it hasn't been cleared.
  sync_pb::DataTypeState::Invalidation inv_1 = BuildInvalidation(1, "hint_1");
  sync_pb::DataTypeState::Invalidation inv_2 = BuildInvalidation(2, "hint_2");
  InitializeToReadyState();
  type_processor()->StorePendingInvalidations({inv_1, inv_2});

  DataTypeState data_type_state(db()->data_type_state());
  EXPECT_EQ(2, data_type_state.invalidations_size());

  EXPECT_EQ(inv_1.hint(), data_type_state.invalidations(0).hint());
  EXPECT_EQ(inv_1.version(), data_type_state.invalidations(0).version());

  EXPECT_EQ(inv_2.hint(), data_type_state.invalidations(1).hint());
  EXPECT_EQ(inv_2.version(), data_type_state.invalidations(1).version());
}

TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldExposeNewlyTrackedCacheGuid) {
  ModelReadyToSync();
  ASSERT_EQ("", type_processor()->TrackedCacheGuid());
  OnSyncStarting();
  worker()->UpdateFromServer();
  EXPECT_EQ(kCacheGuid, type_processor()->TrackedCacheGuid());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldExposePreviouslyTrackedCacheGuid) {
  std::unique_ptr<MetadataBatch> metadata_batch = db()->CreateMetadataBatch();
  sync_pb::DataTypeState data_type_state(metadata_batch->GetDataTypeState());
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  data_type_state.set_cache_guid("PersistedCacheGuid");
  data_type_state.set_authenticated_account_id(kDefaultAuthenticatedAccountId);
  data_type_state.mutable_progress_marker()->set_data_type_id(
      GetSpecificsFieldNumberFromDataType(GetDataType()));
  metadata_batch->SetDataTypeState(data_type_state);
  type_processor()->ModelReadyToSync(std::move(metadata_batch));

  // Even prior to starting sync, the cache guid should be set.
  EXPECT_EQ("PersistedCacheGuid", type_processor()->TrackedCacheGuid());

  // If sync gets started, the cache guid should still be set.
  OnSyncStarting(kDefaultAuthenticatedAccountId, "PersistedCacheGuid");
  EXPECT_EQ("PersistedCacheGuid", type_processor()->TrackedCacheGuid());
}

// Test that an initial sync handles local and remote items properly.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldMergeLocalAndRemoteChanges) {
  ModelReadyToSync();
  OnSyncStarting();

  // Local write before initial sync.
  WritePrefItem(bridge(), kKey1, kValue1);

  // Has data, but no metadata, entity in the processor, or commit request.
  EXPECT_EQ(1U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  EXPECT_EQ(0, bridge()->merge_call_count());
  // Initial sync with one server item.
  worker()->UpdateFromServer(GetPrefHash(kKey2),
                             GeneratePrefSpecifics(kKey2, kValue2));
  EXPECT_EQ(1, bridge()->merge_call_count());

  // Now have data and metadata for both items, as well as a commit request for
  // the local item.
  EXPECT_EQ(2U, db()->data_count());
  EXPECT_EQ(2U, db()->metadata_count());
  EXPECT_EQ(2U, ProcessorEntityCount());
  EXPECT_EQ(1, db()->GetMetadata(kKey1).sequence_number());
  EXPECT_EQ(0, db()->GetMetadata(kKey2).sequence_number());
  EXPECT_TRUE(db()->GetMetadata(kKey1).has_possibly_trimmed_base_specifics());
  EXPECT_TRUE(db()->GetMetadata(kKey2).has_possibly_trimmed_base_specifics());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldExposePossiblyTrimmedRemoteSpecifics) {
  ModelReadyToSync();
  OnSyncStarting();

  sync_pb::EntitySpecifics specififcs = GeneratePrefSpecifics(kKey1, kValue1);
  *specififcs.mutable_preference()->mutable_unknown_fields() = kValue2;
  worker()->UpdateFromServer(GetPrefHash(kKey1), specififcs);

  sync_pb::PreferenceSpecifics cached_preference =
      type_processor()->GetPossiblyTrimmedRemoteSpecifics(kKey1).preference();

  // Below verifies that
  // TestDataTypeSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics() is
  // honored.
  // Preserved fields.
  EXPECT_EQ(cached_preference.name(), kKey1);
  EXPECT_EQ(cached_preference.unknown_fields(), kValue2);
  // Trimmed field.
  EXPECT_FALSE(cached_preference.has_value());
}

// Test that an initial sync filters out tombstones in the processor.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldFilterOutInitialTombstones) {
  ModelReadyToSync();
  OnSyncStarting();

  EXPECT_EQ(0, bridge()->merge_call_count());
  // Initial sync with a tombstone. The fake bridge checks that it doesn't get
  // any tombstones in its MergeFullSyncData function.
  worker()->TombstoneFromServer(GetPrefHash(kKey1));
  EXPECT_EQ(1, bridge()->merge_call_count());

  // Should still have no data, metadata, or commit requests.
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Test that an initial sync filters out updates for root nodes in the
// processor.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldFilterOutInitialRootNodes) {
  ModelReadyToSync();
  OnSyncStarting();

  UpdateResponseDataList update;
  update.push_back(worker()->GenerateTypeRootUpdateData(DataType::SESSIONS));

  worker()->UpdateFromServer(std::move(update));
  // Root node update should be filtered out.
  EXPECT_EQ(0U, ProcessorEntityCount());
}

// Test that subsequent starts don't call MergeFullSyncData.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldApplyIncrementalUpdates) {
  // This sets initial_sync_state to "done".
  InitializeToMetadataLoaded();

  // Write an item before sync connects.
  WritePrefItem(bridge(), kKey1, kValue1);
  EXPECT_EQ(1U, db()->data_count());
  EXPECT_EQ(1U, db()->metadata_count());

  // Check that data coming from sync is treated as a normal GetUpdates.
  OnSyncStarting();
  worker()->UpdateFromServer(GetPrefHash(kKey2),
                             GeneratePrefSpecifics(kKey2, kValue2));
  EXPECT_EQ(0, bridge()->merge_call_count());
  EXPECT_EQ(1, bridge()->apply_call_count());
  EXPECT_EQ(2U, db()->data_count());
  EXPECT_EQ(2U, db()->metadata_count());
}

TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldReportErrorDuringActivation) {
  InitializeToMetadataLoaded();

  std::optional<ModelError> received_error;
  DataTypeActivationRequest request;
  request.error_handler = base::BindLambdaForTesting(
      [&](const ModelError& error) { received_error = error; });

  request.cache_guid = kCacheGuid;
  request.authenticated_account_id =
      CoreAccountId::FromString(kDefaultAuthenticatedAccountId);
  request.sync_mode = SyncMode::kFull;
  request.configuration_start_time = base::Time::Now();

  base::RunLoop loop;
  std::unique_ptr<syncer::DataTypeActivationResponse>
      data_type_activation_response;
  type_processor()->OnSyncStarting(
      request,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<syncer::DataTypeActivationResponse> response) {
            data_type_activation_response = std::move(response);
            loop.Quit();
          }));
  loop.Run();

  ASSERT_NE(nullptr, data_type_activation_response);

  // Report error while the activation is in flight, i.e. before ConnectSync()
  // is invoked.
  ModelError error{FROM_HERE, "boom"};
  type_processor()->ReportError(error);

  // Mimic completion.
  OnReadyToConnect(std::move(data_type_activation_response));

  // The error should have been reported.
  ASSERT_TRUE(type_processor()->GetError().has_value());
  EXPECT_EQ(type_processor()->GetError()->location(), error.location());
  ASSERT_TRUE(received_error.has_value());
  EXPECT_EQ(received_error->location(), error.location());
}

// Test that an error during the merge is propagated to the error handler.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldReportErrorDuringMerge) {
  ModelReadyToSync();
  OnSyncStarting();

  bridge()->ErrorOnNextCall();
  ExpectError(ClientTagBasedDataTypeProcessor::ErrorSite::kApplyFullUpdates);
  worker()->UpdateFromServer();
}

// Test that errors before it's called are passed to |start_callback| correctly.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldDeferErrorsBeforeStart) {
  type_processor()->ReportError({FROM_HERE, "boom"});
  ExpectError(ClientTagBasedDataTypeProcessor::ErrorSite::kReportedByBridge);
  OnSyncStarting();

  // Test OnSyncStarting happening first.
  ResetState(false);
  OnSyncStarting();
  ExpectError(ClientTagBasedDataTypeProcessor::ErrorSite::kReportedByBridge);
  type_processor()->ReportError({FROM_HERE, "boom"});

  // Test an error loading pending data.
  ResetStateWriteItem(kKey1, kValue1);
  bridge()->ErrorOnNextCall();
  InitializeToMetadataLoaded();
  ExpectError(ClientTagBasedDataTypeProcessor::ErrorSite::kReportedByBridge);
  OnSyncStarting();

  // Test an error prior to metadata load.
  ResetState(false);
  type_processor()->ReportError({FROM_HERE, "boom"});
  ExpectError(ClientTagBasedDataTypeProcessor::ErrorSite::kReportedByBridge);
  OnSyncStarting();
  ModelReadyToSync();

  // Test an error prior to pending data load.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  type_processor()->ReportError({FROM_HERE, "boom"});
  ExpectError(ClientTagBasedDataTypeProcessor::ErrorSite::kReportedByBridge);
  OnSyncStarting();
}

// Tests cases where pending data loads synchronously.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldHandleSynchronousDataLoad) {
  // Model, sync.
  EntitySpecifics specifics1 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {GetPrefHash(kKey1)}, {specifics1});

  // Sync, model.
  EntitySpecifics specifics2 = ResetStateWriteItem(kKey1, kValue1);
  OnSyncStarting();
  InitializeToMetadataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {GetPrefHash(kKey1)}, {specifics2});
}

// This test covers race conditions during loading a pending delete. All cases
// start with no processor and one item with a pending delete. There are two
// different events that can occur in any order once metadata is loaded, since
// for a deletion there is no data to load:
//
// - Sync gets connected.
// - Optionally, a put or delete happens to the item (repeated deletes should be
//   handled properly).
//
// This results in 1 + 4 = 5 orderings of the events.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldLoadPendingDelete) {
  // Connect.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {GetPrefHash(kKey1)}, {kEmptySpecifics});

  // Connect, put.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  EntitySpecifics specifics1 = WritePrefItem(bridge(), kKey1, kValue2);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {GetPrefHash(kKey1)}, {kEmptySpecifics});
  worker()->VerifyNthPendingCommit(1, {GetPrefHash(kKey1)}, {specifics1});

  // Put, connect.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  EntitySpecifics specifics2 = WritePrefItem(bridge(), kKey1, kValue2);
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {GetPrefHash(kKey1)}, {specifics2});

  // Connect, delete.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {GetPrefHash(kKey1)}, {kEmptySpecifics});
  worker()->VerifyNthPendingCommit(1, {GetPrefHash(kKey1)}, {kEmptySpecifics});

  // Delete, connect.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  bridge()->DeleteItem(kKey1);
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {GetPrefHash(kKey1)}, {kEmptySpecifics});
}

// Test that loading a committed item does not queue another commit.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotQueueAnotherCommitIfAlreadyCommitted) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  ResetState(true);

  // Test that a new processor loads the metadata without committing.
  InitializeToReadyState();
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Creates a new item locally.
// Thoroughly tests the data generated by a local item creation.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldCommitLocalCreation) {
  base::HistogramTester histogram_tester;
  InitializeToReadyState();
  ASSERT_EQ(0U, worker()->GetNumPendingCommits());
  ASSERT_FALSE(type_processor()->IsEntityUnsynced(kKey1));

  WritePrefItem(bridge(), kKey1, kValue1);

  EXPECT_TRUE(type_processor()->IsEntityUnsynced(kKey1));
  EXPECT_FALSE(type_processor()->GetEntityCreationTime(kKey1).is_null());
  EXPECT_EQ(type_processor()->GetEntityCreationTime(kKey1),
            type_processor()->GetEntityModificationTime(kKey1));

  // Verify the commit request this operation has triggered.
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});
  const CommitRequestData* tag1_request_data =
      worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1));
  ASSERT_TRUE(tag1_request_data);
  const EntityData& tag1_data = *tag1_request_data->entity;

  EXPECT_EQ(kUncommittedVersion, tag1_request_data->base_version);
  EXPECT_TRUE(tag1_data.id.empty());
  EXPECT_FALSE(tag1_data.creation_time.is_null());
  EXPECT_FALSE(tag1_data.modification_time.is_null());
  EXPECT_EQ(kKey1, tag1_data.name);
  EXPECT_FALSE(tag1_data.is_deleted());
  EXPECT_EQ(kKey1, tag1_data.specifics.preference().name());
  EXPECT_EQ(kValue1, tag1_data.specifics.preference().value());

  EXPECT_EQ(1U, db()->metadata_count());
  const EntityMetadata metadata = db()->GetMetadata(kKey1);
  EXPECT_TRUE(metadata.has_client_tag_hash());
  EXPECT_FALSE(metadata.has_server_id());
  EXPECT_FALSE(metadata.is_deleted());
  EXPECT_EQ(1, metadata.sequence_number());
  EXPECT_EQ(0, metadata.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata.server_version());
  EXPECT_TRUE(metadata.has_creation_time());
  EXPECT_TRUE(metadata.has_modification_time());
  EXPECT_TRUE(metadata.has_specifics_hash());
  EXPECT_TRUE(metadata.has_possibly_trimmed_base_specifics());

  worker()->AckOnePendingCommit();
  EXPECT_FALSE(type_processor()->IsEntityUnsynced(kKey1));
  EXPECT_EQ(1U, db()->metadata_count());
  const EntityMetadata acked_metadata = db()->GetMetadata(kKey1);
  EXPECT_TRUE(acked_metadata.has_server_id());
  EXPECT_EQ(1, acked_metadata.sequence_number());
  EXPECT_EQ(1, acked_metadata.acked_sequence_number());
  EXPECT_EQ(1, acked_metadata.server_version());

  histogram_tester.ExpectBucketCount(
      "Sync.DataTypeOrphanMetadata.Put",
      /*bucket=*/DataTypeHistogramValue(GetDataType()), /*count=*/0);
}

// Creates a new item locally while another item exists for the same client tag
// hash.
TEST_F(ClientTagBasedDataTypeProcessorTest, CommitShouldOverwriteExistingItem) {
  base::HistogramTester histogram_tester;
  // Provide custom client tags for this test.
  bridge()->SetSupportsGetClientTag(false);
  const syncer::ClientTagHash kClientTagHash =
      ClientTagHash::FromUnhashed(PREFERENCES, "tag");

  InitializeToReadyState();
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  std::unique_ptr<EntityData> entity_data1 =
      GeneratePrefEntityData(kKey1, kValue1);
  // Use a custom client tag hash - independent of the storage key.
  entity_data1->client_tag_hash = kClientTagHash;
  bridge()->WriteItem(kKey1, std::move(entity_data1));
  EXPECT_EQ(1U, db()->metadata_count());
  worker()->VerifyPendingCommits({{kClientTagHash}});

  std::unique_ptr<EntityData> entity_data2 =
      GeneratePrefEntityData(kKey2, kValue2);
  // Use the same custom client tag hash as for entity 1.
  entity_data2->client_tag_hash = kClientTagHash;
  bridge()->WriteItem(kKey2, std::move(entity_data2));
  EXPECT_EQ(1U, db()->metadata_count());
  EXPECT_TRUE(db()->GetMetadata(kKey2).has_possibly_trimmed_base_specifics());

  histogram_tester.ExpectBucketCount(
      "Sync.DataTypeOrphanMetadata.Put",
      /*bucket=*/DataTypeHistogramValue(GetDataType()),
      /*count=*/1);
}

// Test that an error applying metadata changes from a commit response is
// propagated to the error handler.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldReportErrorApplyingAck) {
  InitializeToReadyState();
  WritePrefItem(bridge(), kKey1, kValue1);
  bridge()->ErrorOnNextCall();
  ExpectError(ClientTagBasedDataTypeProcessor::ErrorSite::
                  kApplyUpdatesOnCommitResponse);
  worker()->AckOnePendingCommit();
}

// The purpose of this test case is to test setting |client_tag_hash| and |id|
// on the EntityData object as we pass it into the Put method of the processor.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldOverrideFieldsForLocalUpdate) {
  const std::string kId1 = "cid1";
  const std::string kId2 = "cid2";

  InitializeToReadyState();
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  std::unique_ptr<EntityData> entity_data = std::make_unique<EntityData>();
  entity_data->specifics.mutable_preference()->set_name(kKey1);
  entity_data->specifics.mutable_preference()->set_value(kValue1);

  entity_data->name = kKey1;
  entity_data->client_tag_hash = GetPrefHash(kKey1);
  entity_data->id = kId1;
  bridge()->WriteItem(kKey1, std::move(entity_data));

  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  ASSERT_FALSE(worker()->HasPendingCommitForHash(GetPrefHash(kKey3)));
  ASSERT_TRUE(worker()->HasPendingCommitForHash(GetPrefHash(kKey1)));
  EXPECT_EQ(1U, db()->metadata_count());
  ASSERT_TRUE(worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1)));
  const EntityData& out_entity1 =
      *worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1))->entity;
  const EntityMetadata metadata_v1 = db()->GetMetadata(kKey1);

  EXPECT_EQ(kId1, out_entity1.id);
  EXPECT_NE(GetPrefHash(kKey3), out_entity1.client_tag_hash);
  EXPECT_EQ(kValue1, out_entity1.specifics.preference().value());
  EXPECT_EQ(kId1, metadata_v1.server_id());
  EXPECT_EQ(metadata_v1.client_tag_hash(), out_entity1.client_tag_hash.value());

  entity_data = std::make_unique<EntityData>();
  // This is a sketchy move here, changing the name will change the generated
  // storage key and client tag values.
  entity_data->specifics.mutable_preference()->set_name(kKey2);
  entity_data->specifics.mutable_preference()->set_value(kValue2);
  entity_data->name = kKey2;
  entity_data->client_tag_hash = GetPrefHash(kKey3);
  // Make sure ID isn't overwritten either.
  entity_data->id = kId2;
  bridge()->WriteItem(kKey1, std::move(entity_data));

  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  ASSERT_FALSE(worker()->HasPendingCommitForHash(GetPrefHash(kKey3)));
  ASSERT_TRUE(worker()->HasPendingCommitForHash(GetPrefHash(kKey1)));
  EXPECT_EQ(1U, db()->metadata_count());
  ASSERT_TRUE(worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1)));
  const EntityData& out_entity2 =
      *worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1))->entity;
  const EntityMetadata metadata_v2 = db()->GetMetadata(kKey1);

  EXPECT_EQ(kValue2, out_entity2.specifics.preference().value());
  // Should still see old cid1 value, override is not respected on update.
  EXPECT_EQ(kId1, out_entity2.id);
  EXPECT_EQ(kId1, metadata_v2.server_id());
  EXPECT_EQ(metadata_v2.client_tag_hash(), out_entity2.client_tag_hash.value());

  // Specifics have changed so the hashes should not match.
  EXPECT_NE(metadata_v1.specifics_hash(), metadata_v2.specifics_hash());
}

// Creates a new local item, then modifies it after it has been acked.
// Thoroughly tests data generated by modification of server-unknown item.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldCommitLocalUpdate) {
  InitializeToReadyState();

  WritePrefItem(bridge(), kKey1, kValue1);
  ASSERT_EQ(1U, db()->metadata_count());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});

  const EntityMetadata metadata_v1 = db()->GetMetadata(kKey1);
  int64_t request_data_v1_sequence_number;
  {
    // request_data_v1 is valid only while the commit is still pending.
    const CommitRequestData* request_data_v1 =
        worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1));
    ASSERT_TRUE(request_data_v1);
    const EntityData& data_v1 = *request_data_v1->entity;
    EXPECT_EQ(data_v1.specifics.preference().value(), kValue1);
    request_data_v1_sequence_number = request_data_v1->sequence_number;
  }

  worker()->AckOnePendingCommit();
  ASSERT_FALSE(type_processor()->IsEntityUnsynced(kKey1));
  const base::Time ctime = type_processor()->GetEntityCreationTime(kKey1);
  ASSERT_FALSE(ctime.is_null());
  ASSERT_EQ(ctime, type_processor()->GetEntityModificationTime(kKey1));

  // Make sure the clock advances.
  base::PlatformThread::Sleep(base::Milliseconds(1));
  ASSERT_NE(ctime, base::Time::Now());

  WritePrefItem(bridge(), kKey1, kValue2);
  EXPECT_EQ(1U, db()->metadata_count());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});

  EXPECT_TRUE(type_processor()->IsEntityUnsynced(kKey1));
  EXPECT_EQ(ctime, type_processor()->GetEntityCreationTime(kKey1));
  const base::Time mtime = type_processor()->GetEntityModificationTime(kKey1);
  EXPECT_NE(ctime, mtime);

  const CommitRequestData* request_data_v2 =
      worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1));
  ASSERT_TRUE(request_data_v2);
  const EntityData& data_v2 = *request_data_v2->entity;
  const EntityMetadata metadata_v2 = db()->GetMetadata(kKey1);

  // Test some of the relations between old and new commit requests.
  EXPECT_GT(request_data_v2->sequence_number, request_data_v1_sequence_number);
  EXPECT_EQ(data_v2.specifics.preference().value(), kValue2);

  // Perform a thorough examination of the update-generated request.
  EXPECT_NE(kUncommittedVersion, request_data_v2->base_version);
  EXPECT_FALSE(data_v2.id.empty());
  EXPECT_EQ(ctime, data_v2.creation_time);
  EXPECT_EQ(mtime, data_v2.modification_time);
  EXPECT_EQ(kKey1, data_v2.name);
  EXPECT_FALSE(data_v2.is_deleted());
  EXPECT_EQ(kKey1, data_v2.specifics.preference().name());
  EXPECT_EQ(kValue2, data_v2.specifics.preference().value());

  // Perform a thorough examination of the local sync metadata.
  EXPECT_FALSE(metadata_v1.has_server_id());
  EXPECT_FALSE(metadata_v1.is_deleted());
  EXPECT_EQ(1, metadata_v1.sequence_number());
  EXPECT_EQ(0, metadata_v1.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v1.server_version());

  EXPECT_FALSE(metadata_v2.server_id().empty());
  EXPECT_FALSE(metadata_v2.is_deleted());
  EXPECT_EQ(2, metadata_v2.sequence_number());
  EXPECT_EQ(1, metadata_v2.acked_sequence_number());
  EXPECT_NE(kUncommittedVersion, metadata_v2.server_version());

  EXPECT_EQ(metadata_v1.client_tag_hash(), metadata_v2.client_tag_hash());
  EXPECT_NE(metadata_v1.specifics_hash(), metadata_v2.specifics_hash());
}

// Same as above, but modifies the item BEFORE it has been acked.
// Thoroughly tests data generated by modification of server-unknown item.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldCommitLocalUpdateBeforeCreationAck) {
  InitializeToReadyState();

  WritePrefItem(bridge(), kKey1, kValue1);
  ASSERT_EQ(1U, db()->metadata_count());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});

  const CommitRequestData* request_data_v1 =
      worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1));
  ASSERT_TRUE(request_data_v1);
  const EntityData& data_v1 = *request_data_v1->entity;
  const EntityMetadata metadata_v1 = db()->GetMetadata(kKey1);

  ASSERT_TRUE(type_processor()->IsEntityUnsynced(kKey1));
  const base::Time ctime = type_processor()->GetEntityCreationTime(kKey1);
  ASSERT_EQ(ctime, type_processor()->GetEntityModificationTime(kKey1));
  ASSERT_FALSE(ctime.is_null());

  // Make sure the clock advances.
  base::PlatformThread::Sleep(base::Milliseconds(1));
  ASSERT_NE(ctime, base::Time::Now());

  WritePrefItem(bridge(), kKey1, kValue2);
  EXPECT_EQ(1U, db()->metadata_count());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}, {GetPrefHash(kKey1)}});

  EXPECT_TRUE(type_processor()->IsEntityUnsynced(kKey1));
  EXPECT_EQ(ctime, type_processor()->GetEntityCreationTime(kKey1));
  const base::Time mtime = type_processor()->GetEntityModificationTime(kKey1);
  EXPECT_NE(mtime, ctime);

  const CommitRequestData* request_data_v2 =
      worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1));
  ASSERT_TRUE(request_data_v2);
  const EntityData& data_v2 = *request_data_v2->entity;
  const EntityMetadata metadata_v2 = db()->GetMetadata(kKey1);

  // Test some of the relations between old and new commit requests.
  EXPECT_GT(request_data_v2->sequence_number, request_data_v1->sequence_number);
  EXPECT_EQ(data_v1.specifics.preference().value(), kValue1);

  // Perform a thorough examination of the update-generated request.
  EXPECT_EQ(kUncommittedVersion, request_data_v2->base_version);
  EXPECT_TRUE(data_v2.id.empty());
  EXPECT_EQ(ctime, data_v2.creation_time);
  EXPECT_EQ(mtime, data_v2.modification_time);
  EXPECT_EQ(kKey1, data_v2.name);
  EXPECT_FALSE(data_v2.is_deleted());
  EXPECT_EQ(kKey1, data_v2.specifics.preference().name());
  EXPECT_EQ(kValue2, data_v2.specifics.preference().value());

  // Perform a thorough examination of the local sync metadata.
  EXPECT_FALSE(metadata_v1.has_server_id());
  EXPECT_FALSE(metadata_v1.is_deleted());
  EXPECT_EQ(1, metadata_v1.sequence_number());
  EXPECT_EQ(0, metadata_v1.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v1.server_version());

  EXPECT_FALSE(metadata_v2.has_server_id());
  EXPECT_FALSE(metadata_v2.is_deleted());
  EXPECT_EQ(2, metadata_v2.sequence_number());
  EXPECT_EQ(0, metadata_v2.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v2.server_version());

  EXPECT_EQ(metadata_v1.client_tag_hash(), metadata_v2.client_tag_hash());
  EXPECT_NE(metadata_v1.specifics_hash(), metadata_v2.specifics_hash());
}

// Tests that a local update that doesn't change specifics doesn't generate a
// commit request.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldIgnoreRedundantLocalUpdate) {
  InitializeToReadyState();
  WritePrefItem(bridge(), kKey1, kValue1);
  ASSERT_EQ(1U, db()->metadata_count());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});

  const base::Time ctime = type_processor()->GetEntityCreationTime(kKey1);
  const base::Time mtime = type_processor()->GetEntityModificationTime(kKey1);
  ASSERT_FALSE(ctime.is_null());
  ASSERT_FALSE(mtime.is_null());

  WritePrefItem(bridge(), kKey1, kValue1);
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});

  EXPECT_EQ(ctime, type_processor()->GetEntityCreationTime(kKey1));
  EXPECT_EQ(mtime, type_processor()->GetEntityModificationTime(kKey1));
}

// Test that an error applying changes from a server update is
// propagated to the error handler.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldReportErrorApplyingUpdate) {
  InitializeToReadyState();
  bridge()->ErrorOnNextCall();
  ExpectError(
      ClientTagBasedDataTypeProcessor::ErrorSite::kApplyIncrementalUpdates);
  worker()->UpdateFromServer(GetPrefHash(kKey1),
                             GeneratePrefSpecifics(kKey1, kValue1));
}

// Tests locally deleting an acknowledged item.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldCommitLocalDeletion) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  const EntityMetadata metadata_v1 = db()->GetMetadata(kKey1);
  EXPECT_FALSE(metadata_v1.is_deleted());
  EXPECT_EQ(1, metadata_v1.sequence_number());
  EXPECT_EQ(1, metadata_v1.acked_sequence_number());
  EXPECT_EQ(1, metadata_v1.server_version());

  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(0U, db()->data_count());
  // Metadata is not removed until the commit response comes back.
  EXPECT_EQ(1U, db()->metadata_count());
  EXPECT_EQ(1U, ProcessorEntityCount());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});

  const EntityMetadata metadata_v2 = db()->GetMetadata(kKey1);
  EXPECT_TRUE(metadata_v2.is_deleted());
  EXPECT_EQ(2, metadata_v2.sequence_number());
  EXPECT_EQ(1, metadata_v2.acked_sequence_number());
  EXPECT_EQ(1, metadata_v2.server_version());

  // Ack the delete and check that the metadata is cleared.
  worker()->AckOnePendingCommit();
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());

  // Create item again.
  WriteItemAndAck(kKey1, kValue1);
  const EntityMetadata metadata_v3 = db()->GetMetadata(kKey1);
  EXPECT_FALSE(metadata_v3.is_deleted());
  EXPECT_EQ(1, metadata_v3.sequence_number());
  EXPECT_EQ(1, metadata_v3.acked_sequence_number());
  EXPECT_EQ(3, metadata_v3.server_version());
}

// Tests that item created and deleted before sync cycle doesn't get committed.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotCommitLocalDeletionOfUncommittedEntity) {
  InitializeToMetadataLoaded();
  WritePrefItem(bridge(), kKey1, kValue1);
  bridge()->DeleteItem(kKey1);

  OnSyncStarting();
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Tests creating and deleting an item locally before receiving a commit
// response, then getting the commit responses.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldHandleLocalDeletionDuringLocalCreationCommit) {
  InitializeToReadyState();
  WritePrefItem(bridge(), kKey1, kValue1);
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});
  const CommitRequestData* data_v1 =
      worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1));
  ASSERT_TRUE(data_v1);

  const EntityMetadata metadata_v1 = db()->GetMetadata(kKey1);
  EXPECT_FALSE(metadata_v1.is_deleted());
  EXPECT_EQ(1, metadata_v1.sequence_number());
  EXPECT_EQ(0, metadata_v1.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v1.server_version());

  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(1U, db()->metadata_count());
  EXPECT_EQ(1U, ProcessorEntityCount());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}, {GetPrefHash(kKey1)}});

  const CommitRequestData* data_v2 =
      worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1));
  ASSERT_TRUE(data_v2);
  EXPECT_GT(data_v2->sequence_number, data_v1->sequence_number);
  EXPECT_TRUE(data_v2->entity->id.empty());
  EXPECT_EQ(kUncommittedVersion, data_v2->base_version);
  EXPECT_TRUE(data_v2->entity->is_deleted());

  const EntityMetadata metadata_v2 = db()->GetMetadata(kKey1);
  EXPECT_TRUE(metadata_v2.is_deleted());
  EXPECT_EQ(2, metadata_v2.sequence_number());
  EXPECT_EQ(0, metadata_v2.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v2.server_version());

  // A response for the first commit doesn't change much.
  worker()->AckOnePendingCommit();
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(1U, db()->metadata_count());
  EXPECT_EQ(1U, ProcessorEntityCount());

  const EntityMetadata metadata_v3 = db()->GetMetadata(kKey1);
  EXPECT_TRUE(metadata_v3.is_deleted());
  EXPECT_EQ(2, metadata_v3.sequence_number());
  EXPECT_EQ(1, metadata_v3.acked_sequence_number());
  EXPECT_EQ(1, metadata_v3.server_version());

  worker()->AckOnePendingCommit();
  // The delete was acked so the metadata should now be cleared.
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
}

TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldProcessRemoteDeletion) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_EQ(1U, db()->metadata_count());
  EXPECT_EQ(1U, db()->data_count());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  worker()->TombstoneFromServer(GetPrefHash(kKey1));
  // Delete from server should clear the data and all the metadata.
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  // Create item again.
  WriteItemAndAck(kKey1, kValue1);
  const EntityMetadata metadata = db()->GetMetadata(kKey1);
  EXPECT_FALSE(metadata.is_deleted());
  EXPECT_EQ(1, metadata.sequence_number());
  EXPECT_EQ(1, metadata.acked_sequence_number());
  EXPECT_EQ(3, metadata.server_version());
}

// Deletes an item we've never seen before.
// Should have no effect and not crash.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldIgnoreLocalDeletionOfUnknownEntity) {
  InitializeToReadyState();
  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Tests that after committing entity fails, processor includes this entity in
// consecutive commits.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldRetryCommitAfterServerError) {
  InitializeToReadyState();
  WritePrefItem(bridge(), kKey1, kValue1);
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});

  // Entity is sent to server. Processor shouldn't include it in local changes.
  CommitRequestDataList commit_request;
  type_processor()->GetLocalChanges(
      INT_MAX, base::BindOnce(&CaptureCommitRequest, &commit_request));
  EXPECT_TRUE(commit_request.empty());

  // Fail commit from worker side indicating this entity was not committed.
  // Processor should include it in consecutive GetLocalChanges responses.
  worker()->FailOneCommit();
  type_processor()->GetLocalChanges(
      INT_MAX, base::BindOnce(&CaptureCommitRequest, &commit_request));
  EXPECT_EQ(1U, commit_request.size());
  EXPECT_EQ(GetPrefHash(kKey1), commit_request[0]->entity->client_tag_hash);
}

// Tests that after committing entity fails, processor includes this entity in
// consecutive commits. This test differs from the above one for the case when
// there is an HTTP error.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldRetryCommitAfterFullCommitFailure) {
  InitializeToReadyState();
  bridge()->EnableRetriesOnCommitFailure();
  WritePrefItem(bridge(), kKey1, kValue1);
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});

  // Entity is sent to server. Processor shouldn't include it in local changes.
  CommitRequestDataList commit_request;
  type_processor()->GetLocalChanges(
      INT_MAX, base::BindOnce(&CaptureCommitRequest, &commit_request));
  EXPECT_TRUE(commit_request.empty());

  // Fail commit from worker side indicating this entity was not committed.
  // Processor should include it in consecutive GetLocalChanges responses.
  worker()->FailFullCommitRequest();
  type_processor()->GetLocalChanges(
      INT_MAX, base::BindOnce(&CaptureCommitRequest, &commit_request));
  ASSERT_EQ(1U, commit_request.size());
  EXPECT_EQ(GetPrefHash(kKey1), commit_request[0]->entity->client_tag_hash);
}

// Tests that GetLocalChanges honors max_entries parameter.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldTruncateLocalChangesToMaxSize) {
  InitializeToReadyState();

  // Avoid that the worker immediately invokes GetLocalChanges() as soon as it
  // gets nudged, otherwise the test body cannot invoke GetLocalChanges()
  // manually.
  worker()->DisableGetLocalChangesUponNudge();

  WritePrefItem(bridge(), kKey1, kValue1);
  WritePrefItem(bridge(), kKey2, kValue2);

  // Request at most one entity per batch, ensure that only one was returned.
  CommitRequestDataList commit_request;
  type_processor()->GetLocalChanges(
      /*max_entries=*/1,
      base::BindOnce(&CaptureCommitRequest, &commit_request));
  EXPECT_EQ(1U, commit_request.size());
}

// Creates two different sync items.
// Verifies that the second has no effect on the first.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldHandleTwoIndependentItems) {
  InitializeToReadyState();
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  WritePrefItem(bridge(), kKey1, kValue1);
  EXPECT_EQ(1U, db()->data_count());
  EXPECT_EQ(1U, db()->metadata_count());
  const EntityMetadata metadata1 = db()->GetMetadata(kKey1);

  // There should be one commit request for this item only.
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});

  WritePrefItem(bridge(), kKey2, kValue2);
  EXPECT_EQ(2U, db()->data_count());
  EXPECT_EQ(2U, db()->metadata_count());
  const EntityMetadata metadata2 = db()->GetMetadata(kKey2);

  // The second write should trigger another single-item commit request.
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}, {GetPrefHash(kKey2)}});

  EXPECT_FALSE(metadata1.is_deleted());
  EXPECT_EQ(1, metadata1.sequence_number());
  EXPECT_EQ(0, metadata1.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata1.server_version());

  EXPECT_FALSE(metadata2.is_deleted());
  EXPECT_EQ(1, metadata2.sequence_number());
  EXPECT_EQ(0, metadata2.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata2.server_version());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotTreatMatchingChangesAsConflict) {
  InitializeToReadyState();
  EntitySpecifics specifics = WritePrefItem(bridge(), kKey1, kValue1);
  EXPECT_EQ(1U, db()->data_change_count());
  EXPECT_EQ(kValue1, GetPrefValue(db()->GetData(kKey1)));
  EXPECT_EQ(1U, db()->metadata_change_count());
  EXPECT_EQ(kUncommittedVersion, db()->GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});
  worker()->VerifyNthPendingCommit(0, {GetPrefHash(kKey1)}, {specifics});

  // Changes match doesn't call ResolveConflict in the bridge.
  base::HistogramTester histogram_tester;
  worker()->UpdateFromServer(GetPrefHash(kKey1), specifics);
  histogram_tester.ExpectUniqueSample(
      "Sync.DataTypeEntityConflictResolution.PREFERENCE",
      ConflictResolution::kChangesMatch, /*expected_bucket_count=*/1);

  // Updated metadata but not data; no new commit request.
  EXPECT_EQ(1U, db()->data_change_count());
  EXPECT_EQ(1, db()->GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldResolveConflictToLocalVersion) {
  InitializeToReadyState();
  // WriteAndAck entity to get id from the server.
  WriteItemAndAck(kKey1, kValue1);
  bridge()->SetConflictResolution(ConflictResolution::kUseLocal);

  // Change value locally and at the same time simulate conflicting update from
  // server.
  base::HistogramTester histogram_tester;
  EntitySpecifics specifics2 = WritePrefItem(bridge(), kKey1, kValue2);
  worker()->UpdateFromServer(GetPrefHash(kKey1),
                             GeneratePrefSpecifics(kKey1, kValue3));
  histogram_tester.ExpectUniqueSample(
      "Sync.DataTypeEntityConflictResolution.PREFERENCE",
      ConflictResolution::kUseLocal, /*expected_bucket_count=*/1);

  // Updated metadata but not data; new commit request.
  EXPECT_EQ(2U, db()->data_change_count());
  EXPECT_EQ(4U, db()->metadata_change_count());
  EXPECT_EQ(2, db()->GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}, {GetPrefHash(kKey1)}});
  worker()->VerifyNthPendingCommit(1, {GetPrefHash(kKey1)}, {specifics2});
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldResolveConflictToLocalUndeletion) {
  InitializeToReadyState();
  ASSERT_EQ(0U, worker()->GetNumPendingCommits());

  WritePrefItem(bridge(), kKey1, kValue1);
  ASSERT_EQ(1U, worker()->GetNumPendingCommits());
  ASSERT_TRUE(worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1)));
  ASSERT_TRUE(worker()
                  ->GetLatestPendingCommitForHash(GetPrefHash(kKey1))
                  ->entity->id.empty());

  // The update from the server should be mostly ignored because local wins, but
  // the server ID should be updated.
  bridge()->SetConflictResolution(ConflictResolution::kUseLocal);
  worker()->UpdateFromServer(GetPrefHash(kKey1),
                             GeneratePrefSpecifics(kKey1, kValue3));
  // In this test setup, the processor's nudge for commit immediately pulls
  // updates from the processor and list them as pending commits, so we should
  // see two commits at this point.
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());

  // Verify the commit request this operation has triggered.
  const CommitRequestData* tag1_request_data =
      worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1));
  ASSERT_TRUE(tag1_request_data);
  const EntityData& tag1_data = *tag1_request_data->entity;

  EXPECT_EQ(1, tag1_request_data->base_version);
  EXPECT_FALSE(tag1_data.id.empty());
  EXPECT_FALSE(tag1_data.creation_time.is_null());
  EXPECT_FALSE(tag1_data.modification_time.is_null());
  EXPECT_EQ(kKey1, tag1_data.name);
  EXPECT_FALSE(tag1_data.is_deleted());
  EXPECT_EQ(kKey1, tag1_data.specifics.preference().name());
  EXPECT_EQ(kValue1, tag1_data.specifics.preference().value());

  EXPECT_EQ(1U, db()->metadata_count());
  const EntityMetadata metadata = db()->GetMetadata(kKey1);
  EXPECT_TRUE(metadata.has_client_tag_hash());
  EXPECT_TRUE(metadata.has_server_id());
  EXPECT_FALSE(metadata.is_deleted());
  EXPECT_EQ(1, metadata.sequence_number());
  EXPECT_EQ(0, metadata.acked_sequence_number());
  EXPECT_EQ(1, metadata.server_version());
  EXPECT_TRUE(metadata.has_creation_time());
  EXPECT_TRUE(metadata.has_modification_time());
  EXPECT_TRUE(metadata.has_specifics_hash());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldResolveConflictToRemoteUndeletion) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  ASSERT_EQ(0U, worker()->GetNumPendingCommits());

  bridge()->DeleteItem(kKey1);
  ASSERT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});
  ASSERT_TRUE(worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1)));
  ASSERT_TRUE(worker()
                  ->GetLatestPendingCommitForHash(GetPrefHash(kKey1))
                  ->entity->is_deleted());
  ASSERT_EQ(2U, db()->data_change_count());
  ASSERT_EQ(3U, db()->metadata_change_count());
  ASSERT_TRUE(type_processor()->IsTrackingEntityForTest(kKey1));

  base::HistogramTester histogram_tester;
  worker()->UpdateFromServer(GetPrefHash(kKey1),
                             GeneratePrefSpecifics(kKey1, kValue2));
  histogram_tester.ExpectUniqueSample(
      "Sync.DataTypeEntityConflictResolution.PREFERENCE",
      ConflictResolution::kUseRemote, /*expected_bucket_count=*/1);

  // Updated client data and metadata; no new commit request.
  EXPECT_TRUE(type_processor()->IsTrackingEntityForTest(kKey1));
  EXPECT_EQ(3U, db()->data_change_count());
  EXPECT_EQ(kValue2, GetPrefValue(db()->GetData(kKey1)));
  EXPECT_EQ(4U, db()->metadata_change_count());
  EXPECT_EQ(2, db()->GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldResolveConflictToRemoteUndeletionWithUpdateStorageKey) {
  bridge()->SetSupportsGetStorageKey(false);
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  ASSERT_EQ(0U, worker()->GetNumPendingCommits());

  bridge()->DeleteItem(kKey1);
  ASSERT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});
  ASSERT_TRUE(worker()->GetLatestPendingCommitForHash(GetPrefHash(kKey1)));
  ASSERT_TRUE(worker()
                  ->GetLatestPendingCommitForHash(GetPrefHash(kKey1))
                  ->entity->is_deleted());
  ASSERT_EQ(2U, db()->data_change_count());
  ASSERT_EQ(3U, db()->metadata_change_count());
  ASSERT_TRUE(type_processor()->IsTrackingEntityForTest(kKey1));

  worker()->UpdateFromServer(GetPrefHash(kKey1),
                             GeneratePrefSpecifics(kKey1, kValue2));

  // A new storage key should have been generated, which should replace the
  // previous when it comes to storing data and metadata.
  const std::string new_storage_key = bridge()->GetLastGeneratedStorageKey();
  ASSERT_NE(kKey1, new_storage_key);
  EXPECT_TRUE(db()->HasData(new_storage_key));
  EXPECT_TRUE(db()->HasMetadata(new_storage_key));
  EXPECT_TRUE(type_processor()->IsTrackingEntityForTest(new_storage_key));
  EXPECT_FALSE(db()->HasData(kKey1));
  EXPECT_FALSE(db()->HasMetadata(kKey1));
  EXPECT_FALSE(type_processor()->IsTrackingEntityForTest(kKey1));

  // Updated client data and metadata; no new commit request.
  EXPECT_EQ(3U, db()->data_change_count());
  EXPECT_EQ(kValue2, GetPrefValue(db()->GetData(new_storage_key)));
  EXPECT_EQ(5U, db()->metadata_change_count());
  EXPECT_EQ(2, db()->GetMetadata(new_storage_key).server_version());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldResolveConflictToRemoteVersion) {
  InitializeToReadyState();
  WritePrefItem(bridge(), kKey1, kValue1);
  bridge()->SetConflictResolution(ConflictResolution::kUseRemote);
  worker()->UpdateFromServer(GetPrefHash(kKey1),
                             GeneratePrefSpecifics(kKey1, kValue2));

  // Updated client data and metadata; no new commit request.
  EXPECT_EQ(2U, db()->data_change_count());
  EXPECT_EQ(kValue2, GetPrefValue(db()->GetData(kKey1)));
  EXPECT_EQ(2U, db()->metadata_change_count());
  EXPECT_EQ(1, db()->GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldResolveConflictToRemoteDeletion) {
  InitializeToReadyState();
  WritePrefItem(bridge(), kKey1, kValue1);
  bridge()->SetConflictResolution(ConflictResolution::kUseRemote);
  worker()->TombstoneFromServer(GetPrefHash(kKey1));

  // Updated client data and metadata; no new commit request.
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(2U, db()->data_change_count());
  EXPECT_EQ(2U, db()->metadata_change_count());
}

// Test proper handling of disconnect and reconnect.
//
// Creates items in various states of commit and verifies they re-attempt to
// commit on reconnect.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldDisconnectAndReconnect) {
  InitializeToReadyState();

  // The first item is fully committed.
  WriteItemAndAck(kKey1, kValue1);

  // The second item has a commit request in progress.
  WritePrefItem(bridge(), kKey2, kValue2);
  EXPECT_TRUE(worker()->HasPendingCommitForHash(GetPrefHash(kKey2)));

  DisconnectSync();

  // The third item is added after stopping.
  WritePrefItem(bridge(), kKey3, kValue3);

  // Reconnect.
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  EXPECT_EQ(2U, worker()->GetNthPendingCommit(0).size());

  // The first item was already in sync.
  EXPECT_FALSE(worker()->HasPendingCommitForHash(GetPrefHash(kKey1)));

  // The second item's commit was interrupted and should be retried.
  EXPECT_TRUE(worker()->HasPendingCommitForHash(GetPrefHash(kKey2)));

  // The third item's commit was not started until the reconnect.
  EXPECT_TRUE(worker()->HasPendingCommitForHash(GetPrefHash(kKey3)));
}

// Test proper handling of stop (without disabling sync) and re-enable.
//
// Creates items in various states of commit and verifies they do NOT attempt to
// commit on re-enable.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldStopAndKeepMetadata) {
  InitializeToReadyState();

  // The first item is fully committed.
  WriteItemAndAck(kKey1, kValue1);

  // The second item has a commit request in progress.
  WritePrefItem(bridge(), kKey2, kValue2);
  EXPECT_TRUE(worker()->HasPendingCommitForHash(GetPrefHash(kKey2)));

  type_processor()->OnSyncStopping(KEEP_METADATA);
  EXPECT_TRUE(type_processor()->IsTrackingMetadata());

  // The third item is added while paused.
  WritePrefItem(bridge(), kKey3, kValue3);

  // Now we re-enable.
  OnSyncStarting();
  worker()->UpdateFromServer();

  // The second item had a commit in progress, but it wasn't confirmed, so it
  // should be re-committed. The third item was added while paused, so should
  // get committed now.
  worker()->VerifyPendingCommits({{GetPrefHash(kKey2), GetPrefHash(kKey3)}});
}

// Test proper handling of disable and re-enable.
//
// Creates items in various states of commit and verifies they re-attempt to
// commit on re-enable.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldStopAndClearMetadata) {
  InitializeToReadyState();

  // The first item is fully committed.
  WriteItemAndAck(kKey1, kValue1);

  // The second item has a commit request in progress.
  WritePrefItem(bridge(), kKey2, kValue2);
  EXPECT_TRUE(worker()->HasPendingCommitForHash(GetPrefHash(kKey2)));

  type_processor()->OnSyncStopping(CLEAR_METADATA);
  EXPECT_FALSE(type_processor()->IsTrackingMetadata());

  // The third item is added after disable.
  WritePrefItem(bridge(), kKey3, kValue3);

  // Now we re-enable.
  OnSyncStarting();
  worker()->UpdateFromServer();
  EXPECT_TRUE(type_processor()->IsTrackingMetadata());

  // Once we're ready to commit, all three local items should consider
  // themselves uncommitted and pending for commit.
  worker()->VerifyPendingCommits(
      {{GetPrefHash(kKey1), GetPrefHash(kKey2), GetPrefHash(kKey3)}});
}

// Test proper handling of disable-sync before initial sync done.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotClearBridgeMetadataPriorToMergeFullSyncData) {
  // Populate the bridge's metadata with some non-empty values for us to later
  // check that it hasn't been cleared.
  const std::string kTestEncryptionKeyName = "TestEncryptionKey";
  DataTypeState data_type_state(db()->data_type_state());
  data_type_state.set_encryption_key_name(kTestEncryptionKeyName);
  bridge()->mutable_db()->set_data_type_state(data_type_state);

  ModelReadyToSync();
  OnSyncStarting();
  ASSERT_FALSE(type_processor()->IsTrackingMetadata());

  type_processor()->OnSyncStopping(CLEAR_METADATA);
  EXPECT_FALSE(type_processor()->IsTrackingMetadata());
  EXPECT_EQ(kTestEncryptionKeyName,
            db()->data_type_state().encryption_key_name());
}

// Test re-encrypt everything when desired encryption key changes.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldReencryptCommitsWithNewKey) {
  InitializeToReadyState();

  // Commit an item.
  EntitySpecifics specifics1 = WriteItemAndAck(kKey1, kValue1);
  // Create another item and don't wait for its commit response.
  EntitySpecifics specifics2 = WritePrefItem(bridge(), kKey2, kValue2);
  worker()->VerifyPendingCommits({{GetPrefHash(kKey2)}});
  EXPECT_EQ(1U, db()->GetMetadata(kKey1).sequence_number());
  EXPECT_EQ(1U, db()->GetMetadata(kKey2).sequence_number());

  // Receive notice that the account's desired encryption key has changed.
  worker()->UpdateWithEncryptionKey("k1");
  ASSERT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, {GetPrefHash(kKey1), GetPrefHash(kKey2)},
                                   {specifics1, specifics2});
  // Sequence numbers in the store are updated.
  EXPECT_EQ(2U, db()->GetMetadata(kKey1).sequence_number());
  EXPECT_EQ(2U, db()->GetMetadata(kKey2).sequence_number());
}

// Test that an error loading pending commit data for re-encryption is
// propagated to the error handler.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldHandleErrorWhileReencrypting) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  bridge()->ErrorOnNextCall();
  ExpectError(
      ClientTagBasedDataTypeProcessor::ErrorSite::kApplyIncrementalUpdates);
  worker()->UpdateWithEncryptionKey("k1");
}

// Test receipt of updates with new and old keys.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldReencryptUpdatesWithNewKey) {
  InitializeToReadyState();

  // Receive an unencrypted update.
  worker()->UpdateFromServer(GetPrefHash(kKey1),
                             GeneratePrefSpecifics(kKey1, kValue1));
  ASSERT_EQ(0U, worker()->GetNumPendingCommits());

  UpdateResponseDataList update;
  // Receive an entity with old encryption as part of the update.
  update.push_back(worker()->GenerateUpdateData(
      GetPrefHash(kKey2), GeneratePrefSpecifics(kKey2, kValue2), 1, "k1"));
  // Receive an entity with up-to-date encryption as part of the update.
  update.push_back(worker()->GenerateUpdateData(
      GetPrefHash(kKey3), GeneratePrefSpecifics(kKey3, kValue3), 1, "k2"));
  // Set desired encryption key to k2 to force updates to some items.
  worker()->UpdateWithEncryptionKey("k2", std::move(update));

  // kKey1 and kKey2 are queued for commit.
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1), GetPrefHash(kKey2)}});

  // Receive a separate update that was encrypted with key k1.
  worker()->UpdateFromServer(GetPrefHash(kKey4),
                             GeneratePrefSpecifics(kKey4, kValue1), 1, "k1");
  // Receipt of updates encrypted with old key also forces a re-encrypt commit.
  worker()->VerifyPendingCommits(
      {{GetPrefHash(kKey1), GetPrefHash(kKey2)}, {GetPrefHash(kKey4)}});

  // Receive an update that was encrypted with key k2.
  worker()->UpdateFromServer(GetPrefHash(kKey5),
                             GeneratePrefSpecifics(kKey5, kValue1), 1, "k2");
  // That was the correct key, so no re-encryption is required.
  worker()->VerifyPendingCommits(
      {{GetPrefHash(kKey1), GetPrefHash(kKey2)}, {GetPrefHash(kKey4)}});
}

// Test that re-encrypting enqueues the right data for kUseLocal conflicts.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldResolveConflictToLocalDuringReencryption) {
  InitializeToReadyState();
  // WriteAndAck entity to get id from the server.
  WriteItemAndAck(kKey1, kValue1);
  worker()->UpdateWithEncryptionKey("k1");

  EntitySpecifics specifics = WritePrefItem(bridge(), kKey1, kValue2);
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}, {GetPrefHash(kKey1)}});

  bridge()->SetConflictResolution(ConflictResolution::kUseLocal);
  // Unencrypted update needs to be re-commited with key k1.
  worker()->UpdateFromServer(GetPrefHash(kKey1),
                             GeneratePrefSpecifics(kKey1, kValue3), 1, "");

  // Ensure the re-commit has the correct value.
  EXPECT_EQ(3U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(2, {GetPrefHash(kKey1)}, {specifics});
  EXPECT_EQ(kValue2, GetPrefValue(db()->GetData(kKey1)));
}

// Test that re-encrypting enqueues the right data for kUseRemote conflicts.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldResolveConflictToRemoteDuringReencryption) {
  InitializeToReadyState();
  worker()->UpdateWithEncryptionKey("k1");
  WritePrefItem(bridge(), kKey1, kValue1);

  bridge()->SetConflictResolution(ConflictResolution::kUseRemote);
  // Unencrypted update needs to be re-commited with key k1.
  EntitySpecifics specifics = GeneratePrefSpecifics(kKey1, kValue2);
  worker()->UpdateFromServer(GetPrefHash(kKey1), specifics, 1, "");

  // Ensure the re-commit has the correct value.
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, {GetPrefHash(kKey1)}, {specifics});
  EXPECT_EQ(kValue2, GetPrefValue(db()->GetData(kKey1)));
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldHandleConflictWhileLoadingForReencryption) {
  InitializeToReadyState();
  // Create item and ack so its data is no longer cached.
  WriteItemAndAck(kKey1, kValue1);
  // Update key so that it needs to re-commit.
  worker()->UpdateWithEncryptionKey("k1");
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());

  // Unencrypted update needs to be re-commited with key k1.
  EntitySpecifics specifics = GeneratePrefSpecifics(kKey1, kValue2);
  worker()->UpdateFromServer(GetPrefHash(kKey1), specifics, 1, "");

  // Ensure the re-commit has the correct value.
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, {GetPrefHash(kKey1)}, {specifics});
  EXPECT_EQ(kValue2, GetPrefValue(db()->GetData(kKey1)));
}

// Tests that a real remote change wins over a local encryption-only change.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldIgnoreLocalEncryptionChange) {
  InitializeToReadyState();
  EntitySpecifics specifics = WriteItemAndAck(kKey1, kValue1);
  worker()->UpdateWithEncryptionKey("k1");
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {GetPrefHash(kKey1)}, {specifics});

  worker()->UpdateFromServer(GetPrefHash(kKey1),
                             GeneratePrefSpecifics(kKey1, kValue2));
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
}

// Tests that updates without client tags get dropped.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldDropRemoteUpdatesWithoutClientTags) {
  InitializeToReadyState();
  UpdateResponseDataList updates;
  updates.push_back(worker()->GenerateUpdateData(
      ClientTagHash(), GeneratePrefSpecifics(kKey1, kValue1), 1, "k1"));

  worker()->UpdateFromServer(std::move(updates));

  // Verify that the data wasn't actually stored.
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, db()->data_count());
}

// Tests that initial updates for transport-only mode (called "ephemeral
// storage" for historical reasons) result in reporting setup duration.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldReportEphemeralConfigurationTime) {
  InitializeToMetadataLoaded(
      sync_pb::DataTypeState::INITIAL_SYNC_STATE_UNSPECIFIED);
  OnSyncStarting(kDefaultAuthenticatedAccountId, kCacheGuid,
                 SyncMode::kTransportOnly);

  base::HistogramTester histogram_tester;

  UpdateResponseDataList updates;
  updates.push_back(worker()->GenerateUpdateData(
      ClientTagHash(), GeneratePrefSpecifics(kKey1, kValue1), 1, "k1"));
  worker()->UpdateFromServer(std::move(updates));

  ASSERT_EQ(1, bridge()->merge_call_count());
  // The duration should get recorded into the right histogram.
  histogram_tester.ExpectTotalCount(
      "Sync.DataTypeConfigurationTime.Ephemeral.PREFERENCE",
      /*count=*/1);
  histogram_tester.ExpectTotalCount(
      "Sync.DataTypeConfigurationTime.Persistent.PREFERENCE",
      /*count=*/0);
}

// Tests that initial updates for full-sync mode (called "persistent storage"
// for historical reasons) do not result in reporting setup duration.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldReportPersistentConfigurationTime) {
  InitializeToMetadataLoaded(
      sync_pb::DataTypeState::INITIAL_SYNC_STATE_UNSPECIFIED);
  OnSyncStarting();

  base::HistogramTester histogram_tester;

  UpdateResponseDataList updates;
  updates.push_back(worker()->GenerateUpdateData(
      ClientTagHash(), GeneratePrefSpecifics(kKey1, kValue1), 1, "k1"));
  worker()->UpdateFromServer(std::move(updates));

  ASSERT_EQ(1, bridge()->merge_call_count());
  // The duration should get recorded into the right histogram.
  histogram_tester.ExpectTotalCount(
      "Sync.DataTypeConfigurationTime.Ephemeral.PREFERENCE",
      /*count=*/0);
  histogram_tester.ExpectTotalCount(
      "Sync.DataTypeConfigurationTime.Persistent.PREFERENCE",
      /*count=*/1);
}

class FullUpdateClientTagBasedDataTypeProcessorTest
    : public ClientTagBasedDataTypeProcessorTest {
 protected:
  bool SupportsIncrementalUpdates() override { return false; }
};

// Tests that ClientTagBasedDataTypeProcessor can do garbage collection by
// version.
// Garbage collection by version is used by the server to replace all data on
// the client, and is implemented by calling MergeFullSyncData on the bridge.
TEST_F(FullUpdateClientTagBasedDataTypeProcessorTest,
       ShouldApplyGarbageCollectionByVersionFullUpdate) {
  InitializeToReadyState();
  UpdateResponseDataList updates;
  updates.push_back(worker()->GenerateUpdateData(
      ClientTagHash(), GeneratePrefSpecifics(kKey1, kValue1), 1, "k1"));
  updates.push_back(worker()->GenerateUpdateData(
      ClientTagHash(), GeneratePrefSpecifics(kKey2, kValue2), 2, "k2"));

  // Create 2 entries, one is version 3, another is version 1.
  sync_pb::GarbageCollectionDirective garbage_collection_directive;
  garbage_collection_directive.set_version_watermark(1);
  worker()->UpdateFromServer(std::move(updates), garbage_collection_directive);
  WriteItemAndAck(kKey1, kValue1);
  WriteItemAndAck(kKey2, kValue2);

  // Verify entries are created correctly.
  ASSERT_EQ(2U, ProcessorEntityCount());
  ASSERT_EQ(2U, db()->metadata_count());
  ASSERT_EQ(2U, db()->data_count());
  ASSERT_EQ(0U, worker()->GetNumPendingCommits());
  ASSERT_EQ(1, bridge()->merge_call_count());

  // Tell the client to delete all data.
  sync_pb::GarbageCollectionDirective new_directive;
  new_directive.set_version_watermark(2);
  worker()->UpdateFromServer({}, new_directive);

  // Verify that merge is called on the bridge to replace the current sync data.
  EXPECT_EQ(2, bridge()->merge_call_count());
  // Verify that the processor cleared all metadata.
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}
// Tests that full updates for transport-only mode (called "ephemeral storage"
// for historical reasons) result in reporting setup duration.
TEST_F(FullUpdateClientTagBasedDataTypeProcessorTest,
       ShouldReportEphemeralConfigurationTimeOnlyForFirstFullUpdate) {
  InitializeToMetadataLoaded(
      sync_pb::DataTypeState::INITIAL_SYNC_STATE_UNSPECIFIED);
  OnSyncStarting(kDefaultAuthenticatedAccountId, kCacheGuid,
                 SyncMode::kTransportOnly);

  UpdateResponseDataList updates1;
  updates1.push_back(worker()->GenerateUpdateData(
      ClientTagHash(), GeneratePrefSpecifics(kKey1, kValue1), 1, "k1"));
  sync_pb::GarbageCollectionDirective garbage_collection_directive;
  garbage_collection_directive.set_version_watermark(1);

  {
    base::HistogramTester histogram_tester;
    worker()->UpdateFromServer(std::move(updates1),
                               garbage_collection_directive);
    ASSERT_EQ(1, bridge()->merge_call_count());

    // The duration should get recorded.
    histogram_tester.ExpectTotalCount(
        "Sync.DataTypeConfigurationTime.Ephemeral.PREFERENCE",
        /*count=*/1);
  }

  {
    UpdateResponseDataList updates2;
    updates2.push_back(worker()->GenerateUpdateData(
        ClientTagHash(), GeneratePrefSpecifics(kKey1, kValue1), 1, "k1"));
    base::HistogramTester histogram_tester;
    // Send one more update with the same data.
    worker()->UpdateFromServer(std::move(updates2),
                               garbage_collection_directive);
    ASSERT_EQ(2, bridge()->merge_call_count());

    // The duration should not get recorded again.
    histogram_tester.ExpectTotalCount(
        "Sync.DataTypeConfigurationTime.Ephemeral.PREFERENCE",
        /*count=*/0);
  }
}

// Tests that the processor reports an error for updates without a version GC
// directive that are received for types that don't support incremental updates.
TEST_F(FullUpdateClientTagBasedDataTypeProcessorTest,
       ShouldReportErrorForUnsupportedIncrementalUpdate) {
  InitializeToReadyState();

  ExpectError(ClientTagBasedDataTypeProcessor::ErrorSite::
                  kSupportsIncrementalUpdatesMismatch);
  worker()->UpdateFromServer(GetPrefHash(kKey1),
                             GeneratePrefSpecifics(kKey1, kValue1));
}

// Tests that empty updates without a version GC are processed for types that
// don't support incremental updates. The only outcome if these updates should
// be storing an updated progress marker.
TEST_F(FullUpdateClientTagBasedDataTypeProcessorTest,
       ShouldProcessEmptyUpdate) {
  // Override the initial progress marker token so that we can check it gets
  // changed.
  sync_pb::DataTypeState initial_data_type_state(db()->data_type_state());
  initial_data_type_state.mutable_progress_marker()->set_token("OLD");
  db()->set_data_type_state(initial_data_type_state);

  InitializeToReadyState();

  sync_pb::DataTypeState new_data_type_state(db()->data_type_state());
  new_data_type_state.mutable_progress_marker()->set_token("NEW");
  worker()->UpdateDataTypeState(new_data_type_state);
  worker()->UpdateFromServer(UpdateResponseDataList());

  // Verify that the empty update was correctly passed into the bridge and that
  // it stored the updated progress marker.
  EXPECT_EQ(0, bridge()->merge_call_count());
  EXPECT_EQ(1, bridge()->apply_call_count());
  EXPECT_EQ("NEW", db()->data_type_state().progress_marker().token());
}

// Tests that the processor correctly handles an initial (non-empty) update
// without any gc directives (as it happens in the migration to USS).
TEST_F(FullUpdateClientTagBasedDataTypeProcessorTest,
       ShouldProcessInitialUpdate) {
  // Do not set any data type state to emulate that initial sync has not been
  // done yet.
  ModelReadyToSync();
  OnSyncStarting();

  worker()->UpdateFromServer(GetPrefHash(kKey1),
                             GeneratePrefSpecifics(kKey1, kValue1));
}

// Tests that a real local change wins over a remote encryption-only change.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldIgnoreRemoteEncryption) {
  InitializeToReadyState();
  EntitySpecifics specifics1 = WriteItemAndAck(kKey1, kValue1);

  EntitySpecifics specifics2 = WritePrefItem(bridge(), kKey1, kValue2);
  UpdateResponseDataList update;
  update.push_back(
      worker()->GenerateUpdateData(GetPrefHash(kKey1), specifics1, 1, "k1"));
  worker()->UpdateWithEncryptionKey("k1", std::move(update));

  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, {GetPrefHash(kKey1)}, {specifics2});
}

// Same as above but with two commit requests before one ack.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldIgnoreRemoteEncryptionInterleaved) {
  InitializeToReadyState();
  // WriteAndAck entity to get id from the server.
  WriteItemAndAck(kKey1, kValue1);
  EntitySpecifics specifics1 = WritePrefItem(bridge(), kKey1, kValue2);
  EntitySpecifics specifics2 = WritePrefItem(bridge(), kKey1, kValue3);
  worker()->AckOnePendingCommit();
  // kValue2 is now the base value.
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {GetPrefHash(kKey1)}, {specifics2});

  UpdateResponseDataList update;
  update.push_back(
      worker()->GenerateUpdateData(GetPrefHash(kKey1), specifics1, 1, "k1"));
  worker()->UpdateWithEncryptionKey("k1", std::move(update));

  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, {GetPrefHash(kKey1)}, {specifics2});
}

// Tests that UpdateStorageKey propagates storage key to ProcessorEntity
// and updates corresponding entity's metadata in MetadataChangeList, and
// UntrackEntity will remove corresponding ProcessorEntity and do not add
// any entity's metadata into MetadataChangeList.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldUpdateStorageKey) {
  // Setup bridge to not support calls to GetStorageKey. This will cause
  // FakeDataTypeSyncBridge to call UpdateStorageKey for new entities and will
  // DCHECK if GetStorageKey gets called.
  bridge()->SetSupportsGetStorageKey(false);
  ModelReadyToSync();
  OnSyncStarting();

  // Initial update from server should be handled by MergeFullSyncData.
  UpdateResponseDataList updates;
  updates.push_back(worker()->GenerateUpdateData(
      GetPrefHash(kKey1), GeneratePrefSpecifics(kKey1, kValue1)));
  // Create update which will be ignored by bridge.
  updates.push_back(worker()->GenerateUpdateData(
      GetPrefHash(kKey3), GeneratePrefSpecifics(kKey3, kValue3)));
  bridge()->AddPrefValueToIgnore(kValue3);
  worker()->UpdateFromServer(std::move(updates));
  EXPECT_EQ(1, bridge()->merge_call_count());
  EXPECT_EQ(1U, ProcessorEntityCount());
  // Metadata should be written under a new storage key. This means that
  // UpdateStorageKey was called and value of storage key got propagated to
  // MetadataChangeList.
  const std::string storage_key1 = bridge()->GetLastGeneratedStorageKey();
  EXPECT_TRUE(db()->HasMetadata(storage_key1));
  EXPECT_EQ(1U, db()->metadata_count());
  EXPECT_EQ(0, bridge()->get_storage_key_call_count());

  // Local update should affect the same entity. This ensures that storage key
  // to client tag hash mapping was updated on the previous step.
  WritePrefItem(bridge(), storage_key1, kValue2);
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_EQ(1U, db()->metadata_count());

  // Second update from server should be handled by ApplyIncrementalSyncChanges.
  // Similarly It should call UpdateStorageKey, not GetStorageKey.
  worker()->UpdateFromServer(GetPrefHash(kKey2),
                             GeneratePrefSpecifics(kKey2, kValue2));
  EXPECT_EQ(1, bridge()->apply_call_count());
  const std::string storage_key2 = bridge()->GetLastGeneratedStorageKey();
  EXPECT_NE(storage_key1, storage_key2);
  EXPECT_TRUE(db()->HasMetadata(storage_key2));
  EXPECT_EQ(2U, db()->metadata_count());
  EXPECT_EQ(0, bridge()->get_storage_key_call_count());
}

// Tests that reencryption scenario works correctly for types that don't support
// GetStorageKey(). When update from server delivers updated encryption key, all
// entities should be reencrypted including new entity that just got received
// from server.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldReencryptDatatypeWithoutStorageKeySupport) {
  bridge()->SetSupportsGetStorageKey(false);
  InitializeToReadyState();

  UpdateResponseDataList update;
  update.push_back(worker()->GenerateUpdateData(
      GetPrefHash(kKey1), GeneratePrefSpecifics(kKey1, kValue1), 1, "ek1"));
  worker()->UpdateWithEncryptionKey("ek2", std::move(update));
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});
}

// Tests that UntrackEntity won't propagate storage key to
// ProcessorEntity, and no entity's metadata are added into
// MetadataChangeList.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldUntrackEntity) {
  // Setup bridge to not support calls to GetStorageKey. This will cause
  // FakeDataTypeSyncBridge to call UpdateStorageKey for new entities and will
  // DCHECK if GetStorageKey gets called.
  bridge()->SetSupportsGetStorageKey(false);
  bridge()->AddPrefValueToIgnore(kValue1);
  ModelReadyToSync();
  OnSyncStarting();

  // Initial update from server should be handled by MergeFullSyncData.
  worker()->UpdateFromServer(GetPrefHash(kKey1),
                             GeneratePrefSpecifics(kKey1, kValue1));
  EXPECT_EQ(1, bridge()->merge_call_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  // Metadata should not be written under kUntrackKey1. This means that
  // UntrackEntity was called and corresponding ProcessorEntity is
  // removed and no storage key got propagated to MetadataChangeList.
  EXPECT_FALSE(db()->HasMetadata(kKey1));
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0, bridge()->get_storage_key_call_count());
}

// Tests that UntrackEntityForStorage won't propagate storage key to
// ProcessorEntity, and no entity's metadata are added into
// MetadataChangeList.
TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldUntrackEntityForStorageKey) {
  InitializeToReadyState();
  WritePrefItem(bridge(), kKey1, kValue1);
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});
  worker()->AckOnePendingCommit();

  // Check the processor tracks the entity.
  TypeEntitiesCount count(GetDataType());
  type_processor()->GetTypeEntitiesCountForDebugging(
      base::BindOnce(&CaptureTypeEntitiesCount, &count));
  ASSERT_EQ(1, count.non_tombstone_entities);
  ASSERT_NE(nullptr, GetEntityForStorageKey(kKey1));

  // The bridge deletes the data locally and does not want to sync the deletion.
  // It only untracks the entity.
  type_processor()->UntrackEntityForStorageKey(kKey1);

  // The deletion is not synced up.
  worker()->VerifyPendingCommits({});
  // The processor tracks no entity any more.
  type_processor()->GetTypeEntitiesCountForDebugging(
      base::BindOnce(&CaptureTypeEntitiesCount, &count));
  EXPECT_EQ(0, count.non_tombstone_entities);
  EXPECT_EQ(nullptr, GetEntityForStorageKey(kKey1));
}

// Tests that UntrackEntityForStorage does not crash if no such entity is being
// tracked.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldIgnoreUntrackEntityForInexistentStorageKey) {
  InitializeToReadyState();

  // This should not crash for an unknown storage key and simply ignore the
  // call.
  type_processor()->UntrackEntityForStorageKey(kKey1);

  // No deletion is not synced up.
  worker()->VerifyPendingCommits({});
  // The processor tracks no entity.
  TypeEntitiesCount count(GetDataType());
  type_processor()->GetTypeEntitiesCountForDebugging(
      base::BindOnce(&CaptureTypeEntitiesCount, &count));
  EXPECT_EQ(0, count.non_tombstone_entities);
  EXPECT_EQ(nullptr, GetEntityForStorageKey(kKey1));
}

// Tests that UntrackEntityForClientTagHash won't propagate storage key to
// ProcessorEntity, and no entity's metadata are added into MetadataChangeList.
// This test is pretty same as ShouldUntrackEntityForStorageKey.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldUntrackEntityForClientTagHash) {
  InitializeToReadyState();

  WritePrefItem(bridge(), kKey1, kValue1);
  worker()->VerifyPendingCommits({{GetPrefHash(kKey1)}});
  worker()->AckOnePendingCommit();

  // Check the processor tracks the entity.
  TypeEntitiesCount count(GetDataType());
  type_processor()->GetTypeEntitiesCountForDebugging(
      base::BindOnce(&CaptureTypeEntitiesCount, &count));
  ASSERT_EQ(1, count.non_tombstone_entities);
  ASSERT_NE(nullptr, GetEntityForStorageKey(kKey1));

  // The bridge deletes the data locally and does not want to sync the deletion.
  // It only untracks the entity.
  type_processor()->UntrackEntityForClientTagHash(GetPrefHash(kKey1));

  // The deletion is not synced up.
  worker()->VerifyPendingCommits({});
  // The processor tracks no entity any more.
  type_processor()->GetTypeEntitiesCountForDebugging(
      base::BindOnce(&CaptureTypeEntitiesCount, &count));
  EXPECT_EQ(0, count.non_tombstone_entities);
  EXPECT_EQ(nullptr, GetEntityForStorageKey(kKey1));
}

// Tests that the processor reports an error for updates with a version GC
// directive that are received for types that support incremental updates.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotApplyGarbageCollectionByVersion) {
  InitializeToReadyState();

  ExpectError(ClientTagBasedDataTypeProcessor::ErrorSite::
                  kSupportsIncrementalUpdatesMismatch);
  sync_pb::GarbageCollectionDirective garbage_collection_directive;
  garbage_collection_directive.set_version_watermark(2);
  worker()->UpdateFromServer({}, garbage_collection_directive);
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldDeleteMetadataWhenCacheGuidMismatch) {
  // Commit item.
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  // Reset the processor to simulate a restart.
  ResetState(/*keep_db=*/true);

  // A new processor loads the metadata after changing the cache GUID.
  bridge()->SetInitialSyncState(sync_pb::DataTypeState::INITIAL_SYNC_DONE);

  std::unique_ptr<MetadataBatch> metadata_batch = db()->CreateMetadataBatch();
  sync_pb::DataTypeState data_type_state(metadata_batch->GetDataTypeState());
  data_type_state.set_cache_guid("WRONG_CACHE_GUID");
  metadata_batch->SetDataTypeState(data_type_state);

  type_processor()->ModelReadyToSync(std::move(metadata_batch));
  ASSERT_TRUE(type_processor()->IsModelReadyToSyncForTest());

  OnSyncStarting("DefaultAuthenticatedAccountId", "TestCacheGuid");

  // Model should still be ready to sync.
  ASSERT_TRUE(type_processor()->IsModelReadyToSyncForTest());
  // OnSyncStarting() should have completed.
  EXPECT_NE(nullptr, worker());
  // Upon a mismatch, metadata should have been cleared.
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_FALSE(type_processor()->IsTrackingMetadata());
  // Initial update.
  worker()->UpdateFromServer();
  EXPECT_TRUE(type_processor()->IsTrackingMetadata());
  EXPECT_EQ("TestCacheGuid", type_processor()->TrackedCacheGuid());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldDeleteMetadataWhenDataTypeIdMismatch) {
  // Commit item.
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  // Reset the processor to simulate a restart.
  ResetState(/*keep_db=*/true);

  // A new processor loads the metadata after changing the data type id.
  bridge()->SetInitialSyncState(sync_pb::DataTypeState::INITIAL_SYNC_DONE);

  std::unique_ptr<MetadataBatch> metadata_batch = db()->CreateMetadataBatch();
  sync_pb::DataTypeState data_type_state(metadata_batch->GetDataTypeState());
  // This processor is supposed to process Preferences. Mark the data type
  // state to be for sessions to simulate a data type id mismatch.
  data_type_state.mutable_progress_marker()->set_data_type_id(
      GetSpecificsFieldNumberFromDataType(SESSIONS));
  metadata_batch->SetDataTypeState(data_type_state);

  type_processor()->ModelReadyToSync(std::move(metadata_batch));
  ASSERT_TRUE(type_processor()->IsModelReadyToSyncForTest());

  OnSyncStarting();

  // Model should still be ready to sync.
  ASSERT_TRUE(type_processor()->IsModelReadyToSyncForTest());
  // OnSyncStarting() should have completed.
  EXPECT_NE(nullptr, worker());
  // Upon a mismatch, metadata should have been cleared.
  EXPECT_EQ(0U, db()->metadata_count());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldClearOrphanMetadataInGetLocalChangesWhenDataIsMissing) {
  InitializeToReadyState();
  WritePrefItem(bridge(), kKey1, kValue1);

  // Loose the entity in the bridge (keeping the metadata around as an orphan).
  bridge()->MimicBugToLooseItemWithoutNotifyingProcessor(kKey1);

  ASSERT_FALSE(db()->HasData(kKey1));
  ASSERT_TRUE(db()->HasMetadata(kKey1));
  ASSERT_NE(nullptr, GetEntityForStorageKey(kKey1));

  // Reset "the browser" so that the processor looses the copy of the data.
  ResetState(/*keep_db=*/true);

  // Initializing the processor will trigger it to commit again. It does not
  // have a copy of the data so it will ask the bridge.
  {
    base::HistogramTester histogram_tester;
    InitializeToReadyState();

    histogram_tester.ExpectBucketCount(
        "Sync.DataTypeOrphanMetadata.GetData",
        /*bucket=*/DataTypeHistogramValue(GetDataType()), /*count=*/1);
  }

  // Orphan metadata should have been deleted.
  EXPECT_EQ(1, bridge()->apply_call_count());
  EXPECT_FALSE(db()->HasMetadata(kKey1));
  EXPECT_EQ(nullptr, GetEntityForStorageKey(kKey1));

  base::HistogramTester histogram_tester;

  // Do it again, explicitly. The processor does not track the entity so it
  // shouldn't ask the bridge or return local changes.
  CommitRequestDataList commit_request;
  type_processor()->GetLocalChanges(
      INT_MAX, base::BindOnce(&CaptureCommitRequest, &commit_request));
  EXPECT_EQ(0U, commit_request.size());

  // The processor should not report orphan again in UMA.
  histogram_tester.ExpectBucketCount(
      "Sync.DataTypeOrphanMetadata.GetData",
      /*bucket=*/DataTypeHistogramValue(GetDataType()), /*count=*/0);
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotReportOrphanMetadataInGetLocalChangesWhenDataIsPresent) {
  InitializeToReadyState();
  WritePrefItem(bridge(), kKey1, kValue1);

  ASSERT_TRUE(db()->HasData(kKey1));
  ASSERT_TRUE(db()->HasMetadata(kKey1));
  ASSERT_NE(nullptr, GetEntityForStorageKey(kKey1));

  // Reset "the browser" so that the processor looses the copy of the data.
  ResetState(/*keep_db=*/true);

  // Initializing the processor will trigger it to commit again. It does not
  // have a copy of the data so it will ask the bridge.
  base::HistogramTester histogram_tester;
  InitializeToReadyState();

  // Now everything is committed and GetLocalChanges is empty again.
  CommitRequestDataList commit_request;
  type_processor()->GetLocalChanges(
      INT_MAX, base::BindOnce(&CaptureCommitRequest, &commit_request));
  EXPECT_EQ(0U, commit_request.size());

  // The processor never reports any orphan.
  histogram_tester.ExpectTotalCount("Sync.DataTypeOrphanMetadata.GetData",
                                    /*count=*/0);

  EXPECT_TRUE(db()->HasData(kKey1));
  EXPECT_TRUE(db()->HasMetadata(kKey1));
  EXPECT_NE(nullptr, GetEntityForStorageKey(kKey1));
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldRecordNumUnsyncedEntitiesOnModelReady) {
  {
    base::HistogramTester histogram_tester;
    InitializeToReadyState();
    // There are no local unsynced entities.
    histogram_tester.ExpectUniqueSample(
        "Sync.DataTypeNumUnsyncedEntitiesOnModelReady.PREFERENCE",
        /*sample=*/0, /*expected_bucket_count=*/1);
  }
  WritePrefItem(bridge(), kKey1, kValue1);

  ASSERT_TRUE(db()->HasData(kKey1));
  ASSERT_TRUE(db()->HasMetadata(kKey1));
  ASSERT_NE(nullptr, GetEntityForStorageKey(kKey1));

  // Reset "the browser" so that the processor looses the copy of the data.
  ResetState(/*keep_db=*/true);

  {
    base::HistogramTester histogram_tester;
    InitializeToReadyState();
    // Entity with kKey1 is unsynced.
    histogram_tester.ExpectUniqueSample(
        "Sync.DataTypeNumUnsyncedEntitiesOnModelReady.PREFERENCE",
        /*sample=*/1, /*expected_bucket_count=*/1);
  }
}

// This tests the case when the bridge deletes an item, and before it's
// committed to the server, it created again with a different storage key.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldDeleteItemAndRecreaeItWithDifferentStorageKey) {
  const std::string kStorageKey1 = "StorageKey1";
  const std::string kStorageKey2 = "StorageKey2";

  // This verifies that the processor doesn't use GetStorageKey() method.
  bridge()->SetSupportsGetStorageKey(false);

  InitializeToReadyState();

  std::unique_ptr<EntityData> entity_data1 =
      GeneratePrefEntityData(kKey1, kValue1);
  bridge()->WriteItem(kStorageKey1, std::move(entity_data1));

  worker()->AckOnePendingCommit();

  EXPECT_TRUE(type_processor()->IsTrackingEntityForTest(kStorageKey1));

  // Delete the data associated with the first storage key.
  bridge()->DeleteItem(kStorageKey1);
  // // Add the same data under a different storage key.
  std::unique_ptr<EntityData> entity_data2 =
      GeneratePrefEntityData(kKey1, kValue1);
  bridge()->WriteItem(kStorageKey2, std::move(entity_data2));

  EXPECT_FALSE(type_processor()->IsTrackingEntityForTest(kStorageKey1));
  EXPECT_FALSE(db()->HasMetadata(kStorageKey1));
  EXPECT_TRUE(type_processor()->IsTrackingEntityForTest(kStorageKey2));
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldPropagateFailedCommitItemsToBridgeWhenCommitCompleted) {
  InitializeToReadyState();
  FailedCommitResponseData response_data;
  response_data.client_tag_hash = GetPrefHash("fake tag");
  response_data.response_type = sync_pb::CommitResponse::TRANSIENT_ERROR;
  response_data.datatype_specific_error.mutable_sharing_message_error()
      ->set_error_code(sync_pb::SharingMessageCommitError::INVALID_ARGUMENT);

  FailedCommitResponseDataList failed_list;
  failed_list.push_back(response_data);

  FailedCommitResponseDataList actual_error_response_list;

  auto on_commit_attempt_errors_callback = base::BindOnce(
      [](FailedCommitResponseDataList* actual_error_response_list,
         const FailedCommitResponseDataList& error_response_list) {
        // We put expectations outside of the callback, so that they fail if
        // callback is not ran.
        *actual_error_response_list = error_response_list;
      },
      &actual_error_response_list);

  bridge()->SetOnCommitAttemptErrorsCallback(
      std::move(on_commit_attempt_errors_callback));

  type_processor()->OnCommitCompleted(
      data_type_state(),
      /*committed_response_list=*/CommitResponseDataList(), failed_list);

  ASSERT_EQ(1u, actual_error_response_list.size());
  EXPECT_EQ(0, bridge()->commit_failures_count());
  EXPECT_EQ(response_data.client_tag_hash,
            actual_error_response_list[0].client_tag_hash);
  EXPECT_EQ(response_data.response_type,
            actual_error_response_list[0].response_type);
  EXPECT_EQ(response_data.datatype_specific_error.sharing_message_error()
                .error_code(),
            actual_error_response_list[0]
                .datatype_specific_error.sharing_message_error()
                .error_code());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotPropagateFailedCommitAttemptToBridgeWhenNoFailedItems) {
  InitializeToReadyState();
  auto on_commit_attempt_errors_callback = base::BindOnce(
      [](const FailedCommitResponseDataList& error_response_list) {
        ADD_FAILURE()
            << "OnCommitAttemptErrors is called when no failed items.";
      });

  bridge()->SetOnCommitAttemptErrorsCallback(
      std::move(on_commit_attempt_errors_callback));

  type_processor()->OnCommitCompleted(
      data_type_state(),
      /*committed_response_list=*/CommitResponseDataList(),
      /*error_response_list=*/FailedCommitResponseDataList());
  EXPECT_EQ(0, bridge()->commit_failures_count());
}

TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldPropagateFullCommitFailure) {
  InitializeToReadyState();
  ASSERT_EQ(0, bridge()->commit_failures_count());

  type_processor()->OnCommitFailed(syncer::SyncCommitError::kNetworkError);
  EXPECT_EQ(1, bridge()->commit_failures_count());
}

class CommitOnlyClientTagBasedDataTypeProcessorTest
    : public ClientTagBasedDataTypeProcessorTest {
 protected:
  DataType GetDataType() override {
    DCHECK(CommitOnlyTypes().Has(USER_EVENTS));
    return USER_EVENTS;
  }
};

TEST_F(CommitOnlyClientTagBasedDataTypeProcessorTest,
       ShouldExposeNewlyTrackedAccountId) {
  ModelReadyToSync();
  ASSERT_EQ("", type_processor()->TrackedAccountId());
  OnSyncStarting();
  EXPECT_EQ(kDefaultAuthenticatedAccountId,
            type_processor()->TrackedAccountId());
}

TEST_F(CommitOnlyClientTagBasedDataTypeProcessorTest,
       ShouldExposePreviouslyTrackedAccountId) {
  std::unique_ptr<MetadataBatch> metadata_batch = db()->CreateMetadataBatch();
  sync_pb::DataTypeState data_type_state(metadata_batch->GetDataTypeState());

  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  data_type_state.set_cache_guid(kCacheGuid);
  data_type_state.set_authenticated_account_id("PersistedAccountId");
  data_type_state.mutable_progress_marker()->set_data_type_id(
      GetSpecificsFieldNumberFromDataType(GetDataType()));
  metadata_batch->SetDataTypeState(data_type_state);
  type_processor()->ModelReadyToSync(std::move(metadata_batch));

  // Even prior to starting sync, the account ID should already be tracked.
  EXPECT_EQ("PersistedAccountId", type_processor()->TrackedAccountId());

  // If sync gets started, the account should still be tracked.
  OnSyncStarting("PersistedAccountId");
  EXPECT_EQ("PersistedAccountId", type_processor()->TrackedAccountId());
}

TEST_F(CommitOnlyClientTagBasedDataTypeProcessorTest,
       ShouldCallMergeWhenSyncEnabled) {
  ModelReadyToSync();
  ASSERT_EQ("", type_processor()->TrackedAccountId());
  ASSERT_EQ(0, bridge()->merge_call_count());
  OnSyncStarting();
  EXPECT_EQ(1, bridge()->merge_call_count());
}

TEST_F(CommitOnlyClientTagBasedDataTypeProcessorTest,
       ShouldNotCallMergeAfterRestart) {
  std::unique_ptr<MetadataBatch> metadata_batch = db()->CreateMetadataBatch();
  sync_pb::DataTypeState data_type_state(metadata_batch->GetDataTypeState());
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  data_type_state.set_cache_guid(kCacheGuid);
  data_type_state.set_authenticated_account_id("PersistedAccountId");
  data_type_state.mutable_progress_marker()->set_data_type_id(
      GetSpecificsFieldNumberFromDataType(GetDataType()));
  metadata_batch->SetDataTypeState(data_type_state);
  type_processor()->ModelReadyToSync(std::move(metadata_batch));

  // Even prior to starting sync, the account ID should already be tracked.
  ASSERT_EQ("PersistedAccountId", type_processor()->TrackedAccountId());

  // When sync gets started, MergeFullSyncData() should not be called.
  OnSyncStarting("PersistedAccountId");
  ASSERT_EQ(0, bridge()->merge_call_count());
}

// Test that commit only types are deleted after commit response.
TEST_F(CommitOnlyClientTagBasedDataTypeProcessorTest,
       ShouldCommitAndDeleteWhenAcked) {
  InitializeToReadyState();
  EXPECT_EQ(db()->data_type_state().initial_sync_state(),
            sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  const uint64_t key1 = 1234;
  const std::string key1s = base::NumberToString(key1);

  WriteUserEventItem(bridge(), key1, 4321);
  worker()->VerifyPendingCommits({{GetHash(USER_EVENTS, key1s)}});
  EXPECT_EQ(1U, db()->data_count());
  EXPECT_EQ(1U, db()->metadata_count());

  worker()->AckOnePendingCommit();
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
}

// Test that commit only types maintain tracking of entities while unsynced
// changes exist.
TEST_F(CommitOnlyClientTagBasedDataTypeProcessorTest,
       ShouldTrackUnsyncedChangesAfterPartialCommit) {
  InitializeToReadyState();

  const uint64_t key1 = 1234;
  const uint64_t key2 = 2345;
  const std::string key1s = base::NumberToString(key1);
  const std::string key2s = base::NumberToString(key2);

  WriteUserEventItem(bridge(), key1, 4321);
  worker()->VerifyPendingCommits({{GetHash(USER_EVENTS, key1s)}});
  EXPECT_EQ(1U, db()->data_count());
  EXPECT_EQ(1U, db()->metadata_count());

  WriteUserEventItem(bridge(), key1, 5432);
  worker()->VerifyPendingCommits(
      {{GetHash(USER_EVENTS, key1s)}, {GetHash(USER_EVENTS, key1s)}});
  EXPECT_EQ(1U, db()->data_count());
  EXPECT_EQ(1U, db()->metadata_count());

  worker()->AckOnePendingCommit();
  worker()->VerifyPendingCommits({{GetHash(USER_EVENTS, key1s)}});
  EXPECT_EQ(1U, db()->data_count());
  EXPECT_EQ(1U, db()->metadata_count());

  // The version field isn't meaningful on commit only types, so force a value
  // that isn't incremented to verify everything still works.
  worker()->AckOnePendingCommit(0 /* version_offset */);
  worker()->VerifyPendingCommits({});
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
}

TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldResetOnInvalidCacheGuid) {
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  ASSERT_EQ(1U, ProcessorEntityCount());

  ResetStateWriteItem(kKey1, kValue1);
  sync_pb::DataTypeState data_type_state = db()->data_type_state();
  data_type_state.set_cache_guid("OtherCacheGuid");
  db()->set_data_type_state(data_type_state);

  ModelReadyToSync();
  OnSyncStarting();
  EXPECT_EQ(0U, ProcessorEntityCount());
}

TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldResetOnInvalidDataTypeId) {
  ResetStateWriteItem(kKey1, kValue1);
  ASSERT_EQ(0U, ProcessorEntityCount());

  // Initialize change processor, expect to load data from the bridge.
  InitializeToMetadataLoaded();
  OnSyncStarting();
  ASSERT_EQ(1U, ProcessorEntityCount());

  ResetStateWriteItem(kKey1, kValue1);

  OnSyncStarting();
  // Set different data type id.
  sync_pb::DataTypeState data_type_state = db()->data_type_state();

  ASSERT_NE(data_type_state.progress_marker().data_type_id(),
            GetSpecificsFieldNumberFromDataType(AUTOFILL));
  data_type_state.mutable_progress_marker()->set_data_type_id(
      GetSpecificsFieldNumberFromDataType(AUTOFILL));
  db()->set_data_type_state(data_type_state);

  ModelReadyToSync();
  EXPECT_EQ(0U, ProcessorEntityCount());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldResetForEntityMetadataWithoutInitialSyncDone) {
  base::HistogramTester histogram_tester;

  sync_pb::EntityMetadata entity_metadata1;
  entity_metadata1.set_client_tag_hash(
      ClientTagHash::FromUnhashed(GetDataType(), "tag1").value());
  entity_metadata1.set_creation_time(0);
  sync_pb::EntityMetadata entity_metadata2;
  entity_metadata2.set_client_tag_hash(
      ClientTagHash::FromUnhashed(GetDataType(), "tag2").value());
  entity_metadata2.set_creation_time(0);

  db()->PutMetadata(kKey1, std::move(entity_metadata1));
  db()->PutMetadata(kKey2, std::move(entity_metadata2));

  InitializeToMetadataLoaded(
      sync_pb::DataTypeState::INITIAL_SYNC_STATE_UNSPECIFIED);
  OnSyncStarting();

  // Since initial_sync_state was not set, metadata should have been cleared.
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_FALSE(type_processor()->IsTrackingMetadata());
  // Initial update.
  worker()->UpdateFromServer();
  EXPECT_TRUE(type_processor()->IsTrackingMetadata());

  histogram_tester.ExpectBucketCount(
      "Sync.DataTypeEntityMetadataWithoutInitialSync",
      /*sample=*/DataTypeHistogramValue(GetDataType()),
      /*expected_count=*/1);
}

// Regression test for crbug.com/1427000.
TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotResetWhenInitialSyncPartiallyDone) {
  base::HistogramTester histogram_tester;

  sync_pb::EntityMetadata entity_metadata1;
  entity_metadata1.set_client_tag_hash(
      ClientTagHash::FromUnhashed(GetDataType(), "tag1").value());
  entity_metadata1.set_creation_time(0);
  sync_pb::EntityMetadata entity_metadata2;
  entity_metadata2.set_client_tag_hash(
      ClientTagHash::FromUnhashed(GetDataType(), "tag2").value());
  entity_metadata2.set_creation_time(0);

  db()->PutMetadata(kKey1, std::move(entity_metadata1));
  db()->PutMetadata(kKey2, std::move(entity_metadata2));

  InitializeToMetadataLoaded(
      sync_pb::DataTypeState::INITIAL_SYNC_PARTIALLY_DONE);
  OnSyncStarting();

  // The initial_sync_state was *partially* done, which is enough to keep the
  // (meta)data and carry on.
  EXPECT_EQ(2U, db()->metadata_count());
  EXPECT_EQ(2U, ProcessorEntityCount());
  EXPECT_TRUE(type_processor()->IsTrackingMetadata());

  histogram_tester.ExpectTotalCount(
      "Sync.DataTypeEntityMetadataWithoutInitialSync",
      /*expected_count=*/0);
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldResetForDuplicateClientTagHash) {
  base::HistogramTester histogram_tester;

  const syncer::ClientTagHash kClientTagHash =
      ClientTagHash::FromUnhashed(GetDataType(), "tag");
  sync_pb::EntityMetadata entity_metadata1;
  entity_metadata1.set_client_tag_hash(kClientTagHash.value());
  entity_metadata1.set_creation_time(0);
  sync_pb::EntityMetadata entity_metadata2;
  entity_metadata2.set_client_tag_hash(kClientTagHash.value());
  entity_metadata2.set_creation_time(0);
  sync_pb::EntityMetadata entity_metadata3;
  entity_metadata3.set_client_tag_hash(kClientTagHash.value());
  entity_metadata3.set_creation_time(0);

  db()->PutMetadata(kKey1, std::move(entity_metadata1));
  db()->PutMetadata(kKey2, std::move(entity_metadata2));
  db()->PutMetadata(kKey3, std::move(entity_metadata3));

  InitializeToReadyState();

  // With a client tag hash duplicate, metadata should have been cleared.
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_FALSE(type_processor()->IsTrackingMetadata());
  // Initial update.
  worker()->UpdateFromServer();
  EXPECT_TRUE(type_processor()->IsTrackingMetadata());

  // There were three entities with the same client-tag-hash which indicates
  // that two of them were metadata oprhans.
  histogram_tester.ExpectBucketCount(
      "Sync.DataTypeOrphanMetadata.ModelReadyToSync",
      /*sample=*/DataTypeHistogramValue(GetDataType()),
      /*expected_count=*/2);
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotProcessInvalidRemoteIncrementalUpdate) {
  // To ensure the update is not ignored because of empty storage key.
  bridge()->SetSupportsGetStorageKey(false);

  InitializeToReadyState();
  UpdateResponseDataList updates;
  updates.push_back(worker()->GenerateUpdateData(
      GetPrefHash(kKey1), GeneratePrefSpecifics(kKey1, kValue1)));

  // Force invalidate the next remote update.
  bridge()->TreatRemoteUpdateAsInvalid(GetPrefHash(kKey1));

  worker()->UpdateFromServer(std::move(updates));

  // Verify that the data wasn't actually stored.
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, db()->data_count());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotProcessInvalidRemoteFullUpdate) {
  InitializeToMetadataLoaded(
      sync_pb::DataTypeState::INITIAL_SYNC_STATE_UNSPECIFIED);
  OnSyncStarting();

  UpdateResponseDataList updates;
  updates.push_back(worker()->GenerateUpdateData(
      GetPrefHash(kKey1), GeneratePrefSpecifics(kKey1, kValue1)));

  // Force invalidate the next remote update.
  bridge()->TreatRemoteUpdateAsInvalid(GetPrefHash(kKey1));

  base::HistogramTester histogram_tester;
  worker()->UpdateFromServer(std::move(updates));

  // Verify that the data wasn't actually stored.
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, db()->data_count());

  // Update was dropped by the bridge.
  histogram_tester.ExpectBucketCount(
      "Sync.DataTypeUpdateDrop.DroppedByBridge",
      /*bucket=*/DataTypeHistogramValue(GetDataType()),
      /*count=*/1);
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotReportErrorAfterOnSyncStopping) {
  InitializeToReadyState();
  // This should reset activation_request and consequently, the error_handler.
  type_processor()->OnSyncStopping(KEEP_METADATA);
  ASSERT_FALSE(error_reported());

  ModelError error{FROM_HERE, "boom"};
  type_processor()->ReportError(error);
  // Error was raised but did not trigger ErrorReceived().
  // Note: If an error is issued to the error_handler but the expectation is not
  // set via ExpectError(), the test fails.
  ASSERT_TRUE(type_processor()->GetError().has_value());
  ASSERT_EQ(type_processor()->GetError()->location(), error.location());

  // No call to error_handler since ErrorReceived() was not triggered.
  EXPECT_FALSE(error_reported());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotInvokeBridgeOnSyncStartingFromOnSyncStopping) {
  InitializeToReadyState();
  ASSERT_TRUE(bridge()->sync_started());
  // OnSyncStopping() calls bridge's ApplyDisableSyncChanges(), which should
  // reset `sync_started_` flag.
  type_processor()->OnSyncStopping(CLEAR_METADATA);
  // OnSyncStopping() should clear the activation request, hence avoiding call
  // to bridge's OnSyncStarting().
  EXPECT_FALSE(bridge()->sync_started());
}

TEST_F(ClientTagBasedDataTypeProcessorTest, ShouldClearMetadataIfStopped) {
  // Bring the processor to a stopped state.
  InitializeToReadyState();
  WritePrefItem(bridge(), kKey1, kValue1);
  type_processor()->OnSyncStopping(KEEP_METADATA);

  // Metadata is still being kept and tracked.
  ASSERT_TRUE(type_processor()->IsTrackingMetadata());
  ASSERT_EQ(1U, db()->metadata_count());

  base::HistogramTester histogram_tester;

  // Should clear the metadata even if already stopped.
  type_processor()->ClearMetadataIfStopped();
  EXPECT_FALSE(type_processor()->IsTrackingMetadata());
  EXPECT_EQ(0U, db()->data_type_state().ByteSizeLong());
  EXPECT_EQ(0U, db()->metadata_count());
  // Expect an entry to the histogram.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 1);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 1);

  // Metadata clearing logic should not trigger bridge's OnSyncStarting().
  EXPECT_FALSE(bridge()->sync_started());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldClearMetadataIfStoppedUponModelReadyToSync) {
  base::HistogramTester histogram_tester;

  // Called before ModelReadyToSync().
  type_processor()->ClearMetadataIfStopped();

  // Nothing recorded to the histograms yet.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.DelayedClear", 0);

  // Prepare metadata.
  const syncer::ClientTagHash kClientTagHash =
      ClientTagHash::FromUnhashed(AUTOFILL, "tag");
  sync_pb::EntityMetadata entity_metadata1;
  entity_metadata1.set_client_tag_hash(kClientTagHash.value());
  entity_metadata1.set_creation_time(0);

  db()->PutMetadata(kKey1, std::move(entity_metadata1));
  ASSERT_EQ(1U, db()->metadata_count());

  InitializeToMetadataLoaded();
  // Tracker should have not been set.
  EXPECT_FALSE(type_processor()->IsTrackingMetadata());
  // Metadata should have been cleared from the persistent storage.
  EXPECT_EQ(0U, db()->data_type_state().ByteSizeLong());
  EXPECT_EQ(0U, db()->metadata_count());
  // Expect recording of the delayed clear.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 1);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.DelayedClear", 1);

  // Metadata clearing logic should not trigger bridge's OnSyncStarting().
  EXPECT_FALSE(bridge()->sync_started());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldClearMetadataWhileStoppedUponModelReadyToSyncWithoutEntities) {
  base::HistogramTester histogram_tester;

  // Called before ModelReadyToSync().
  type_processor()->ClearMetadataIfStopped();

  // Nothing recorded to the histograms yet.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.DelayedClear", 0);

  // Don't set up any entity metadata - there will only be the non-empty
  // DataTypeState.

  InitializeToMetadataLoaded();
  // Tracker should have not been set.
  EXPECT_FALSE(type_processor()->IsTrackingMetadata());
  // Metadata should have been cleared from the persistent storage.
  EXPECT_EQ(0U, db()->data_type_state().ByteSizeLong());
  ASSERT_EQ(0U, db()->metadata_count());
  // Expect recording of the delayed clear.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 1);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.DelayedClear", 1);

  // Metadata clearing logic should not trigger bridge's OnSyncStarting().
  EXPECT_FALSE(bridge()->sync_started());
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotClearMetadataIfNotStopped) {
  // Initialize the processor with some metadata.
  InitializeToReadyState();
  WritePrefItem(bridge(), kKey1, kValue1);

  // Metadata is being kept and tracked.
  ASSERT_TRUE(type_processor()->IsTrackingMetadata());
  ASSERT_EQ(1U, db()->metadata_count());

  base::HistogramTester histogram_tester;

  // Should NOT clear the metadata since the processor is not stopped.
  type_processor()->ClearMetadataIfStopped();
  EXPECT_TRUE(type_processor()->IsTrackingMetadata());
  EXPECT_NE(0U, db()->data_type_state().ByteSizeLong());
  EXPECT_NE(0U, db()->metadata_count());
  // Expect no entries in the histogram.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 0);
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotClearMetadataIfStoppedIfNotTracking) {
  // Bring the processor to a stopped state.
  InitializeToReadyState();
  type_processor()->OnSyncStopping(CLEAR_METADATA);

  // Metadata is not being tracked anymore.
  ASSERT_FALSE(type_processor()->IsTrackingMetadata());

  base::HistogramTester histogram_tester;

  // Should do nothing since there's nothing to clear.
  type_processor()->ClearMetadataIfStopped();
  // Expect no entry to the histogram.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 0);
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotClearMetadataIfStoppedWithoutMetadataInitially) {
  InitializeToMetadataLoaded(
      sync_pb::DataTypeState::INITIAL_SYNC_STATE_UNSPECIFIED);
  ASSERT_FALSE(type_processor()->IsTrackingMetadata());

  base::HistogramTester histogram_tester;

  // Call ClearMetadataIfStopped() without a prior call to OnSyncStopping().
  // Since there's no metadata, this should do nothing.
  type_processor()->ClearMetadataIfStopped();
  // Expect no entry to the histogram.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 0);
}

TEST_F(ClientTagBasedDataTypeProcessorTest,
       ShouldNotClearMetadataIfStoppedUponModelReadyToSyncWithoutMetadata) {
  base::HistogramTester histogram_tester;

  // Called before ModelReadyToSync().
  type_processor()->ClearMetadataIfStopped();

  InitializeToMetadataLoaded(
      sync_pb::DataTypeState::INITIAL_SYNC_STATE_UNSPECIFIED);
  ASSERT_FALSE(type_processor()->IsTrackingMetadata());
  // Nothing recorded to the histograms.
  histogram_tester.ExpectTotalCount("Sync.ClearMetadataWhileStopped", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.ImmediateClear", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.ClearMetadataWhileStopped.DelayedClear", 0);
}

class PasswordsClientTagBasedDataTypeProcessorTest
    : public ClientTagBasedDataTypeProcessorTest {
 protected:
  DataType GetDataType() override { return PASSWORDS; }
};

TEST_F(PasswordsClientTagBasedDataTypeProcessorTest,
       ShouldSetPasswordsRedownloadedForNotesFlag) {
  ModelReadyToSync();
  OnSyncStarting();
  worker()->UpdateFromServer(UpdateResponseDataList());

  EXPECT_TRUE(db()->data_type_state()
                  .notes_enabled_before_initial_sync_for_passwords());
}

// Test suite for testing the bridge with UniquePosition support.
// SHARED_TAB_GROUP_DATA is used as an example data type with unique_position
// in specifics.
class ClientTagBasedDataTypeProcessorWithUniquePositionTest
    : public ClientTagBasedDataTypeProcessorTest {
 public:
  void SetUp() override {
    ClientTagBasedDataTypeProcessorTest::SetUp();
    CHECK(bridge());

    bridge()->EnableUniquePositionSupport(
        base::BindRepeating(&ExtractUniquePositionFromSharedTab));
  }

  void WriteItemWithUniquePositionToBridge(
      const std::string& guid,
      sync_pb::UniquePosition unique_position) {
    bridge()->WriteItem(guid, GenerateSharedTabGroupEntityData(
                                  guid, std::move(unique_position)));
  }

 protected:
  DataType GetDataType() override { return SHARED_TAB_GROUP_DATA; }
};

TEST_F(ClientTagBasedDataTypeProcessorWithUniquePositionTest,
       ShouldProcessUniquePositionForNewEntity) {
  const std::string kGuid = "guid";
  InitializeToReadyState();

  sync_pb::UniquePosition unique_position =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();
  WriteItemWithUniquePositionToBridge(kGuid, unique_position);

  const ProcessorEntity* entity = GetEntityForStorageKey(kGuid);
  ASSERT_THAT(entity, NotNull());
  EXPECT_THAT(db()->GetMetadata(kGuid).unique_position(),
              EqualsProto(unique_position));
  EXPECT_THAT(entity->metadata().unique_position(),
              EqualsProto(unique_position));
}

TEST_F(ClientTagBasedDataTypeProcessorWithUniquePositionTest,
       ShouldUpdateUniquePositionForExistingEntity) {
  const std::string kGuid = "guid";
  InitializeToReadyState();

  WriteItemWithUniquePositionToBridge(
      kGuid, UniquePosition::InitialPosition(UniquePosition::RandomSuffix())
                 .ToProto());

  const ProcessorEntity* entity = GetEntityForStorageKey(kGuid);
  ASSERT_THAT(entity, NotNull());
  ASSERT_TRUE(entity->metadata().has_unique_position());
  ASSERT_TRUE(db()->GetMetadata(kGuid).has_unique_position());

  // Update the entity with a new unique position.
  sync_pb::UniquePosition new_unique_position =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();
  ASSERT_THAT(entity->metadata().unique_position(),
              Not(EqualsProto(new_unique_position)));

  WriteItemWithUniquePositionToBridge(kGuid, new_unique_position);

  ASSERT_EQ(entity, GetEntityForStorageKey(kGuid));
  EXPECT_THAT(db()->GetMetadata(kGuid).unique_position(),
              EqualsProto(new_unique_position));
  EXPECT_THAT(entity->metadata().unique_position(),
              EqualsProto(new_unique_position));
}

TEST_F(ClientTagBasedDataTypeProcessorWithUniquePositionTest,
       ShouldUpdateUniquePositionAfterDeletion) {
  const std::string kGuid = "guid";
  InitializeToReadyState();

  WriteItemWithUniquePositionToBridge(
      kGuid, UniquePosition::InitialPosition(UniquePosition::RandomSuffix())
                 .ToProto());

  const ProcessorEntity* entity = GetEntityForStorageKey(kGuid);
  ASSERT_THAT(entity, NotNull());
  ASSERT_TRUE(entity->metadata().has_unique_position());
  ASSERT_TRUE(db()->GetMetadata(kGuid).has_unique_position());

  // Delete the local entity keeping its local metadata, and update afterwards.
  bridge()->DeleteItem(kGuid);
  ASSERT_TRUE(db()->HasMetadata(kGuid));
  ASSERT_FALSE(db()->HasData(kGuid));
  ASSERT_FALSE(db()->GetMetadata(kGuid).has_unique_position());

  // Restore the item and update its unique position.
  sync_pb::UniquePosition new_unique_position =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();
  WriteItemWithUniquePositionToBridge(kGuid, new_unique_position);

  ASSERT_EQ(entity, GetEntityForStorageKey(kGuid));
  EXPECT_THAT(db()->GetMetadata(kGuid).unique_position(),
              EqualsProto(new_unique_position));
  EXPECT_THAT(entity->metadata().unique_position(),
              EqualsProto(new_unique_position));
}

TEST_F(ClientTagBasedDataTypeProcessorWithUniquePositionTest,
       ShouldStoreRemoteUniquePositionOnFullUpdate) {
  const std::string kGuid = "guid";
  ModelReadyToSync();
  OnSyncStarting();
  ASSERT_FALSE(type_processor()->IsTrackingMetadata());

  sync_pb::UniquePosition unique_position =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();
  worker()->UpdateFromServer(
      GetSharedTabHash(kGuid),
      GenerateSharedTabSpecifics(kGuid, unique_position));
  ASSERT_TRUE(type_processor()->IsTrackingMetadata());

  const ProcessorEntity* entity = GetEntityForStorageKey(kGuid);
  ASSERT_THAT(entity, NotNull());
  EXPECT_THAT(db()->GetMetadata(kGuid).unique_position(),
              EqualsProto(unique_position));
  EXPECT_THAT(entity->metadata().unique_position(),
              EqualsProto(unique_position));
}

TEST_F(ClientTagBasedDataTypeProcessorWithUniquePositionTest,
       ShouldGenerateUniquePositionForInitialEntity) {
  InitializeToReadyState();

  sync_pb::UniquePosition initial_entity_position =
      type_processor()->UniquePositionForInitialEntity(
          GetSharedTabHash("guid"));
  EXPECT_TRUE(UniquePosition::FromProto(initial_entity_position).IsValid());
}

TEST_F(ClientTagBasedDataTypeProcessorWithUniquePositionTest,
       ShouldGenerateUniquePositionsBeforeAndAfter) {
  const std::string kGuid = "guid";
  InitializeToReadyState();

  UniquePosition unique_position =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix());
  WriteItemWithUniquePositionToBridge(kGuid, unique_position.ToProto());
  ASSERT_TRUE(GetEntityForStorageKey(kGuid)->metadata().has_unique_position());

  sync_pb::UniquePosition position_after =
      type_processor()->UniquePositionAfter(kGuid,
                                            GetSharedTabHash("guid_after"));
  sync_pb::UniquePosition position_before =
      type_processor()->UniquePositionBefore(kGuid,
                                             GetSharedTabHash("guid_before"));
  ASSERT_TRUE(UniquePosition::FromProto(position_before).IsValid());
  ASSERT_TRUE(UniquePosition::FromProto(position_after).IsValid());

  EXPECT_TRUE(
      unique_position.LessThan(UniquePosition::FromProto(position_after)));
  EXPECT_TRUE(
      UniquePosition::FromProto(position_before).LessThan(unique_position));
  EXPECT_TRUE(UniquePosition::FromProto(position_before)
                  .LessThan(UniquePosition::FromProto(position_after)));
}

TEST_F(ClientTagBasedDataTypeProcessorWithUniquePositionTest,
       ShouldGenerateUniquePositionBetween) {
  const std::string kGuidBefore = "guid_before";
  const std::string kGuidAfter = "guid_after";
  InitializeToReadyState();

  UniquePosition unique_position_before = UniquePosition::FromProto(
      type_processor()->UniquePositionForInitialEntity(
          GetSharedTabHash(kGuidBefore)));
  WriteItemWithUniquePositionToBridge(kGuidBefore,
                                      unique_position_before.ToProto());

  UniquePosition unique_position_after =
      UniquePosition::FromProto(type_processor()->UniquePositionAfter(
          kGuidBefore, GetSharedTabHash(kGuidAfter)));
  WriteItemWithUniquePositionToBridge(kGuidAfter,
                                      unique_position_after.ToProto());
  ASSERT_TRUE(unique_position_before.LessThan(unique_position_after));
  ASSERT_TRUE(
      GetEntityForStorageKey(kGuidBefore)->metadata().has_unique_position());
  ASSERT_TRUE(
      GetEntityForStorageKey(kGuidAfter)->metadata().has_unique_position());

  sync_pb::UniquePosition position_between =
      type_processor()->UniquePositionBetween(kGuidBefore, kGuidAfter,
                                              GetSharedTabHash("guid_between"));
  ASSERT_TRUE(UniquePosition::FromProto(position_between).IsValid());

  EXPECT_TRUE(unique_position_before.LessThan(
      UniquePosition::FromProto(position_between)));
  EXPECT_TRUE(UniquePosition::FromProto(position_between)
                  .LessThan(unique_position_after));
}

TEST_F(ClientTagBasedDataTypeProcessorWithUniquePositionTest,
       ShouldReturnUniquePosition) {
  InitializeToReadyState();

  sync_pb::UniquePosition unique_position =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();
  WriteItemWithUniquePositionToBridge("guid", unique_position);

  EXPECT_THAT(
      type_processor()->GetUniquePositionForStorageKey("does_not_exist"),
      EqualsProto(sync_pb::UniquePosition()));
  EXPECT_THAT(type_processor()->GetUniquePositionForStorageKey("guid"),
              EqualsProto(unique_position));
}

}  // namespace syncer
