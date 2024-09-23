// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/storage/power_bookmark_sync_metadata_database.h"

#include "base/files/scoped_temp_dir.h"
#include "components/power_bookmarks/storage/power_bookmark_database_impl.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_bookmarks {

class PowerBookmarkSyncMetadataDatabaseTest : public testing::Test {
 public:
  PowerBookmarkSyncMetadataDatabaseTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());

    power_bookmark_db_ = std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
    power_bookmark_db_->Init();
  }

  void TearDown() override {
    power_bookmark_db_.reset();
    EXPECT_TRUE(temp_directory_.Delete());
  }

  base::FilePath db_dir() { return temp_directory_.GetPath(); }

  base::FilePath db_file_path() {
    return temp_directory_.GetPath().Append(kDatabaseName);
  }

  PowerBookmarkSyncMetadataDatabase* sync_db() {
    return static_cast<PowerBookmarkSyncMetadataDatabase*>(
        power_bookmark_db_->GetSyncMetadataDatabase());
  }

  sql::Database* sql_db() {
    return static_cast<PowerBookmarkSyncMetadataDatabase*>(
               power_bookmark_db_->GetSyncMetadataDatabase())
        ->db_;
  }

  sql::MetaTable* meta_table() { return sync_db()->meta_table_; }

 private:
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<PowerBookmarkDatabaseImpl> power_bookmark_db_;
};

TEST_F(PowerBookmarkSyncMetadataDatabaseTest, Init) {
  // Database should have 4 tables: meta, saves, blobs, sync_meta.
  EXPECT_EQ(4u, sql::test::CountSQLTables(sql_db()));
}

TEST_F(PowerBookmarkSyncMetadataDatabaseTest, EmptyStateIsValid) {
  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      sync_db()->GetAllSyncMetadata();
  EXPECT_NE(nullptr, metadata_batch);
  EXPECT_EQ(0u, metadata_batch->TakeAllMetadata().size());
  EXPECT_EQ(sync_pb::DataTypeState().SerializeAsString(),
            metadata_batch->GetDataTypeState().SerializeAsString());
}

TEST_F(PowerBookmarkSyncMetadataDatabaseTest, UpdateEntityMetadata) {
  sync_pb::EntityMetadata entity_metadata;
  sync_db()->UpdateEntityMetadata(syncer::DataType::UNSPECIFIED, "test",
                                  entity_metadata);

  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      sync_db()->GetAllSyncMetadata();
  EXPECT_NE(nullptr, metadata_batch);
  EXPECT_EQ(1u, metadata_batch->TakeAllMetadata().size());
  EXPECT_EQ(sync_pb::DataTypeState().SerializeAsString(),
            metadata_batch->GetDataTypeState().SerializeAsString());
}

TEST_F(PowerBookmarkSyncMetadataDatabaseTest, ClearEntityMetadata) {
  sync_pb::EntityMetadata entity_metadata;
  sync_db()->UpdateEntityMetadata(syncer::DataType::UNSPECIFIED, "test",
                                  entity_metadata);

  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      sync_db()->GetAllSyncMetadata();
  EXPECT_NE(nullptr, metadata_batch);
  EXPECT_EQ(1u, metadata_batch->TakeAllMetadata().size());

  sync_db()->ClearEntityMetadata(syncer::DataType::UNSPECIFIED, "test");
  metadata_batch = sync_db()->GetAllSyncMetadata();
  EXPECT_NE(nullptr, metadata_batch);
  EXPECT_EQ(0u, metadata_batch->TakeAllMetadata().size());
  EXPECT_EQ(sync_pb::DataTypeState().SerializeAsString(),
            metadata_batch->GetDataTypeState().SerializeAsString());
}

TEST_F(PowerBookmarkSyncMetadataDatabaseTest, FailsToReadCorruptSyncMetadata) {
  // Manually insert some corrupt data into the underlying sql DB.
  sql::Statement s(sql_db()->GetUniqueStatement(
      "INSERT OR REPLACE INTO sync_metadata (storage_key, value) "
      "VALUES(1, 'unparseable')"));
  ASSERT_TRUE(s.Run());

  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      sync_db()->GetAllSyncMetadata();
  EXPECT_EQ(nullptr, metadata_batch);
}

TEST_F(PowerBookmarkSyncMetadataDatabaseTest, FailsToReadCorruptDataTypeState) {
  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      sync_db()->GetAllSyncMetadata();
  EXPECT_NE(nullptr, metadata_batch);

  meta_table()->SetValue("power_bookmark_type_state", "unparseable");

  metadata_batch = sync_db()->GetAllSyncMetadata();
  EXPECT_EQ(nullptr, metadata_batch);
}

}  // namespace power_bookmarks