// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/typed_url_sync_metadata_database.h"

#include "base/big_endian.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "components/history/core/browser/url_row.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::EntityMetadata;
using sync_pb::ModelTypeState;
using syncer::EntityMetadataMap;
using syncer::MetadataBatch;

namespace history {

namespace {

std::string IntToStorageKey(int i) {
  std::string storage_key(sizeof(URLID), 0);
  base::WriteBigEndian<URLID>(&storage_key[0], i);
  return storage_key;
}

}  // namespace

class TypedURLSyncMetadataDatabaseTest : public testing::Test {
 public:
  TypedURLSyncMetadataDatabaseTest() : sync_metadata_db_(&db_, &meta_table_) {}

  TypedURLSyncMetadataDatabaseTest(const TypedURLSyncMetadataDatabaseTest&) =
      delete;
  TypedURLSyncMetadataDatabaseTest& operator=(
      const TypedURLSyncMetadataDatabaseTest&) = delete;

  ~TypedURLSyncMetadataDatabaseTest() override = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath db_file =
        temp_dir_.GetPath().AppendASCII("TypedURLSyncMetadataDatabaseTest.db");

    EXPECT_TRUE(db_.Open(db_file));

    // Initialize the tables for this test.
    sync_metadata_db_.Init();

    meta_table_.Init(&db_, 1, 1);
  }
  void TearDown() override { db_.Close(); }

  base::ScopedTempDir temp_dir_;
  sql::Database db_;
  sql::MetaTable meta_table_;

  TypedURLSyncMetadataDatabase sync_metadata_db_;
};

TEST_F(TypedURLSyncMetadataDatabaseTest, TypedURLNoMetadata) {
  MetadataBatch metadata_batch;
  EXPECT_TRUE(sync_metadata_db_.GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(0u, metadata_batch.TakeAllMetadata().size());
  EXPECT_EQ(ModelTypeState().SerializeAsString(),
            metadata_batch.GetModelTypeState().SerializeAsString());
}

TEST_F(TypedURLSyncMetadataDatabaseTest, TypedURLGetAllSyncMetadata) {
  EntityMetadata metadata;
  std::string storage_key = IntToStorageKey(1);
  std::string storage_key2 = IntToStorageKey(2);
  metadata.set_sequence_number(1);

  EXPECT_TRUE(sync_metadata_db_.UpdateEntityMetadata(syncer::TYPED_URLS,
                                                     storage_key, metadata));

  ModelTypeState model_type_state;
  model_type_state.set_initial_sync_done(true);

  EXPECT_TRUE(sync_metadata_db_.UpdateModelTypeState(syncer::TYPED_URLS,
                                                     model_type_state));

  metadata.set_sequence_number(2);
  EXPECT_TRUE(sync_metadata_db_.UpdateEntityMetadata(syncer::TYPED_URLS,
                                                     storage_key2, metadata));

  MetadataBatch metadata_batch;
  EXPECT_TRUE(sync_metadata_db_.GetAllSyncMetadata(&metadata_batch));

  EXPECT_TRUE(metadata_batch.GetModelTypeState().initial_sync_done());

  EntityMetadataMap metadata_records = metadata_batch.TakeAllMetadata();

  EXPECT_EQ(metadata_records.size(), 2u);
  EXPECT_EQ(metadata_records[storage_key]->sequence_number(), 1);
  EXPECT_EQ(metadata_records[storage_key2]->sequence_number(), 2);

  // Now check that a model type state update replaces the old value
  model_type_state.set_initial_sync_done(false);
  EXPECT_TRUE(sync_metadata_db_.UpdateModelTypeState(syncer::TYPED_URLS,
                                                     model_type_state));

  EXPECT_TRUE(sync_metadata_db_.GetAllSyncMetadata(&metadata_batch));
  EXPECT_FALSE(metadata_batch.GetModelTypeState().initial_sync_done());
}

TEST_F(TypedURLSyncMetadataDatabaseTest, TypedURLWriteThenDeleteSyncMetadata) {
  EntityMetadata metadata;
  MetadataBatch metadata_batch;
  std::string storage_key = IntToStorageKey(1);
  ModelTypeState model_type_state;

  model_type_state.set_initial_sync_done(true);

  metadata.set_client_tag_hash("client_hash");

  // Write the data into the store.
  EXPECT_TRUE(sync_metadata_db_.UpdateEntityMetadata(syncer::TYPED_URLS,
                                                     storage_key, metadata));
  EXPECT_TRUE(sync_metadata_db_.UpdateModelTypeState(syncer::TYPED_URLS,
                                                     model_type_state));
  // Delete the data we just wrote.
  EXPECT_TRUE(
      sync_metadata_db_.ClearEntityMetadata(syncer::TYPED_URLS, storage_key));
  // It shouldn't be there any more.
  EXPECT_TRUE(sync_metadata_db_.GetAllSyncMetadata(&metadata_batch));

  EntityMetadataMap metadata_records = metadata_batch.TakeAllMetadata();
  EXPECT_EQ(metadata_records.size(), 0u);

  // Now delete the model type state.
  EXPECT_TRUE(sync_metadata_db_.ClearModelTypeState(syncer::TYPED_URLS));
  EXPECT_TRUE(sync_metadata_db_.GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(ModelTypeState().SerializeAsString(),
            metadata_batch.GetModelTypeState().SerializeAsString());
}

TEST_F(TypedURLSyncMetadataDatabaseTest, TypedURLCorruptSyncMetadata) {
  MetadataBatch metadata_batch;
  sql::Statement s(
      db_.GetUniqueStatement("INSERT OR REPLACE INTO typed_url_sync_metadata "
                             "(storage_key, value) VALUES(?, ?)"));
  s.BindInt64(0, 1);
  s.BindString(1, "unparseable");
  EXPECT_TRUE(s.Run());

  EXPECT_FALSE(sync_metadata_db_.GetAllSyncMetadata(&metadata_batch));
}

TEST_F(TypedURLSyncMetadataDatabaseTest, TypedURLCorruptModelTypeState) {
  MetadataBatch metadata_batch;
  meta_table_.SetValue("typed_url_model_type_state", "unparseable");

  EXPECT_FALSE(sync_metadata_db_.GetAllSyncMetadata(&metadata_batch));
}

}  // namespace history
