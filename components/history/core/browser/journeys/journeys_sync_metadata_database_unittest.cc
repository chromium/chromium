// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_sync_metadata_database.h"

#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::DataTypeState;
using sync_pb::EntityMetadata;
using syncer::EntityMetadataMap;
using syncer::MetadataBatch;

namespace history::journeys {

namespace {

constexpr char kTestStorageKey1[] = "test_guid_1";
constexpr char kTestStorageKey2[] = "test_guid_2";

class JourneysSyncMetadataDatabaseTest : public testing::Test {
 public:
  JourneysSyncMetadataDatabaseTest() : metadata_db_(&db_, &meta_table_) {}

  JourneysSyncMetadataDatabaseTest(const JourneysSyncMetadataDatabaseTest&) =
      delete;
  JourneysSyncMetadataDatabaseTest& operator=(
      const JourneysSyncMetadataDatabaseTest&) = delete;

  ~JourneysSyncMetadataDatabaseTest() override = default;

  JourneysSyncMetadataDatabase* metadata_db() { return &metadata_db_; }

  sql::Database* sql_db() { return &db_; }
  sql::MetaTable* sql_meta_table() { return &meta_table_; }

 protected:
  void SetUp() override {
    ASSERT_TRUE(db_.OpenInMemory());
    ASSERT_TRUE(metadata_db_.Init());
    ASSERT_TRUE(meta_table_.Init(&db_, 1, 1));
  }
  void TearDown() override { db_.Close(); }

 private:
  sql::Database db_{sql::test::kTestTag};
  sql::MetaTable meta_table_;

  JourneysSyncMetadataDatabase metadata_db_;
};

TEST_F(JourneysSyncMetadataDatabaseTest, EmptyStateIsValid) {
  MetadataBatch metadata_batch;
  EXPECT_TRUE(metadata_db()->GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(0u, metadata_batch.TakeAllMetadata().size());
  EXPECT_EQ(DataTypeState().SerializeAsString(),
            metadata_batch.GetDataTypeState().SerializeAsString());
}

TEST_F(JourneysSyncMetadataDatabaseTest, StoresAndReturnsMetadata) {
  // Store some data - both entity metadata and data type state.
  EntityMetadata metadata1;
  metadata1.set_sequence_number(1);
  metadata1.set_client_tag_hash("client_hash1");
  ASSERT_TRUE(metadata_db()->UpdateEntityMetadata(syncer::JOURNEY,
                                                  kTestStorageKey1, metadata1));

  DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  ASSERT_TRUE(
      metadata_db()->UpdateDataTypeState(syncer::JOURNEY, data_type_state));

  EntityMetadata metadata2;
  metadata2.set_sequence_number(2);
  metadata2.set_client_tag_hash("client_hash2");
  ASSERT_TRUE(metadata_db()->UpdateEntityMetadata(syncer::JOURNEY,
                                                  kTestStorageKey2, metadata2));

  // Read the metadata and make sure it matches what we wrote.
  MetadataBatch metadata_batch;
  EXPECT_TRUE(metadata_db()->GetAllSyncMetadata(&metadata_batch));

  EXPECT_EQ(metadata_batch.GetDataTypeState().initial_sync_state(),
            sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  EntityMetadataMap metadata_records = metadata_batch.TakeAllMetadata();
  EXPECT_EQ(metadata_records.size(), 2u);
  EXPECT_EQ(metadata_records[kTestStorageKey1]->sequence_number(), 1);
  EXPECT_EQ(metadata_records[kTestStorageKey1]->client_tag_hash(),
            "client_hash1");
  EXPECT_EQ(metadata_records[kTestStorageKey2]->sequence_number(), 2);
  EXPECT_EQ(metadata_records[kTestStorageKey2]->client_tag_hash(),
            "client_hash2");

  // Now check that an entity update and a data type state update replace the
  // old values.
  metadata1.set_sequence_number(2);
  ASSERT_TRUE(metadata_db()->UpdateEntityMetadata(syncer::JOURNEY,
                                                  kTestStorageKey1, metadata1));
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
  ASSERT_TRUE(
      metadata_db()->UpdateDataTypeState(syncer::JOURNEY, data_type_state));

  MetadataBatch metadata_batch2;
  ASSERT_TRUE(metadata_db()->GetAllSyncMetadata(&metadata_batch2));
  EXPECT_EQ(
      metadata_batch2.GetDataTypeState().initial_sync_state(),
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);

  EntityMetadataMap metadata_records2 = metadata_batch2.TakeAllMetadata();
  EXPECT_EQ(metadata_records2.size(), 2u);
  EXPECT_EQ(metadata_records2[kTestStorageKey1]->sequence_number(), 2);
}

TEST_F(JourneysSyncMetadataDatabaseTest, DeletesSyncMetadata) {
  // Write some data into the store.
  DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  ASSERT_TRUE(
      metadata_db()->UpdateDataTypeState(syncer::JOURNEY, data_type_state));

  EntityMetadata metadata;
  metadata.set_client_tag_hash("client_hash");
  ASSERT_TRUE(metadata_db()->UpdateEntityMetadata(syncer::JOURNEY,
                                                  kTestStorageKey1, metadata));

  // Delete the entity metadata we just wrote.
  ASSERT_TRUE(
      metadata_db()->ClearEntityMetadata(syncer::JOURNEY, kTestStorageKey1));

  // It shouldn't be there anymore.
  MetadataBatch metadata_batch;
  ASSERT_TRUE(metadata_db()->GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(metadata_batch.GetAllMetadata().size(), 0u);

  // Now delete the data type state and make sure it's gone.
  ASSERT_NE(DataTypeState().SerializeAsString(),
            metadata_batch.GetDataTypeState().SerializeAsString());
  ASSERT_TRUE(metadata_db()->ClearDataTypeState(syncer::JOURNEY));
  ASSERT_TRUE(metadata_db()->GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(DataTypeState().SerializeAsString(),
            metadata_batch.GetDataTypeState().SerializeAsString());
}

TEST_F(JourneysSyncMetadataDatabaseTest, ClearAllEntityMetadata) {
  EntityMetadata metadata;
  metadata.set_client_tag_hash("client_hash");
  ASSERT_TRUE(metadata_db()->UpdateEntityMetadata(syncer::JOURNEY,
                                                  kTestStorageKey1, metadata));
  ASSERT_TRUE(metadata_db()->UpdateEntityMetadata(syncer::JOURNEY,
                                                  kTestStorageKey2, metadata));

  ASSERT_TRUE(metadata_db()->ClearAllEntityMetadata());

  MetadataBatch metadata_batch;
  ASSERT_TRUE(metadata_db()->GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(metadata_batch.GetAllMetadata().size(), 0u);
}

TEST_F(JourneysSyncMetadataDatabaseTest, FailsToReadCorruptSyncMetadata) {
  // Manually insert some corrupt data into the underlying sql DB.
  sql::Statement s(sql_db()->GetUniqueStatement(
      "INSERT OR REPLACE INTO journey_sync_metadata (storage_key, value) "
      "VALUES('guid', 'unparseable')"));
  ASSERT_TRUE(s.Run());

  MetadataBatch metadata_batch;
  EXPECT_FALSE(metadata_db()->GetAllSyncMetadata(&metadata_batch));
}

TEST_F(JourneysSyncMetadataDatabaseTest, FailsToReadCorruptDataTypeState) {
  // Insert some corrupt data into the meta table.
  sql_meta_table()->SetValue("journey_data_type_state", "unparseable");

  MetadataBatch metadata_batch;
  EXPECT_FALSE(metadata_db()->GetAllSyncMetadata(&metadata_batch));
}

}  // namespace

}  // namespace history::journeys
