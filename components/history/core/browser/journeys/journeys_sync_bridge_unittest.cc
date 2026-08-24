// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_sync_bridge.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/journeys/journeys_sync_metadata_database.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/journey_specifics.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/unknown_field_util.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history::journeys {
namespace {

using base::test::EqualsProto;
using sync_pb::EntityMetadata;
using sync_pb::EntitySpecifics;
using sync_pb::JourneySpecifics;
using syncer::EntityData;
using syncer::ModelError;
using testing::_;
using testing::NiceMock;
using testing::Property;

constexpr char kTestJourneyId[] = "test_guid";
constexpr char kTestTitle[] = "test title";
constexpr char kTestEmoji[] = "test emoji";

JourneySpecifics CreateTestJourneySpecifics(
    const std::string& journey_id = kTestJourneyId) {
  JourneySpecifics specifics;
  specifics.set_journey_id(journey_id);
  specifics.set_title(kTestTitle);
  specifics.set_emoji(kTestEmoji);
  specifics.set_creation_time_windows_epoch_micros(100);
  return specifics;
}

EntityData CreateTestJourneyEntityData(const std::string& id = kTestJourneyId) {
  EntityData entity_data;
  *entity_data.specifics.mutable_journey() = CreateTestJourneySpecifics(id);
  entity_data.name = id;
  return entity_data;
}

class JourneysSyncDatabaseWrapper {
 public:
  JourneysSyncDatabaseWrapper() : sync_metadata_db_(&db_, &meta_table_) {
    EXPECT_TRUE(db_.OpenInMemory());
    EXPECT_TRUE(
        meta_table_.Init(&db_, /*version=*/1, /*compatible_version=*/1));
    EXPECT_TRUE(sync_metadata_db_.Init());
  }
  ~JourneysSyncDatabaseWrapper() = default;

  JourneysSyncMetadataDatabase* sync_metadata_db() {
    return &sync_metadata_db_;
  }

  bool GetAllSyncMetadata(syncer::MetadataBatch* metadata_batch) {
    return sync_metadata_db_.GetAllSyncMetadata(metadata_batch);
  }

  bool UpdateEntityMetadata(syncer::DataType data_type,
                            const std::string& storage_key,
                            const sync_pb::EntityMetadata& metadata) {
    return sync_metadata_db_.UpdateEntityMetadata(data_type, storage_key,
                                                  metadata);
  }
  sql::Database* db() { return &db_; }

 private:
  sql::Database db_{sql::test::kTestTag};
  sql::MetaTable meta_table_;
  JourneysSyncMetadataDatabase sync_metadata_db_;
};

class JourneysSyncBridgeTest : public testing::Test {
 public:
  JourneysSyncBridgeTest() = default;
  ~JourneysSyncBridgeTest() override = default;

  JourneysSyncBridge CreateBridge() {
    return JourneysSyncBridge(db_.sync_metadata_db(),
                              mock_processor_.CreateForwardingProcessor());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  JourneysSyncDatabaseWrapper db_;
  NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
};

TEST_F(JourneysSyncBridgeTest, ModelReadyToSyncOnInitialization) {
  EXPECT_CALL(mock_processor_, ModelReadyToSync(_));
  JourneysSyncBridge bridge = CreateBridge();
}

TEST_F(JourneysSyncBridgeTest, ModelReadyToSyncWithExistingMetadata) {
  EntityMetadata metadata;
  metadata.set_client_tag_hash("test_hash");
  EXPECT_TRUE(
      db_.UpdateEntityMetadata(syncer::JOURNEY, kTestJourneyId, metadata));

  EXPECT_CALL(mock_processor_, ModelReadyToSync(_))
      .WillOnce([](std::unique_ptr<syncer::MetadataBatch> batch) {
        ASSERT_TRUE(batch);
        EXPECT_EQ(batch->GetAllMetadata().size(), 1u);
      });
  JourneysSyncBridge bridge = CreateBridge();
}

TEST_F(JourneysSyncBridgeTest, IsEntityDataValid) {
  JourneysSyncBridge bridge = CreateBridge();
  EntityData data = CreateTestJourneyEntityData(kTestJourneyId);
  EXPECT_TRUE(bridge.IsEntityDataValid(data));

  EntityData invalid_data = CreateTestJourneyEntityData("");
  EXPECT_FALSE(bridge.IsEntityDataValid(invalid_data));
}

TEST_F(JourneysSyncBridgeTest, GetClientTagAndStorageKey) {
  JourneysSyncBridge bridge = CreateBridge();
  EntityData entity_data = CreateTestJourneyEntityData(kTestJourneyId);

  EXPECT_EQ(kTestJourneyId, bridge.GetClientTag(entity_data));
  EXPECT_EQ(kTestJourneyId, bridge.GetStorageKey(entity_data));
}

TEST_F(JourneysSyncBridgeTest, TrimAllSupportedFieldsFromRemoteSpecifics) {
  JourneysSyncBridge bridge = CreateBridge();
  EntitySpecifics specifics;
  *specifics.mutable_journey() = CreateTestJourneySpecifics();

  EXPECT_THAT(bridge.TrimAllSupportedFieldsFromRemoteSpecifics(specifics),
              EqualsProto(EntitySpecifics()));
}

TEST_F(JourneysSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecificsPreservesUnknownFields) {
  JourneysSyncBridge bridge = CreateBridge();
  EntitySpecifics specifics;
  JourneySpecifics* journey_specifics = specifics.mutable_journey();
  *journey_specifics = CreateTestJourneySpecifics();
  syncer::test::AddUnknownFieldToProto(*journey_specifics, "unknown_field");

  EntitySpecifics trimmed_specifics =
      bridge.TrimAllSupportedFieldsFromRemoteSpecifics(specifics);

  EXPECT_TRUE(trimmed_specifics.has_journey());
  EXPECT_THAT(trimmed_specifics.journey(),
              syncer::test::HasUnknownField("unknown_field"));
}

TEST_F(JourneysSyncBridgeTest, ApplyDisableSyncChangesDeletesMetadata) {
  JourneysSyncBridge bridge = CreateBridge();

  EntityMetadata metadata;
  metadata.set_client_tag_hash("test_hash");
  EXPECT_TRUE(
      db_.UpdateEntityMetadata(syncer::JOURNEY, kTestJourneyId, metadata));

  bridge.ApplyDisableSyncChanges(bridge.CreateMetadataChangeList());

  syncer::MetadataBatch metadata_batch;
  EXPECT_TRUE(db_.GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(metadata_batch.GetAllMetadata().size(), 0u);
}

TEST_F(JourneysSyncBridgeTest, LoadMetadataReportsErrorOnCorruptedData) {
  EXPECT_TRUE(db_.db()->Execute(
      "INSERT INTO journey_sync_metadata (storage_key, value) "
      "VALUES('invalid_key', 'not_a_valid_serialized_proto')"));

  EXPECT_CALL(
      mock_processor_,
      ReportError(Property(&ModelError::type,
                           ModelError::Type::kJourneysFailedToLoadMetadata)));
  EXPECT_CALL(mock_processor_, ModelReadyToSync(_)).Times(0);

  JourneysSyncBridge bridge = CreateBridge();
}
}  // namespace
}  // namespace history::journeys
