// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_data_type_processor.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/nigori/nigori_sync_bridge.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/test/mock_commit_queue.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::Eq;
using testing::Ne;
using testing::NotNull;

// TODO(mamir): remove those and adjust the code accordingly.
const char kRawNigoriClientTagHash[] = "NigoriClientTagHash";

const char kNigoriNonUniqueName[] = "nigori";
const char kNigoriServerId[] = "nigori_server_id";
const char kCacheGuid[] = "generated_id";

// |*arg| must be of type std::optional<EntityData>.
MATCHER_P(OptionalEntityDataHasDecryptorTokenKeyName, expected_key_name, "") {
  return arg->specifics.nigori().keystore_decryptor_token().key_name() ==
         expected_key_name;
}

void CaptureCommitRequest(CommitRequestDataList* dst,
                          CommitRequestDataList&& src) {
  *dst = std::move(src);
}

sync_pb::DataTypeState CreateDataTypeState() {
  sync_pb::DataTypeState data_type_state;
  data_type_state.set_cache_guid(kCacheGuid);
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  return data_type_state;
}

// Creates a fake Nigori UpdateResponseData that has the keystore decryptor
// token key name set.
syncer::UpdateResponseData CreateFakeNigoriUpdateResponseData(
    const std::string keystore_decryptor_token_key_name,
    int response_version) {
  syncer::EntityData entity_data;
  entity_data.id = kNigoriServerId;
  sync_pb::NigoriSpecifics* nigori_specifics =
      entity_data.specifics.mutable_nigori();
  nigori_specifics->mutable_keystore_decryptor_token()->set_key_name(
      keystore_decryptor_token_key_name);
  entity_data.name = kNigoriNonUniqueName;

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(entity_data);
  response_data.response_version = response_version;
  return response_data;
}

CommitResponseData CreateNigoriCommitResponseData(
    const CommitRequestData& commit_request_data,
    int response_version) {
  CommitResponseData commit_response_data;
  commit_response_data.id = kNigoriServerId;
  commit_response_data.client_tag_hash =
      ClientTagHash::FromHashed(kRawNigoriClientTagHash);
  commit_response_data.sequence_number = commit_request_data.sequence_number;
  commit_response_data.response_version = response_version;
  commit_response_data.specifics_hash = commit_request_data.specifics_hash;
  commit_response_data.unsynced_time = commit_request_data.unsynced_time;
  return commit_response_data;
}

class MockNigoriSyncBridge : public NigoriSyncBridge {
 public:
  MockNigoriSyncBridge() = default;
  ~MockNigoriSyncBridge() override = default;
  MOCK_METHOD(std::optional<ModelError>,
              MergeFullSyncData,
              (std::optional<EntityData> data),
              (override));
  MOCK_METHOD(std::optional<ModelError>,
              ApplyIncrementalSyncChanges,
              (std::optional<EntityData> data),
              (override));
  MOCK_METHOD(std::unique_ptr<EntityData>, GetDataForCommit, (), (override));
  MOCK_METHOD(std::unique_ptr<EntityData>, GetDataForDebugging, (), (override));
  MOCK_METHOD(void, ApplyDisableSyncChanges, (), (override));
};

class NigoriDataTypeProcessorTest : public testing::Test {
 public:
  NigoriDataTypeProcessorTest() = default;

  void SimulateModelReadyToSync(bool initial_sync_done, int server_version) {
    NigoriMetadataBatch nigori_metadata_batch;
    nigori_metadata_batch.data_type_state.set_initial_sync_state(
        initial_sync_done
            ? sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE
            : sync_pb::
                  DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
    nigori_metadata_batch.data_type_state.set_cache_guid(kCacheGuid);
    nigori_metadata_batch.entity_metadata = sync_pb::EntityMetadata();
    nigori_metadata_batch.entity_metadata->set_creation_time(
        TimeToProtoTime(base::Time::Now()));
    nigori_metadata_batch.entity_metadata->set_sequence_number(0);
    nigori_metadata_batch.entity_metadata->set_acked_sequence_number(0);
    nigori_metadata_batch.entity_metadata->set_server_version(server_version);
    processor_.ModelReadyToSync(mock_nigori_sync_bridge(),
                                std::move(nigori_metadata_batch));
  }

  void SimulateModelReadyToSync(bool initial_sync_done) {
    SimulateModelReadyToSync(initial_sync_done, /*server_version=*/1);
  }

  void SimulateConnectSync() {
    processor_.ConnectSync(std::move(mock_commit_queue_));
  }

  void SimulateSyncStopping(SyncStopMetadataFate fate) {
    // Drop unowned reference before stopping processor which will destroy it.
    mock_commit_queue_ptr_ = nullptr;
    processor_.OnSyncStopping(fate);
  }

  MockNigoriSyncBridge* mock_nigori_sync_bridge() {
    return &mock_nigori_sync_bridge_;
  }

  MockCommitQueue* mock_commit_queue() { return mock_commit_queue_ptr_; }

  NigoriDataTypeProcessor* processor() { return &processor_; }

  bool ProcessorHasEntity() {
    TypeEntitiesCount count(NIGORI);
    base::MockCallback<base::OnceCallback<void(const TypeEntitiesCount&)>>
        capture_callback;
    EXPECT_CALL(capture_callback, Run).WillOnce(testing::SaveArg<0>(&count));
    processor()->GetTypeEntitiesCountForDebugging(capture_callback.Get());
    return count.non_tombstone_entities > 0;
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
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::NiceMock<MockNigoriSyncBridge> mock_nigori_sync_bridge_;
  std::unique_ptr<testing::NiceMock<MockCommitQueue>> mock_commit_queue_ =
      std::make_unique<testing::NiceMock<MockCommitQueue>>();
  raw_ptr<MockCommitQueue, DanglingUntriaged> mock_commit_queue_ptr_ =
      mock_commit_queue_.get();
  NigoriDataTypeProcessor processor_;
};

TEST_F(NigoriDataTypeProcessorTest, ShouldTrackTheMetadataWhenInitialSyncDone) {
  // Build a data type state with a specific cache guid.
  const std::string kOtherCacheGuid = "cache_guid";
  sync_pb::DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  data_type_state.set_cache_guid(kOtherCacheGuid);

  // Build entity metadata with a specific sequence number.
  const int kSequenceNumber = 100;
  sync_pb::EntityMetadata entity_metadata;
  entity_metadata.set_sequence_number(kSequenceNumber);
  entity_metadata.set_creation_time(TimeToProtoTime(base::Time::Now()));

  NigoriMetadataBatch nigori_metadata_batch;
  nigori_metadata_batch.data_type_state = data_type_state;
  nigori_metadata_batch.entity_metadata = entity_metadata;

  processor()->ModelReadyToSync(mock_nigori_sync_bridge(),
                                std::move(nigori_metadata_batch));

  // The data type state and the metadata should have been stored in the
  // processor.
  NigoriMetadataBatch processor_metadata_batch = processor()->GetMetadata();
  EXPECT_THAT(processor_metadata_batch.data_type_state.cache_guid(),
              Eq(kOtherCacheGuid));
  ASSERT_TRUE(processor_metadata_batch.entity_metadata);
  EXPECT_THAT(processor_metadata_batch.entity_metadata->sequence_number(),
              Eq(kSequenceNumber));
}

TEST_F(NigoriDataTypeProcessorTest, ShouldIncrementSequenceNumberWhenPut) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);
  std::optional<sync_pb::EntityMetadata> entity_metadata1 =
      processor()->GetMetadata().entity_metadata;
  ASSERT_TRUE(entity_metadata1);

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_nigori();
  entity_data->name = kNigoriNonUniqueName;

  processor()->Put(std::move(entity_data));

  std::optional<sync_pb::EntityMetadata> entity_metadata2 =
      processor()->GetMetadata().entity_metadata;
  ASSERT_TRUE(entity_metadata1);

  EXPECT_THAT(entity_metadata2->sequence_number(),
              Eq(entity_metadata1->sequence_number() + 1));
}

TEST_F(NigoriDataTypeProcessorTest, ShouldGetEmptyLocalChanges) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);
  {
    base::MockOnceCallback<void(bool)> has_unsynced_data_cb;
    EXPECT_CALL(has_unsynced_data_cb, Run(false));
    processor()->HasUnsyncedData(has_unsynced_data_cb.Get());
  }
  CommitRequestDataList commit_request;
  processor()->GetLocalChanges(
      /*max_entries=*/10,
      base::BindOnce(&CaptureCommitRequest, &commit_request));
  EXPECT_EQ(0U, commit_request.size());
}

TEST_F(NigoriDataTypeProcessorTest, ShouldGetLocalChangesWhenPut) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);

  {
    base::MockOnceCallback<void(bool)> has_unsynced_data_cb;
    EXPECT_CALL(has_unsynced_data_cb, Run(false));
    processor()->HasUnsyncedData(has_unsynced_data_cb.Get());
  }

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_nigori();
  entity_data->name = kNigoriNonUniqueName;

  processor()->Put(std::move(entity_data));

  {
    base::MockOnceCallback<void(bool)> has_unsynced_data_cb;
    EXPECT_CALL(has_unsynced_data_cb, Run(true));
    processor()->HasUnsyncedData(has_unsynced_data_cb.Get());
  }

  CommitRequestDataList commit_request;
  processor()->GetLocalChanges(
      /*max_entries=*/10,
      base::BindOnce(&CaptureCommitRequest, &commit_request));
  ASSERT_EQ(1U, commit_request.size());
  EXPECT_EQ(kNigoriNonUniqueName, commit_request[0]->entity->name);
}

TEST_F(NigoriDataTypeProcessorTest,
       ShouldSquashCommitRequestUponCommitCompleted) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);

  {
    base::MockOnceCallback<void(bool)> has_unsynced_data_cb;
    EXPECT_CALL(has_unsynced_data_cb, Run(false));
    processor()->HasUnsyncedData(has_unsynced_data_cb.Get());
  }

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_nigori();
  entity_data->name = kNigoriNonUniqueName;

  processor()->Put(std::move(entity_data));

  {
    base::MockOnceCallback<void(bool)> has_unsynced_data_cb;
    EXPECT_CALL(has_unsynced_data_cb, Run(true));
    processor()->HasUnsyncedData(has_unsynced_data_cb.Get());
  }

  CommitRequestDataList commit_request_list;
  processor()->GetLocalChanges(
      /*max_entries=*/10,
      base::BindOnce(&CaptureCommitRequest, &commit_request_list));
  ASSERT_EQ(1U, commit_request_list.size());

  CommitResponseDataList commit_response_list;
  commit_response_list.push_back(CreateNigoriCommitResponseData(
      *commit_request_list[0], /*response_version=*/processor()
                                       ->GetMetadata()
                                       .entity_metadata->server_version() +
                                   1));

  // ApplyIncrementalSyncChanges() should be called to trigger persistence of
  // the metadata.
  EXPECT_CALL(*mock_nigori_sync_bridge(),
              ApplyIncrementalSyncChanges(Eq(std::nullopt)));
  processor()->OnCommitCompleted(
      CreateDataTypeState(), std::move(commit_response_list),
      /*error_response_list=*/FailedCommitResponseDataList());

  // There should be no more local changes.
  {
    base::MockOnceCallback<void(bool)> has_unsynced_data_cb;
    EXPECT_CALL(has_unsynced_data_cb, Run(false));
    processor()->HasUnsyncedData(has_unsynced_data_cb.Get());
  }
  commit_response_list.clear();
  processor()->GetLocalChanges(
      /*max_entries=*/10,
      base::BindOnce(&CaptureCommitRequest, &commit_request_list));
  EXPECT_TRUE(commit_request_list.empty());
}

TEST_F(NigoriDataTypeProcessorTest,
       ShouldNotSquashCommitRequestUponEmptyCommitResponse) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_nigori();
  entity_data->name = kNigoriNonUniqueName;

  processor()->Put(std::move(entity_data));
  CommitRequestDataList commit_request_list;
  processor()->GetLocalChanges(
      /*max_entries=*/10,
      base::BindOnce(&CaptureCommitRequest, &commit_request_list));
  ASSERT_EQ(1U, commit_request_list.size());

  // ApplyIncrementalSyncChanges() should be called to trigger persistence of
  // the metadata.
  EXPECT_CALL(*mock_nigori_sync_bridge(),
              ApplyIncrementalSyncChanges(Eq(std::nullopt)));
  processor()->OnCommitCompleted(
      CreateDataTypeState(),
      /*committed_response_list=*/CommitResponseDataList(),
      /*error_response_list=*/FailedCommitResponseDataList());

  // Data has been moved into the previous request, so the processor will ask
  // for the commit data once more.
  ON_CALL(*mock_nigori_sync_bridge(), GetDataForCommit()).WillByDefault([&]() {
    auto entity_data = std::make_unique<syncer::EntityData>();
    entity_data->specifics.mutable_nigori();
    entity_data->name = kNigoriNonUniqueName;
    return entity_data;
  });

  // The commit should still be pending.
  CommitResponseDataList commit_response_list;
  processor()->GetLocalChanges(
      /*max_entries=*/10,
      base::BindOnce(&CaptureCommitRequest, &commit_request_list));
  EXPECT_EQ(1U, commit_request_list.size());
}

TEST_F(NigoriDataTypeProcessorTest,
       ShouldKeepAnotherCommitRequestUponCommitCompleted) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);

  auto entity_data = std::make_unique<syncer::EntityData>();
  sync_pb::NigoriSpecifics* nigori_specifics =
      entity_data->specifics.mutable_nigori();
  nigori_specifics->set_passphrase_type(
      sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
  entity_data->name = kNigoriNonUniqueName;

  processor()->Put(std::move(entity_data));
  CommitRequestDataList commit_request_list;
  processor()->GetLocalChanges(
      /*max_entries=*/10,
      base::BindOnce(&CaptureCommitRequest, &commit_request_list));
  ASSERT_EQ(1U, commit_request_list.size());

  CommitResponseDataList commit_response_list;
  commit_response_list.push_back(CreateNigoriCommitResponseData(
      *commit_request_list[0], /*response_version=*/processor()
                                       ->GetMetadata()
                                       .entity_metadata->server_version() +
                                   1));

  // Make another local change before the commit response is received.
  entity_data = std::make_unique<syncer::EntityData>();
  nigori_specifics = entity_data->specifics.mutable_nigori();
  entity_data->name = kNigoriNonUniqueName;
  nigori_specifics->set_passphrase_type(
      sync_pb::NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE);
  processor()->Put(std::move(entity_data));

  // ApplyIncrementalSyncChanges() should be called to trigger persistence of
  // the metadata.
  EXPECT_CALL(*mock_nigori_sync_bridge(),
              ApplyIncrementalSyncChanges(Eq(std::nullopt)));
  // Receive the commit response of the first request.
  processor()->OnCommitCompleted(
      CreateDataTypeState(), std::move(commit_response_list),
      /*error_response_list=*/FailedCommitResponseDataList());

  // There should still be a local change.
  commit_response_list.clear();
  processor()->GetLocalChanges(
      /*max_entries=*/10,
      base::BindOnce(&CaptureCommitRequest, &commit_request_list));
  EXPECT_EQ(1U, commit_request_list.size());
}

TEST_F(NigoriDataTypeProcessorTest,
       ShouldNudgeForCommitUponConnectSyncIfReadyToSyncAndLocalChanges) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_nigori();
  entity_data->name = kNigoriNonUniqueName;

  processor()->Put(std::move(entity_data));

  EXPECT_CALL(*mock_commit_queue(), NudgeForCommit());
  SimulateConnectSync();
}

TEST_F(NigoriDataTypeProcessorTest, ShouldNudgeForCommitUponPutIfReadyToSync) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);
  SimulateConnectSync();

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_nigori();
  entity_data->name = kNigoriNonUniqueName;

  EXPECT_CALL(*mock_commit_queue(), NudgeForCommit());
  processor()->Put(std::move(entity_data));
}

TEST_F(NigoriDataTypeProcessorTest, ShouldInvokeSyncStartCallback) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);

  syncer::DataTypeActivationRequest request;
  request.error_handler = base::DoNothing();
  request.cache_guid = kCacheGuid;

  base::MockCallback<DataTypeControllerDelegate::StartCallback> start_callback;
  std::unique_ptr<DataTypeActivationResponse> captured_response;
  EXPECT_CALL(start_callback, Run)
      .WillOnce(testing::Invoke(
          [&captured_response](
              std::unique_ptr<DataTypeActivationResponse> response) {
            captured_response = std::move(response);
          }));
  processor()->OnSyncStarting(request, start_callback.Get());
  ASSERT_THAT(captured_response, NotNull());
  EXPECT_EQ(kCacheGuid, captured_response->data_type_state.cache_guid());

  // Test that the |processor()| has been set in the activation response.
  ASSERT_FALSE(processor()->IsConnectedForTest());
  captured_response->type_processor->ConnectSync(
      std::make_unique<testing::NiceMock<MockCommitQueue>>());
  EXPECT_TRUE(processor()->IsConnectedForTest());
}

TEST_F(NigoriDataTypeProcessorTest, ShouldMergeFullSyncData) {
  base::HistogramTester histogram_tester;

  SimulateModelReadyToSync(/*initial_sync_done=*/false);

  const std::string kDecryptorTokenKeyName = "key_name";
  UpdateResponseDataList updates;
  updates.push_back(CreateFakeNigoriUpdateResponseData(kDecryptorTokenKeyName,
                                                       /*server_version=*/1));

  EXPECT_CALL(*mock_nigori_sync_bridge(),
              MergeFullSyncData(OptionalEntityDataHasDecryptorTokenKeyName(
                  kDecryptorTokenKeyName)));

  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);

  histogram_tester.ExpectTotalCount(
      "Sync.NonReflectionUpdateFreshnessPossiblySkewed2", 0);
}

TEST_F(NigoriDataTypeProcessorTest, ShouldApplyIncrementalSyncChanges) {
  base::HistogramTester histogram_tester;

  SimulateModelReadyToSync(/*initial_sync_done=*/true, /*server_version=*/1);

  const std::string kDecryptorTokenKeyName = "key_name";
  UpdateResponseDataList updates;
  updates.push_back(CreateFakeNigoriUpdateResponseData(kDecryptorTokenKeyName,
                                                       /*server_version=*/2));
  updates.back().entity.modification_time = base::Time::Now() - base::Hours(1);

  EXPECT_CALL(
      *mock_nigori_sync_bridge(),
      ApplyIncrementalSyncChanges(
          OptionalEntityDataHasDecryptorTokenKeyName(kDecryptorTokenKeyName)));

  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);

  histogram_tester.ExpectUniqueTimeSample(
      "Sync.NonReflectionUpdateFreshnessPossiblySkewed2", base::Hours(1), 1);
}

TEST_F(NigoriDataTypeProcessorTest,
       ShouldApplyIncrementalSyncChangesWhenEmptyUpdates) {
  const int kServerVersion = 1;
  SimulateModelReadyToSync(/*initial_sync_done=*/true, kServerVersion);

  // ApplyIncrementalSyncChanges() should still be called to trigger persistence
  // of the metadata.
  EXPECT_CALL(*mock_nigori_sync_bridge(),
              ApplyIncrementalSyncChanges(Eq(std::nullopt)));

  processor()->OnUpdateReceived(CreateDataTypeState(), UpdateResponseDataList(),
                                /*gc_directive=*/std::nullopt);
}

TEST_F(NigoriDataTypeProcessorTest,
       ShouldApplyIncrementalSyncChangesWhenReflection) {
  base::HistogramTester histogram_tester;

  const int kServerVersion = 1;
  SimulateModelReadyToSync(/*initial_sync_done=*/true, kServerVersion);

  UpdateResponseDataList updates;
  updates.push_back(CreateFakeNigoriUpdateResponseData(
      /*keystore_decryptor_token_key_name=*/"key_name", kServerVersion));

  // ApplyIncrementalSyncChanges() should still be called to trigger persistence
  // of the metadata.
  EXPECT_CALL(*mock_nigori_sync_bridge(),
              ApplyIncrementalSyncChanges(Eq(std::nullopt)));

  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);

  histogram_tester.ExpectTotalCount(
      "Sync.NonReflectionUpdateFreshnessPossiblySkewed2", 0);
}

TEST_F(NigoriDataTypeProcessorTest, ShouldStopSyncingAndKeepMetadata) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);
  syncer::DataTypeActivationRequest request;
  request.error_handler = base::DoNothing();
  request.cache_guid = kCacheGuid;
  processor()->OnSyncStarting(request, base::DoNothing());
  SimulateConnectSync();

  ASSERT_TRUE(processor()->IsConnectedForTest());
  EXPECT_CALL(*mock_nigori_sync_bridge(), ApplyDisableSyncChanges()).Times(0);
  SimulateSyncStopping(syncer::KEEP_METADATA);
  EXPECT_FALSE(processor()->IsConnectedForTest());
}

TEST_F(NigoriDataTypeProcessorTest, ShouldStopSyncingAndClearMetadata) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);
  syncer::DataTypeActivationRequest request;
  request.error_handler = base::DoNothing();
  request.cache_guid = kCacheGuid;
  processor()->OnSyncStarting(request, base::DoNothing());
  SimulateConnectSync();

  ASSERT_TRUE(processor()->IsConnectedForTest());
  EXPECT_CALL(*mock_nigori_sync_bridge(), ApplyDisableSyncChanges());
  SimulateSyncStopping(syncer::CLEAR_METADATA);
  EXPECT_FALSE(processor()->IsConnectedForTest());
}

TEST_F(NigoriDataTypeProcessorTest, ShouldResetDataOnCacheGuidMismatch) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);
  ASSERT_TRUE(ProcessorHasEntity());

  syncer::DataTypeActivationRequest request;
  request.error_handler = base::DoNothing();
  const char kOtherCacheGuid[] = "OtherCacheGuid";
  request.cache_guid = kOtherCacheGuid;
  ASSERT_NE(processor()->GetMetadata().data_type_state.cache_guid(),
            kOtherCacheGuid);
  ASSERT_TRUE(processor()->IsTrackingMetadata());

  EXPECT_CALL(*mock_nigori_sync_bridge(), ApplyDisableSyncChanges());
  processor()->OnSyncStarting(request, base::DoNothing());

  EXPECT_FALSE(processor()->IsTrackingMetadata());
  EXPECT_EQ(processor()->GetDataTypeStateForTest().cache_guid(),
            kOtherCacheGuid);

  EXPECT_FALSE(ProcessorHasEntity());

  // Check that sync can be started.
  const std::string kDecryptorTokenKeyName = "key_name";
  UpdateResponseDataList updates;
  updates.push_back(CreateFakeNigoriUpdateResponseData(kDecryptorTokenKeyName,
                                                       /*server_version=*/1));

  EXPECT_CALL(*mock_nigori_sync_bridge(),
              MergeFullSyncData(OptionalEntityDataHasDecryptorTokenKeyName(
                  kDecryptorTokenKeyName)));

  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);
}

TEST_F(NigoriDataTypeProcessorTest,
       ShouldDisconnectWhenMergeFullSyncDataFails) {
  SimulateModelReadyToSync(/*initial_sync_done=*/false);

  syncer::DataTypeActivationRequest request;
  base::MockCallback<ModelErrorHandler> error_handler_callback;
  request.error_handler = error_handler_callback.Get();
  request.cache_guid = kCacheGuid;
  processor()->OnSyncStarting(request, base::DoNothing());
  SimulateConnectSync();

  // Simulate returning error at MergeFullSyncData()
  ON_CALL(*mock_nigori_sync_bridge(), MergeFullSyncData)
      .WillByDefault([&](const std::optional<EntityData>& data) {
        return ModelError(FROM_HERE, "some error");
      });

  UpdateResponseDataList updates;
  updates.push_back(CreateFakeNigoriUpdateResponseData(
      /*keystore_decryptor_token_key_name=*/"some key",
      /*server_version=*/1));

  ASSERT_TRUE(processor()->IsConnectedForTest());
  EXPECT_CALL(error_handler_callback, Run);
  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);
  EXPECT_FALSE(processor()->IsConnectedForTest());
}

TEST_F(NigoriDataTypeProcessorTest,
       ShouldDisconnectWhenApplyIncrementalSyncChangesFails) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true, /*server_version=*/1);

  syncer::DataTypeActivationRequest request;
  base::MockCallback<ModelErrorHandler> error_handler_callback;
  request.error_handler = error_handler_callback.Get();
  request.cache_guid = kCacheGuid;
  processor()->OnSyncStarting(request, base::DoNothing());
  SimulateConnectSync();

  // Simulate returning error at ApplyIncrementalSyncChanges()
  ON_CALL(*mock_nigori_sync_bridge(), ApplyIncrementalSyncChanges)
      .WillByDefault([&](const std::optional<EntityData>& data) {
        return ModelError(FROM_HERE, "some error");
      });

  UpdateResponseDataList updates;
  updates.push_back(CreateFakeNigoriUpdateResponseData(
      /*keystore_decryptor_token_key_name=*/"some key",
      /*server_version=*/2));

  ASSERT_TRUE(processor()->IsConnectedForTest());
  EXPECT_CALL(error_handler_callback, Run);
  processor()->OnUpdateReceived(CreateDataTypeState(), std::move(updates),
                                /*gc_directive=*/std::nullopt);
  EXPECT_FALSE(processor()->IsConnectedForTest());
}

TEST_F(NigoriDataTypeProcessorTest,
       ShouldCallErrorHandlerIfModelErrorBeforeSyncStarts) {
  processor()->ReportError(ModelError(FROM_HERE, "some error"));

  syncer::DataTypeActivationRequest request;
  base::MockCallback<ModelErrorHandler> error_handler_callback;
  request.error_handler = error_handler_callback.Get();
  request.cache_guid = kCacheGuid;

  EXPECT_CALL(error_handler_callback, Run);
  processor()->OnSyncStarting(request, base::DoNothing());
}

TEST_F(NigoriDataTypeProcessorTest,
       ShouldUpdateDataTypeStateUponHandlingInvalidations) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);
  // Build invalidations.
  sync_pb::DataTypeState::Invalidation inv_1 = BuildInvalidation(1, "hint_1");
  sync_pb::DataTypeState::Invalidation inv_2 = BuildInvalidation(2, "hint_2");

  processor()->StorePendingInvalidations({inv_1, inv_2});

  // The data type state and the metadata should have been stored in the
  // processor.
  NigoriMetadataBatch processor_metadata_batch = processor()->GetMetadata();
  sync_pb::DataTypeState data_type_state =
      processor_metadata_batch.data_type_state;
  EXPECT_EQ(2, data_type_state.invalidations_size());

  EXPECT_EQ(inv_1.hint(), data_type_state.invalidations(0).hint());
  EXPECT_EQ(inv_1.version(), data_type_state.invalidations(0).version());

  EXPECT_EQ(inv_2.hint(), data_type_state.invalidations(1).hint());
  EXPECT_EQ(inv_2.version(), data_type_state.invalidations(1).version());
}

}  // namespace

}  // namespace syncer
