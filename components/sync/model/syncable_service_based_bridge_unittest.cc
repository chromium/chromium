// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/syncable_service_based_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/mock_model_type_worker.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::Invoke;
using testing::IsEmpty;
using testing::NotNull;
using testing::Pair;
using testing::Return;

const ModelType kModelType = PREFERENCES;

sync_pb::EntitySpecifics GetTestSpecifics(const std::string& name = "name") {
  sync_pb::EntitySpecifics specifics;
  // Make specifics non empty, to avoid it being interpreted as a tombstone.
  specifics.mutable_preference()->set_name(name);
  return specifics;
}

MATCHER_P(SyncDataMatches, name, "") {
  return arg.IsValid() && arg.GetDataType() == kModelType &&
         arg.GetSpecifics().preference().name() == name;
}

MATCHER_P2(SyncChangeMatches, change_type, name, "") {
  return arg.change_type() == change_type &&
         arg.sync_data().GetDataType() == kModelType &&
         arg.sync_data().GetSpecifics().preference().name() == name;
}

MATCHER_P(HasName, name, "") {
  return arg && arg->specifics.preference().name() == name;
}

class MockSyncableService : public SyncableService {
 public:
  MOCK_METHOD(void, WaitUntilReadyToSync, (base::OnceClosure done), (override));
  MOCK_METHOD(absl::optional<syncer::ModelError>,
              MergeDataAndStartSyncing,
              (ModelType type,
               const SyncDataList& initial_sync_data,
               std::unique_ptr<SyncChangeProcessor> sync_processor,
               std::unique_ptr<SyncErrorFactory> sync_error_factory),
              (override));
  MOCK_METHOD(void, StopSyncing, (ModelType type), (override));
  MOCK_METHOD(absl::optional<ModelError>,
              ProcessSyncChanges,
              (const base::Location& from_here,
               const SyncChangeList& change_list),
              (override));
  MOCK_METHOD(SyncDataList, GetAllSyncData, (ModelType type), (const override));
};

class SyncableServiceBasedBridgeTest : public ::testing::Test {
 public:
  SyncableServiceBasedBridgeTest(const SyncableServiceBasedBridgeTest&) =
      delete;
  SyncableServiceBasedBridgeTest& operator=(
      const SyncableServiceBasedBridgeTest&) = delete;

 protected:
  SyncableServiceBasedBridgeTest()
      : store_(ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    ON_CALL(syncable_service_, WaitUntilReadyToSync)
        .WillByDefault(
            Invoke([](base::OnceClosure done) { std::move(done).Run(); }));
    ON_CALL(syncable_service_, MergeDataAndStartSyncing)
        .WillByDefault(
            [&](ModelType type, const SyncDataList& initial_sync_data,
                std::unique_ptr<SyncChangeProcessor> sync_processor,
                std::unique_ptr<SyncErrorFactory> sync_error_factory) {
              start_syncing_sync_processor_ = std::move(sync_processor);
              return absl::nullopt;
            });
  }

  ~SyncableServiceBasedBridgeTest() override = default;

  void InitializeBridge(ModelType model_type = kModelType) {
    real_processor_ =
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            model_type, /*dump_stack=*/base::DoNothing());
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
    bridge_ = std::make_unique<SyncableServiceBasedBridge>(
        model_type,
        ModelTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        mock_processor_.CreateForwardingProcessor(), &syncable_service_);
  }

  void ShutdownBridge() {
    bridge_.reset();
    // The mock is still delegating to |real_processor_|, so we reset it too.
    ASSERT_TRUE(testing::Mock::VerifyAndClear(&mock_processor_));
    real_processor_.reset();
  }

  syncer::DataTypeActivationRequest GetTestActivationRequest() {
    syncer::DataTypeActivationRequest request;
    request.error_handler = mock_error_handler_.Get();
    request.cache_guid = "TestCacheGuid";
    request.authenticated_account_id = CoreAccountId("SomeAccountId");
    return request;
  }

  void StartSyncing() {
    base::RunLoop loop;
    real_processor_->OnSyncStarting(
        GetTestActivationRequest(),
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
  const ClientTagHash kClientTagHash =
      ClientTagHash::FromUnhashed(kModelType, kClientTag);

  base::test::SingleThreadTaskEnvironment task_environment_;
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
};

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldStartSyncingWithEmptyInitialRemoteData) {
  // Bridge initialization alone, without sync itself starting, should not
  // issue calls to the syncable service.
  EXPECT_CALL(syncable_service_, MergeDataAndStartSyncing).Times(0);
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
              MergeDataAndStartSyncing(kModelType,
                                       ElementsAre(SyncDataMatches("name1")),
                                       NotNull(), NotNull()));
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));
  EXPECT_THAT(GetAllData(), ElementsAre(Pair(kClientTagHash.value(), _)));
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldWaitUntilModelReadyToSync) {
  base::OnceClosure syncable_service_ready_cb;
  ON_CALL(syncable_service_, WaitUntilReadyToSync)
      .WillByDefault(Invoke([&](base::OnceClosure done) {
        syncable_service_ready_cb = std::move(done);
      }));

  EXPECT_CALL(mock_processor_, ModelReadyToSync).Times(0);
  EXPECT_CALL(syncable_service_, WaitUntilReadyToSync).Times(0);
  EXPECT_CALL(syncable_service_, MergeDataAndStartSyncing).Times(0);

  // Bridge initialization alone, without sync itself starting, should not
  // issue calls to the syncable service.
  InitializeBridge();

  EXPECT_CALL(syncable_service_, WaitUntilReadyToSync);
  // Required to initialize the store.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(syncable_service_ready_cb);

  // Sync itself starting should wait until the syncable service becomes ready,
  // before issuing any other call (e.g. MergeDataAndStartSyncing()).
  real_processor_->OnSyncStarting(GetTestActivationRequest(),
                                  base::DoNothing());

  // When the SyncableService gets ready, the bridge should propagate this
  // information to the processor.
  EXPECT_CALL(mock_processor_, ModelReadyToSync);
  std::move(syncable_service_ready_cb).Run();
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldStopSyncableServiceIfPreviouslyStarted) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer();

  EXPECT_CALL(syncable_service_, StopSyncing(kModelType));
  real_processor_->OnSyncStopping(CLEAR_METADATA);

  EXPECT_CALL(syncable_service_, StopSyncing).Times(0);
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
  EXPECT_CALL(syncable_service_, StopSyncing).Times(0);
  InitializeBridge();
  StartSyncing();
  real_processor_->OnSyncStopping(KEEP_METADATA);
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldNotStopSyncableServiceDuringShutdownIfNotPreviouslyStarted) {
  EXPECT_CALL(syncable_service_, StopSyncing).Times(0);
  InitializeBridge();
  StartSyncing();
  ShutdownBridge();
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldPropagateErrorDuringStart) {
  // Instrument MergeDataAndStartSyncing() to return an error.
  ON_CALL(syncable_service_, MergeDataAndStartSyncing)
      .WillByDefault(Return(ModelError(FROM_HERE, "Test error")));

  EXPECT_CALL(mock_error_handler_, Run);

  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer();

  // Since the syncable service failed to start, it shouldn't be stopped.
  EXPECT_CALL(syncable_service_, StopSyncing).Times(0);
  ShutdownBridge();
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldKeepSyncingWhenSyncStoppedTemporarily) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));

  // Stopping Sync temporarily (KEEP_METADATA) should *not* result in the
  // SyncableService being stopped.
  EXPECT_CALL(syncable_service_, StopSyncing).Times(0);
  real_processor_->OnSyncStopping(KEEP_METADATA);
  EXPECT_THAT(GetAllData(), ElementsAre(Pair(kClientTagHash.value(), _)));

  // Since the SyncableService wasn't stopped, it shouldn't get restarted either
  // when Sync starts up again.
  EXPECT_CALL(syncable_service_, MergeDataAndStartSyncing).Times(0);
  StartSyncing();

  // Finally, shutting down the bridge (during browser shutdown) should also
  // stop the SyncableService.
  EXPECT_CALL(syncable_service_, StopSyncing(kModelType));
  ShutdownBridge();
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldStartSyncingWithPreviousDirectoryDataAfterRestart) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));

  // Mimic restart, which shouldn't start syncing until OnSyncStarting() is
  // received (exercised in StartSyncing()).
  EXPECT_CALL(syncable_service_, MergeDataAndStartSyncing).Times(0);
  ShutdownBridge();
  InitializeBridge();

  EXPECT_CALL(syncable_service_,
              MergeDataAndStartSyncing(kModelType,
                                       ElementsAre(SyncDataMatches("name1")),
                                       NotNull(), NotNull()));
  StartSyncing();
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldSupportDisableReenableSequence) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics());
  real_processor_->OnSyncStopping(CLEAR_METADATA);
  EXPECT_THAT(GetAllData(), IsEmpty());

  EXPECT_CALL(syncable_service_, MergeDataAndStartSyncing).Times(0);
  StartSyncing();
  EXPECT_CALL(
      syncable_service_,
      MergeDataAndStartSyncing(kModelType, IsEmpty(), NotNull(), NotNull()));
  worker_->UpdateFromServer();
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldPropagateLocalEntitiesDuringMerge) {
  ON_CALL(syncable_service_, MergeDataAndStartSyncing)
      .WillByDefault([&](ModelType type, const SyncDataList& initial_sync_data,
                         std::unique_ptr<SyncChangeProcessor> sync_processor,
                         std::unique_ptr<SyncErrorFactory> sync_error_factory) {
        SyncChangeList change_list;
        change_list.emplace_back(
            FROM_HERE, SyncChange::ACTION_ADD,
            SyncData::CreateLocalData(kClientTag, "title", GetTestSpecifics()));
        const absl::optional<ModelError> error =
            sync_processor->ProcessSyncChanges(FROM_HERE, change_list);
        EXPECT_FALSE(error.has_value());
        return absl::nullopt;
      });

  InitializeBridge();
  StartSyncing();

  EXPECT_CALL(mock_processor_,
              Put(kClientTagHash.value(), NotNull(), NotNull()));
  worker_->UpdateFromServer();
  EXPECT_THAT(GetAllData(), ElementsAre(Pair(kClientTagHash.value(), _)));
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldPropagateLocalCreation) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer();
  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(), IsEmpty());

  EXPECT_CALL(mock_processor_,
              Put(kClientTagHash.value(), NotNull(), NotNull()));

  SyncChangeList change_list;
  change_list.emplace_back(
      FROM_HERE, SyncChange::ACTION_ADD,
      SyncData::CreateLocalData(kClientTag, "title", GetTestSpecifics()));
  const absl::optional<ModelError> error =
      start_syncing_sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.has_value());
  EXPECT_THAT(GetAllData(), ElementsAre(Pair(kClientTagHash.value(), _)));
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldPropagateLocalUpdate) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));
  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash.value(), HasName("name1"))));

  EXPECT_CALL(mock_processor_,
              Put(kClientTagHash.value(), NotNull(), NotNull()));

  SyncChangeList change_list;
  change_list.emplace_back(FROM_HERE, SyncChange::ACTION_UPDATE,
                           SyncData::CreateLocalData(
                               kClientTag, "title", GetTestSpecifics("name2")));
  const absl::optional<ModelError> error =
      start_syncing_sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.has_value());
  EXPECT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash.value(), HasName("name2"))));
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldPropagateLocalDeletion) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));
  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash.value(), HasName("name1"))));

  EXPECT_CALL(mock_processor_, Delete(kClientTagHash.value(), NotNull()));

  SyncChangeList change_list;
  change_list.emplace_back(FROM_HERE, SyncChange::ACTION_DELETE,
                           SyncData::CreateLocalDelete(kClientTag, kModelType));

  const absl::optional<ModelError> error =
      start_syncing_sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.has_value());
  EXPECT_THAT(GetAllData(), IsEmpty());
}

TEST_F(SyncableServiceBasedBridgeTest,
       ShouldIgnoreLocalCreationIfPreviousError) {
  EXPECT_CALL(mock_processor_, Put).Times(0);

  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer();
  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(), IsEmpty());

  // We fake an error, reported by the bridge.
  EXPECT_CALL(mock_error_handler_, Run);
  real_processor_->ReportError(ModelError(FROM_HERE, "Fake error"));
  ASSERT_TRUE(real_processor_->GetError());

  // Further local changes should be ignored.
  SyncChangeList change_list;
  change_list.emplace_back(
      FROM_HERE, SyncChange::ACTION_ADD,
      SyncData::CreateLocalData(kClientTag, "title", GetTestSpecifics()));
  const absl::optional<ModelError> error =
      start_syncing_sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_TRUE(error.has_value());
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
              ElementsAre(Pair(kClientTagHash.value(), HasName("name1"))));
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldPropagateRemoteUpdates) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));
  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash.value(), HasName("name1"))));

  EXPECT_CALL(syncable_service_,
              ProcessSyncChanges(_, ElementsAre(SyncChangeMatches(
                                        SyncChange::ACTION_UPDATE, "name2"))));
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name2"));
  EXPECT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash.value(), HasName("name2"))));

  // A second update for the same entity.
  EXPECT_CALL(syncable_service_,
              ProcessSyncChanges(_, ElementsAre(SyncChangeMatches(
                                        SyncChange::ACTION_UPDATE, "name3"))));
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name3"));
  EXPECT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash.value(), HasName("name3"))));
}

TEST_F(SyncableServiceBasedBridgeTest, ShouldPropagateRemoteDeletion) {
  InitializeBridge();
  StartSyncing();
  worker_->UpdateFromServer(kClientTagHash, GetTestSpecifics("name1"));
  ASSERT_THAT(start_syncing_sync_processor_, NotNull());
  ASSERT_THAT(GetAllData(),
              ElementsAre(Pair(kClientTagHash.value(), HasName("name1"))));

  EXPECT_CALL(syncable_service_,
              ProcessSyncChanges(_, ElementsAre(SyncChangeMatches(
                                        SyncChange::ACTION_DELETE, "name1"))));
  worker_->TombstoneFromServer(kClientTagHash);
  EXPECT_THAT(GetAllData(), IsEmpty());
}

TEST(SyncableServiceBasedBridgeLocalChangeProcessorTest,
     ShouldDropIfCommitted) {
  const std::string kClientTagHash = "clienttaghash1";

  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<ModelTypeStore> store =
      ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
  SyncableServiceBasedBridge::InMemoryStore in_memory_store;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor;

  in_memory_store[kClientTagHash] = sync_pb::EntitySpecifics();

  std::unique_ptr<SyncChangeProcessor> sync_change_processor =
      SyncableServiceBasedBridge::CreateLocalChangeProcessorForTesting(
          HISTORY_DELETE_DIRECTIVES, store.get(), &in_memory_store,
          &mock_processor);

  EXPECT_CALL(mock_processor, IsEntityUnsynced(kClientTagHash))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_processor, UntrackEntityForStorageKey(kClientTagHash));

  sync_pb::EntitySpecifics specifics;
  specifics.mutable_history_delete_directive();

  SyncChangeList change_list;
  change_list.push_back(
      SyncChange(FROM_HERE, SyncChange::ACTION_DELETE,
                 SyncData::CreateRemoteData(
                     specifics, ClientTagHash::FromHashed(kClientTagHash))));

  sync_change_processor->ProcessSyncChanges(FROM_HERE, change_list);

  EXPECT_EQ(0U, in_memory_store.count(kClientTagHash));
}

TEST(SyncableServiceBasedBridgeLocalChangeProcessorTest,
     ShouldNotDropIfUnsynced) {
  const std::string kClientTagHash = "clienttaghash1";

  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<ModelTypeStore> store =
      ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
  SyncableServiceBasedBridge::InMemoryStore in_memory_store;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor;

  in_memory_store[kClientTagHash] = sync_pb::EntitySpecifics();

  std::unique_ptr<SyncChangeProcessor> sync_change_processor =
      SyncableServiceBasedBridge::CreateLocalChangeProcessorForTesting(
          HISTORY_DELETE_DIRECTIVES, store.get(), &in_memory_store,
          &mock_processor);

  EXPECT_CALL(mock_processor, IsEntityUnsynced(kClientTagHash))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_processor, UntrackEntityForStorageKey).Times(0);

  sync_pb::EntitySpecifics specifics;
  specifics.mutable_history_delete_directive();

  SyncChangeList change_list;
  change_list.push_back(
      SyncChange(FROM_HERE, SyncChange::ACTION_DELETE,
                 SyncData::CreateRemoteData(
                     specifics, ClientTagHash::FromHashed(kClientTagHash))));

  sync_change_processor->ProcessSyncChanges(FROM_HERE, change_list);

  EXPECT_EQ(1U, in_memory_store.count(kClientTagHash));
}

TEST_F(SyncableServiceBasedBridgeTest, ConflictShouldUseRemote) {
  InitializeBridge();

  EntityData remote_data;
  remote_data.client_tag_hash = kClientTagHash;
  remote_data.specifics = GetTestSpecifics();
  ASSERT_FALSE(remote_data.is_deleted());

  EXPECT_THAT(bridge_->ResolveConflict("storagekey1", remote_data),
              Eq(ConflictResolution::kUseRemote));
}

TEST_F(SyncableServiceBasedBridgeTest,
       ConflictWithRemoteDeletionShouldUseLocal) {
  InitializeBridge();

  EntityData remote_data;
  remote_data.client_tag_hash = kClientTagHash;
  ASSERT_TRUE(remote_data.is_deleted());

  EXPECT_THAT(bridge_->ResolveConflict("storagekey1", remote_data),
              Eq(ConflictResolution::kUseLocal));
}

// This ensures that for extensions, the conflict is resolved in favor of the
// server, to prevent extensions from being reinstalled after uninstall.
TEST_F(SyncableServiceBasedBridgeTest,
       ConflictWithRemoteExtensionUninstallShouldUseRemote) {
  InitializeBridge(EXTENSIONS);

  EntityData remote_data;
  remote_data.client_tag_hash = kClientTagHash;
  ASSERT_TRUE(remote_data.is_deleted());

  EXPECT_THAT(bridge_->ResolveConflict("storagekey1", remote_data),
              Eq(ConflictResolution::kUseRemote));
}

// Same as above but for APPS.
TEST_F(SyncableServiceBasedBridgeTest,
       ConflictWithRemoteAppUninstallShouldUseRemote) {
  InitializeBridge(APPS);

  EntityData remote_data;
  remote_data.client_tag_hash = kClientTagHash;
  ASSERT_TRUE(remote_data.is_deleted());

  EXPECT_THAT(bridge_->ResolveConflict("storagekey1", remote_data),
              Eq(ConflictResolution::kUseRemote));
}

}  // namespace
}  // namespace syncer
