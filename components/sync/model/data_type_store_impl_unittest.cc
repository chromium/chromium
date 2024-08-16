// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/data_type_store_impl.h"

#include <map>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::IsEmpty;
using testing::Not;
using testing::Pair;
using testing::SizeIs;

sync_pb::DataTypeState CreateDataTypeState(const std::string& value) {
  sync_pb::DataTypeState state;
  state.set_encryption_key_name(value);
  return state;
}

sync_pb::EntityMetadata CreateEntityMetadata(const std::string& value) {
  sync_pb::EntityMetadata metadata;
  metadata.set_client_tag_hash(value);
  return metadata;
}

// Following functions capture parameters passed to callbacks into variables
// provided by test. They can be passed as callbacks to DataTypeStore
// functions.
static void CaptureError(std::optional<ModelError>* dst,
                         const std::optional<ModelError>& error) {
  *dst = error;
}

void CaptureErrorAndRecords(
    std::optional<ModelError>* dst_error,
    std::unique_ptr<DataTypeStore::RecordList>* dst_records,
    const std::optional<ModelError>& error,
    std::unique_ptr<DataTypeStore::RecordList> records) {
  *dst_error = error;
  *dst_records = std::move(records);
}

void CaptureErrorAndMetadataBatch(std::optional<ModelError>* dst_error,
                                  std::unique_ptr<MetadataBatch>* dst_batch,
                                  const std::optional<ModelError>& error,
                                  std::unique_ptr<MetadataBatch> batch) {
  *dst_error = error;
  *dst_batch = std::move(batch);
}

void CaptureErrorRecordsAndIdList(
    std::optional<ModelError>* dst_error,
    std::unique_ptr<DataTypeStore::RecordList>* dst_records,
    std::unique_ptr<DataTypeStore::IdList>* dst_id_list,
    const std::optional<ModelError>& error,
    std::unique_ptr<DataTypeStore::RecordList> records,
    std::unique_ptr<DataTypeStore::IdList> missing_id_list) {
  *dst_error = error;
  *dst_records = std::move(records);
  *dst_id_list = std::move(missing_id_list);
}

void WriteData(DataTypeStore* store,
               const std::string& key,
               const std::string& data) {
  std::unique_ptr<DataTypeStore::WriteBatch> write_batch =
      store->CreateWriteBatch();
  write_batch->WriteData(key, data);
  std::optional<ModelError> error;
  store->CommitWriteBatch(std::move(write_batch),
                          base::BindOnce(&CaptureError, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(error) << error->ToString();
}

void WriteMetadata(DataTypeStore* store,
                   const std::string& key,
                   const sync_pb::EntityMetadata& metadata) {
  std::unique_ptr<DataTypeStore::WriteBatch> write_batch =
      store->CreateWriteBatch();
  write_batch->GetMetadataChangeList()->UpdateMetadata(key, metadata);

  std::optional<ModelError> error;
  store->CommitWriteBatch(std::move(write_batch),
                          base::BindOnce(&CaptureError, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(error) << error->ToString();
}

void WriteDataTypeState(DataTypeStore* store,
                        const sync_pb::DataTypeState& state) {
  std::unique_ptr<DataTypeStore::WriteBatch> write_batch =
      store->CreateWriteBatch();
  write_batch->GetMetadataChangeList()->UpdateDataTypeState(state);

  std::optional<ModelError> error;
  store->CommitWriteBatch(std::move(write_batch),
                          base::BindOnce(&CaptureError, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(error) << error->ToString();
}

void ReadStoreContents(DataTypeStore* store,
                       std::unique_ptr<DataTypeStore::RecordList>* data_records,
                       std::unique_ptr<MetadataBatch>* metadata_batch) {
  std::optional<ModelError> error;
  store->ReadAllData(
      base::BindOnce(&CaptureErrorAndRecords, &error, data_records));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(error) << error->ToString();
  store->ReadAllMetadata(
      base::BindOnce(&CaptureErrorAndMetadataBatch, &error, metadata_batch));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(error) << error->ToString();
}

void VerifyMetadata(
    const std::unique_ptr<MetadataBatch>& batch,
    const sync_pb::DataTypeState& state,
    std::map<std::string, sync_pb::EntityMetadata> expected_metadata) {
  EXPECT_EQ(state.SerializeAsString(),
            batch->GetDataTypeState().SerializeAsString());
  EntityMetadataMap actual_metadata = batch->TakeAllMetadata();
  for (const auto& [storage_key, metadata] : expected_metadata) {
    auto it = actual_metadata.find(storage_key);
    ASSERT_TRUE(it != actual_metadata.end());
    EXPECT_EQ(metadata.SerializeAsString(), it->second->SerializeAsString());
    actual_metadata.erase(it);
  }
  EXPECT_EQ(0U, actual_metadata.size());
}

// Matcher to verify contents of returned RecordList .
MATCHER_P2(RecordMatches, id, value, "") {
  return arg.id == id && arg.value == value;
}

}  // namespace

class DataTypeStoreImplTest : public testing::TestWithParam<StorageType> {
 public:
  DataTypeStoreImplTest()
      : store_(DataTypeStoreTestUtil::CreateInMemoryStoreForTest(
            PREFERENCES,
            /*storage_type=*/GetParam())) {}

  ~DataTypeStoreImplTest() override {
    if (store_) {
      store_.reset();
      base::RunLoop().RunUntilIdle();
    }
  }

  DataTypeStore* store() { return store_.get(); }

  void WriteTestData() {
    WriteData(store(), "id1", "data1");
    WriteMetadata(store(), "id1", CreateEntityMetadata("metadata1"));
    WriteData(store(), "id2", "data2");
    WriteDataTypeState(store(), CreateDataTypeState("type_state"));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<DataTypeStore> store_;
};

INSTANTIATE_TEST_SUITE_P(StorageType,
                         DataTypeStoreImplTest,
                         testing::Values(StorageType::kUnspecified,
                                         StorageType::kAccount));

// Test read functions on empty store.
TEST_P(DataTypeStoreImplTest, ReadEmptyStore) {
  std::unique_ptr<DataTypeStore::RecordList> data_records;
  std::unique_ptr<MetadataBatch> metadata_batch;
  ReadStoreContents(store(), &data_records, &metadata_batch);
  ASSERT_TRUE(data_records->empty());
  VerifyMetadata(std::move(metadata_batch), sync_pb::DataTypeState(),
                 std::map<std::string, sync_pb::EntityMetadata>());
}

// Test that records that are written to store later can be read from it.
TEST_P(DataTypeStoreImplTest, WriteThenRead) {
  WriteTestData();

  std::unique_ptr<DataTypeStore::RecordList> data_records;
  std::unique_ptr<MetadataBatch> metadata_batch;
  ReadStoreContents(store(), &data_records, &metadata_batch);
  ASSERT_THAT(*data_records,
              testing::UnorderedElementsAre(RecordMatches("id1", "data1"),
                                            RecordMatches("id2", "data2")));
  VerifyMetadata(std::move(metadata_batch), CreateDataTypeState("type_state"),
                 {{"id1", CreateEntityMetadata("metadata1")}});
}

TEST_P(DataTypeStoreImplTest, WriteThenReadWithPreprocessing) {
  WriteTestData();

  base::RunLoop loop;
  std::map<std::string, std::string> preprocessed;
  store()->ReadAllDataAndPreprocess(
      base::BindLambdaForTesting(
          [&](std::unique_ptr<DataTypeStore::RecordList> record_list)
              -> std::optional<ModelError> {
            for (const DataTypeStore::Record& record : *record_list) {
              preprocessed[std::string("key_") + record.id] =
                  std::string("value_") + record.value;
            }
            return std::nullopt;
          }),
      base::BindLambdaForTesting([&](const std::optional<ModelError>& error) {
        EXPECT_FALSE(error) << error->ToString();
        loop.Quit();
      }));
  loop.Run();

  // Preprocessing function above prefixes "key_" and "value_" to keys and
  // values respectively.
  EXPECT_THAT(preprocessed,
              testing::ElementsAre(Pair("key_id1", "value_data1"),
                                   Pair("key_id2", "value_data2")));
}

TEST_P(DataTypeStoreImplTest, WriteThenReadWithPreprocessingError) {
  WriteTestData();

  base::RunLoop loop;
  store()->ReadAllDataAndPreprocess(
      base::BindLambdaForTesting(
          [&](std::unique_ptr<DataTypeStore::RecordList> record_list)
              -> std::optional<ModelError> {
            return ModelError(FROM_HERE, "Preprocessing error");
          }),
      base::BindLambdaForTesting([&](const std::optional<ModelError>& error) {
        EXPECT_TRUE(error);
        loop.Quit();
      }));
  loop.Run();
}

// Test that records that DeleteAllDataAndMetadata() deletes everything.
TEST_P(DataTypeStoreImplTest, WriteThenDeleteAll) {
  WriteTestData();

  {
    std::unique_ptr<DataTypeStore::RecordList> data_records;
    std::unique_ptr<MetadataBatch> metadata_batch;
    ReadStoreContents(store(), &data_records, &metadata_batch);
    ASSERT_THAT(*data_records, SizeIs(2));
    ASSERT_THAT(metadata_batch, Not(IsEmptyMetadataBatch()));
  }

  store()->DeleteAllDataAndMetadata(base::DoNothing());

  {
    std::unique_ptr<DataTypeStore::RecordList> data_records;
    std::unique_ptr<MetadataBatch> metadata_batch;
    ReadStoreContents(store(), &data_records, &metadata_batch);
    EXPECT_THAT(*data_records, IsEmpty());
    EXPECT_THAT(metadata_batch, IsEmptyMetadataBatch());
  }
}

// Test that if DataTypeState is not set then ReadAllMetadata still succeeds
// and returns entry metadata records.
TEST_P(DataTypeStoreImplTest, MissingDataTypeState) {
  WriteTestData();

  std::optional<ModelError> error;

  std::unique_ptr<DataTypeStore::WriteBatch> write_batch =
      store()->CreateWriteBatch();
  write_batch->GetMetadataChangeList()->ClearDataTypeState();
  store()->CommitWriteBatch(std::move(write_batch),
                            base::BindOnce(&CaptureError, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(error) << error->ToString();

  std::unique_ptr<MetadataBatch> metadata_batch;
  store()->ReadAllMetadata(
      base::BindOnce(&CaptureErrorAndMetadataBatch, &error, &metadata_batch));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(error) << error->ToString();
  VerifyMetadata(std::move(metadata_batch), sync_pb::DataTypeState(),
                 {{"id1", CreateEntityMetadata("metadata1")}});
}

// Test that when reading data records by id, if one of the ids is missing
// operation still succeeds and missing id is returned in missing_id_list.
TEST_P(DataTypeStoreImplTest, ReadMissingDataRecords) {
  WriteTestData();

  std::optional<ModelError> error;

  DataTypeStore::IdList id_list;
  id_list.push_back("id1");
  id_list.push_back("id3");

  std::unique_ptr<DataTypeStore::RecordList> records;
  std::unique_ptr<DataTypeStore::IdList> missing_id_list;

  store()->ReadData(
      id_list, base::BindOnce(&CaptureErrorRecordsAndIdList, &error, &records,
                              &missing_id_list));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(error) << error->ToString();
  ASSERT_THAT(*records,
              testing::UnorderedElementsAre(RecordMatches("id1", "data1")));
  ASSERT_THAT(*missing_id_list, testing::UnorderedElementsAre("id3"));
}

TEST_P(DataTypeStoreImplTest, ReadAllDataAndMetadata) {
  WriteData(store(), "id", "data");
  WriteMetadata(store(), "id", CreateEntityMetadata("metadata"));
  base::test::TestFuture<const std::optional<ModelError>&,
                         std::unique_ptr<DataTypeStore::RecordList>,
                         std::unique_ptr<MetadataBatch>>
      result;
  store()->ReadAllDataAndMetadata(result.GetCallback());
  EXPECT_EQ(result.Get<0>(), std::nullopt);
  EXPECT_THAT(result.Get<1>(), testing::Pointee(testing::UnorderedElementsAre(
                                   RecordMatches("id", "data"))));
  VerifyMetadata(result.Get<2>(), sync_pb::DataTypeState(),
                 {{"id", CreateEntityMetadata("metadata")}});
}

// Test that stores for different types that share the same backend don't
// interfere with each other's records.
TEST(DataTypeStoreImplWithTwoStoreTest, TwoStoresWithSharedBackend) {
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<DataTypeStore> store_1 =
      DataTypeStoreTestUtil::CreateInMemoryStoreForTest(AUTOFILL);
  std::unique_ptr<DataTypeStore> store_2 =
      DataTypeStoreTestUtil::CreateInMemoryStoreForTest(BOOKMARKS);

  const sync_pb::EntityMetadata metadata1 = CreateEntityMetadata("metadata1");
  const sync_pb::EntityMetadata metadata2 = CreateEntityMetadata("metadata2");
  const sync_pb::DataTypeState state1 = CreateDataTypeState("state1");
  const sync_pb::DataTypeState state2 = CreateDataTypeState("state2");

  WriteData(store_1.get(), "key", "data1");
  WriteMetadata(store_1.get(), "key", metadata1);
  WriteDataTypeState(store_1.get(), state1);

  WriteData(store_2.get(), "key", "data2");
  WriteMetadata(store_2.get(), "key", metadata2);
  WriteDataTypeState(store_2.get(), state2);

  std::unique_ptr<DataTypeStore::RecordList> data_records;
  std::unique_ptr<MetadataBatch> metadata_batch;

  ReadStoreContents(store_1.get(), &data_records, &metadata_batch);

  EXPECT_THAT(*data_records,
              testing::ElementsAre(RecordMatches("key", "data1")));
  VerifyMetadata(std::move(metadata_batch), state1, {{"key", metadata1}});

  ReadStoreContents(store_2.get(), &data_records, &metadata_batch);

  EXPECT_THAT(*data_records,
              testing::ElementsAre(RecordMatches("key", "data2")));
  VerifyMetadata(std::move(metadata_batch), state2, {{"key", metadata2}});
}

// Test that records that DeleteAllDataAndMetadata() does not delete data from
// another store when the backend is shared.
TEST(DataTypeStoreImplWithTwoStoreTest, DeleteAllWithSharedBackend) {
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<DataTypeStore> store_1 =
      DataTypeStoreTestUtil::CreateInMemoryStoreForTest(AUTOFILL);
  std::unique_ptr<DataTypeStore> store_2 =
      DataTypeStoreTestUtil::CreateInMemoryStoreForTest(BOOKMARKS);

  const sync_pb::EntityMetadata metadata1 = CreateEntityMetadata("metadata1");
  const sync_pb::EntityMetadata metadata2 = CreateEntityMetadata("metadata2");

  WriteData(store_1.get(), "key", "data1");
  WriteMetadata(store_1.get(), "key", metadata1);

  WriteData(store_2.get(), "key", "data2");
  WriteMetadata(store_2.get(), "key", metadata2);

  {
    std::unique_ptr<DataTypeStore::RecordList> data_records;
    std::unique_ptr<MetadataBatch> metadata_batch;
    ReadStoreContents(store_1.get(), &data_records, &metadata_batch);
    ASSERT_THAT(*data_records, SizeIs(1));
    ASSERT_THAT(metadata_batch, Not(IsEmptyMetadataBatch()));
    ReadStoreContents(store_2.get(), &data_records, &metadata_batch);
    ASSERT_THAT(*data_records, SizeIs(1));
    ASSERT_THAT(metadata_batch, Not(IsEmptyMetadataBatch()));
  }

  store_2->DeleteAllDataAndMetadata(base::DoNothing());

  {
    std::unique_ptr<DataTypeStore::RecordList> data_records;
    std::unique_ptr<MetadataBatch> metadata_batch;
    ReadStoreContents(store_1.get(), &data_records, &metadata_batch);
    EXPECT_THAT(*data_records, SizeIs(1));
    EXPECT_THAT(metadata_batch, Not(IsEmptyMetadataBatch()));
    ReadStoreContents(store_2.get(), &data_records, &metadata_batch);
    EXPECT_THAT(*data_records, IsEmpty());
    EXPECT_THAT(metadata_batch, IsEmptyMetadataBatch());
  }
}

TEST(DataTypeStoreImplWithTwoStoreTest,
     AccountStoreDeleteAllWithSharedBackendAndSameDataType) {
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<DataTypeStore> store_1 =
      DataTypeStoreTestUtil::CreateInMemoryStoreForTest(
          PREFERENCES, StorageType::kUnspecified);
  std::unique_ptr<DataTypeStore> store_2 =
      DataTypeStoreTestUtil::CreateInMemoryStoreForTest(PREFERENCES,
                                                        StorageType::kAccount);

  const sync_pb::EntityMetadata metadata1 = CreateEntityMetadata("metadata1");
  const sync_pb::EntityMetadata metadata2 = CreateEntityMetadata("metadata2");
  const sync_pb::DataTypeState state1 = CreateDataTypeState("state1");
  const sync_pb::DataTypeState state2 = CreateDataTypeState("state2");

  WriteData(store_1.get(), "key", "data1");
  WriteMetadata(store_1.get(), "key", metadata1);
  WriteDataTypeState(store_1.get(), state1);

  WriteData(store_2.get(), "key", "data2");
  WriteMetadata(store_2.get(), "key", metadata2);
  WriteDataTypeState(store_2.get(), state2);

  {
    std::unique_ptr<DataTypeStore::RecordList> data_records;
    std::unique_ptr<MetadataBatch> metadata_batch;
    ReadStoreContents(store_1.get(), &data_records, &metadata_batch);
    ASSERT_THAT(*data_records, SizeIs(1));
    ASSERT_THAT(metadata_batch, Not(IsEmptyMetadataBatch()));
    ReadStoreContents(store_2.get(), &data_records, &metadata_batch);
    ASSERT_THAT(*data_records, SizeIs(1));
    ASSERT_THAT(metadata_batch, Not(IsEmptyMetadataBatch()));
  }

  store_2->DeleteAllDataAndMetadata(base::DoNothing());

  {
    std::unique_ptr<DataTypeStore::RecordList> data_records;
    std::unique_ptr<MetadataBatch> metadata_batch;
    ReadStoreContents(store_1.get(), &data_records, &metadata_batch);
    EXPECT_THAT(*data_records, SizeIs(1));
    EXPECT_THAT(metadata_batch, Not(IsEmptyMetadataBatch()));
    ReadStoreContents(store_2.get(), &data_records, &metadata_batch);
    EXPECT_THAT(*data_records, IsEmpty());
    EXPECT_THAT(metadata_batch, IsEmptyMetadataBatch());
  }
}

TEST(DataTypeStoreImplWithTwoStoreTest,
     UnspecifiedStoreDeleteAllWithSharedBackendAndSameDataType) {
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<DataTypeStore> store_1 =
      DataTypeStoreTestUtil::CreateInMemoryStoreForTest(PREFERENCES,
                                                        StorageType::kAccount);
  std::unique_ptr<DataTypeStore> store_2 =
      DataTypeStoreTestUtil::CreateInMemoryStoreForTest(
          PREFERENCES, StorageType::kUnspecified);

  const sync_pb::EntityMetadata metadata1 = CreateEntityMetadata("metadata1");
  const sync_pb::EntityMetadata metadata2 = CreateEntityMetadata("metadata2");
  const sync_pb::DataTypeState state1 = CreateDataTypeState("state1");
  const sync_pb::DataTypeState state2 = CreateDataTypeState("state2");

  WriteData(store_1.get(), "key", "data1");
  WriteMetadata(store_1.get(), "key", metadata1);
  WriteDataTypeState(store_1.get(), state1);

  WriteData(store_2.get(), "key", "data2");
  WriteMetadata(store_2.get(), "key", metadata2);
  WriteDataTypeState(store_2.get(), state2);

  {
    std::unique_ptr<DataTypeStore::RecordList> data_records;
    std::unique_ptr<MetadataBatch> metadata_batch;
    ReadStoreContents(store_1.get(), &data_records, &metadata_batch);
    ASSERT_THAT(*data_records, SizeIs(1));
    ASSERT_THAT(metadata_batch, Not(IsEmptyMetadataBatch()));
    ReadStoreContents(store_2.get(), &data_records, &metadata_batch);
    ASSERT_THAT(*data_records, SizeIs(1));
    ASSERT_THAT(metadata_batch, Not(IsEmptyMetadataBatch()));
  }

  store_2->DeleteAllDataAndMetadata(base::DoNothing());

  {
    std::unique_ptr<DataTypeStore::RecordList> data_records;
    std::unique_ptr<MetadataBatch> metadata_batch;
    ReadStoreContents(store_1.get(), &data_records, &metadata_batch);
    EXPECT_THAT(*data_records, SizeIs(1));
    EXPECT_THAT(metadata_batch, Not(IsEmptyMetadataBatch()));
    ReadStoreContents(store_2.get(), &data_records, &metadata_batch);
    EXPECT_THAT(*data_records, IsEmpty());
    EXPECT_THAT(metadata_batch, IsEmptyMetadataBatch());
  }
}

}  // namespace syncer
