// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/typed_url_sync_metadata_database.h"

#include "base/big_endian.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "components/history/core/browser/url_row.h"
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

class TypedURLSyncMetadataDatabaseTest : public testing::Test,
                                         public TypedURLSyncMetadataDatabase {
 public:
  TypedURLSyncMetadataDatabaseTest() {}
  ~TypedURLSyncMetadataDatabaseTest() override {}

 protected:
  sql::Database& GetDB() override { return db_; }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath db_file =
        temp_dir_.GetPath().AppendASCII("TypedURLSyncMetadataDatabaseTest.db");

    EXPECT_TRUE(db_.Open(db_file));

    // Initialize the tables for this test.
    InitSyncTable();

    GetMetaTable().Init(&db_, 1, 1);
  }
  void TearDown() override { db_.Close(); }

  sql::MetaTable& GetMetaTable() override { return meta_table_; }

  base::ScopedTempDir temp_dir_;
  sql::Database db_;
  sql::MetaTable meta_table_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TypedURLSyncMetadataDatabaseTest);
};

TEST_F(TypedURLSyncMetadataDatabaseTest, TypedURLNoMetadata) {
  MetadataBatch metadata_batch;
  EXPECT_TRUE(GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(0u, metadata_batch.TakeAllMetadata().size());
  EXPECT_EQ(ModelTypeState().SerializeAsString(),
            metadata_batch.GetModelTypeState().SerializeAsString());
}

TEST_F(TypedURLSyncMetadataDatabaseTest, TypedURLGetAllSyncMetadata) {
  EntityMetadata metadata;
  std::string storage_key = IntToStorageKey(1);
  std::string storage_key2 = IntToStorageKey(2);
  metadata.set_sequence_number(1);

  EXPECT_TRUE(UpdateSyncMetadata(syncer::TYPED_URLS, storage_key, metadata));

  ModelTypeState model_type_state;
  model_type_state.set_initial_sync_done(true);

  EXPECT_TRUE(UpdateModelTypeState(syncer::TYPED_URLS, model_type_state));

  metadata.set_sequence_number(2);
  EXPECT_TRUE(UpdateSyncMetadata(syncer::TYPED_URLS, storage_key2, metadata));

  MetadataBatch metadata_batch;
  EXPECT_TRUE(GetAllSyncMetadata(&metadata_batch));

  EXPECT_TRUE(metadata_batch.GetModelTypeState().initial_sync_done());

  EntityMetadataMap metadata_records = metadata_batch.TakeAllMetadata();

  EXPECT_EQ(metadata_records.size(), 2u);
  EXPECT_EQ(metadata_records[storage_key]->sequence_number(), 1);
  EXPECT_EQ(metadata_records[storage_key2]->sequence_number(), 2);

  // Now check that a model type state update replaces the old value
  model_type_state.set_initial_sync_done(false);
  EXPECT_TRUE(UpdateModelTypeState(syncer::TYPED_URLS, model_type_state));

  EXPECT_TRUE(GetAllSyncMetadata(&metadata_batch));
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
  EXPECT_TRUE(UpdateSyncMetadata(syncer::TYPED_URLS, storage_key, metadata));
  EXPECT_TRUE(UpdateModelTypeState(syncer::TYPED_URLS, model_type_state));
  // Delete the data we just wrote.
  EXPECT_TRUE(ClearSyncMetadata(syncer::TYPED_URLS, storage_key));
  // It shouldn't be there any more.
  EXPECT_TRUE(GetAllSyncMetadata(&metadata_batch));

  EntityMetadataMap metadata_records = metadata_batch.TakeAllMetadata();
  EXPECT_EQ(metadata_records.size(), 0u);

  // Now delete the model type state.
  EXPECT_TRUE(ClearModelTypeState(syncer::TYPED_URLS));
  EXPECT_TRUE(GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(ModelTypeState().SerializeAsString(),
            metadata_batch.GetModelTypeState().SerializeAsString());
}

TEST_F(TypedURLSyncMetadataDatabaseTest, TypedURLCorruptSyncMetadata) {
  MetadataBatch metadata_batch;
  sql::Statement s(GetDB().GetUniqueStatement(
      "INSERT OR REPLACE INTO typed_url_sync_metadata "
      "(storage_key, value) VALUES(?, ?)"));
  s.BindInt64(0, 1);
  s.BindString(1, "unparseable");
  EXPECT_TRUE(s.Run());

  EXPECT_FALSE(GetAllSyncMetadata(&metadata_batch));
}

TEST_F(TypedURLSyncMetadataDatabaseTest, TypedURLCorruptModelTypeState) {
  MetadataBatch metadata_batch;
  GetMetaTable().SetValue("typed_url_model_type_state", "unparseable");

  EXPECT_FALSE(GetAllSyncMetadata(&metadata_batch));
}

}  // namespace history
