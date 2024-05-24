// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/client_tag_based_remote_update_handler.h"

#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/forwarding_model_type_processor.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/processor_entity_tracker.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/fake_model_type_sync_bridge.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/mock_model_type_processor.h"
#include "components/sync/test/mock_model_type_worker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::ElementsAre;
using testing::IsEmpty;

const char kKey1[] = "key1";
const char kKey2[] = "key2";
const char kValue1[] = "value1";
const char kValue2[] = "value2";

sync_pb::ModelTypeState GenerateModelTypeState() {
  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  return model_type_state;
}

std::unique_ptr<DataTypeActivationResponse> GenerateDataTypeActivationResponse(
    ModelTypeProcessor* processor) {
  auto response = std::make_unique<DataTypeActivationResponse>();
  response->model_type_state = GenerateModelTypeState();
  response->type_processor =
      std::make_unique<ForwardingModelTypeProcessor>(processor);
  return response;
}

ClientTagHash GetPrefHash(const std::string& key) {
  return ClientTagHash::FromUnhashed(
      PREFERENCES, FakeModelTypeSyncBridge::ClientTagFromKey(key));
}

ClientTagHash GetSharedTabGroupDataHash(const std::string& key) {
  return ClientTagHash::FromUnhashed(
      SHARED_TAB_GROUP_DATA, FakeModelTypeSyncBridge::ClientTagFromKey(key));
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

class ClientTagBasedRemoteUpdateHandlerTest : public ::testing::Test {
 public:
  ClientTagBasedRemoteUpdateHandlerTest()
      : ClientTagBasedRemoteUpdateHandlerTest(PREFERENCES) {}

  explicit ClientTagBasedRemoteUpdateHandlerTest(ModelType type)
      : processor_entity_tracker_(GenerateModelTypeState(),
                                  EntityMetadataMap()),
        model_type_sync_bridge_(type,
                                change_processor_.CreateForwardingProcessor()),
        remote_update_handler_(type,
                               &model_type_sync_bridge_,
                               &processor_entity_tracker_),
        worker_(MockModelTypeWorker::CreateWorkerAndConnectSync(
            GenerateDataTypeActivationResponse(&model_type_processor_))) {}

  ~ClientTagBasedRemoteUpdateHandlerTest() override = default;

  void ProcessSingleUpdate(const sync_pb::ModelTypeState& model_type_state,
                           UpdateResponseData update,
                           std::optional<sync_pb::GarbageCollectionDirective>
                               gc_directive = std::nullopt) {
    UpdateResponseDataList updates;
    updates.push_back(std::move(update));
    remote_update_handler_.ProcessIncrementalUpdate(
        model_type_state, std::move(updates), gc_directive);
  }

  void ProcessSingleUpdate(UpdateResponseData update) {
    ProcessSingleUpdate(GenerateModelTypeState(), std::move(update));
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
    const sync_pb::ModelTypeState model_type_state = GenerateModelTypeState();
    const sync_pb::EntitySpecifics specifics =
        GeneratePrefSpecifics(key, value);
    return worker()->GenerateUpdateData(client_tag_hash, specifics,
                                        version_offset,
                                        model_type_state.encryption_key_name());
  }

  size_t ProcessorEntityCount() const {
    return processor_entity_tracker_.GetAllEntitiesIncludingTombstones().size();
  }

  FakeModelTypeSyncBridge* bridge() { return &model_type_sync_bridge_; }
  ClientTagBasedRemoteUpdateHandler* remote_update_handler() {
    return &remote_update_handler_;
  }
  FakeModelTypeSyncBridge::Store* db() { return bridge()->mutable_db(); }
  ProcessorEntityTracker* entity_tracker() {
    return &processor_entity_tracker_;
  }
  testing::NiceMock<MockModelTypeChangeProcessor>* change_processor() {
    return &change_processor_;
  }
  MockModelTypeWorker* worker() { return worker_.get(); }

 private:
  testing::NiceMock<MockModelTypeChangeProcessor> change_processor_;
  ProcessorEntityTracker processor_entity_tracker_;
  FakeModelTypeSyncBridge model_type_sync_bridge_;
  ClientTagBasedRemoteUpdateHandler remote_update_handler_;
  testing::NiceMock<MockModelTypeProcessor> model_type_processor_;
  std::unique_ptr<MockModelTypeWorker> worker_;
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
  ProcessSingleUpdate(
      worker()->GenerateTypeRootUpdateData(ModelType::SESSIONS));
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
  // Changes match doesn't call ResolveConflict.
  ProcessSingleUpdate(std::move(update));

  EXPECT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(0U, bridge()->trimmed_specifics_change_count());
  EXPECT_EQ(2U, db()->GetMetadata(kKey1).server_version());
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_FALSE(entity_tracker()->HasLocalChanges());
}

// Test for the case from crbug.com/1046309. Tests that there is no redundant
// deletion when processing remote deletion with different encryption key.
TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldNotIssueDeletionUponRemoteDeletion) {
  const std::string kTestEncryptionKeyName = "TestEncryptionKey";
  const std::string kDifferentEncryptionKeyName = "DifferentEncryptionKey";
  const ClientTagHash kClientTagHash = GetPrefHash(kKey1);

  sync_pb::ModelTypeState model_type_state = GenerateModelTypeState();
  model_type_state.set_encryption_key_name(kTestEncryptionKeyName);

  ProcessSingleUpdate(GeneratePrefUpdate(kClientTagHash, kKey1, kValue1));

  // Generate a remote deletion with a different encryption key.
  model_type_state.set_encryption_key_name(kDifferentEncryptionKeyName);
  ProcessSingleUpdate(model_type_state,
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

  void ProcessSharedSingleUpdate(
      UpdateResponseData update,
      const std::vector<std::string>& active_collaborations) {
    sync_pb::GarbageCollectionDirective gc_directive;
    for (const std::string& active_collaboration : active_collaborations) {
      gc_directive.mutable_collaboration_gc()->add_active_collaboration_ids(
          active_collaboration);
    }
    ProcessSingleUpdate(GenerateModelTypeState(), std::move(update),
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

}  // namespace

}  // namespace syncer
