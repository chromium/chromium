// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/syncable_service_based_bridge.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/sync_merge_result.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/engine/mock_model_type_worker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using testing::DoAll;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::NotNull;
using testing::Pair;
using testing::Return;
using testing::SaveArg;
using testing::_;

using ModelCryptographer = SyncableServiceBasedBridge::ModelCryptographer;

const ModelType kModelType = PREFERENCES;

sync_pb::EntitySpecifics GetTestSpecifics(const std::string& name = "name") {
  sync_pb::EntitySpecifics specifics;
  // Make specifics non empty, to avoid it being interpreted as a tombstone.
  specifics.mutable_preference()->set_name(name);
  return specifics;
}

MATCHER_P(SyncDataRemoteMatches, name, "") {
  return arg.IsValid() && !arg.IsLocal() && arg.GetDataType() == kModelType &&
         arg.GetSpecifics().preference().name() == name;
}

MATCHER_P2(SyncChangeMatches, change_type, name, "") {
  return arg.IsValid() && change_type == arg.change_type() &&
         arg.sync_data().GetDataType() == kModelType &&
         arg.sync_data().GetSpecifics().preference().name() == name;
}

MATCHER_P(HasName, name, "") {
  return arg && arg->specifics.preference().name() == name;
}

MATCHER(IsEncrypted, "") {
  return arg && arg->specifics.encrypted().has_blob();
}

MATCHER_P(IsEncryptedWithKey, key, "") {
  return arg && arg->specifics.encrypted().key_name() == key;
}

class MockSyncableService : public SyncableService {
 public:
  MOCK_METHOD4(
      MergeDataAndStartSyncing,
      SyncMergeResult(ModelType type,
                      const SyncDataList& initial_sync_data,
                      std::unique_ptr<SyncChangeProcessor> sync_processor,
                      std::unique_ptr<SyncErrorFactory> sync_error_factory));
  MOCK_METHOD1(StopSyncing, void(ModelType type));
  MOCK_METHOD2(ProcessSyncChanges,
               SyncError(const base::Location& from_here,
                         const SyncChangeList& change_list));
  MOCK_CONST_METHOD1(GetAllSyncData, SyncDataList(ModelType type));
};

// Simple ModelCryptographer implementation that serializes the input proto to a
// string, prepends a prefix (representing the encryption key) and puts the
// result in proto field EntitySpecifics.encrypted.blob.
class TestModelCryptographer : public ModelCryptographer {
 public:
  TestModelCryptographer() { AddKey("DefaultKey"); }

  void AddKey(const std::string& key) { keys_.push_back(key); }

  base::Optional<ModelError> Decrypt(
      sync_pb::EntitySpecifics* specifics) override {
    if (!specifics->has_encrypted()) {
      return ModelError(FROM_HERE, "Not encrypted");
    }
    const std::string& actual_key = specifics->encrypted().key_name();
    if (std::find(keys_.begin(), keys_.end(), actual_key) == keys_.end()) {
      return ModelError(FROM_HERE, "Unknown key");
    }
    if (specifics->encrypted().blob().find(actual_key) != 0) {
      return ModelError(FROM_HERE, "Corrupt blob");
    }
    sync_pb::EntitySpecifics unencrypted_specifics;
    const std::string blob_without_key =
        specifics->encrypted().blob().substr(actual_key.size());
    if (!unencrypted_specifics.ParseFromString(blob_without_key)) {
      return ModelError(FROM_HERE, "Cannot parse blob");
    }
    *specifics = unencrypted_specifics;
    return base::nullopt;
  }

  base::Optional<ModelError> Encrypt(
      sync_pb::EntitySpecifics* specifics) override {
    if (specifics->has_encrypted()) {
      return ModelError(FROM_HERE, "Already encrypted");
    }
    sync_pb::EntitySpecifics encrypted_specifics;
    encrypted_specifics.mutable_encrypted()->set_key_name(keys_.back());
    encrypted_specifics.mutable_encrypted()->set_blob(
        keys_.back() + specifics->SerializeAsString());
    *specifics = encrypted_specifics;
    return base::nullopt;
  }

 protected:
  ~TestModelCryptographer() override {}

 private:
  std::vector<std::string> keys_;

  DISALLOW_COPY_AND_ASSIGN(TestModelCryptographer);
};

class SyncableServiceBasedBridgeTest : public ::testing::Test {
 protected:
  SyncableServiceBasedBridgeTest()
      : store_(ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    ON_CALL(syncable_service_, MergeDataAndStartSyncing(_, _, _, _))
        .WillByDefault(
            [&](ModelType type, const SyncDataList& initial_sync_data,
                std::unique_ptr<SyncChangeProcessor> sync_processor,
                std::unique_ptr<SyncErrorFactory> sync_error_factory) {
              start_syncing_sync_processor_ = std::move(sync_processor);
              return SyncMergeResult(kModelType);
            });
  }

  ~SyncableServiceBasedBridgeTest() override {}

  void InitializeBridge(
      scoped_refptr<ModelCryptographer> cryptographer = nullptr) {
    real_processor_ =
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            kModelType, /*dump_stack=*/base::DoNothing(),
            /*commit_only=*/false);
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
    bridge_ = std::make_unique<SyncableServiceBasedBridge>(
        kModelType,
        ModelTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        mock_processor_.CreateForwardingProcessor(), &syncable_service_,
        cryptographer);
  }

  void ShutdownBridge() {
    bridge_.reset();
    // The mock is still delegating to |real_processor_|, so we reset it too.
    ASSERT_TRUE(testing::Mock::VerifyAndClear(&mock_processor_));
    real_processor_.reset();
  }

  void StartSyncing() {
    syncer::DataTypeActivationRequest request;
    request.error_handler = mock_error_handler_.Get();
    request.cache_guid = "TestCacheGuid";
    request.authenticated_account_id = "SomeAccountId";

    base::RunLoop loop;
    real_processor_->OnSyncStarting(
        request,
        base::BindLambdaForTesting(
            [&](std::unique_ptr<syncer::DataTypeActivationResponse> response) {
              worker_ = std::make_unique<MockModelTypeWorker>(
                  response->model_type_state, real_processor_.get());
              loop.Quit();
            }));
    loop.Run();
  }

  std::map<std::string, std::unique_ptr<EntityData>> GetAllData() {
    base::RunLoop loop;
    std::unique_ptr<DataBatch> batch;
    bridge_->GetAllDataForDebugging(base::BindLambdaForTesting(
        [&loop, &batch](std::unique_ptr<DataBatch> input_batch) {
          batch = std::move(input_batch);
          loop.Quit();
        }));
    loop.Run();
    EXPECT_NE(nullptr, batch);

    std::map<std::string, std::unique_ptr<EntityData>> storage_key_to_data;
    while (batch && batch->HasNext()) {
      storage_key_to_data.insert(batch->Next());
    }
    return storage_key_to_data;
  }

  const std::string kClientTag = "clienttag";
  const std::string kClientTagHash =
      GenerateSyncableHash(kModelType, kClientTag);

  base::MessageLoop message_loop_;
  testing::NiceMock<MockSyncableService> syncable_service_;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  base::MockCallback<ModelErrorHandler> mock_error_handler_;
  const std::unique_ptr<ModelTypeStore> store_;
  std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor> real_processor_;
  std::unique_ptr<SyncableServiceBasedBridge> bridge_;
  std::unique_ptr<MockModelTypeWorker> worker_;
  // SyncChangeProcessor received via MergeDataAndStartSyncing(), or null if it
  // hasn't been called.
  std::unique_ptr<SyncChangeProcessor> start_syncing_sync_processor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncableServiceBasedBridgeTest);
};

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldStartSyncingWithEmptyInitialRemoteData) {
  // Bridge initialization alone, without sync itself starting, should not
  // issue calls to the syncable service.
  EXPECT_CALL(syncable_service_, MergeDataAndStartSyncing(_, _, _, _)).Times(0);
  InitializeBridge();

  // Starting sync itself is also not sufficient, until initial remote data is
  // received.
  StartSyncing();

  // Once the initial data is fetched from the server,
  // MergeDataAndStartSyncing() should be exercised.
  EXPECT_CALL(
      syncable_service_,
      MergeDataAndStartSyncing(kModelType, IsEmpty(), NotNull(), NotNull()));
  worker_->UpdateFromServer();
  EXPECT_THAT(GetAllData(), IsEmpty());
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldStartSyncingWithNonEmptyInitialRemoteData) {
  InitializeBridge();
  StartSyncing();

  // Once the initial data is fetched from the server,
  // MergeDataAndStartSyncing() should be exercised.
  EXPECT_CALL(syncable_service_,
              MergeDataAndStartSyncing(
                  kModelType, ElementsAre(SyncDataRemoteMatches("name1")),
                  NotNull(), NotNull()));
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));
  EXPECT_THAT(GetAllData(), ElementsAre(Pair(kClientTagHash, _)));
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldStopSyncableServiceIfPreviouslyStarted) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer();

  EXPECT_CALL(syncable_service_, StopSyncing(kModelType));
  real_processor_->OnSyncStopping(KEEP_METADATA);

  EXPECT_CALL(syncable_service_, StopSyncing(_)).Times(0);
  ShutdownBridge();
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldStopSyncableServiceDuringShutdownIfPreviouslyStarted) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer();

  EXPECT_CALL(syncable_service_, StopSyncing(kModelType));
  ShutdownBridge();
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldNotStopSyncableServiceIfNotPreviouslyStarted) {
  EXPECT_CALL(syncable_service_, StopSyncing(_)).Times(0);
  InitializeBridge();
  StartSyncing();
  real_processor_->OnSyncStopping(KEEP_METADATA);
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldNotStopSyncableServiceDuringShutdownIfNotPreviouslyStarted) {
  EXPECT_CALL(syncable_service_, StopSyncing(_)).Times(0);
  InitializeBridge();
  StartSyncing();
  ShutdownBridge();
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldPropagateErrorDuringStart) {
  // Instrument MergeDataAndStartSyncing() to return an error.
  SyncMergeResult merge_result(kModelType);
  merge_result.set_error(SyncError(FROM_HERE, SyncError::PERSISTENCE_ERROR,
                                   "Test error", kModelType));
  ON_CALL(syncable_service_, MergeDataAndStartSyncing(_, _, _, _))
      .WillByDefault(Return(merge_result));

  EXPECT_CALL(mock_error_handler_, Run(_));

  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer();

  // Since the syncable service failed to start, it shouldn't be stopped.
  EXPECT_CALL(syncable_service_, StopSyncing(_)).Times(0);
  ShutdownBridge();
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldStartSyncingWithPreviousDirectoryDataWithoutRestart) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));
  real_processor_->OnSyncStopping(KEEP_METADATA);
  EXPECT_THAT(GetAllData(), ElementsAre(Pair(kClientTagHash, _)));

  EXPECT_CALL(syncable_service_,
              MergeDataAndStartSyncing(
                  kModelType, ElementsAre(SyncDataRemoteMatches("name1")),
                  NotNull(), NotNull()));
  StartSyncing();
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldStartSyncingWithPreviousDirectoryDataAfterRestart) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));

  // Mimic restart, which shouldn't start syncing until OnSyncStarting() is
  // received (exercised in StartSyncing()).
  EXPECT_CALL(syncable_service_, MergeDataAndStartSyncing(_, _, _, _)).Times(0);
  ShutdownBridge();
  InitializeBridge();

  EXPECT_CALL(syncable_service_,
              MergeDataAndStartSyncing(
                  kModelType, ElementsAre(SyncDataRemoteMatches("name1")),
                  NotNull(), NotNull()));
  StartSyncing();
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldSupportDisableReenableSequence) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics());
  real_processor_->OnSyncStopping(CLEAR_METADATA);
  EXPECT_THAT(GetAllData(), IsEmpty());

  EXPECT_CALL(syncable_service_, MergeDataAndStartSyncing(_, _, _, _)).Times(0);
  StartSyncing();
  EXPECT_CALL(
      syncable_service_,
      MergeDataAndStartSyncing(kModelType, IsEmpty(), NotNull(), NotNull()));
  worker_->UpdateFromServer();
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldPropagateLocalEntitiesDuringMerge) {
  ON_CALL(syncable_service_, MergeDataAndStartSyncing(_, _, _, _))
      .WillByDefault([&](ModelType type, const SyncDataList& initial_sync_data,
                         std::unique_ptr<SyncChangeProcessor> sync_processor,
                         std::unique_ptr<SyncErrorFactory> sync_error_factory) {
        SyncChangeList change_list;
        change_list.emplace_back(
            FROM_HERE, SyncChange::ACTION_ADD,
            SyncData::CreateLocalData(kClientTag, "title", GetTestSpecifics()));
        const SyncError error =
            sync_processor->ProcessSyncChanges(FROM_HERE, change_list);
        EXPECT_FALSE(error.IsSet());
        return SyncMergeResult(kModelType);
      });

  InitializeBridge();
  StartSyncing();

  EXPECT_CALL(mock_processor_, Put(kClientTagHash, NotNull(), NotNull()));
  worker_->UpdateFromServer();
  EXPECT_THAT(GetAllData(), ElementsAre(Pair(kClientTagHash, _)));
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldPropagateLocalCreation) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer();
  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(), IsEmpty());

  EXPECT_CALL(mock_processor_, Put(kClientTagHash, NotNull(), NotNull()));

  SyncChangeList change_list;
  change_list.emplace_back(
      FROM_HERE, SyncChange::ACTION_ADD,
      SyncData::CreateLocalData(kClientTag, "title", GetTestSpecifics()));
  const SyncError error =
      start_syncing_sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet());
  EXPECT_THAT(GetAllData(), ElementsAre(Pair(kClientTagHash, _)));
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldPropagateLocalUpdate) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));
  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash, HasName("name1"))));

  EXPECT_CALL(mock_processor_, Put(GenerateSyncableHash(kModelType, kClientTag),
                                   NotNull(), NotNull()));

  SyncChangeList change_list;
  change_list.emplace_back(FROM_HERE, SyncChange::ACTION_UPDATE,
                           SyncData::CreateLocalData(
                               kClientTag, "title", GetTestSpecifics("name2")));
  const SyncError error =
      start_syncing_sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet());
  EXPECT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash, HasName("name2"))));
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldPropagateLocalDeletion) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));
  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash, HasName("name1"))));

  EXPECT_CALL(mock_processor_,
              Delete(GenerateSyncableHash(kModelType, kClientTag), NotNull()));

  SyncChangeList change_list;
  change_list.emplace_back(FROM_HERE, SyncChange::ACTION_DELETE,
                           SyncData::CreateLocalDelete(kClientTag, kModelType));

  const SyncError error =
      start_syncing_sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet());
  EXPECT_THAT(GetAllData(), IsEmpty());
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldIgnoreLocalCreationIfPreviousError) {
  EXPECT_CALL(mock_processor_, Put(_, _, _)).Times(0);

  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer();
  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(), IsEmpty());

  // We fake an error, reported by the bridge.
  EXPECT_CALL(mock_error_handler_, Run(_));
  real_processor_->ReportError(ModelError(FROM_HERE, "Fake error"));
  ASSERT_TRUE(real_processor_->GetError());

  // Further local changes should be ignored.
  SyncChangeList change_list;
  change_list.emplace_back(
      FROM_HERE, SyncChange::ACTION_ADD,
      SyncData::CreateLocalData(kClientTag, "title", GetTestSpecifics()));
  const SyncError error =
      start_syncing_sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_TRUE(error.IsSet());
  EXPECT_THAT(GetAllData(), IsEmpty());
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldPropagateRemoteCreation) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer();
  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(), IsEmpty());

  EXPECT_CALL(syncable_service_,
              ProcessSyncChanges(_, ElementsAre(SyncChangeMatches(
                                        SyncChange::ACTION_ADD, "name1"))));
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));
  EXPECT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash, HasName("name1"))));
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldPropagateRemoteUpdates) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));
  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash, HasName("name1"))));

  EXPECT_CALL(syncable_service_,
              ProcessSyncChanges(_, ElementsAre(SyncChangeMatches(
                                        SyncChange::ACTION_UPDATE, "name2"))));
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name2"));
  EXPECT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash, HasName("name2"))));

  // A second update for the same entity.
  EXPECT_CALL(syncable_service_,
              ProcessSyncChanges(_, ElementsAre(SyncChangeMatches(
                                        SyncChange::ACTION_UPDATE, "name3"))));
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name3"));
  EXPECT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash, HasName("name3"))));
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldPropagateRemoteDeletion) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));
  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash, HasName("name1"))));

  EXPECT_CALL(syncable_service_,
              ProcessSyncChanges(_, ElementsAre(SyncChangeMatches(
                                        SyncChange::ACTION_DELETE, "name1"))));
  worker_->TombstoneFromServer(kClientTagHash);
  EXPECT_THAT(GetAllData(), IsEmpty());
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldDecryptInitialRemoteData) {
  auto cryptographer = base::MakeRefCounted<TestModelCryptographer>();
  InitializeBridge(cryptographer);
  StartSyncing();

  EXPECT_CALL(mock_error_handler_, Run(_)).Times(0);
  // Once the initial encrypted data is fetched from the server,
  // MergeDataAndStartSyncing() should be exercised, which should receive
  // decrypted specifics.
  EXPECT_CALL(syncable_service_,
              MergeDataAndStartSyncing(
                  kModelType, ElementsAre(SyncDataRemoteMatches("name1")),
                  NotNull(), NotNull()));
  sync_pb::EntitySpecifics specifics = GetTestSpecifics("name1");
  ASSERT_FALSE(cryptographer->Encrypt(&specifics));
  worker_->UpdateFromServer(kClientTagHash, specifics);
  // The bridge's store should contain encrypted content.
  EXPECT_THAT(GetAllData(), ElementsAre(Pair(kClientTagHash, IsEncrypted())));
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldHandleDecryptionErrorForInitialRemoteData) {
  auto cryptographer = base::MakeRefCounted<TestModelCryptographer>();
  InitializeBridge(cryptographer);
  StartSyncing();

  EXPECT_CALL(mock_error_handler_, Run(_));
  EXPECT_CALL(syncable_service_, MergeDataAndStartSyncing(_, _, _, _)).Times(0);

  sync_pb::EntitySpecifics specifics;
  specifics.mutable_encrypted()->set_blob("corrupt-encrypted-blob");
  ASSERT_TRUE(cryptographer->Decrypt(&specifics));
  worker_->UpdateFromServer(kClientTagHash, specifics);
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldDecryptForRemoteDeletion) {
  auto cryptographer = base::MakeRefCounted<TestModelCryptographer>();
  InitializeBridge(cryptographer);
  StartSyncing();

  sync_pb::EntitySpecifics specifics = GetTestSpecifics("name1");
  ASSERT_FALSE(cryptographer->Encrypt(&specifics));
  worker_->UpdateFromServer(kClientTagHash, specifics);
  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(), ElementsAre(Pair(kClientTagHash, IsEncrypted())));

  EXPECT_CALL(mock_error_handler_, Run(_)).Times(0);
  EXPECT_CALL(syncable_service_,
              ProcessSyncChanges(_, ElementsAre(SyncChangeMatches(
                                        SyncChange::ACTION_DELETE, "name1"))));
  worker_->TombstoneFromServer(kClientTagHash);
  EXPECT_THAT(GetAllData(), IsEmpty());
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldDecryptPreviousDirectoryDataAfterRestart) {
  auto cryptographer = base::MakeRefCounted<TestModelCryptographer>();
  InitializeBridge(cryptographer);
  StartSyncing();

  sync_pb::EntitySpecifics specifics = GetTestSpecifics("name1");
  ASSERT_FALSE(cryptographer->Encrypt(&specifics));
  worker_->UpdateFromServer(kClientTagHash, specifics);
  // The bridge's store should contain encrypted content.
  ASSERT_THAT(GetAllData(), ElementsAre(Pair(kClientTagHash, IsEncrypted())));

  EXPECT_CALL(mock_error_handler_, Run(_)).Times(0);
  // Mimic restart, which shouldn't start syncing until OnSyncStarting() is
  // received (exercised in StartSyncing()).
  ShutdownBridge();
  InitializeBridge(cryptographer);

  EXPECT_CALL(syncable_service_,
              MergeDataAndStartSyncing(
                  kModelType, ElementsAre(SyncDataRemoteMatches("name1")),
                  NotNull(), NotNull()));
  StartSyncing();

  // The bridge's store should still contain encrypted content.
  EXPECT_THAT(GetAllData(), ElementsAre(Pair(kClientTagHash, IsEncrypted())));
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldEncryptLocalCreation) {
  auto cryptographer = base::MakeRefCounted<TestModelCryptographer>();
  InitializeBridge(cryptographer);
  StartSyncing();
  worker_->UpdateFromServer();

  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(), IsEmpty());
  EXPECT_CALL(mock_error_handler_, Run(_)).Times(0);

  // The processor should receive encrypted data.
  EXPECT_CALL(mock_processor_, Put(kClientTagHash, IsEncrypted(), NotNull()));

  SyncChangeList change_list;
  change_list.emplace_back(
      FROM_HERE, SyncChange::ACTION_ADD,
      SyncData::CreateLocalData(kClientTag, "title", GetTestSpecifics()));
  const SyncError error =
      start_syncing_sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet());
  EXPECT_THAT(GetAllData(), ElementsAre(Pair(kClientTagHash, IsEncrypted())));
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldReencryptDataUponKeyChange) {
  const std::string kEncryptionKey1 = "EncryptionKey1";
  const std::string kEncryptionKey2 = "EncryptionKey2";

  auto cryptographer = base::MakeRefCounted<TestModelCryptographer>();
  cryptographer->AddKey(kEncryptionKey1);
  InitializeBridge(cryptographer);
  StartSyncing();

  EXPECT_CALL(mock_error_handler_, Run(_)).Times(0);

  sync_pb::EntitySpecifics specifics = GetTestSpecifics("name1");
  ASSERT_FALSE(cryptographer->Encrypt(&specifics));
  worker_->UpdateFromServer(kClientTagHash, specifics);
  // The bridge's store should contain encrypted content.
  ASSERT_THAT(
      GetAllData(),
      ElementsAre(Pair(kClientTagHash, IsEncryptedWithKey(kEncryptionKey1))));

  // Mimic reencryption.
  EXPECT_CALL(mock_processor_, Put(_, _, _)).Times(0);
  cryptographer->AddKey(kEncryptionKey2);
  worker_->UpdateWithEncryptionKey(kEncryptionKey2);

  // The bridge's store should still contain encrypted content.
  EXPECT_THAT(
      GetAllData(),
      ElementsAre(Pair(kClientTagHash, IsEncryptedWithKey(kEncryptionKey2))));

  // Mimic restart, which shouldn't start syncing until OnSyncStarting() is
  // received (exercised in StartSyncing()). Having reencrypted the data should
  // not be noticeable by the SyncableService, which should receive the data
  // unencrypted.
  ShutdownBridge();
  InitializeBridge(cryptographer);

  EXPECT_CALL(syncable_service_,
              MergeDataAndStartSyncing(
                  kModelType, ElementsAre(SyncDataRemoteMatches("name1")),
                  NotNull(), NotNull()));
  StartSyncing();
}

}  // namespace
}  // namespace syncer
