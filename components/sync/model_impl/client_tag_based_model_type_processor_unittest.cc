// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/fake_model_type_sync_bridge.h"
#include "components/sync/test/engine/mock_model_type_worker.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::AutofillWalletSpecifics;
using sync_pb::EntityMetadata;
using sync_pb::EntitySpecifics;
using sync_pb::ModelTypeState;

namespace syncer {

namespace {

const char kKey1[] = "key1";
const char kKey2[] = "key2";
const char kKey3[] = "key3";
const char kKey4[] = "key4";
const char kKey5[] = "key5";
const char kValue1[] = "value1";
const char kValue2[] = "value2";
const char kValue3[] = "value3";
const std::string kHash1(FakeModelTypeSyncBridge::TagHashFromKey(kKey1));
const std::string kHash2(FakeModelTypeSyncBridge::TagHashFromKey(kKey2));
const std::string kHash3(FakeModelTypeSyncBridge::TagHashFromKey(kKey3));
const std::string kHash4(FakeModelTypeSyncBridge::TagHashFromKey(kKey4));
const std::string kHash5(FakeModelTypeSyncBridge::TagHashFromKey(kKey5));

// Typically used for verification after a delete. The specifics given to the
// worker/processor will not have been initialized and thus empty.
const EntitySpecifics kEmptySpecifics;

EntitySpecifics GenerateSpecifics(const std::string& key,
                                  const std::string& value) {
  return FakeModelTypeSyncBridge::GenerateSpecifics(key, value);
}

std::unique_ptr<EntityData> GenerateEntityData(const std::string& key,
                                               const std::string& value) {
  return FakeModelTypeSyncBridge::GenerateEntityData(key, value);
}

void CaptureCommitRequest(CommitRequestDataList* dst,
                          CommitRequestDataList&& src) {
  *dst = std::move(src);
}

void CaptureStatusCounters(StatusCounters* dst,
                           ModelType model_type,
                           const StatusCounters& counters) {
  *dst = counters;
}

class TestModelTypeSyncBridge : public FakeModelTypeSyncBridge {
 public:
  explicit TestModelTypeSyncBridge(bool commit_only,
                                   ModelType model_type,
                                   bool supports_incremental_updates)
      : FakeModelTypeSyncBridge(
            std::make_unique<ClientTagBasedModelTypeProcessor>(
                model_type,
                /*dump_stack=*/base::RepeatingClosure(),
                commit_only)) {
    supports_incremental_updates_ = supports_incremental_updates;
  }

  TestModelTypeSyncBridge(std::unique_ptr<TestModelTypeSyncBridge> other,
                          bool commit_only,
                          ModelType model_type,
                          bool supports_clear_all)
      : TestModelTypeSyncBridge(commit_only, model_type, supports_clear_all) {
    std::swap(db_, other->db_);
  }

  ~TestModelTypeSyncBridge() override {
    EXPECT_FALSE(synchronous_data_callback_);
    EXPECT_FALSE(data_callback_);
  }

  std::string GetStorageKey(const EntityData& entity_data) override {
    get_storage_key_call_count_++;
    return FakeModelTypeSyncBridge::GetStorageKey(entity_data);
  }

  void OnCommitDataLoaded() {
    ASSERT_TRUE(data_callback_);
    std::move(data_callback_).Run();
  }

  base::OnceClosure GetDataCallback() { return std::move(data_callback_); }

  void SetInitialSyncDone(bool is_done) {
    ModelTypeState model_type_state(db().model_type_state());
    model_type_state.set_initial_sync_done(is_done);
    db_->set_model_type_state(model_type_state);
  }

  // Expect a GetData call in the future and return its data immediately.
  void ExpectSynchronousDataCallback() { synchronous_data_callback_ = true; }

  int merge_call_count() const { return merge_call_count_; }
  int apply_call_count() const { return apply_call_count_; }
  int get_storage_key_call_count() const { return get_storage_key_call_count_; }

  // FakeModelTypeSyncBridge overrides.

  bool SupportsIncrementalUpdates() const override {
    return supports_incremental_updates_;
  }

  base::Optional<ModelError> MergeSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_data) override {
    merge_call_count_++;
    if (!SupportsIncrementalUpdates()) {
      // If the bridge does not support incremental updates, it should clear
      // local data in MergeSyncData.
      db_->ClearAllData();
    }
    return FakeModelTypeSyncBridge::MergeSyncData(
        std::move(metadata_change_list), entity_data);
  }
  base::Optional<ModelError> ApplySyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override {
    apply_call_count_++;
    return FakeModelTypeSyncBridge::ApplySyncChanges(
        std::move(metadata_change_list), entity_changes);
  }

  void GetData(StorageKeyList keys, DataCallback callback) override {
    if (synchronous_data_callback_) {
      synchronous_data_callback_ = false;
      FakeModelTypeSyncBridge::GetData(keys, std::move(callback));
    } else {
      FakeModelTypeSyncBridge::GetData(
          keys, base::BindOnce(&TestModelTypeSyncBridge::CaptureDataCallback,
                               base::Unretained(this), std::move(callback)));
    }
  }

 private:
  void CaptureDataCallback(DataCallback callback,
                           std::unique_ptr<DataBatch> data) {
    EXPECT_FALSE(data_callback_);
    data_callback_ = base::BindOnce(std::move(callback), std::move(data));
  }

  bool supports_incremental_updates_;

  // The number of times MergeSyncData has been called.
  int merge_call_count_ = 0;
  int apply_call_count_ = 0;
  int get_storage_key_call_count_ = 0;

  // Stores the data callback between GetData() and OnCommitDataLoaded().
  base::OnceClosure data_callback_;

  // Whether to return GetData results synchronously. Overrides the default
  // callback capture behavior if set to true.
  bool synchronous_data_callback_ = false;
};

}  // namespace

// Tests the various functionality of ClientTagBasedModelTypeProcessor.
//
// The processor sits between the bridge (implemented by this test class) and
// the worker, which is represented by a MockModelTypeWorker. This test suite
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
class ClientTagBasedModelTypeProcessorTest : public ::testing::Test {
 public:
  ClientTagBasedModelTypeProcessorTest() {}
  ~ClientTagBasedModelTypeProcessorTest() override { CheckPostConditions(); }

  void SetUp() override {
    bridge_ = std::make_unique<TestModelTypeSyncBridge>(
        IsCommitOnly(), GetModelType(), SupportsIncrementalUpdates());
  }

  void InitializeToMetadataLoaded() {
    bridge()->SetInitialSyncDone(true);
    ModelReadyToSync();
  }

  // Initialize to a "ready-to-commit" state.
  void InitializeToReadyState() {
    InitializeToMetadataLoaded();
    OnSyncStarting();
  }

  void ModelReadyToSync() {
    type_processor()->ModelReadyToSync(db().CreateMetadataBatch());
  }

  void OnCommitDataLoaded() { bridge()->OnCommitDataLoaded(); }

  void OnSyncStarting(
      const std::string& authenticated_account_id = "SomeAccountId") {
    DataTypeActivationRequest request;
    request.error_handler = base::BindRepeating(
        &ClientTagBasedModelTypeProcessorTest::ErrorReceived,
        base::Unretained(this));
    request.cache_guid = "TestCacheGuid";
    request.authenticated_account_id = authenticated_account_id;
    type_processor()->OnSyncStarting(
        request,
        base::BindOnce(&ClientTagBasedModelTypeProcessorTest::OnReadyToConnect,
                       base::Unretained(this)));
  }

  void DisconnectSync() {
    type_processor()->DisconnectSync();
    worker_ = nullptr;
  }

  // Writes data for |key| and simulates a commit response for it.
  EntitySpecifics WriteItemAndAck(const std::string& key,
                                  const std::string& value) {
    EntitySpecifics specifics = bridge()->WriteItem(key, value);
    base::RunLoop().RunUntilIdle();
    worker()->VerifyPendingCommits(
        {{FakeModelTypeSyncBridge::TagHashFromKey(key)}});
    worker()->AckOnePendingCommit();
    EXPECT_EQ(0U, worker()->GetNumPendingCommits());
    return specifics;
  }

  void WriteItemAndAck(const std::string& key,
                       std::unique_ptr<EntityData> entity_data) {
    bridge()->WriteItem(key, std::move(entity_data));
    worker()->VerifyPendingCommits(
        {{FakeModelTypeSyncBridge::TagHashFromKey(key)}});
    worker()->AckOnePendingCommit();
    EXPECT_EQ(0U, worker()->GetNumPendingCommits());
    return;
  }

  ProcessorEntityTracker* GetEntityForStorageKey(
      const std::string& storage_key) {
    return type_processor()->GetEntityForStorageKey(storage_key);
  }

  void ResetState(bool keep_db) {
    bridge_ = keep_db ? std::make_unique<TestModelTypeSyncBridge>(
                            std::move(bridge_), IsCommitOnly(), GetModelType(),
                            SupportsIncrementalUpdates())
                      : std::make_unique<TestModelTypeSyncBridge>(
                            IsCommitOnly(), GetModelType(),
                            SupportsIncrementalUpdates());
    worker_ = nullptr;
    CheckPostConditions();
  }

  virtual bool IsCommitOnly() { return false; }

  virtual ModelType GetModelType() { return PREFERENCES; }

  virtual bool SupportsIncrementalUpdates() { return true; }

  // Wipes existing DB and simulates a pending update of a server-known item.
  EntitySpecifics ResetStateWriteItem(const std::string& name,
                                      const std::string& value) {
    ResetState(false);
    InitializeToReadyState();
    EXPECT_EQ(0U, ProcessorEntityCount());
    WriteItemAndAck(name, "acked-value");
    EntitySpecifics specifics = bridge()->WriteItem(name, value);
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
    return type_processor()->entities_.size();
  }

  // Expect to receive an error from the processor.
  void ExpectError() {
    EXPECT_FALSE(expect_error_);
    expect_error_ = true;
  }

  TestModelTypeSyncBridge* bridge() const { return bridge_.get(); }

  const FakeModelTypeSyncBridge::Store& db() const { return bridge()->db(); }

  MockModelTypeWorker* worker() const { return worker_; }

  ClientTagBasedModelTypeProcessor* type_processor() const {
    return static_cast<ClientTagBasedModelTypeProcessor*>(
        bridge()->change_processor());
  }

 protected:
  void CheckPostConditions() { EXPECT_FALSE(expect_error_); }

  void OnReadyToConnect(std::unique_ptr<DataTypeActivationResponse> context) {
    std::unique_ptr<MockModelTypeWorker> worker(
        new MockModelTypeWorker(context->model_type_state, type_processor()));
    // Keep an unsafe pointer to the commit queue the processor will use.
    worker_ = worker.get();
    // The context contains a proxy to the processor, but this call is
    // side-stepping that completely and connecting directly to the real
    // processor, since these tests are single-threaded and don't need proxies.
    type_processor()->ConnectSync(std::move(worker));
  }

  void ErrorReceived(const ModelError& error) {
    EXPECT_TRUE(expect_error_);
    expect_error_ = false;
  }

 private:
  std::unique_ptr<TestModelTypeSyncBridge> bridge_;

  // This sets SequencedTaskRunnerHandle on the current thread, which the type
  // processor will pick up as the sync task runner.
  base::MessageLoop sync_loop_;

  // The current mock queue, which is owned by |type_processor()|.
  MockModelTypeWorker* worker_;

  // Whether to expect an error from the processor.
  bool expect_error_ = false;
};

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldExposeNewlyTrackedAccountId) {
  ModelReadyToSync();
  ASSERT_EQ("", type_processor()->TrackedAccountId());
  OnSyncStarting("SomeAccountId");
  worker()->UpdateFromServer();
  EXPECT_EQ("SomeAccountId", type_processor()->TrackedAccountId());
}

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldExposePreviouslyTrackedAccountId) {
  std::unique_ptr<MetadataBatch> metadata_batch = db().CreateMetadataBatch();
  sync_pb::ModelTypeState model_type_state(metadata_batch->GetModelTypeState());
  model_type_state.set_initial_sync_done(true);
  model_type_state.set_authenticated_account_id("PersistedAccountId");
  metadata_batch->SetModelTypeState(model_type_state);
  type_processor()->ModelReadyToSync(std::move(metadata_batch));

  // Even prior to starting sync, the account ID should already be tracked.
  EXPECT_EQ("PersistedAccountId", type_processor()->TrackedAccountId());

  // If sync gets started, the account should still be tracked.
  OnSyncStarting("PersistedAccountId");
  EXPECT_EQ("PersistedAccountId", type_processor()->TrackedAccountId());
}

// Test that an initial sync handles local and remote items properly.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldMergeLocalAndRemoteChanges) {
  ModelReadyToSync();
  OnSyncStarting();

  // Local write before initial sync.
  bridge()->WriteItem(kKey1, kValue1);

  // Has data, but no metadata, entity in the processor, or commit request.
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  EXPECT_EQ(0, bridge()->merge_call_count());
  // Initial sync with one server item.
  worker()->UpdateFromServer(kHash2, GenerateSpecifics(kKey2, kValue2));
  EXPECT_EQ(1, bridge()->merge_call_count());

  // Now have data and metadata for both items, as well as a commit request for
  // the local item.
  EXPECT_EQ(2U, db().data_count());
  EXPECT_EQ(2U, db().metadata_count());
  EXPECT_EQ(2U, ProcessorEntityCount());
  EXPECT_EQ(1, db().GetMetadata(kKey1).sequence_number());
  EXPECT_EQ(0, db().GetMetadata(kKey2).sequence_number());
  worker()->VerifyPendingCommits({{kHash1}});
}

// Test that an initial sync filters out tombstones in the processor.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldFilterOutInitialTombstones) {
  ModelReadyToSync();
  OnSyncStarting();

  EXPECT_EQ(0, bridge()->merge_call_count());
  // Initial sync with a tombstone. The fake bridge checks that it doesn't get
  // any tombstones in its MergeSyncData function.
  worker()->TombstoneFromServer(kHash1);
  EXPECT_EQ(1, bridge()->merge_call_count());

  // Should still have no data, metadata, or commit requests.
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Test that an initial sync filters out updates for root nodes in the
// processor.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldFilterOutInitialRootNodes) {
  ModelReadyToSync();
  OnSyncStarting();

  UpdateResponseDataList update;
  update.push_back(worker()->GenerateTypeRootUpdateData(ModelType::SESSIONS));

  worker()->UpdateFromServer(update);
  // Root node update should be filtered out.
  EXPECT_EQ(0U, ProcessorEntityCount());
}

// Test that subsequent starts don't call MergeSyncData.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldApplyIncrementalUpdates) {
  // This sets initial_sync_done to true.
  InitializeToMetadataLoaded();

  // Write an item before sync connects.
  bridge()->WriteItem(kKey1, kValue1);
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());

  // Check that data coming from sync is treated as a normal GetUpdates.
  OnSyncStarting();
  worker()->UpdateFromServer(kHash2, GenerateSpecifics(kKey2, kValue2));
  EXPECT_EQ(0, bridge()->merge_call_count());
  EXPECT_EQ(1, bridge()->apply_call_count());
  EXPECT_EQ(2U, db().data_count());
  EXPECT_EQ(2U, db().metadata_count());
}

// Test that an error during the merge is propagated to the error handler.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldReportErrorDuringMerge) {
  ModelReadyToSync();
  OnSyncStarting();

  bridge()->ErrorOnNextCall();
  ExpectError();
  worker()->UpdateFromServer();
}

// Test that errors before it's called are passed to |start_callback| correctly.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldDeferErrorsBeforeStart) {
  type_processor()->ReportError({FROM_HERE, "boom"});
  ExpectError();
  OnSyncStarting();

  // Test OnSyncStarting happening first.
  ResetState(false);
  OnSyncStarting();
  ExpectError();
  type_processor()->ReportError({FROM_HERE, "boom"});

  // Test an error loading pending data.
  ResetStateWriteItem(kKey1, kValue1);
  bridge()->ErrorOnNextCall();
  InitializeToMetadataLoaded();
  ExpectError();
  OnSyncStarting();

  // Test an error prior to metadata load.
  ResetState(false);
  type_processor()->ReportError({FROM_HERE, "boom"});
  ExpectError();
  OnSyncStarting();
  ModelReadyToSync();

  // Test an error prior to pending data load.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  type_processor()->ReportError({FROM_HERE, "boom"});
  ExpectError();
  OnSyncStarting();
}

// This test covers race conditions during loading pending data. All cases
// start with no processor and one acked (committed to the server) item with a
// pending commit. There are three different events that occur once metadata
// is loaded:
//
// - Sync gets connected once sync in ready.
// - Commit data is loaded. This happens only after Sync gets connected.
// - Optionally, a put or delete happens to the item.
//
// This results in 1 + 6 = 7 orderings of the events.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldLoadDataForPendingCommit) {
  // Connect, data.
  EntitySpecifics specifics2 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  OnCommitDataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {specifics2});

  // Connect, data, put.
  EntitySpecifics specifics6 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  OnCommitDataLoaded();
  EntitySpecifics specifics7 = bridge()->WriteItem(kKey1, kValue2);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {specifics6});
  worker()->VerifyNthPendingCommit(1, {kHash1}, {specifics7});

  // Connect, put, data.
  EntitySpecifics specifics100 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EntitySpecifics specifics8 = bridge()->WriteItem(kKey1, kValue2);
  OnCommitDataLoaded();
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {specifics8});
  // GetData was launched as a result of GetLocalChanges call(). Since all data
  // are in memory, the 2nd pending commit should be empty.
  worker()->VerifyNthPendingCommit(1, {}, {});

  // Put, connect, data.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  EntitySpecifics specifics10 = bridge()->WriteItem(kKey1, kValue2);
  OnSyncStarting();
  EXPECT_FALSE(bridge()->GetDataCallback());
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {specifics10});

  // Connect, data, delete.
  EntitySpecifics specifics12 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  OnCommitDataLoaded();
  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {specifics12});
  worker()->VerifyNthPendingCommit(1, {kHash1}, {kEmptySpecifics});

  // Connect, delete, data.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  bridge()->DeleteItem(kKey1);
  OnCommitDataLoaded();
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {kEmptySpecifics});
  // GetData was launched as a result of GetLocalChanges call(). Since all data
  // are in memory, the 2nd pending commit should be empty.
  worker()->VerifyNthPendingCommit(1, {}, {});

  // Delete, connect, data.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  bridge()->DeleteItem(kKey1);
  OnSyncStarting();
  EXPECT_FALSE(bridge()->GetDataCallback());
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {kEmptySpecifics});
}

// Tests cases where pending data loads synchronously.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldHandleSynchronousDataLoad) {
  // Model, sync.
  EntitySpecifics specifics1 = ResetStateWriteItem(kKey1, kValue1);
  bridge()->ExpectSynchronousDataCallback();
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {specifics1});

  // Sync, model.
  EntitySpecifics specifics2 = ResetStateWriteItem(kKey1, kValue1);
  OnSyncStarting();
  bridge()->ExpectSynchronousDataCallback();
  InitializeToMetadataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {specifics2});
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
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldLoadPendingDelete) {
  // Connect.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {kEmptySpecifics});

  // Connect, put.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  EntitySpecifics specifics1 = bridge()->WriteItem(kKey1, kValue2);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {kEmptySpecifics});
  worker()->VerifyNthPendingCommit(1, {kHash1}, {specifics1});

  // Put, connect.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  EntitySpecifics specifics2 = bridge()->WriteItem(kKey1, kValue2);
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {specifics2});

  // Connect, delete.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {kEmptySpecifics});
  worker()->VerifyNthPendingCommit(1, {kHash1}, {kEmptySpecifics});

  // Delete, connect.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  bridge()->DeleteItem(kKey1);
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {kEmptySpecifics});
}

// Test that loading a committed item does not queue another commit.
TEST_F(ClientTagBasedModelTypeProcessorTest,
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
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldCommitLocalCreation) {
  InitializeToReadyState();
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  bridge()->WriteItem(kKey1, kValue1);

  // Verify the commit request this operation has triggered.
  worker()->VerifyPendingCommits({{kHash1}});
  const CommitRequestData& tag1_request_data =
      worker()->GetLatestPendingCommitForHash(kHash1);
  const EntityData& tag1_data = tag1_request_data.entity.value();

  EXPECT_EQ(kUncommittedVersion, tag1_request_data.base_version);
  EXPECT_TRUE(tag1_data.id.empty());
  EXPECT_FALSE(tag1_data.creation_time.is_null());
  EXPECT_FALSE(tag1_data.modification_time.is_null());
  EXPECT_EQ(kKey1, tag1_data.non_unique_name);
  EXPECT_FALSE(tag1_data.is_deleted());
  EXPECT_EQ(kKey1, tag1_data.specifics.preference().name());
  EXPECT_EQ(kValue1, tag1_data.specifics.preference().value());

  EXPECT_EQ(1U, db().metadata_count());
  const EntityMetadata metadata = db().GetMetadata(kKey1);
  EXPECT_TRUE(metadata.has_client_tag_hash());
  EXPECT_FALSE(metadata.has_server_id());
  EXPECT_FALSE(metadata.is_deleted());
  EXPECT_EQ(1, metadata.sequence_number());
  EXPECT_EQ(0, metadata.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata.server_version());
  EXPECT_TRUE(metadata.has_creation_time());
  EXPECT_TRUE(metadata.has_modification_time());
  EXPECT_TRUE(metadata.has_specifics_hash());

  worker()->AckOnePendingCommit();
  EXPECT_EQ(1U, db().metadata_count());
  const EntityMetadata acked_metadata = db().GetMetadata(kKey1);
  EXPECT_TRUE(acked_metadata.has_server_id());
  EXPECT_EQ(1, acked_metadata.sequence_number());
  EXPECT_EQ(1, acked_metadata.acked_sequence_number());
  EXPECT_EQ(1, acked_metadata.server_version());
}

// Test that an error applying metadata changes from a commit response is
// propagated to the error handler.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldReportErrorApplyingAck) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);
  bridge()->ErrorOnNextCall();
  ExpectError();
  worker()->AckOnePendingCommit();
}

// The purpose of this test case is to test setting |client_tag_hash| and |id|
// on the EntityData object as we pass it into the Put method of the processor.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldOverrideFieldsForLocalUpdate) {
  const std::string kId1 = "cid1";
  const std::string kId2 = "cid2";

  InitializeToReadyState();
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  std::unique_ptr<EntityData> entity_data = std::make_unique<EntityData>();
  entity_data->specifics.mutable_preference()->set_name(kKey1);
  entity_data->specifics.mutable_preference()->set_value(kValue1);

  entity_data->non_unique_name = kKey1;
  entity_data->client_tag_hash = kHash1;
  entity_data->id = kId1;
  bridge()->WriteItem(kKey1, std::move(entity_data));

  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  ASSERT_FALSE(worker()->HasPendingCommitForHash(kHash3));
  ASSERT_TRUE(worker()->HasPendingCommitForHash(kHash1));
  EXPECT_EQ(1U, db().metadata_count());
  const EntityData& out_entity1 =
      worker()->GetLatestPendingCommitForHash(kHash1).entity.value();
  const EntityMetadata metadata_v1 = db().GetMetadata(kKey1);

  EXPECT_EQ(kId1, out_entity1.id);
  EXPECT_NE(kHash3, out_entity1.client_tag_hash);
  EXPECT_EQ(kValue1, out_entity1.specifics.preference().value());
  EXPECT_EQ(kId1, metadata_v1.server_id());
  EXPECT_EQ(metadata_v1.client_tag_hash(), out_entity1.client_tag_hash);

  entity_data = std::make_unique<EntityData>();
  // This is a sketchy move here, changing the name will change the generated
  // storage key and client tag values.
  entity_data->specifics.mutable_preference()->set_name(kKey2);
  entity_data->specifics.mutable_preference()->set_value(kValue2);
  entity_data->non_unique_name = kKey2;
  entity_data->client_tag_hash = kHash3;
  // Make sure ID isn't overwritten either.
  entity_data->id = kId2;
  bridge()->WriteItem(kKey1, std::move(entity_data));

  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  ASSERT_FALSE(worker()->HasPendingCommitForHash(kHash3));
  ASSERT_TRUE(worker()->HasPendingCommitForHash(kHash1));
  EXPECT_EQ(1U, db().metadata_count());
  const EntityData& out_entity2 =
      worker()->GetLatestPendingCommitForHash(kHash1).entity.value();
  const EntityMetadata metadata_v2 = db().GetMetadata(kKey1);

  EXPECT_EQ(kValue2, out_entity2.specifics.preference().value());
  // Should still see old cid1 value, override is not respected on update.
  EXPECT_EQ(kId1, out_entity2.id);
  EXPECT_EQ(kId1, metadata_v2.server_id());
  EXPECT_EQ(metadata_v2.client_tag_hash(), out_entity2.client_tag_hash);

  // Specifics have changed so the hashes should not match.
  EXPECT_NE(metadata_v1.specifics_hash(), metadata_v2.specifics_hash());
}

// Creates a new local item then modifies it.
// Thoroughly tests data generated by modification of server-unknown item.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldCommitLocalUpdate) {
  InitializeToReadyState();

  bridge()->WriteItem(kKey1, kValue1);
  ASSERT_EQ(1U, db().metadata_count());
  worker()->VerifyPendingCommits({{kHash1}});

  const CommitRequestData& request_data_v1 =
      worker()->GetLatestPendingCommitForHash(kHash1);
  const EntityData& data_v1 = request_data_v1.entity.value();
  const EntityMetadata metadata_v1 = db().GetMetadata(kKey1);

  bridge()->WriteItem(kKey1, kValue2);
  EXPECT_EQ(1U, db().metadata_count());
  worker()->VerifyPendingCommits({{kHash1}, {kHash1}});

  const CommitRequestData& request_data_v2 =
      worker()->GetLatestPendingCommitForHash(kHash1);
  const EntityData& data_v2 = request_data_v2.entity.value();
  const EntityMetadata metadata_v2 = db().GetMetadata(kKey1);

  // Test some of the relations between old and new commit requests.
  EXPECT_GT(request_data_v2.sequence_number, request_data_v1.sequence_number);
  EXPECT_EQ(data_v1.specifics.preference().value(), kValue1);

  // Perform a thorough examination of the update-generated request.
  EXPECT_EQ(kUncommittedVersion, request_data_v2.base_version);
  EXPECT_TRUE(data_v2.id.empty());
  EXPECT_FALSE(data_v2.creation_time.is_null());
  EXPECT_FALSE(data_v2.modification_time.is_null());
  EXPECT_EQ(kKey1, data_v2.non_unique_name);
  EXPECT_FALSE(data_v2.is_deleted());
  EXPECT_EQ(kKey1, data_v2.specifics.preference().name());
  EXPECT_EQ(kValue2, data_v2.specifics.preference().value());

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
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldIgnoreRedundantLocalUpdate) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);
  EXPECT_EQ(1U, db().metadata_count());
  worker()->VerifyPendingCommits({{kHash1}});

  bridge()->WriteItem(kKey1, kValue1);
  worker()->VerifyPendingCommits({{kHash1}});
}

// Thoroughly tests the data generated by a server item creation.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldProcessRemoteCreation) {
  InitializeToReadyState();
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  const EntityData& data = db().GetData(kKey1);
  EXPECT_FALSE(data.id.empty());
  EXPECT_EQ(kKey1, data.specifics.preference().name());
  EXPECT_EQ(kValue1, data.specifics.preference().value());
  EXPECT_FALSE(data.creation_time.is_null());
  EXPECT_FALSE(data.modification_time.is_null());
  EXPECT_EQ(kKey1, data.non_unique_name);
  EXPECT_FALSE(data.is_deleted());

  const EntityMetadata metadata = db().GetMetadata(kKey1);
  EXPECT_TRUE(metadata.has_client_tag_hash());
  EXPECT_TRUE(metadata.has_server_id());
  EXPECT_FALSE(metadata.is_deleted());
  EXPECT_EQ(0, metadata.sequence_number());
  EXPECT_EQ(0, metadata.acked_sequence_number());
  EXPECT_EQ(1, metadata.server_version());
  EXPECT_TRUE(metadata.has_creation_time());
  EXPECT_TRUE(metadata.has_modification_time());
  EXPECT_TRUE(metadata.has_specifics_hash());
}

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldIgnoreRemoteUpdatesForRootNodes) {
  InitializeToReadyState();
  UpdateResponseDataList update;
  update.push_back(worker()->GenerateTypeRootUpdateData(ModelType::SESSIONS));

  worker()->UpdateFromServer(update);
  // Root node update should be filtered out.
  EXPECT_EQ(0U, ProcessorEntityCount());
}

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldIgnoreRemoteUpdatesWithUnexpectedClientTagHash) {
  InitializeToReadyState();
  worker()->UpdateFromServer(kHash2, GenerateSpecifics(kKey1, kValue1));
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
}

// Test that an error applying changes from a server update is
// propagated to the error handler.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldReportErrorApplyingUpdate) {
  InitializeToReadyState();
  bridge()->ErrorOnNextCall();
  ExpectError();
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
}

// Thoroughly tests the data generated by a server item creation.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldProcessRemoteUpdate) {
  InitializeToReadyState();

  // Local add writes data and metadata; ack writes metadata again.
  WriteItemAndAck(kKey1, kValue1);
  EXPECT_EQ(1U, db().data_change_count());
  EXPECT_EQ(2U, db().metadata_change_count());

  // Redundant update from server doesn't write data but updates metadata.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
  EXPECT_EQ(1U, db().data_change_count());
  EXPECT_EQ(3U, db().metadata_change_count());

  // A reflection (update already received) is ignored completely.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1),
                             0 /* version_offset */);
  EXPECT_EQ(1U, db().data_change_count());
  EXPECT_EQ(3U, db().metadata_change_count());
}

// Tests locally deleting an acknowledged item.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldCommitLocalDeletion) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  const EntityMetadata metadata_v1 = db().GetMetadata(kKey1);
  EXPECT_FALSE(metadata_v1.is_deleted());
  EXPECT_EQ(1, metadata_v1.sequence_number());
  EXPECT_EQ(1, metadata_v1.acked_sequence_number());
  EXPECT_EQ(1, metadata_v1.server_version());

  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(0U, db().data_count());
  // Metadata is not removed until the commit response comes back.
  EXPECT_EQ(1U, db().metadata_count());
  EXPECT_EQ(1U, ProcessorEntityCount());
  worker()->VerifyPendingCommits({{kHash1}});

  const EntityMetadata metadata_v2 = db().GetMetadata(kKey1);
  EXPECT_TRUE(metadata_v2.is_deleted());
  EXPECT_EQ(2, metadata_v2.sequence_number());
  EXPECT_EQ(1, metadata_v2.acked_sequence_number());
  EXPECT_EQ(1, metadata_v2.server_version());

  // Ack the delete and check that the metadata is cleared.
  worker()->AckOnePendingCommit();
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());

  // Create item again.
  WriteItemAndAck(kKey1, kValue1);
  const EntityMetadata metadata_v3 = db().GetMetadata(kKey1);
  EXPECT_FALSE(metadata_v3.is_deleted());
  EXPECT_EQ(1, metadata_v3.sequence_number());
  EXPECT_EQ(1, metadata_v3.acked_sequence_number());
  EXPECT_EQ(3, metadata_v3.server_version());
}

// Tests that item created and deleted before sync cycle doesn't get committed.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldNotCommitLocalDeletionOfUncommittedEntity) {
  InitializeToMetadataLoaded();
  bridge()->WriteItem(kKey1, kValue1);
  bridge()->DeleteItem(kKey1);

  OnSyncStarting();
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Tests creating and deleting an item locally before receiving a commit
// response, then getting the commit responses.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldHandleLocalDeletionDuringLocalCreationCommit) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);
  worker()->VerifyPendingCommits({{kHash1}});
  const CommitRequestData& data_v1 =
      worker()->GetLatestPendingCommitForHash(kHash1);

  const EntityMetadata metadata_v1 = db().GetMetadata(kKey1);
  EXPECT_FALSE(metadata_v1.is_deleted());
  EXPECT_EQ(1, metadata_v1.sequence_number());
  EXPECT_EQ(0, metadata_v1.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v1.server_version());

  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());
  EXPECT_EQ(1U, ProcessorEntityCount());
  worker()->VerifyPendingCommits({{kHash1}, {kHash1}});

  const CommitRequestData& data_v2 =
      worker()->GetLatestPendingCommitForHash(kHash1);
  EXPECT_GT(data_v2.sequence_number, data_v1.sequence_number);
  EXPECT_TRUE(data_v2.entity->id.empty());
  EXPECT_EQ(kUncommittedVersion, data_v2.base_version);
  EXPECT_TRUE(data_v2.entity->is_deleted());

  const EntityMetadata metadata_v2 = db().GetMetadata(kKey1);
  EXPECT_TRUE(metadata_v2.is_deleted());
  EXPECT_EQ(2, metadata_v2.sequence_number());
  EXPECT_EQ(0, metadata_v2.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v2.server_version());

  // A response for the first commit doesn't change much.
  worker()->AckOnePendingCommit();
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());
  EXPECT_EQ(1U, ProcessorEntityCount());

  const EntityMetadata metadata_v3 = db().GetMetadata(kKey1);
  EXPECT_TRUE(metadata_v3.is_deleted());
  EXPECT_EQ(2, metadata_v3.sequence_number());
  EXPECT_EQ(1, metadata_v3.acked_sequence_number());
  EXPECT_EQ(1, metadata_v3.server_version());

  worker()->AckOnePendingCommit();
  // The delete was acked so the metadata should now be cleared.
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
}

TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldProcessRemoteDeletion) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_EQ(1U, db().metadata_count());
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  worker()->TombstoneFromServer(kHash1);
  // Delete from server should clear the data and all the metadata.
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  // Create item again.
  WriteItemAndAck(kKey1, kValue1);
  const EntityMetadata metadata = db().GetMetadata(kKey1);
  EXPECT_FALSE(metadata.is_deleted());
  EXPECT_EQ(1, metadata.sequence_number());
  EXPECT_EQ(1, metadata.acked_sequence_number());
  EXPECT_EQ(3, metadata.server_version());
}

// Deletes an item we've never seen before.
// Should have no effect and not crash.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldIgnoreLocalDeletionOfUnknownEntity) {
  InitializeToReadyState();
  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Deletes an item we've never seen before.
// Should have no effect and not crash.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldIgnoreRemoteDeletionOfUnknownEntity) {
  InitializeToReadyState();
  worker()->TombstoneFromServer(kHash1);
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Tests that after committing entity fails, processor includes this entity in
// consecutive commits.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldRetryCommitAfterServerError) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);
  worker()->VerifyPendingCommits({{kHash1}});

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
  EXPECT_EQ(kHash1, commit_request[0].entity->client_tag_hash);
}

// Tests that GetLocalChanges honors max_entries parameter.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldTruncateLocalChangesToMaxSize) {
  InitializeToMetadataLoaded();
  bridge()->WriteItem(kKey1, kValue1);
  bridge()->WriteItem(kKey2, kValue2);

  // Reqeust at most one intity per batch, ensure that only one was returned.
  CommitRequestDataList commit_request;
  type_processor()->GetLocalChanges(
      1, base::BindOnce(&CaptureCommitRequest, &commit_request));
  EXPECT_EQ(1U, commit_request.size());
}

// Creates two different sync items.
// Verifies that the second has no effect on the first.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldHandleTwoIndependentItems) {
  InitializeToReadyState();
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  bridge()->WriteItem(kKey1, kValue1);
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());
  const EntityMetadata metadata1 = db().GetMetadata(kKey1);

  // There should be one commit request for this item only.
  worker()->VerifyPendingCommits({{kHash1}});

  bridge()->WriteItem(kKey2, kValue2);
  EXPECT_EQ(2U, db().data_count());
  EXPECT_EQ(2U, db().metadata_count());
  const EntityMetadata metadata2 = db().GetMetadata(kKey2);

  // The second write should trigger another single-item commit request.
  worker()->VerifyPendingCommits({{kHash1}, {kHash2}});

  EXPECT_FALSE(metadata1.is_deleted());
  EXPECT_EQ(1, metadata1.sequence_number());
  EXPECT_EQ(0, metadata1.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata1.server_version());

  EXPECT_FALSE(metadata2.is_deleted());
  EXPECT_EQ(1, metadata2.sequence_number());
  EXPECT_EQ(0, metadata2.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata2.server_version());
}

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldNotTreatMatchingChangesAsConflict) {
  InitializeToReadyState();
  EntitySpecifics specifics = bridge()->WriteItem(kKey1, kValue1);
  EXPECT_EQ(1U, db().data_change_count());
  EXPECT_EQ(kValue1, db().GetValue(kKey1));
  EXPECT_EQ(1U, db().metadata_change_count());
  EXPECT_EQ(kUncommittedVersion, db().GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({{kHash1}});
  worker()->VerifyNthPendingCommit(0, {kHash1}, {specifics});

  // Changes match doesn't call ResolveConflict.
  worker()->UpdateFromServer(kHash1, specifics);

  // Updated metadata but not data; no new commit request.
  EXPECT_EQ(1U, db().data_change_count());
  EXPECT_EQ(1, db().GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({{kHash1}});
}

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldResolveConflictToLocalVersion) {
  InitializeToReadyState();
  // WriteAndAck entity to get id from the server.
  WriteItemAndAck(kKey1, kValue1);
  bridge()->SetConflictResolution(ConflictResolution::UseLocal());

  // Change value locally and at the same time simulate conflicting update from
  // server.
  EntitySpecifics specifics2 = bridge()->WriteItem(kKey1, kValue2);
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue3));

  // Updated metadata but not data; new commit request.
  EXPECT_EQ(2U, db().data_change_count());
  EXPECT_EQ(4U, db().metadata_change_count());
  EXPECT_EQ(2, db().GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({{kHash1}, {kHash1}});
  worker()->VerifyNthPendingCommit(1, {kHash1}, {specifics2});
}

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldResolveConflictToLocalUndeletion) {
  InitializeToReadyState();
  ASSERT_EQ(0U, worker()->GetNumPendingCommits());

  bridge()->WriteItem(kKey1, kValue1);
  ASSERT_EQ(1U, worker()->GetNumPendingCommits());
  ASSERT_TRUE(
      worker()->GetLatestPendingCommitForHash(kHash1).entity->id.empty());

  // The update from the server should be mostly ignored because local wins, but
  // the server ID should be updated.
  bridge()->SetConflictResolution(ConflictResolution::UseLocal());
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue3));
  // In this test setup, the processor's nudge for commit immediately pulls
  // updates from the processor and list them as pending commits, so we should
  // see two commits at this point.
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());

  // Verify the commit request this operation has triggered.
  const CommitRequestData& tag1_request_data =
      worker()->GetLatestPendingCommitForHash(kHash1);
  const EntityData& tag1_data = tag1_request_data.entity.value();

  EXPECT_EQ(1, tag1_request_data.base_version);
  EXPECT_FALSE(tag1_data.id.empty());
  EXPECT_FALSE(tag1_data.creation_time.is_null());
  EXPECT_FALSE(tag1_data.modification_time.is_null());
  EXPECT_EQ(kKey1, tag1_data.non_unique_name);
  EXPECT_FALSE(tag1_data.is_deleted());
  EXPECT_EQ(kKey1, tag1_data.specifics.preference().name());
  EXPECT_EQ(kValue1, tag1_data.specifics.preference().value());

  EXPECT_EQ(1U, db().metadata_count());
  const EntityMetadata metadata = db().GetMetadata(kKey1);
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

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldResolveConflictToRemoteVersion) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);
  bridge()->SetConflictResolution(ConflictResolution::UseRemote());
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue2));

  // Updated client data and metadata; no new commit request.
  EXPECT_EQ(2U, db().data_change_count());
  EXPECT_EQ(kValue2, db().GetValue(kKey1));
  EXPECT_EQ(2U, db().metadata_change_count());
  EXPECT_EQ(1, db().GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({{kHash1}});
}

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldResolveConflictToRemoteDeletion) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);
  bridge()->SetConflictResolution(ConflictResolution::UseRemote());
  worker()->TombstoneFromServer(kHash1);

  // Updated client data and metadata; no new commit request.
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(2U, db().data_change_count());
  EXPECT_EQ(2U, db().metadata_change_count());
}

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldResolveConflictToNewVersion) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);
  bridge()->SetConflictResolution(
      ConflictResolution::UseNew(GenerateEntityData(kKey1, kValue3)));

  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue2));
  EXPECT_EQ(2U, db().data_change_count());
  EXPECT_EQ(kValue3, db().GetValue(kKey1));
  EXPECT_EQ(2U, db().metadata_change_count());
  EXPECT_EQ(1, db().GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({{kHash1}, {kHash1}});
  worker()->VerifyNthPendingCommit(1, {kHash1},
                                   {GenerateSpecifics(kKey1, kValue3)});
}

// Test proper handling of disconnect and reconnect.
//
// Creates items in various states of commit and verifies they re-attempt to
// commit on reconnect.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldDisconnectAndReconnect) {
  InitializeToReadyState();

  // The first item is fully committed.
  WriteItemAndAck(kKey1, kValue1);

  // The second item has a commit request in progress.
  bridge()->WriteItem(kKey2, kValue2);
  EXPECT_TRUE(worker()->HasPendingCommitForHash(kHash2));

  DisconnectSync();

  // The third item is added after stopping.
  bridge()->WriteItem(kKey3, kValue3);

  // Reconnect.
  OnSyncStarting();

  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  EXPECT_EQ(2U, worker()->GetNthPendingCommit(0).size());

  // The first item was already in sync.
  EXPECT_FALSE(worker()->HasPendingCommitForHash(kHash1));

  // The second item's commit was interrupted and should be retried.
  EXPECT_TRUE(worker()->HasPendingCommitForHash(kHash2));

  // The third item's commit was not started until the reconnect.
  EXPECT_TRUE(worker()->HasPendingCommitForHash(kHash3));
}

// Test proper handling of stop (without disabling sync) and re-enable.
//
// Creates items in various states of commit and verifies they do NOT attempt to
// commit on re-enable.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldStopAndKeepMetadata) {
  InitializeToReadyState();

  // The first item is fully committed.
  WriteItemAndAck(kKey1, kValue1);

  // The second item has a commit request in progress.
  bridge()->WriteItem(kKey2, kValue2);
  EXPECT_TRUE(worker()->HasPendingCommitForHash(kHash2));

  type_processor()->OnSyncStopping(KEEP_METADATA);
  EXPECT_TRUE(type_processor()->IsTrackingMetadata());

  // The third item is added after disable.
  bridge()->WriteItem(kKey3, kValue3);

  // Now we re-enable.
  OnSyncStarting();
  worker()->UpdateFromServer();

  // Once we're ready to commit, only the newest items should be committed.
  worker()->VerifyPendingCommits({{kHash3}});
}

// Test proper handling of disable and re-enable.
//
// Creates items in various states of commit and verifies they re-attempt to
// commit on re-enable.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldStopAndClearMetadata) {
  InitializeToReadyState();

  // The first item is fully committed.
  WriteItemAndAck(kKey1, kValue1);

  // The second item has a commit request in progress.
  bridge()->WriteItem(kKey2, kValue2);
  EXPECT_TRUE(worker()->HasPendingCommitForHash(kHash2));

  type_processor()->OnSyncStopping(CLEAR_METADATA);
  EXPECT_FALSE(type_processor()->IsTrackingMetadata());

  // The third item is added after disable.
  bridge()->WriteItem(kKey3, kValue3);

  // Now we re-enable.
  OnSyncStarting();
  worker()->UpdateFromServer();
  EXPECT_TRUE(type_processor()->IsTrackingMetadata());

  // Once we're ready to commit, all three local items should consider
  // themselves uncommitted and pending for commit.
  worker()->VerifyPendingCommits({{kHash1}, {kHash2}, {kHash3}});
}

// Test proper handling of disable-sync before initial sync done.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldNotClearBridgeMetadataPriorToMergeSyncData) {
  // Populate the bridge's metadata with some non-empty values for us to later
  // check that it hasn't been cleared.
  const std::string kTestEncryptionKeyName = "TestEncryptionKey";
  ModelTypeState model_type_state(db().model_type_state());
  model_type_state.set_encryption_key_name(kTestEncryptionKeyName);
  bridge()->mutable_db()->set_model_type_state(model_type_state);

  ModelReadyToSync();
  OnSyncStarting();
  ASSERT_FALSE(type_processor()->IsTrackingMetadata());

  type_processor()->OnSyncStopping(CLEAR_METADATA);
  EXPECT_FALSE(type_processor()->IsTrackingMetadata());
  EXPECT_EQ(kTestEncryptionKeyName,
            db().model_type_state().encryption_key_name());
}

// Test re-encrypt everything when desired encryption key changes.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldReencryptCommitsWithNewKey) {
  InitializeToReadyState();

  // Commit an item.
  EntitySpecifics specifics1 = WriteItemAndAck(kKey1, kValue1);
  // Create another item and don't wait for its commit response.
  EntitySpecifics specifics2 = bridge()->WriteItem(kKey2, kValue2);
  worker()->VerifyPendingCommits({{kHash2}});
  EXPECT_EQ(1U, db().GetMetadata(kKey1).sequence_number());
  EXPECT_EQ(1U, db().GetMetadata(kKey2).sequence_number());

  // Receive notice that the account's desired encryption key has changed.
  worker()->UpdateWithEncryptionKey("k1");
  // No pending commits because Tag 1 requires data load.
  ASSERT_EQ(1U, worker()->GetNumPendingCommits());
  // Tag 1 needs to go to the store to load its data before recommitting.
  OnCommitDataLoaded();
  // All data are in memory now.
  ASSERT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, {kHash1, kHash2},
                                   {specifics1, specifics2});
  // Sequence numbers in the store are updated.
  EXPECT_EQ(2U, db().GetMetadata(kKey1).sequence_number());
  EXPECT_EQ(2U, db().GetMetadata(kKey2).sequence_number());
}

// Test that an error loading pending commit data for re-encryption is
// propagated to the error handler.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldHandleErrorWhileReencrypting) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  bridge()->ErrorOnNextCall();
  ExpectError();
  worker()->UpdateWithEncryptionKey("k1");
}

// Test receipt of updates with new and old keys.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldReencryptUpdatesWithNewKey) {
  InitializeToReadyState();

  // Receive an unencrypted update.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
  ASSERT_EQ(0U, worker()->GetNumPendingCommits());

  UpdateResponseDataList update;
  // Receive an entity with old encryption as part of the update.
  update.push_back(worker()->GenerateUpdateData(
      kHash2, GenerateSpecifics(kKey2, kValue2), 1, "k1"));
  // Receive an entity with up-to-date encryption as part of the update.
  update.push_back(worker()->GenerateUpdateData(
      kHash3, GenerateSpecifics(kKey3, kValue3), 1, "k2"));
  // Set desired encryption key to k2 to force updates to some items.
  worker()->UpdateWithEncryptionKey("k2", update);

  OnCommitDataLoaded();
  // kKey1 needed data so once that's loaded, kKey1 and kKey2 are queued for
  // commit.
  worker()->VerifyPendingCommits({{kHash1, kHash2}});

  // Receive a separate update that was encrypted with key k1.
  worker()->UpdateFromServer(kHash4, GenerateSpecifics(kKey4, kValue1), 1,
                             "k1");
  // Receipt of updates encrypted with old key also forces a re-encrypt commit.
  worker()->VerifyPendingCommits({{kHash1, kHash2}, {kHash4}});

  // Receive an update that was encrypted with key k2.
  worker()->UpdateFromServer(kHash5, GenerateSpecifics(kKey5, kValue1), 1,
                             "k2");
  // That was the correct key, so no re-encryption is required.
  worker()->VerifyPendingCommits({{kHash1, kHash2}, {kHash4}});
}

// Test that re-encrypting enqueues the right data for USE_LOCAL conflicts.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldResolveConflictToLocalDuringReencryption) {
  InitializeToReadyState();
  // WriteAndAck entity to get id from the server.
  WriteItemAndAck(kKey1, kValue1);
  worker()->UpdateWithEncryptionKey("k1");
  EntitySpecifics specifics = bridge()->WriteItem(kKey1, kValue2);
  worker()->VerifyPendingCommits({{kHash1}});

  bridge()->SetConflictResolution(ConflictResolution::UseLocal());
  // Unencrypted update needs to be re-commited with key k1.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue3), 1, "");

  // Ensure the re-commit has the correct value.
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, {kHash1}, {specifics});
  EXPECT_EQ(kValue2, db().GetValue(kKey1));

  // GetData was launched as a result of GetLocalChanges call(). Since the
  // conflict resolution encrypted all entities, no data is required.
  // The extra pending commit should be empty.
  OnCommitDataLoaded();
  EXPECT_EQ(3U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(2, {}, {});
}

// Test that re-encrypting enqueues the right data for USE_REMOTE conflicts.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldResolveConflictToRemoteDuringReencryption) {
  InitializeToReadyState();
  worker()->UpdateWithEncryptionKey("k1");
  bridge()->WriteItem(kKey1, kValue1);

  bridge()->SetConflictResolution(ConflictResolution::UseRemote());
  // Unencrypted update needs to be re-commited with key k1.
  EntitySpecifics specifics = GenerateSpecifics(kKey1, kValue2);
  worker()->UpdateFromServer(kHash1, specifics, 1, "");

  // Ensure the re-commit has the correct value.
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, {kHash1}, {specifics});
  EXPECT_EQ(kValue2, db().GetValue(kKey1));
}

// Test that re-encrypting enqueues the right data for USE_NEW conflicts.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldResolveConflictToNewDuringReencryption) {
  InitializeToReadyState();
  worker()->UpdateWithEncryptionKey("k1");
  bridge()->WriteItem(kKey1, kValue1);

  bridge()->SetConflictResolution(
      ConflictResolution::UseNew(GenerateEntityData(kKey1, kValue3)));
  // Unencrypted update needs to be re-commited with key k1.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue2), 1, "");

  // Ensure the re-commit has the correct value.
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, {kHash1},
                                   {GenerateSpecifics(kKey1, kValue3)});
  EXPECT_EQ(kValue3, db().GetValue(kKey1));
}

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldHandleConflictWhileLoadingForReencryption) {
  InitializeToReadyState();
  // Create item and ack so its data is no longer cached.
  WriteItemAndAck(kKey1, kValue1);
  // Update key so that it needs to fetch data to re-commit.
  worker()->UpdateWithEncryptionKey("k1");
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  // Unencrypted update needs to be re-commited with key k1.
  EntitySpecifics specifics = GenerateSpecifics(kKey1, kValue2);
  worker()->UpdateFromServer(kHash1, specifics, 1, "");

  // Ensure the re-commit has the correct value.
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {specifics});
  EXPECT_EQ(kValue2, db().GetValue(kKey1));

  // Data load completing should add no commit requests.
  OnCommitDataLoaded();
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, {}, {});
}

// Tests that a real remote change wins over a local encryption-only change.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldIgnoreLocalEncryptionChange) {
  InitializeToReadyState();
  EntitySpecifics specifics = WriteItemAndAck(kKey1, kValue1);
  worker()->UpdateWithEncryptionKey("k1");
  OnCommitDataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {specifics});

  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue2));
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
}

// Tests that updates without client tags get dropped.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldDropRemoteUpdatesWithoutClientTags) {
  InitializeToReadyState();
  UpdateResponseDataList updates;
  updates.push_back(worker()->GenerateUpdateData(
      /*tag_hash=*/"", GenerateSpecifics(kKey1, kValue1), 1, "k1"));

  worker()->UpdateFromServer(updates);

  // Verify that the data wasn't actually stored.
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, db().data_count());
}

class WalletDataClientTagBasedModelTypeProcessorTest
    : public ClientTagBasedModelTypeProcessorTest {
 protected:
  ModelType GetModelType() override { return AUTOFILL_WALLET_DATA; }
};

// Tests that updates for Wallet data without client tags get client tags
// assigned, and not dropped.
// TODO(crbug.com/874001): Remove this feature-specific logic when the right
// solution for Wallet data has been decided.
TEST_F(WalletDataClientTagBasedModelTypeProcessorTest,
       ShouldCreateClientTagsForWallet) {
  InitializeToReadyState();

  // Commit an item.
  UpdateResponseDataList updates;
  updates.push_back(worker()->GenerateUpdateData(
      /*tag_hash=*/"", GenerateSpecifics(kKey1, kValue1), 1, "k1"));
  worker()->UpdateFromServer(updates);

  // Verify that the data was stored.
  EXPECT_EQ(1U, db().metadata_count());
  EXPECT_EQ(1U, db().data_count());
  EXPECT_FALSE(db().GetMetadata(kKey1).client_tag_hash().empty());
}

// Tests that a real local change wins over a remote encryption-only change.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldIgnoreRemoteEncryption) {
  InitializeToReadyState();
  EntitySpecifics specifics1 = WriteItemAndAck(kKey1, kValue1);

  EntitySpecifics specifics2 = bridge()->WriteItem(kKey1, kValue2);
  UpdateResponseDataList update;
  update.push_back(worker()->GenerateUpdateData(kHash1, specifics1, 1, "k1"));
  worker()->UpdateWithEncryptionKey("k1", update);

  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, {kHash1}, {specifics2});
}

// Same as above but with two commit requests before one ack.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldIgnoreRemoteEncryptionInterleaved) {
  InitializeToReadyState();
  // WriteAndAck entity to get id from the server.
  WriteItemAndAck(kKey1, kValue1);
  EntitySpecifics specifics1 = bridge()->WriteItem(kKey1, kValue2);
  EntitySpecifics specifics2 = bridge()->WriteItem(kKey1, kValue3);
  worker()->AckOnePendingCommit();
  // kValue2 is now the base value.
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, {kHash1}, {specifics2});

  UpdateResponseDataList update;
  update.push_back(worker()->GenerateUpdateData(kHash1, specifics1, 1, "k1"));
  worker()->UpdateWithEncryptionKey("k1", update);

  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, {kHash1}, {specifics2});
}

// Tests that UpdateStorageKey propagates storage key to ProcessorEntityTracker
// and updates corresponding entity's metadata in MetadataChangeList, and
// UntrackEntity will remove corresponding ProcessorEntityTracker and do not add
// any entity's metadata into MetadataChangeList.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldUpdateStorageKey) {
  // Setup bridge to not support calls to GetStorageKey. This will cause
  // FakeModelTypeSyncBridge to call UpdateStorageKey for new entities and will
  // DCHECK if GetStorageKey gets called.
  bridge()->SetSupportsGetStorageKey(false);
  ModelReadyToSync();
  OnSyncStarting();

  // Initial update from server should be handled by MergeSyncData.
  UpdateResponseDataList updates;
  updates.push_back(
      worker()->GenerateUpdateData(kHash1, GenerateSpecifics(kKey1, kValue1)));
  // Create update which will be ignored by bridge.
  updates.push_back(
      worker()->GenerateUpdateData(kHash3, GenerateSpecifics(kKey3, kValue3)));
  bridge()->SetKeyToIgnore(kKey3);
  worker()->UpdateFromServer(updates);
  EXPECT_EQ(1, bridge()->merge_call_count());
  EXPECT_EQ(1U, ProcessorEntityCount());
  // Metadata should be written under kKey1. This means that UpdateStorageKey
  // was called and value of storage key got propagated to MetadataChangeList.
  EXPECT_TRUE(db().HasMetadata(kKey1));
  EXPECT_EQ(1U, db().metadata_count());
  EXPECT_EQ(0, bridge()->get_storage_key_call_count());

  // Local update of kKey1 should affect the same entity. This ensures that
  // storage key to client tag hash mapping was updated on the previous step.
  bridge()->WriteItem(kKey1, kValue2);
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_EQ(1U, db().metadata_count());

  // Second update from server should be handled by ApplySyncChanges. Similarly
  // It should call UpdateStorageKey, not GetStorageKey.
  worker()->UpdateFromServer(kHash2, GenerateSpecifics(kKey2, kValue2));
  EXPECT_EQ(1, bridge()->apply_call_count());
  EXPECT_TRUE(db().HasMetadata(kKey2));
  EXPECT_EQ(2U, db().metadata_count());
  EXPECT_EQ(0, bridge()->get_storage_key_call_count());
}

// Tests that reencryption scenario works correctly for types that don't support
// GetStorageKey(). When update from server delivers updated encryption key, all
// entities should be reencrypted including new entity that just got received
// from server.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldReencryptDatatypeWithoutStorageKeySupport) {
  bridge()->SetSupportsGetStorageKey(false);
  InitializeToReadyState();

  UpdateResponseDataList update;
  update.push_back(worker()->GenerateUpdateData(
      kHash1, GenerateSpecifics(kKey1, kValue1), 1, "ek1"));
  worker()->UpdateWithEncryptionKey("ek2", update);
  worker()->VerifyPendingCommits({{kHash1}});
}

// Tests that UntrackEntity won't propagate storage key to
// ProcessorEntityTracker, and no entity's metadata are added into
// MetadataChangeList.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldUntrackEntity) {
  // Setup bridge to not support calls to GetStorageKey. This will cause
  // FakeModelTypeSyncBridge to call UpdateStorageKey for new entities and will
  // DCHECK if GetStorageKey gets called.
  bridge()->SetSupportsGetStorageKey(false);
  bridge()->SetKeyToIgnore(kKey1);
  ModelReadyToSync();
  OnSyncStarting();

  // Initial update from server should be handled by MergeSyncData.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
  EXPECT_EQ(1, bridge()->merge_call_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  // Metadata should not be written under kUntrackKey1. This means that
  // UntrackEntity was called and corresponding ProcessorEntityTracker is
  // removed and no storage key got propagated to MetadataChangeList.
  EXPECT_FALSE(db().HasMetadata(kHash1));
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0, bridge()->get_storage_key_call_count());
}

// Tests that UntrackEntityForStorage won't propagate storage key to
// ProcessorEntityTracker, and no entity's metadata are added into
// MetadataChangeList.
TEST_F(ClientTagBasedModelTypeProcessorTest, ShouldUntrackEntityForStorageKey) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);
  worker()->VerifyPendingCommits({{kHash1}});
  worker()->AckOnePendingCommit();

  // Check the processor tracks the entity.
  StatusCounters status_counters;
  type_processor()->GetStatusCountersForDebugging(
      base::BindOnce(&CaptureStatusCounters, &status_counters));
  ASSERT_EQ(1u, status_counters.num_entries);
  ASSERT_NE(nullptr, GetEntityForStorageKey(kKey1));

  // The bridge deletes the data locally and does not want to sync the deletion.
  // It only untracks the entity.
  type_processor()->UntrackEntityForStorageKey(kKey1);

  // The deletion is not synced up.
  worker()->VerifyPendingCommits({});
  // The processor tracks no entity any more.
  type_processor()->GetStatusCountersForDebugging(
      base::BindOnce(&CaptureStatusCounters, &status_counters));
  EXPECT_EQ(status_counters.num_entries, 0U);
  EXPECT_EQ(nullptr, GetEntityForStorageKey(kKey1));
}

// Tests that UntrackEntityForStorage does not crash if no such entity is being
// tracked.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldIgnoreUntrackEntityForInexistentStorageKey) {
  InitializeToReadyState();

  // This should not crash for an unknown storage key and simply ignore the
  // call.
  type_processor()->UntrackEntityForStorageKey(kKey1);

  // No deletion is not synced up.
  worker()->VerifyPendingCommits({});
  // The processor tracks no entity.
  StatusCounters status_counters;
  type_processor()->GetStatusCountersForDebugging(
      base::BindOnce(&CaptureStatusCounters, &status_counters));
  EXPECT_EQ(status_counters.num_entries, 0U);
  EXPECT_EQ(nullptr, GetEntityForStorageKey(kKey1));
}

class FullUpdateClientTagBasedModelTypeProcessorTest
    : public ClientTagBasedModelTypeProcessorTest {
 protected:
  bool SupportsIncrementalUpdates() override { return false; }
};

// Tests that ClientTagBasedModelTypeProcessor can do garbage collection by
// version.
// Garbage collection by version is used by the server to replace all data on
// the client, and is implemented by calling MergeSyncData on the bridge.
TEST_F(FullUpdateClientTagBasedModelTypeProcessorTest,
       ShouldApplyGarbageCollectionByVersionFullUpdate) {
  InitializeToReadyState();
  UpdateResponseDataList updates;
  updates.push_back(worker()->GenerateUpdateData(
      /*tag_hash=*/"", GenerateSpecifics(kKey1, kValue1), 1, "k1"));
  updates.push_back(worker()->GenerateUpdateData(
      /*tag_hash=*/"", GenerateSpecifics(kKey2, kValue2), 2, "k2"));

  // Create 2 entries, one is version 3, another is version 1.
  sync_pb::GarbageCollectionDirective garbage_collection_directive;
  garbage_collection_directive.set_version_watermark(1);
  worker()->UpdateWithGarbageCollection(updates, garbage_collection_directive);
  WriteItemAndAck(kKey1, kValue1);
  WriteItemAndAck(kKey2, kValue2);

  // Verify entries are created correctly.
  ASSERT_EQ(2U, ProcessorEntityCount());
  ASSERT_EQ(2U, db().metadata_count());
  ASSERT_EQ(2U, db().data_count());
  ASSERT_EQ(0U, worker()->GetNumPendingCommits());
  ASSERT_EQ(1, bridge()->merge_call_count());

  // Tell the client to delete all data.
  sync_pb::GarbageCollectionDirective new_directive;
  new_directive.set_version_watermark(2);
  worker()->UpdateWithGarbageCollection(new_directive);

  // Verify that merge is called on the bridge to replace the current sync data.
  EXPECT_EQ(2, bridge()->merge_call_count());
  // Verify that the processor cleared all metadata.
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Tests that the processor reports an error for updates without a version GC
// directive that are received for types that don't support incremental updates.
TEST_F(FullUpdateClientTagBasedModelTypeProcessorTest,
       ShouldReportErrorForUnsupportedIncrementalUpdate) {
  InitializeToReadyState();

  ExpectError();
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
}

// Tests that empty updates without a version GC are ignored for types that
// don't support incremental updates.
TEST_F(FullUpdateClientTagBasedModelTypeProcessorTest,
       ShouldIgnoreEmptyUpdate) {
  InitializeToReadyState();

  worker()->UpdateFromServer(UpdateResponseDataList());

  // Verify that the empty update was ignored in the processor.
  EXPECT_EQ(0, bridge()->merge_call_count());
  EXPECT_EQ(0, bridge()->apply_call_count());
}

// Tests that the processor correctly handles an initial (non-empty) update
// without any gc directives (as it happens in the migration to USS).
TEST_F(FullUpdateClientTagBasedModelTypeProcessorTest,
       ShouldProcessInitialUpdate) {
  // Do not set any model type state to emulate that initial sync has not been
  // done yet.
  ModelReadyToSync();
  OnSyncStarting();

  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
}

// Tests that the processor reports an error for updates with a version GC
// directive that are received for types that support incremental updates.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldApplyGarbageCollectionByVersion) {
  InitializeToReadyState();

  ExpectError();
  sync_pb::GarbageCollectionDirective garbage_collection_directive;
  garbage_collection_directive.set_version_watermark(2);
  worker()->UpdateWithGarbageCollection(garbage_collection_directive);
}

// Tests that ClientTagBasedModelTypeProcessor can do garbage collection by age.
// Create 2 entries, one is 15-days-old, another is 5-days-old. Check if sync
// will delete 15-days-old entry when server set expired age is 10 days.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldApplyGarbageCollectionByAge) {
  InitializeToReadyState();

  // Create 2 entries, one is 15-days-old, another is 5-days-old.
  std::unique_ptr<EntityData> entity_data =
      bridge()->GenerateEntityData(kKey1, kValue1);
  entity_data->modification_time =
      base::Time::Now() - base::TimeDelta::FromDays(15);
  WriteItemAndAck(kKey1, std::move(entity_data));
  entity_data = bridge()->GenerateEntityData(kKey2, kValue2);
  entity_data->modification_time =
      base::Time::Now() - base::TimeDelta::FromDays(5);
  WriteItemAndAck(kKey2, std::move(entity_data));

  // Verify entries are created correctly.
  EXPECT_EQ(2U, ProcessorEntityCount());
  EXPECT_EQ(2U, db().metadata_count());
  EXPECT_EQ(2U, db().data_count());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  // Expired the entries which are older than 10 days.
  sync_pb::GarbageCollectionDirective garbage_collection_directive;
  garbage_collection_directive.set_age_watermark_in_days(10);
  worker()->UpdateWithGarbageCollection(garbage_collection_directive);

  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_EQ(1U, db().metadata_count());
  EXPECT_EQ(2U, db().data_count());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Tests that ClientTagBasedModelTypeProcessor can do garbage collection by item
// limit. Create 3 entries, one is 15-days-old, one is 10-days-old, another is
// 5-days-old. Check if sync will delete 15-days-old entry when server set
// limited item is 2 days.
TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldApplyGarbageCollectionByItemLimit) {
  InitializeToReadyState();

  // Create 3 entries, one is 15-days-old, one is 10-days-old, another is
  // 5-days-old.
  std::unique_ptr<EntityData> entity_data =
      bridge()->GenerateEntityData(kKey1, kValue1);
  entity_data->modification_time =
      base::Time::Now() - base::TimeDelta::FromDays(15);
  WriteItemAndAck(kKey1, std::move(entity_data));
  entity_data = bridge()->GenerateEntityData(kKey2, kValue2);
  entity_data->modification_time =
      base::Time::Now() - base::TimeDelta::FromDays(5);
  WriteItemAndAck(kKey2, std::move(entity_data));
  entity_data = bridge()->GenerateEntityData(kKey3, kValue3);
  entity_data->modification_time =
      base::Time::Now() - base::TimeDelta::FromDays(10);
  WriteItemAndAck(kKey3, std::move(entity_data));

  // Verify entries are created correctly.
  EXPECT_EQ(3U, ProcessorEntityCount());
  EXPECT_EQ(3U, db().metadata_count());
  EXPECT_EQ(3U, db().data_count());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  // Expired the entries which are older than 10 days.
  sync_pb::GarbageCollectionDirective garbage_collection_directive;
  garbage_collection_directive.set_max_number_of_items(2);
  worker()->UpdateWithGarbageCollection(garbage_collection_directive);

  EXPECT_EQ(2U, ProcessorEntityCount());
  EXPECT_EQ(2U, db().metadata_count());
  EXPECT_EQ(3U, db().data_count());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldDeleteMetadataWhenCacheGuidMismatch) {
  // Commit item.
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  // Reset the processor to simulate a restart.
  ResetState(/*keep_db=*/true);

  // A new processor loads the metadata after changing the cache GUID.
  bridge()->SetInitialSyncDone(true);

  std::unique_ptr<MetadataBatch> metadata_batch = db().CreateMetadataBatch();
  sync_pb::ModelTypeState model_type_state(metadata_batch->GetModelTypeState());
  model_type_state.set_cache_guid("WRONG_CACHE_GUID");
  metadata_batch->SetModelTypeState(model_type_state);

  type_processor()->ModelReadyToSync(std::move(metadata_batch));
  ASSERT_TRUE(type_processor()->IsModelReadyToSyncForTest());

  OnSyncStarting();

  // Model should still be ready to sync.
  ASSERT_TRUE(type_processor()->IsModelReadyToSyncForTest());
  // OnSyncStarting() should have completed.
  EXPECT_NE(nullptr, worker());
  // Upon a mismatch, metadata should have been cleared.
  EXPECT_EQ(0U, db().metadata_count());
}

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldNotConnectImmediatelyAfterGuidMismatchIfNotReadyToSync) {
  // Commit item.
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  // Reset the processor to simulate a restart.
  ResetState(/*keep_db=*/true);

  // Force future stops cause the model to become unready.
  bridge()->SetStopSyncResponse(
      ModelTypeSyncBridge::StopSyncResponse::kModelNoLongerReadyToSync);

  // A new processor loads the metadata after changing the cache GUID.
  bridge()->SetInitialSyncDone(true);

  std::unique_ptr<MetadataBatch> metadata_batch = db().CreateMetadataBatch();
  sync_pb::ModelTypeState model_type_state(metadata_batch->GetModelTypeState());
  model_type_state.set_cache_guid("WRONG_CACHE_GUID");
  metadata_batch->SetModelTypeState(model_type_state);

  type_processor()->ModelReadyToSync(std::move(metadata_batch));
  ASSERT_TRUE(type_processor()->IsModelReadyToSyncForTest());

  OnSyncStarting();

  // Model should not be ready to sync.
  ASSERT_FALSE(type_processor()->IsModelReadyToSyncForTest());
  // OnSyncStarting() should NOT have completed.
  EXPECT_EQ(nullptr, worker());
  // Upon a mismatch, metadata should have been cleared.
  EXPECT_EQ(0U, db().metadata_count());

  // Calling ModelReadyToSync() should complete OnSyncStarting().
  type_processor()->ModelReadyToSync(std::make_unique<MetadataBatch>());
  EXPECT_NE(nullptr, worker());
}

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldClearOrphanMetadataInGetLocalChangesWhenDataIsMissing) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);

  // Loose the entity in the bridge (keeping the metadata around as an orphan).
  bridge()->MimicBugToLooseItemWithoutNotifyingProcessor(kKey1);

  ASSERT_FALSE(db().HasData(kKey1));
  ASSERT_TRUE(db().HasMetadata(kKey1));
  ASSERT_NE(nullptr, GetEntityForStorageKey(kKey1));

  // Reset "the browser" so that the processor looses the copy of the data.
  ResetState(/*keep_db=*/true);

  // Initializing the processor will trigger it to commit again. It does not
  // have a copy of the data so it will ask the bridge.
  {
    base::HistogramTester histogram_tester;
    bridge()->ExpectSynchronousDataCallback();
    InitializeToReadyState();

    histogram_tester.ExpectBucketCount(
        "Sync.ModelTypeOrphanMetadata",
        /*bucket=*/ModelTypeToHistogramInt(GetModelType()), /*count=*/1);
  }

  // Orphan metadata should have been deleted.
  EXPECT_EQ(1, bridge()->apply_call_count());
  EXPECT_FALSE(db().HasMetadata(kKey1));
  EXPECT_EQ(nullptr, GetEntityForStorageKey(kKey1));

  base::HistogramTester histogram_tester;

  // Do it again, explicitly. The processor does not track the entity so it
  // shouldn't ask the bridge or return local changes.
  CommitRequestDataList commit_request;
  type_processor()->GetLocalChanges(
      INT_MAX, base::BindOnce(&CaptureCommitRequest, &commit_request));
  EXPECT_EQ(0U, commit_request.size());
  EXPECT_TRUE(bridge()->GetDataCallback().is_null());

  // The processor should not report orphan again in UMA.
  histogram_tester.ExpectBucketCount(
      "Sync.ModelTypeOrphanMetadata",
      /*bucket=*/ModelTypeToHistogramInt(GetModelType()), /*count=*/0);
}

TEST_F(
    ClientTagBasedModelTypeProcessorTest,
    ShouldNotReportOrphanMetadataInGetLocalChangesWhenDataIsAlreadyUntracked) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);

  // Loose the entity in the bridge (keeping the metadata around as an orphan).
  bridge()->MimicBugToLooseItemWithoutNotifyingProcessor(kKey1);

  ASSERT_FALSE(db().HasData(kKey1));
  ASSERT_TRUE(db().HasMetadata(kKey1));
  ASSERT_NE(nullptr, GetEntityForStorageKey(kKey1));

  // Reset "the browser" so that the processor looses the copy of the data.
  ResetState(/*keep_db=*/true);

  // Initializing the processor will trigger it to commit again. It does not
  // have a copy of the data so it will ask the bridge.
  base::HistogramTester histogram_tester;
  InitializeToReadyState();

  // The bridge has not passed the data back to the processor, we untrack the
  // entity.
  type_processor()->UntrackEntityForStorageKey(kKey1);

  // Make the bridge pass the data back to the processor. Because the entity is
  // already deleted in the processor, no further orphan gets reported.
  std::move(bridge()->GetDataCallback()).Run();
  histogram_tester.ExpectTotalCount("Sync.ModelTypeOrphanMetadata",
                                    /*count=*/0);

  EXPECT_EQ(0, bridge()->apply_call_count());
  EXPECT_EQ(nullptr, GetEntityForStorageKey(kKey1));

  // The expectation below documents the fact that bridges are responsible for
  // clearing the untracked metadata from their databases.
  EXPECT_TRUE(db().HasMetadata(kKey1));
}

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldNotReportOrphanMetadataInGetLocalChangesWhenDataIsAlreadyDeleted) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);

  // Loose the entity in the bridge (keeping the metadata around as an orphan).
  bridge()->MimicBugToLooseItemWithoutNotifyingProcessor(kKey1);

  ASSERT_FALSE(db().HasData(kKey1));
  ASSERT_TRUE(db().HasMetadata(kKey1));
  ASSERT_NE(nullptr, GetEntityForStorageKey(kKey1));

  // Reset "the browser" so that the processor looses the copy of the data.
  ResetState(/*keep_db=*/true);

  // Initializing the processor will trigger it to commit again. It does not
  // have a copy of the data so it will ask the bridge.
  base::HistogramTester histogram_tester;
  InitializeToReadyState();

  // The bridge has not passed the data back to the processor, we delete the
  // entity.
  bridge()->DeleteItem(kKey1);

  // Make the bridge pass the data back to the processor. Because the entity is
  // already deleted in the processor, no further orphan gets reported.
  std::move(bridge()->GetDataCallback()).Run();
  histogram_tester.ExpectTotalCount("Sync.ModelTypeOrphanMetadata",
                                    /*count=*/0);

  EXPECT_EQ(0, bridge()->apply_call_count());
  EXPECT_EQ(nullptr, GetEntityForStorageKey(kKey1));
}

TEST_F(ClientTagBasedModelTypeProcessorTest,
       ShouldNotReportOrphanMetadataInGetLocalChangesWhenDataIsPresent) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);

  ASSERT_TRUE(db().HasData(kKey1));
  ASSERT_TRUE(db().HasMetadata(kKey1));
  ASSERT_NE(nullptr, GetEntityForStorageKey(kKey1));

  // Reset "the browser" so that the processor looses the copy of the data.
  ResetState(/*keep_db=*/true);

  // Initializing the processor will trigger it to commit again. It does not
  // have a copy of the data so it will ask the bridge.
  base::HistogramTester histogram_tester;
  bridge()->ExpectSynchronousDataCallback();
  InitializeToReadyState();

  // Now everything is committed and GetLocalChanges is empty again.
  CommitRequestDataList commit_request;
  type_processor()->GetLocalChanges(
      INT_MAX, base::BindOnce(&CaptureCommitRequest, &commit_request));
  EXPECT_EQ(0U, commit_request.size());

  // The processor never reports any orphan.
  histogram_tester.ExpectTotalCount("Sync.ModelTypeOrphanMetadata",
                                    /*count=*/0);

  EXPECT_TRUE(db().HasData(kKey1));
  EXPECT_TRUE(db().HasMetadata(kKey1));
  EXPECT_NE(nullptr, GetEntityForStorageKey(kKey1));
}

class CommitOnlyClientTagBasedModelTypeProcessorTest
    : public ClientTagBasedModelTypeProcessorTest {
 protected:
  bool IsCommitOnly() override { return true; }
};

TEST_F(CommitOnlyClientTagBasedModelTypeProcessorTest,
       ShouldExposeNewlyTrackedAccountId) {
  ModelReadyToSync();
  ASSERT_EQ("", type_processor()->TrackedAccountId());
  OnSyncStarting("SomeAccountId");
  EXPECT_EQ("SomeAccountId", type_processor()->TrackedAccountId());
}

TEST_F(CommitOnlyClientTagBasedModelTypeProcessorTest,
       ShouldExposePreviouslyTrackedAccountId) {
  std::unique_ptr<MetadataBatch> metadata_batch = db().CreateMetadataBatch();
  sync_pb::ModelTypeState model_type_state(metadata_batch->GetModelTypeState());
  model_type_state.set_initial_sync_done(true);
  model_type_state.set_authenticated_account_id("PersistedAccountId");
  metadata_batch->SetModelTypeState(model_type_state);
  type_processor()->ModelReadyToSync(std::move(metadata_batch));

  // Even prior to starting sync, the account ID should already be tracked.
  EXPECT_EQ("PersistedAccountId", type_processor()->TrackedAccountId());

  // If sync gets started, the account should still be tracked.
  OnSyncStarting("PersistedAccountId");
  EXPECT_EQ("PersistedAccountId", type_processor()->TrackedAccountId());
}

// Test that commit only types are deleted after commit response.
TEST_F(CommitOnlyClientTagBasedModelTypeProcessorTest,
       ShouldCommitAndDeleteWhenAcked) {
  InitializeToReadyState();
  EXPECT_TRUE(db().model_type_state().initial_sync_done());

  bridge()->WriteItem(kKey1, kValue1);
  worker()->VerifyPendingCommits({{kHash1}});
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());

  worker()->AckOnePendingCommit();
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
}

// Test that commit only types maintain tracking of entities while unsynced
// changes exist.
TEST_F(CommitOnlyClientTagBasedModelTypeProcessorTest,
       ShouldTrackUnsyncedChangesAfterPartialCommit) {
  InitializeToReadyState();

  bridge()->WriteItem(kKey1, kValue1);
  worker()->VerifyPendingCommits({{kHash1}});
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());

  bridge()->WriteItem(kKey1, kValue2);
  worker()->VerifyPendingCommits({{kHash1}, {kHash1}});
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());

  worker()->AckOnePendingCommit();
  worker()->VerifyPendingCommits({{kHash1}});
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());

  // The version field isn't meaningful on commit only types, so force a value
  // that isn't incremented to verify everything still works.
  worker()->AckOnePendingCommit(0 /* version_offset */);
  worker()->VerifyPendingCommits({});
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
}

}  // namespace syncer
