// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/client_tag_based_remote_update_handler.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "components/sync/base/features.h"
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

const char kKey1[] = "key1";
const char kKey2[] = "key2";
const char kValue1[] = "value1";
const char kValue2[] = "value2";

sync_pb::ModelTypeState GenerateModelTypeState() {
  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_initial_sync_done(true);
  return model_type_state;
}

ClientTagHash GetPrefHash(const std::string& key) {
  return ClientTagHash::FromUnhashed(
      PREFERENCES, FakeModelTypeSyncBridge::ClientTagFromKey(key));
}

sync_pb::EntitySpecifics GeneratePrefSpecifics(const std::string& key,
                                               const std::string& value) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(key);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

class ClientTagBasedRemoteUpdateHandlerTest : public ::testing::Test {
 public:
  ClientTagBasedRemoteUpdateHandlerTest()
      : processor_entity_tracker_(GenerateModelTypeState(),
                                  EntityMetadataMap()),
        model_type_sync_bridge_(PREFERENCES,
                                change_processor_.CreateForwardingProcessor()),
        remote_update_handler_(PREFERENCES,
                               &model_type_sync_bridge_,
                               &processor_entity_tracker_),
        worker_(GenerateModelTypeState(), &model_type_processor_) {}

  ~ClientTagBasedRemoteUpdateHandlerTest() override = default;

  void ProcessSingleUpdate(const sync_pb::ModelTypeState& model_type_state,
                           UpdateResponseData update) {
    UpdateResponseDataList updates;
    updates.push_back(std::move(update));
    remote_update_handler_.ProcessIncrementalUpdate(model_type_state,
                                                    std::move(updates));
  }

  void ProcessSingleUpdate(UpdateResponseData update) {
    ProcessSingleUpdate(GenerateModelTypeState(), std::move(update));
  }

  UpdateResponseData GenerateUpdate(const std::string& key,
                                    const std::string& value) {
    const ClientTagHash client_tag_hash = GetPrefHash(key);
    return GenerateUpdate(client_tag_hash, key, value);
  }

  UpdateResponseData GenerateUpdate(const ClientTagHash& client_tag_hash,
                                    const std::string& key,
                                    const std::string& value) {
    return worker()->GenerateUpdateData(client_tag_hash,
                                        GeneratePrefSpecifics(key, value));
  }

  UpdateResponseData GenerateUpdate(const std::string& key,
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
  MockModelTypeWorker* worker() { return &worker_; }

 private:
  testing::NiceMock<MockModelTypeChangeProcessor> change_processor_;
  ProcessorEntityTracker processor_entity_tracker_;
  FakeModelTypeSyncBridge model_type_sync_bridge_;
  ClientTagBasedRemoteUpdateHandler remote_update_handler_;
  testing::NiceMock<MockModelTypeProcessor> model_type_processor_;
  MockModelTypeWorker worker_;
};

// Thoroughly tests the data generated by a server item creation.
TEST_F(ClientTagBasedRemoteUpdateHandlerTest, ShouldProcessRemoteCreation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kCacheBaseEntitySpecificsInMetadata);

  ProcessSingleUpdate(GenerateUpdate(kKey1, kValue1));
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
  ProcessSingleUpdate(GenerateUpdate(GetPrefHash(kKey2), kKey1, kValue1));
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldNotClearTrimmedSpecificsOnNoopRemoteUpdates) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kCacheBaseEntitySpecificsInMetadata);

  const std::string kUnknownField = "unknown_field";

  // Initial update containing unsupported fields.
  UpdateResponseData update1 = GenerateUpdate(kKey1, kValue1);
  *update1.entity.specifics.mutable_unknown_fields() = kUnknownField;
  ProcessSingleUpdate(std::move(update1));
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());
  ASSERT_EQ(1U, bridge()->trimmed_specifics_change_count());

  // Redundant update should not clear trimmed specifics.
  UpdateResponseData update2 = GenerateUpdate(kKey1, kValue1);
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
  ProcessSingleUpdate(GenerateUpdate(kKey1, kValue1));
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());

  // Redundant update from server doesn't write data but updates metadata.
  const int64_t time_before_update =
      db()->GetMetadata(kKey1).modification_time();
  ProcessSingleUpdate(GenerateUpdate(kKey1, kValue1));
  EXPECT_EQ(1U, db()->data_change_count());
  EXPECT_EQ(2U, db()->metadata_change_count());
  // Check that `modification_time` was updated.
  EXPECT_NE(time_before_update, db()->GetMetadata(kKey1).modification_time());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldIgnoreReflectionsOnRemoteUpdates) {
  ProcessSingleUpdate(GenerateUpdate(kKey1, kValue1));
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());

  // A reflection (update already received) is ignored completely.
  ProcessSingleUpdate(GenerateUpdate(kKey1, kValue1, /*version_offset=*/0));
  EXPECT_EQ(1U, db()->data_change_count());
  EXPECT_EQ(1U, db()->metadata_change_count());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest, ShouldProcessRemoteUpdates) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kCacheBaseEntitySpecificsInMetadata);

  ProcessSingleUpdate(GenerateUpdate(kKey1, kValue1));
  ASSERT_EQ(1U, ProcessorEntityCount());
  ASSERT_EQ(1U, db()->data_change_count());
  ASSERT_EQ(1U, db()->metadata_change_count());
  ASSERT_EQ(0U, bridge()->trimmed_specifics_change_count());

  // Should update both data and metadata.
  ProcessSingleUpdate(GenerateUpdate(kKey1, kValue2));
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
  ProcessSingleUpdate(GenerateUpdate(kKey1, kValue1));
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
  UpdateResponseData update = GenerateUpdate(kKey1, kValue1);
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

  update = GenerateUpdate(kKey1, kValue1);
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

  ProcessSingleUpdate(GenerateUpdate(kClientTagHash, kKey1, kValue1));

  // Generate a remote deletion with a different encryption key.
  model_type_state.set_encryption_key_name(kDifferentEncryptionKeyName);
  ProcessSingleUpdate(model_type_state,
                      worker()->GenerateTombstoneUpdateData(kClientTagHash));

  EXPECT_EQ(0u, ProcessorEntityCount());
}

TEST_F(ClientTagBasedRemoteUpdateHandlerTest,
       ShouldNotProcessInvalidRemoteCreationWithInvalidStorageKey) {
  ASSERT_EQ(0U, ProcessorEntityCount());
  UpdateResponseData update = GenerateUpdate("", "");
  ASSERT_TRUE(bridge()->SupportsGetStorageKey());
  // Bridge will generate an empty storage key.
  ProcessSingleUpdate(std::move(update));
  // Update should be filtered out.
  EXPECT_EQ(0U, db()->data_count());
  EXPECT_EQ(0U, db()->metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
}

}  // namespace

}  // namespace syncer
