// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/client_tag_based_remote_update_handler.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/features.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/forwarding_data_type_processor.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/processor_entity.h"
#include "components/sync/model/processor_entity_tracker.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync/test/fake_data_type_sync_bridge.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/mock_data_type_processor.h"
#include "components/sync/test/mock_data_type_worker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using base::test::EqualsProto;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::IsNull;
using testing::Not;
using testing::NotNull;

const char kKey1[] = "key1";
const char kKey2[] = "key2";
const char kValue1[] = "value1";
const char kValue2[] = "value2";

sync_pb::DataTypeState GenerateDataTypeState() {
  sync_pb::DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  return data_type_state;
}

std::unique_ptr<DataTypeActivationResponse> GenerateDataTypeActivationResponse(
    DataTypeProcessor* processor) {
  auto response = std::make_unique<DataTypeActivationResponse>();
  response->data_type_state = GenerateDataTypeState();
  response->type_processor =
      std::make_unique<ForwardingDataTypeProcessor>(processor);
  return response;
}

ClientTagHash GetPrefHash(const std::string& key) {
  return ClientTagHash::FromUnhashed(
      PREFERENCES, FakeDataTypeSyncBridge::ClientTagFromKey(key));
}

ClientTagHash GetSharedTabGroupDataHash(const std::string& key) {
  return ClientTagHash::FromUnhashed(
      SHARED_TAB_GROUP_DATA, FakeDataTypeSyncBridge::ClientTagFromKey(key));
}

sync_pb::EntitySpecifics GeneratePrefSpecifics(const std::string& key,
                                               const std::string& value) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(key);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

sync_pb::EntitySpecifics GenerateSharedTabGroupSpecifics(
    const std::string& guid) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_shared_tab_group_data()->set_guid(guid);
  return specifics;
}

sync_pb::EntitySpecifics GenerateSharedTabGroupTabSpecifics(
    const std::string& guid,
    sync_pb::UniquePosition unique_position) {
  sync_pb::EntitySpecifics specifics = GenerateSharedTabGroupSpecifics(guid);
  *specifics.mutable_shared_tab_group_data()
       ->mutable_tab()
       ->mutable_unique_position() = std::move(unique_position);
  return specifics;
}

sync_pb::UniquePosition ExtractUniquePositionFromSharedTab(
    const sync_pb::EntitySpecifics& specifics) {
  return specifics.shared_tab_group_data().tab().unique_position();
}

class ClientTagBasedRemoteUpdateHandlerTest : public ::testing::Test {
 public:
  ClientTagBasedRemoteUpdateHandlerTest()
      : ClientTagBasedRemoteUpdateHandlerTest(PREFERENCES) {}

  explicit ClientTagBasedRemoteUpdateHandlerTest(DataType type)
      : processor_entity_tracker_(GenerateDataTypeState(), EntityMetadataMap()),
        data_type_sync_bridge_(type,
                               change_processor_.CreateForwardingProcessor()),
        remote_update_handler_(type,
                               &data_type_sync_bridge_,
                               &processor_entity_tracker_),
        worker_(MockDataTypeWorker::CreateWorkerAndConnectSync(
            GenerateDataTypeActivationResponse(&data_type_processor_))) {}

  ~ClientTagBasedRemoteUpdateHandlerTest() override = default;

  void ProcessSingleUpdate(const sync_pb::DataTypeState& data_type_state,
                           UpdateResponseData update,
                           std::optional<sync_pb::GarbageCollectionDirective>
                               gc_directive = std::nullopt) {
    UpdateResponseDataList updates;
    updates.push_back(std::move(update));
    remote_update_handler_.ProcessIncrementalUpdate(
        data_type_state, std::move(updates), gc_directive);
  }

  void ProcessSingleUpdate(UpdateResponseData update) {
    ProcessSingleUpdate(GenerateDataTypeState(), std::move(update));
  }

  UpdateResponseData GeneratePrefUpdate(const std::string& key,
                                        const std::string& value) {
    const ClientTagHash client_tag_hash = GetPrefHash(key);
    return GeneratePrefUpdate(client_tag_hash, key, value);
  }

  UpdateResponseData GeneratePrefUpdate(const ClientTagHash& client_tag_hash,
                                        const std::string& key,
                                        const std::string& value) {
    return worker()->GenerateUpdateData(client_tag_hash,
                                        GeneratePrefSpecifics(key, value));
  }

  UpdateResponseData GeneratePrefUpdate(const std::string& key,
                                        const std::string& value,
                                        int64_t version_offset) {
    const ClientTagHash client_tag_hash = GetPrefHash(key);
    const sync_pb::DataTypeState data_type_state = GenerateDataTypeState();
    const sync_pb::EntitySpecifics specifics =
        GeneratePrefSpecifics(key, value);
    return worker()->GenerateUpdateData(client_tag_hash, specifics,
                                        version_offset,
                                        data_type_state.encryption_key_name());
  }

  std::unique_ptr<EntityData> GeneratePrefEntityData(const std::string& key,
                                                     const std::string& value) {
    auto entity_data = std::make_unique<EntityData>();
    entity_data->specifics = GeneratePrefSpecifics(key, value);
    return entity_data;
  }

  size_t ProcessorEntityCount() const {
    return processor_entity_tracker_.GetAllEntitiesIncludingTombstones().size();
  }

  FakeDataTypeSyncBridge* bridge() { return &data_type_sync_bridge_; }
  ClientTagBasedRemoteUpdateHandler* remote_update_handler() {
    return &remote_update_handler_;
  }
  FakeDataTypeSyncBridge::Store* db() { return bridge()->mutable_db(); }
  ProcessorEntityTracker* entity_tracker() {
    return &processor_entity_tracker_;
  }
  testing::NiceMock<MockDataTypeLocalChangeProcessor>* change_processor() {
    return &change_processor_;
  }
  MockDataTypeWorker* worker() { return worker_.get(); }

 private:
  testing::NiceMock<MockDataTypeLocalChangeProcessor> change_processor_;
  ProcessorEntityTracker processor_entity_tracker_;
  FakeDataTypeSyncBridge data_type_sync_bridge_;
  ClientTagBasedRemoteUpdateHandler remote_update_handler_;
  testing::NiceMock<MockDataTypeProcessor> data_type_processor_;
  std::unique_ptr<MockDataTypeWorker> worker_;
};

// Thoroughly tests the data generated by a server item creation.
TEST_F(ClientTagBasedRemoteUpdateHandlerTest, ShouldProcessRemoteCreation) {
  ProcessSingleUpdate(GeneratePrefUpdate(kKey1, kValue1));
  EXPECT_EQ(1u, db()->data_count());
  EXPECT_EQ(1u, db()->metadata_count());
  EXPECT_EQ(0u, bridge()->trimmed_specifics_change_count());

  const EntityData& data = db()->GetData(kKey1);
  EXPECT_FALSE(data.id.empty());
  EXPECT_EQ(kKey1, data.specifics.preference().name());
  EXPECT_EQ(kValue1, data.specifics.preference().value());
  EXPECT_FALSE(data.creation_time.is_null());
  EXPECT_FALSE(data.modification_time.is_null());
  EXPECT_EQ(kKey1, data.name);
  EXPECT_FALSE(data.is_deleted());

  const sync_pb::EntityMetadata& metadata = db()->GetMetadata(kKey1);
  EXPECT_TRUE(metadata.has_client_tag_hash());
  EXPECT_TRUE(metadata.has_server_id());
  EXPECT_FALSE(metadata.is_deleted());
  EXPECT_EQ(0, metadata.sequence_number());
  EXPECT_EQ(0, metadata.acked_sequence_number());
  EXPECT_EQ(1, metadata.server_version());
  EXPECT_TRUE(metadata.has_creation_time());
  EXPECT_TRUE(metadata.has_modification_time());
  EXPECT_TRUE(metadata.has_specifics_hash());
  EXPECT_TRUE(metadata.has_possibly_trimmed_base_specifics());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldIgnoreRemoteUpdatesForRootNodes) {
  ASSERT_EQ(0U, ProcessorEntityCount());
  ProcessSingleUpdate(worker()->GenerateTypeRootUpdateData(DataType::SESSIONS));
  // Root node update should be filtered out.
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldIgnoreRemoteUpdatesWithUnexpectedClientTagHash) {
  ASSERT_EQ(0U, ProcessorEntityCount());
  ProcessSingleUpdate(GeneratePrefUpdate(GetPrefHash(kKey2), kKey1, kValue1));
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldNotClearTrimmedSpecificsOnNoopRemoteUpdates) {
  const std::string kUnknownField = "unknown_field";

  // Initial update containing unsupported fields.
  UpdateResponseData update1 = GeneratePrefUpdate(kKey1, kValue1);
  *update1.entity.specifics.mutable_unknown_fields() = kUnknownField;
  ProcessSingleUpdate(std::move(update1));
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());
  ASSERT_EQ(1U, bridge()->trimmed_specifics_change_count());

  // Redundant update should not clear trimmed specifics.
  UpdateResponseData update2 = GeneratePrefUpdate(kKey1, kValue1);
  *update2.entity.specifics.mutable_unknown_fields() = kUnknownField;
  ProcessSingleUpdate(std::move(update2));
  EXPECT_EQ(1U, db()->data_change_count());
  EXPECT_EQ(2U, db()->metadata_change_count());
  ASSERT_EQ(2U, bridge()->trimmed_specifics_change_count());
  EXPECT_EQ(kUnknownField, db()->GetMetadata(kKey1)
                               .possibly_trimmed_base_specifics()
                               .unknown_fields());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldUpdateMetadataOnNoopRemoteUpdates) {
  ProcessSingleUpdate(GeneratePrefUpdate(kKey1, kValue1));
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());

  // Redundant update from server doesn't write data but updates metadata.
  const int64_t time_before_update =
      db()->GetMetadata(kKey1).modification_time();
  ProcessSingleUpdate(GeneratePrefUpdate(kKey1, kValue1));
  EXPECT_EQ(1U, db()->data_change_count());
  EXPECT_EQ(2U, db()->metadata_change_count());
  // Check that `modification_time` was updated.
  EXPECT_NE(time_before_update, db()->GetMetadata(kKey1).modification_time());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldIgnoreReflectionsOnRemoteUpdates) {
  ProcessSingleUpdate(GeneratePrefUpdate(kKey1, kValue1));
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());

  // A reflection (update already received) is ignored completely.
  ProcessSingleUpdate(GeneratePrefUpdate(kKey1, kValue1, /*version_offset=*/0));
  EXPECT_EQ(1U, db()->data_change_count());
  EXPECT_EQ(1U, db()->metadata_change_count());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest, ShouldProcessRemoteUpdates) {
  ProcessSingleUpdate(GeneratePrefUpdate(kKey1, kValue1));
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());
  ASSERT_EQ(0U, bridge()->trimmed_specifics_change_count());

  // Should update both data and metadata.
  ProcessSingleUpdate(GeneratePrefUpdate(kKey1, kValue2));
  ASSERT_EQ(2U, db()->data_change_count());
  ASSERT_EQ(2U, db()->metadata_change_count());
  EXPECT_EQ(1U, db()->data_count());
  EXPECT_EQ(1U, db()->metadata_count());

  EXPECT_EQ(kValue2, db()->GetData(kKey1).specifics.preference().value());
  const sync_pb::EntityMetadata& metadata = db()->GetMetadata(kKey1);
  EXPECT_EQ(0, metadata.sequence_number());
  EXPECT_EQ(0, metadata.acked_sequence_number());
  EXPECT_EQ(2, metadata.server_version());
  EXPECT_TRUE(metadata.has_possibly_trimmed_base_specifics());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest, ShouldProcessRemoteDeletion) {
  ProcessSingleUpdate(GeneratePrefUpdate(kKey1, kValue1));
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());

  ProcessSingleUpdate(
      worker()->GenerateTombstoneUpdateData(GetPrefHash(kKey1)));
  // Delete from server should clear the data and all the metadata.
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
}

// Deletes an item we've never seen before. Should have no effect and not crash.
TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldIgnoreRemoteDeletionOfUnknownEntity) {
  ASSERT_EQ(0U, ProcessorEntityCount());
  ProcessSingleUpdate(
      worker()->GenerateTombstoneUpdateData(GetPrefHash(kKey1)));
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldNotTreatMatchingChangesAsConflict) {
  UpdateResponseData update = GeneratePrefUpdate(kKey1, kValue1);
  sync_pb::EntitySpecifics specifics = update.entity.specifics;
  ProcessSingleUpdate(std::move(update));
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());
  ASSERT_EQ(1U, db()->GetMetadata(kKey1).server_version());
  ASSERT_EQ(0U, bridge()->trimmed_specifics_change_count());

  // Mark local entity as changed.
  entity_tracker()->IncrementSequenceNumberForAllExcept({});
  ASSERT_TRUE(entity_tracker()->HasLocalChanges());

  update = GeneratePrefUpdate(kKey1, kValue1);
  // Make sure to have the same specifics.
  update.entity.specifics = specifics;

  base::HistogramTester histogram_tester;
  ProcessSingleUpdate(std::move(update));
  histogram_tester.ExpectUniqueSample(
      "Sync.DataTypeEntityConflictResolution.PREFERENCE",
      ConflictResolution::kChangesMatch, /*expected_bucket_count=*/1);

  EXPECT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(0U, bridge()->trimmed_specifics_change_count());
  EXPECT_EQ(2U, db()->GetMetadata(kKey1).server_version());
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_FALSE(entity_tracker()->HasLocalChanges());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldPreferRemoteNonDeletionOverLocalTombstoneOnConflict) {
  UpdateResponseData update = GeneratePrefUpdate(kKey1, kValue1);
  sync_pb::EntitySpecifics specifics = update.entity.specifics;
  ProcessSingleUpdate(std::move(update));
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());
  ASSERT_EQ(1U, db()->GetMetadata(kKey1).server_version());

  // Mark local entity as deleted (tombstone).
  db()->RemoveData(kKey1);
  entity_tracker()->GetEntityForStorageKey(kKey1)->RecordLocalDeletion(
      DeletionOrigin::Unspecified());
  entity_tracker()->IncrementSequenceNumberForAllExcept({});
  ASSERT_EQ(2U, db()->data_change_count());
  ASSERT_TRUE(entity_tracker()->HasLocalChanges());

  update = GeneratePrefUpdate(kKey1, kValue1);
  // Make sure to have the same specifics.
  update.entity.specifics = specifics;

  base::HistogramTester histogram_tester;
  ProcessSingleUpdate(std::move(update));
  histogram_tester.ExpectUniqueSample(
      "Sync.DataTypeEntityConflictResolution.PREFERENCE",
      ConflictResolution::kUseRemote, /*expected_bucket_count=*/1);

  EXPECT_EQ(3U, db()->data_change_count());
  EXPECT_EQ(2U, db()->GetMetadata(kKey1).server_version());
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_FALSE(entity_tracker()->HasLocalChanges());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldPreferRemoteNonDeletionOverLocalEncryptionOnConflict) {
  UpdateResponseData update = GeneratePrefUpdate(kKey1, kValue1);
  sync_pb::EntitySpecifics specifics = update.entity.specifics;
  ProcessSingleUpdate(std::move(update));
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());
  ASSERT_EQ(1U, db()->GetMetadata(kKey1).server_version());

  // Mark local entity as updated but having the same specifics (local
  // re-encryption).
  entity_tracker()->IncrementSequenceNumberForAllExcept({});
  ASSERT_TRUE(entity_tracker()->HasLocalChanges());
  ASSERT_TRUE(
      entity_tracker()->GetEntityForStorageKey(kKey1)->MatchesOwnBaseData());

  // Remote update has different specifics so data does not match.
  update = GeneratePrefUpdate(kKey1, kValue2);

  base::HistogramTester histogram_tester;
  ProcessSingleUpdate(std::move(update));
  histogram_tester.ExpectUniqueSample(
      "Sync.DataTypeEntityConflictResolution.PREFERENCE",
      ConflictResolution::kIgnoreLocalEncryption, /*expected_bucket_count=*/1);

  EXPECT_EQ(2U, db()->data_change_count());
  ASSERT_EQ(0U, bridge()->trimmed_specifics_change_count());
  EXPECT_EQ(2U, db()->GetMetadata(kKey1).server_version());
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_FALSE(entity_tracker()->HasLocalChanges());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldPreferLocalChangeOverRemoteEncryptionOnConflict) {
  UpdateResponseData update = GeneratePrefUpdate(kKey1, kValue1);
  sync_pb::EntitySpecifics specifics = update.entity.specifics;
  ProcessSingleUpdate(std::move(update));
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());
  ASSERT_EQ(1U, db()->GetMetadata(kKey1).server_version());

  // Update the local entity to not match the remote update.
  entity_tracker()->GetEntityForStorageKey(kKey1)->RecordLocalUpdate(
      GeneratePrefEntityData(kKey1, kValue2),
      /*trimmed_specifics=*/sync_pb::EntitySpecifics(),
      /*unique_position=*/std::nullopt);
  ASSERT_TRUE(entity_tracker()->HasLocalChanges());

  // Remote update has the same specifics to represent re-encryption.
  update = GeneratePrefUpdate(kKey1, kValue1);
  update.entity.specifics = specifics;

  base::HistogramTester histogram_tester;
  ProcessSingleUpdate(std::move(update));
  histogram_tester.ExpectUniqueSample(
      "Sync.DataTypeEntityConflictResolution.PREFERENCE",
      ConflictResolution::kIgnoreRemoteEncryption, /*expected_bucket_count=*/1);

  EXPECT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(0U, bridge()->trimmed_specifics_change_count());
  EXPECT_EQ(2U, db()->GetMetadata(kKey1).server_version());
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_TRUE(entity_tracker()->HasLocalChanges());
}

// Test for the case from crbug.com/1046309. Tests that there is no redundant
// deletion when processing remote deletion with different encryption key.
TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldNotIssueDeletionUponRemoteDeletion) {
  const std::string kTestEncryptionKeyName = "TestEncryptionKey";
  const std::string kDifferentEncryptionKeyName = "DifferentEncryptionKey";
  const ClientTagHash kClientTagHash = GetPrefHash(kKey1);

  sync_pb::DataTypeState data_type_state = GenerateDataTypeState();
  data_type_state.set_encryption_key_name(kTestEncryptionKeyName);

  ProcessSingleUpdate(GeneratePrefUpdate(kClientTagHash, kKey1, kValue1));

  // Generate a remote deletion with a different encryption key.
  data_type_state.set_encryption_key_name(kDifferentEncryptionKeyName);
  ProcessSingleUpdate(data_type_state,
                      worker()->GenerateTombstoneUpdateData(kClientTagHash));

  EXPECT_EQ(0u, ProcessorEntityCount());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldNotProcessInvalidRemoteCreationWithInvalidStorageKey) {
  ASSERT_EQ(0U, ProcessorEntityCount());
  UpdateResponseData update = GeneratePrefUpdate("", "");
  ASSERT_TRUE(bridge()->SupportsGetStorageKey());
  // Bridge will generate an empty storage key.
  ProcessSingleUpdate(std::move(update));
  // Update should be filtered out.
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldIgnoreInvalidRemoteCreation) {
  // To ensure the update is not ignored because of empty storage key.
  bridge()->SetSupportsGetStorageKey(false);
  // Force flag next remote update as invalid.
  bridge()->TreatRemoteUpdateAsInvalid(GetPrefHash(kKey1));

  ASSERT_EQ(0U, ProcessorEntityCount());
  ProcessSingleUpdate(GeneratePrefUpdate(GetPrefHash(kKey1), kKey1, kValue1));
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest, ShouldIgnoreInvalidRemoteUpdate) {
  ProcessSingleUpdate(GeneratePrefUpdate(kKey1, kValue1));
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());

  // To ensure the update is not ignored because of empty storage key.
  bridge()->SetSupportsGetStorageKey(false);
  // Force flag next remote update as invalid.
  bridge()->TreatRemoteUpdateAsInvalid(GetPrefHash(kKey1));

  ProcessSingleUpdate(GeneratePrefUpdate(kKey1, kValue2));
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());
  EXPECT_EQ(1U, ProcessorEntityCount());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest, ShouldLogFreshnessToUma) {
  {
    base::HistogramTester histogram_tester;
    ProcessSingleUpdate(GeneratePrefUpdate(kKey1, kValue1));
    histogram_tester.ExpectTotalCount(
        "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.PREFERENCE", 1);
  }

  // Process the same update again, which should be ignored because the version
  // hasn't increased.
  {
    base::HistogramTester histogram_tester;
    ProcessSingleUpdate(
        GeneratePrefUpdate(kKey1, kValue1, /*version_offset=*/0));
    histogram_tester.ExpectTotalCount(
        "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.PREFERENCE", 0);
  }

  // Increase version and process again; should log freshness.
  {
    base::HistogramTester histogram_tester;
    ProcessSingleUpdate(
        GeneratePrefUpdate(kKey1, kValue1, /*version_offset=*/1));
    histogram_tester.ExpectTotalCount(
        "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.PREFERENCE", 1);
  }

  // Process remote deletion; should log freshness.
  {
    base::HistogramTester histogram_tester;
    ProcessSingleUpdate(
        worker()->GenerateTombstoneUpdateData(GetPrefHash(kKey1)));
    histogram_tester.ExpectTotalCount(
        "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.PREFERENCE", 1);
  }

  // Process a deletion for an entity that doesn't exist; should not log.
  {
    base::HistogramTester histogram_tester;
    ProcessSingleUpdate(
        worker()->GenerateTombstoneUpdateData(GetPrefHash(kKey2)));
    histogram_tester.ExpectTotalCount(
        "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.PREFERENCE", 0);
  }
}

class ClientTagBasedRemoteUpdateHandlerForSharedTest
    : public ClientTagBasedRemoteUpdateHandlerTest {
 public:
  ClientTagBasedRemoteUpdateHandlerForSharedTest()
      : ClientTagBasedRemoteUpdateHandlerTest(SHARED_TAB_GROUP_DATA) {}

  void SetUp() override {
    ClientTagBasedRemoteUpdateHandlerTest::SetUp();
    bridge()->EnableUniquePositionSupport(
        base::BindRepeating(&ExtractUniquePositionFromSharedTab));
  }

  UpdateResponseData GenerateSharedTabGroupDataUpdate(
      const std::string& guid,
      const std::string& collaboration_id) {
    const ClientTagHash client_tag_hash = GetSharedTabGroupDataHash(guid);
    return GenerateSharedTabGroupDataUpdate(client_tag_hash, guid,
                                            collaboration_id);
  }

  UpdateResponseData GenerateSharedTabGroupDataUpdate(
      const ClientTagHash& client_tag_hash,
      const std::string& guid,
      const std::string& collaboration_id) {
    return worker()->GenerateSharedUpdateData(
        client_tag_hash, GenerateSharedTabGroupSpecifics(guid),
        collaboration_id);
  }

  UpdateResponseData GenerateSharedTabGroupTabUpdate(
      const std::string& guid,
      const std::string& collaboration_id) {
    ClientTagHash client_tag_hash = GetSharedTabGroupDataHash(guid);
    return worker()->GenerateSharedUpdateData(
        client_tag_hash,
        GenerateSharedTabGroupTabSpecifics(
            guid, UniquePosition::InitialPosition(
                      UniquePosition::GenerateSuffix(client_tag_hash))
                      .ToProto()),
        collaboration_id);
  }

  void ProcessSharedSingleUpdate(
      UpdateResponseData update,
      const std::vector<std::string>& active_collaborations) {
    sync_pb::GarbageCollectionDirective gc_directive;
    for (const std::string& active_collaboration : active_collaborations) {
      gc_directive.mutable_collaboration_gc()->add_active_collaboration_ids(
          active_collaboration);
    }
    ProcessSingleUpdate(GenerateDataTypeState(), std::move(update),
                        std::move(gc_directive));
  }
};

TEST_F(ClientTagBasedRemoteUpdateHandlerForSharedTest,
       ShouldClearEntitiesForInactiveCollaborations) {
  const std::string kGuidInactiveCollaboration = "guid_inactive";

  ProcessSharedSingleUpdate(
      GenerateSharedTabGroupDataUpdate("guid_1", "active_collaboration"),
      {"active_collaboration"});
  ProcessSharedSingleUpdate(
      GenerateSharedTabGroupDataUpdate(kGuidInactiveCollaboration,
                                       "inactive_collaboration"),
      {"active_collaboration", "inactive_collaboration"});
  EXPECT_EQ(2U, ProcessorEntityCount());
  EXPECT_EQ(2U, db()->data_change_count());
  EXPECT_EQ(2U, db()->metadata_change_count());
  EXPECT_EQ("active_collaboration",
            db()->GetMetadata("guid_1").collaboration().collaboration_id());
  EXPECT_EQ("inactive_collaboration",
            db()->GetMetadata(kGuidInactiveCollaboration)
                .collaboration()
                .collaboration_id());
  ASSERT_THAT(bridge()->deleted_collaboration_membership_storage_keys(),
              IsEmpty());

  // Simulate another update to remove entities for the inactive collaboration
  // (only one collaboration remains active).
  ProcessSharedSingleUpdate(
      GenerateSharedTabGroupDataUpdate("guid_1", "active_collaboration"),
      {"active_collaboration"});
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_EQ(3U, db()->data_change_count());

  // 3 remote updates plus 1 change for inactive collaboration.
  EXPECT_EQ(4U, db()->metadata_change_count());
  EXPECT_EQ(1U, db()->metadata_count());

  EXPECT_THAT(bridge()->deleted_collaboration_membership_storage_keys(),
              ElementsAre(kGuidInactiveCollaboration));
}

TEST_F(ClientTagBasedRemoteUpdateHandlerForSharedTest,
       ShouldCreateDeletionForActiveCollaborationMembership) {
  ProcessSharedSingleUpdate(
      GenerateSharedTabGroupDataUpdate("guid", "active_collaboration"),
      {"active_collaboration"});
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());
  ASSERT_EQ("active_collaboration",
            db()->GetMetadata("guid").collaboration().collaboration_id());
  ASSERT_THAT(bridge()->deleted_collaboration_membership_storage_keys(),
              IsEmpty());

  // Normal deletion (tombstone) from the server while the collaboration is
  // still active.
  ProcessSingleUpdate(
      worker()->GenerateTombstoneUpdateData(GetSharedTabGroupDataHash("guid")));
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(2U, db()->metadata_change_count());
  EXPECT_THAT(bridge()->deleted_collaboration_membership_storage_keys(),
              IsEmpty());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerForSharedTest,
       ShouldProcessUniquePositionForRemoteCreation) {
  const std::string collaboration_id = "collaboration";
  ASSERT_THAT(entity_tracker()->GetEntityForStorageKey("guid"), IsNull());

  ProcessSharedSingleUpdate(
      GenerateSharedTabGroupTabUpdate("guid", collaboration_id),
      {collaboration_id});

  const ProcessorEntity* entity =
      entity_tracker()->GetEntityForStorageKey("guid");
  ASSERT_THAT(entity, NotNull());
  EXPECT_TRUE(entity->metadata().has_unique_position());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerForSharedTest,
       ShouldProcessUniquePositionForRemoteUpdate) {
  const std::string collaboration_id = "collaboration";
  ProcessSharedSingleUpdate(
      GenerateSharedTabGroupTabUpdate("guid", collaboration_id),
      {collaboration_id});

  const ProcessorEntity* entity =
      entity_tracker()->GetEntityForStorageKey("guid");
  ASSERT_THAT(entity, NotNull());
  ASSERT_TRUE(entity->metadata().has_unique_position());

  // Generate update with a new unique position.
  UpdateResponseData update =
      GenerateSharedTabGroupTabUpdate("guid", collaboration_id);
  *update.entity.specifics.mutable_shared_tab_group_data()
       ->mutable_tab()
       ->mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();
  ASSERT_THAT(
      update.entity.specifics.shared_tab_group_data().tab().unique_position(),
      Not(EqualsProto(entity->metadata().unique_position())));
  sync_pb::EntitySpecifics specifics_copy = update.entity.specifics;
  ProcessSharedSingleUpdate(std::move(update), {collaboration_id});
  EXPECT_THAT(
      entity->metadata().unique_position(),
      EqualsProto(
          specifics_copy.shared_tab_group_data().tab().unique_position()));

  // Remote update matching data by re-using the same specifics.
  update = GenerateSharedTabGroupTabUpdate("guid", collaboration_id);
  update.entity.specifics = specifics_copy;
  ProcessSharedSingleUpdate(std::move(update), {collaboration_id});
  EXPECT_THAT(
      entity->metadata().unique_position(),
      EqualsProto(
          specifics_copy.shared_tab_group_data().tab().unique_position()));
}

TEST_F(ClientTagBasedRemoteUpdateHandlerForSharedTest,
       ShouldPreferRemoteUniquePositionOverLocalDeletion) {
  const std::string collaboration_id = "collaboration";
  const std::string guid = "guid";

  ProcessSharedSingleUpdate(
      GenerateSharedTabGroupTabUpdate(guid, collaboration_id),
      {collaboration_id});
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_TRUE(db()->HasData(guid));
  ASSERT_EQ(1U, db()->HasMetadata(guid));

  // Mark local entity as deleted (tombstone).
  db()->RemoveData(guid);
  entity_tracker()->GetEntityForStorageKey(guid)->RecordLocalDeletion(
      DeletionOrigin::Unspecified());
  entity_tracker()->IncrementSequenceNumberForAllExcept({});
  ASSERT_TRUE(entity_tracker()->HasLocalChanges());
  const ProcessorEntity* entity =
      entity_tracker()->GetEntityForStorageKey(guid);
  ASSERT_THAT(entity, NotNull());
  ASSERT_FALSE(entity->metadata().has_unique_position());

  ProcessSharedSingleUpdate(
      GenerateSharedTabGroupTabUpdate(guid, collaboration_id),
      {collaboration_id});

  ASSERT_EQ(entity, entity_tracker()->GetEntityForStorageKey(guid));
  ASSERT_FALSE(entity->metadata().is_deleted());
  EXPECT_TRUE(entity->metadata().has_unique_position());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerForSharedTest,
       ShouldPreferRemoteUniquePositionOnConflict) {
  const std::string collaboration_id = "collaboration";
  const std::string guid = "guid";

  ProcessSharedSingleUpdate(
      GenerateSharedTabGroupTabUpdate(guid, collaboration_id),
      {collaboration_id});

  // Mark the local entity as updated for a conflict.
  entity_tracker()->IncrementSequenceNumberForAllExcept({});
  const ProcessorEntity* entity =
      entity_tracker()->GetEntityForStorageKey(guid);
  ASSERT_THAT(entity, NotNull());
  ASSERT_TRUE(entity_tracker()->HasLocalChanges());
  ASSERT_TRUE(entity->metadata().has_unique_position());

  const sync_pb::UniquePosition original_unique_position =
      entity->metadata().unique_position();

  // Remote update with a new unique position.
  UpdateResponseData update =
      GenerateSharedTabGroupTabUpdate(guid, collaboration_id);
  sync_pb::UniquePosition new_unique_position =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();
  *update.entity.specifics.mutable_shared_tab_group_data()
       ->mutable_tab()
       ->mutable_unique_position() = new_unique_position;
  ASSERT_THAT(new_unique_position, Not(EqualsProto(original_unique_position)));
  ProcessSharedSingleUpdate(std::move(update), {collaboration_id});

  ASSERT_EQ(entity, entity_tracker()->GetEntityForStorageKey(guid));
  EXPECT_TRUE(entity->metadata().has_unique_position());
  EXPECT_THAT(entity->metadata().unique_position(),
              EqualsProto(new_unique_position));
}

}  // namespace

}  // namespace syncer
