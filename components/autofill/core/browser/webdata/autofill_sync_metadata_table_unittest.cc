// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using sync_pb::DataTypeState;
using sync_pb::EntityMetadata;
using syncer::EntityMetadataMap;
using syncer::MetadataBatch;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

class AutfillSyncMetadataTableTest
    : public testing::Test,
      public testing::WithParamInterface<syncer::DataType> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");
    table_ = std::make_unique<AutofillSyncMetadataTable>();
    db_ = std::make_unique<WebDatabase>();
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_));
  }

  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AutofillSyncMetadataTable> table_;
  std::unique_ptr<WebDatabase> db_;
};

TEST_P(AutfillSyncMetadataTableTest, AutofillNoMetadata) {
  syncer::DataType data_type = GetParam();
  MetadataBatch metadata_batch;
  EXPECT_TRUE(table_->GetAllSyncMetadata(data_type, &metadata_batch));
  EXPECT_EQ(0u, metadata_batch.TakeAllMetadata().size());
  EXPECT_EQ(DataTypeState().SerializeAsString(),
            metadata_batch.GetDataTypeState().SerializeAsString());
}

TEST_P(AutfillSyncMetadataTableTest, AutofillGetAllSyncMetadata) {
  syncer::DataType data_type = GetParam();
  EntityMetadata metadata;
  std::string storage_key = "storage_key";
  std::string storage_key2 = "storage_key2";
  metadata.set_sequence_number(1);

  EXPECT_TRUE(table_->UpdateEntityMetadata(data_type, storage_key, metadata));

  DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  EXPECT_TRUE(table_->UpdateDataTypeState(data_type, data_type_state));

  metadata.set_sequence_number(2);
  EXPECT_TRUE(table_->UpdateEntityMetadata(data_type, storage_key2, metadata));

  MetadataBatch metadata_batch;
  EXPECT_TRUE(table_->GetAllSyncMetadata(data_type, &metadata_batch));

  EXPECT_EQ(metadata_batch.GetDataTypeState().initial_sync_state(),
            sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  EntityMetadataMap metadata_records = metadata_batch.TakeAllMetadata();

  EXPECT_EQ(metadata_records.size(), 2u);
  EXPECT_EQ(metadata_records[storage_key]->sequence_number(), 1);
  EXPECT_EQ(metadata_records[storage_key2]->sequence_number(), 2);

  // Now check that a data type state update replaces the old value
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
  EXPECT_TRUE(table_->UpdateDataTypeState(data_type, data_type_state));

  EXPECT_TRUE(table_->GetAllSyncMetadata(data_type, &metadata_batch));
  EXPECT_EQ(
      metadata_batch.GetDataTypeState().initial_sync_state(),
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
}

TEST_P(AutfillSyncMetadataTableTest, AutofillWriteThenDeleteSyncMetadata) {
  syncer::DataType data_type = GetParam();
  EntityMetadata metadata;
  MetadataBatch metadata_batch;
  std::string storage_key = "storage_key";
  DataTypeState data_type_state;

  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  metadata.set_client_tag_hash("client_hash");

  // Write the data into the store.
  EXPECT_TRUE(table_->UpdateEntityMetadata(data_type, storage_key, metadata));
  EXPECT_TRUE(table_->UpdateDataTypeState(data_type, data_type_state));
  // Delete the data we just wrote.
  EXPECT_TRUE(table_->ClearEntityMetadata(data_type, storage_key));
  // It shouldn't be there any more.
  EXPECT_TRUE(table_->GetAllSyncMetadata(data_type, &metadata_batch));

  EntityMetadataMap metadata_records = metadata_batch.TakeAllMetadata();
  EXPECT_EQ(metadata_records.size(), 0u);

  // Now delete the data type state.
  EXPECT_TRUE(table_->ClearDataTypeState(data_type));
  EXPECT_TRUE(table_->GetAllSyncMetadata(data_type, &metadata_batch));
  EXPECT_EQ(DataTypeState().SerializeAsString(),
            metadata_batch.GetDataTypeState().SerializeAsString());
}

TEST_P(AutfillSyncMetadataTableTest, AutofillCorruptSyncMetadata) {
  syncer::DataType data_type = GetParam();
  MetadataBatch metadata_batch;
  sql::Statement s(db_->GetSQLConnection()->GetUniqueStatement(
      "INSERT OR REPLACE INTO autofill_sync_metadata "
      "(model_type, storage_key, value) VALUES(?, ?, ?)"));
  s.BindInt(0, syncer::DataTypeToStableIdentifier(data_type));
  s.BindString(1, "storage_key");
  s.BindString(2, "unparseable");
  EXPECT_TRUE(s.Run());

  EXPECT_FALSE(table_->GetAllSyncMetadata(data_type, &metadata_batch));
}

TEST_P(AutfillSyncMetadataTableTest, AutofillCorruptDataTypeState) {
  syncer::DataType data_type = GetParam();
  MetadataBatch metadata_batch;
  sql::Statement s(db_->GetSQLConnection()->GetUniqueStatement(
      "INSERT OR REPLACE INTO autofill_model_type_state "
      "(model_type, value) VALUES(?, ?)"));
  s.BindInt(0, syncer::DataTypeToStableIdentifier(data_type));
  s.BindString(1, "unparseable");
  EXPECT_TRUE(s.Run());

  EXPECT_FALSE(table_->GetAllSyncMetadata(data_type, &metadata_batch));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillTableTest,
    AutfillSyncMetadataTableTest,
    testing::ValuesIn([] {
      std::vector<syncer::DataType> supported_types;
      for (syncer::DataType data_type : syncer::DataTypeSet::All()) {
        if (AutofillSyncMetadataTable::SupportsMetadataForDataType(data_type)) {
          supported_types.push_back(data_type);
        }
      }
      return supported_types;
    }()));

}  // namespace

}  // namespace autofill
