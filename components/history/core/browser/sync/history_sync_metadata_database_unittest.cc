// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_sync_metadata_database.h"

#include "base/big_endian.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "components/history/core/browser/url_row.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::DataTypeState;
using sync_pb::EntityMetadata;
using syncer::EntityMetadataMap;
using syncer::MetadataBatch;

namespace history {

namespace {

// Some arbitrary timestamps (which happen to map to 20 May 2022).
constexpr base::Time kVisitTime1 = base::Time::FromDeltaSinceWindowsEpoch(
    base::Microseconds(13297523045341512ull));
constexpr base::Time kVisitTime2 = base::Time::FromDeltaSinceWindowsEpoch(
    base::Microseconds(13297523047664774ull));

class HistorySyncMetadataDatabaseTest : public testing::Test {
 public:
  HistorySyncMetadataDatabaseTest() : metadata_db_(&db_, &meta_table_) {}

  HistorySyncMetadataDatabaseTest(const HistorySyncMetadataDatabaseTest&) =
      delete;
  HistorySyncMetadataDatabaseTest& operator=(
      const HistorySyncMetadataDatabaseTest&) = delete;

  ~HistorySyncMetadataDatabaseTest() override = default;

  HistorySyncMetadataDatabase* metadata_db() { return &metadata_db_; }

  sql::Database* sql_db() { return &db_; }
  sql::MetaTable* sql_meta_table() { return &meta_table_; }

 protected:
  void SetUp() override {
    EXPECT_TRUE(db_.OpenInMemory());
    metadata_db_.Init();
    ASSERT_TRUE(meta_table_.Init(&db_, 1, 1));
  }
  void TearDown() override { db_.Close(); }

 private:
  sql::Database db_;
  sql::MetaTable meta_table_;

  HistorySyncMetadataDatabase metadata_db_;
};

TEST_F(HistorySyncMetadataDatabaseTest,
       ConvertsBetweenStorageKeysAndTimestamps) {
  ASSERT_NE(kVisitTime1, kVisitTime2);

  const std::string storage_key1 =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(kVisitTime1);
  const std::string storage_key2 =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(kVisitTime2);

  // Different timestamps should result in different storage keys.
  EXPECT_NE(storage_key1, storage_key2);

  // StorageKeyFromMicros and StorageKeyFromVisitTime should be equivalent.
  EXPECT_EQ(storage_key1,
            HistorySyncMetadataDatabase::StorageKeyFromMicrosSinceWindowsEpoch(
                kVisitTime1.ToDeltaSinceWindowsEpoch().InMicroseconds()));
  EXPECT_EQ(storage_key2,
            HistorySyncMetadataDatabase::StorageKeyFromMicrosSinceWindowsEpoch(
                kVisitTime2.ToDeltaSinceWindowsEpoch().InMicroseconds()));

  // Conversion from storage key back to base::Time should be lossless.
  EXPECT_EQ(kVisitTime1,
            HistorySyncMetadataDatabase::StorageKeyToVisitTime(storage_key1));
  EXPECT_EQ(kVisitTime2,
            HistorySyncMetadataDatabase::StorageKeyToVisitTime(storage_key2));
}

TEST_F(HistorySyncMetadataDatabaseTest, EmptyStateIsValid) {
  MetadataBatch metadata_batch;
  EXPECT_TRUE(metadata_db()->GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(0u, metadata_batch.TakeAllMetadata().size());
  EXPECT_EQ(DataTypeState().SerializeAsString(),
            metadata_batch.GetDataTypeState().SerializeAsString());
}

TEST_F(HistorySyncMetadataDatabaseTest, StoresAndReturnsMetadata) {
  const std::string storage_key1 =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(kVisitTime1);
  const std::string storage_key2 =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(kVisitTime2);

  // Store some data - both entity metadata and data type state.
  EntityMetadata metadata1;
  metadata1.set_sequence_number(1);
  metadata1.set_client_tag_hash("client_hash1");
  ASSERT_TRUE(metadata_db()->UpdateEntityMetadata(syncer::HISTORY, storage_key1,
                                                  metadata1));

  DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  ASSERT_TRUE(
      metadata_db()->UpdateDataTypeState(syncer::HISTORY, data_type_state));

  EntityMetadata metadata2;
  metadata2.set_sequence_number(2);
  metadata2.set_client_tag_hash("client_hash2");
  ASSERT_TRUE(metadata_db()->UpdateEntityMetadata(syncer::HISTORY, storage_key2,
                                                  metadata2));

  // Read the metadata and make sure it matches what we wrote.
  MetadataBatch metadata_batch;
  EXPECT_TRUE(metadata_db()->GetAllSyncMetadata(&metadata_batch));

  EXPECT_EQ(metadata_batch.GetDataTypeState().initial_sync_state(),
            sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  EntityMetadataMap metadata_records = metadata_batch.TakeAllMetadata();
  EXPECT_EQ(metadata_records.size(), 2u);
  EXPECT_EQ(metadata_records[storage_key1]->sequence_number(), 1);
  EXPECT_EQ(metadata_records[storage_key1]->client_tag_hash(), "client_hash1");
  EXPECT_EQ(metadata_records[storage_key2]->sequence_number(), 2);
  EXPECT_EQ(metadata_records[storage_key2]->client_tag_hash(), "client_hash2");

  // Now check that an entity update and a data type state update replace the
  // old values.
  metadata1.set_sequence_number(2);
  ASSERT_TRUE(metadata_db()->UpdateEntityMetadata(syncer::HISTORY, storage_key1,
                                                  metadata1));
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
  ASSERT_TRUE(
      metadata_db()->UpdateDataTypeState(syncer::HISTORY, data_type_state));

  MetadataBatch metadata_batch2;
  ASSERT_TRUE(metadata_db()->GetAllSyncMetadata(&metadata_batch2));
  EXPECT_EQ(
      metadata_batch2.GetDataTypeState().initial_sync_state(),
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);

  EntityMetadataMap metadata_records2 = metadata_batch2.TakeAllMetadata();
  EXPECT_EQ(metadata_records2.size(), 2u);
  EXPECT_EQ(metadata_records2[storage_key1]->sequence_number(), 2);
}

TEST_F(HistorySyncMetadataDatabaseTest, DeletesSyncMetadata) {
  const std::string storage_key =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(kVisitTime1);

  // Write some data into the store.
  DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  ASSERT_TRUE(
      metadata_db()->UpdateDataTypeState(syncer::HISTORY, data_type_state));

  EntityMetadata metadata;
  metadata.set_client_tag_hash("client_hash");
  ASSERT_TRUE(metadata_db()->UpdateEntityMetadata(syncer::HISTORY, storage_key,
                                                  metadata));

  // Delete the data we just wrote.
  ASSERT_TRUE(metadata_db()->ClearEntityMetadata(syncer::HISTORY, storage_key));

  // It shouldn't be there anymore.
  MetadataBatch metadata_batch;
  ASSERT_TRUE(metadata_db()->GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(metadata_batch.GetAllMetadata().size(), 0u);

  // Now delete the data type state and make sure it's gone.
  ASSERT_NE(DataTypeState().SerializeAsString(),
            metadata_batch.GetDataTypeState().SerializeAsString());
  ASSERT_TRUE(metadata_db()->ClearDataTypeState(syncer::HISTORY));
  ASSERT_TRUE(metadata_db()->GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(DataTypeState().SerializeAsString(),
            metadata_batch.GetDataTypeState().SerializeAsString());
}

TEST_F(HistorySyncMetadataDatabaseTest, FailsToReadCorruptSyncMetadata) {
  // Manually insert some corrupt data into the underlying sql DB.
  sql::Statement s(sql_db()->GetUniqueStatement(
      "INSERT OR REPLACE INTO history_sync_metadata (storage_key, value) "
      "VALUES(1, 'unparseable')"));
  ASSERT_TRUE(s.Run());

  MetadataBatch metadata_batch;
  EXPECT_FALSE(metadata_db()->GetAllSyncMetadata(&metadata_batch));
}

TEST_F(HistorySyncMetadataDatabaseTest, FailsToReadCorruptDataTypeState) {
  // Insert some corrupt data into the meta table.
  sql_meta_table()->SetValue("history_model_type_state", "unparseable");

  MetadataBatch metadata_batch;
  EXPECT_FALSE(metadata_db()->GetAllSyncMetadata(&metadata_batch));
}

}  // namespace

}  // namespace history
