// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/model_type_store_impl.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::IsEmpty;
using testing::Not;
using testing::Pair;
using testing::SizeIs;

sync_pb::ModelTypeState CreateModelTypeState(const std::string& value) {
  sync_pb::ModelTypeState state;
  state.set_encryption_key_name(value);
  return state;
}

sync_pb::EntityMetadata CreateEntityMetadata(const std::string& value) {
  sync_pb::EntityMetadata metadata;
  metadata.set_client_tag_hash(value);
  return metadata;
}

// Following functions capture parameters passed to callbacks into variables
// provided by test. They can be passed as callbacks to ModelTypeStore
// functions.
static void CaptureError(base::Optional<ModelError>* dst,
                         const base::Optional<ModelError>& error) {
  *dst = error;
}

void CaptureErrorAndRecords(
    base::Optional<ModelError>* dst_error,
    std::unique_ptr<ModelTypeStore::RecordList>* dst_records,
    const base::Optional<ModelError>& error,
    std::unique_ptr<ModelTypeStore::RecordList> records) {
  *dst_error = error;
  *dst_records = std::move(records);
}

void CaptureErrorAndMetadataBatch(base::Optional<ModelError>* dst_error,
                                  std::unique_ptr<MetadataBatch>* dst_batch,
                                  const base::Optional<ModelError>& error,
                                  std::unique_ptr<MetadataBatch> batch) {
  *dst_error = error;
  *dst_batch = std::move(batch);
}

void CaptureErrorRecordsAndIdList(
    base::Optional<ModelError>* dst_error,
    std::unique_ptr<ModelTypeStore::RecordList>* dst_records,
    std::unique_ptr<ModelTypeStore::IdList>* dst_id_list,
    const base::Optional<ModelError>& error,
    std::unique_ptr<ModelTypeStore::RecordList> records,
    std::unique_ptr<ModelTypeStore::IdList> missing_id_list) {
  *dst_error = error;
  *dst_records = std::move(records);
  *dst_id_list = std::move(missing_id_list);
}

void WriteData(ModelTypeStore* store,
               const std::string& key,
               const std::string& data) {
  auto write_batch = store->CreateWriteBatch();
  write_batch->WriteData(key, data);
  base::Optional<ModelError> error;
  store->CommitWriteBatch(std::move(write_batch),
                          base::BindOnce(&CaptureError, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(error) << error->ToString();
}

void WriteMetadata(ModelTypeStore* store,
                   const std::string& key,
                   const sync_pb::EntityMetadata& metadata) {
  auto write_batch = store->CreateWriteBatch();
  write_batch->GetMetadataChangeList()->UpdateMetadata(key, metadata);

  base::Optional<ModelError> error;
  store->CommitWriteBatch(std::move(write_batch),
                          base::BindOnce(&CaptureError, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(error) << error->ToString();
}

void WriteModelTypeState(ModelTypeStore* store,
                         const sync_pb::ModelTypeState& state) {
  auto write_batch = store->CreateWriteBatch();
  write_batch->GetMetadataChangeList()->UpdateModelTypeState(state);

  base::Optional<ModelError> error;
  store->CommitWriteBatch(std::move(write_batch),
                          base::BindOnce(&CaptureError, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(error) << error->ToString();
}

void ReadStoreContents(
    ModelTypeStore* store,
    std::unique_ptr<ModelTypeStore::RecordList>* data_records,
    std::unique_ptr<MetadataBatch>* metadata_batch) {
  base::Optional<ModelError> error;
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
    std::unique_ptr<MetadataBatch> batch,
    const sync_pb::ModelTypeState& state,
    std::map<std::string, sync_pb::EntityMetadata> expected_metadata) {
  EXPECT_EQ(state.SerializeAsString(),
            batch->GetModelTypeState().SerializeAsString());
  EntityMetadataMap actual_metadata = batch->TakeAllMetadata();
  for (const auto& kv : expected_metadata) {
    auto it = actual_metadata.find(kv.first);
    ASSERT_TRUE(it != actual_metadata.end());
    EXPECT_EQ(kv.second.SerializeAsString(), it->second->SerializeAsString());
    actual_metadata.erase(it);
  }
  EXPECT_EQ(0U, actual_metadata.size());
}

// Matcher to verify contents of returned RecordList .
MATCHER_P2(RecordMatches, id, value, "") {
  return arg.id == id && arg.value == value;
}

}  // namespace

class ModelTypeStoreImplTest : public testing::Test {
 public:
  ModelTypeStoreImplTest()
      : store_(ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  ~ModelTypeStoreImplTest() override {
    if (store_) {
      store_.reset();
      base::RunLoop().RunUntilIdle();
    }
  }

  ModelTypeStore* store() { return store_.get(); }

  void WriteTestData() {
    WriteData(store(), "id1", "data1");
    WriteMetadata(store(), "id1", CreateEntityMetadata("metadata1"));
    WriteData(store(), "id2", "data2");
    WriteModelTypeState(store(), CreateModelTypeState("type_state"));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ModelTypeStore> store_;
};

// Test read functions on empty store.
TEST_F(ModelTypeStoreImplTest, ReadEmptyStore) {
  std::unique_ptr<ModelTypeStore::RecordList> data_records;
  std::unique_ptr<MetadataBatch> metadata_batch;
  ReadStoreContents(store(), &data_records, &metadata_batch);
  ASSERT_TRUE(data_records->empty());
  VerifyMetadata(std::move(metadata_batch), sync_pb::ModelTypeState(),
                 std::map<std::string, sync_pb::EntityMetadata>());
}

// Test that records that are written to store later can be read from it.
TEST_F(ModelTypeStoreImplTest, WriteThenRead) {
  WriteTestData();

  std::unique_ptr<ModelTypeStore::RecordList> data_records;
  std::unique_ptr<MetadataBatch> metadata_batch;
  ReadStoreContents(store(), &data_records, &metadata_batch);
  ASSERT_THAT(*data_records,
              testing::UnorderedElementsAre(RecordMatches("id1", "data1"),
                                            RecordMatches("id2", "data2")));
  VerifyMetadata(std::move(metadata_batch), CreateModelTypeState("type_state"),
                 {{"id1", CreateEntityMetadata("metadata1")}});
}

TEST_F(ModelTypeStoreImplTest, WriteThenReadWithPreprocessing) {
  WriteTestData();

  base::RunLoop loop;
  std::map<std::string, std::string> preprocessed;
  store()->ReadAllDataAndPreprocess(
      base::BindLambdaForTesting(
          [&](std::unique_ptr<ModelTypeStore::RecordList> record_list)
              -> base::Optional<ModelError> {
            for (const auto& record : *record_list) {
              preprocessed[std::string("key_") + record.id] =
                  std::string("value_") + record.value;
            }
            return base::nullopt;
          }),
      base::BindLambdaForTesting([&](const base::Optional<ModelError>& error) {
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

TEST_F(ModelTypeStoreImplTest, WriteThenReadWithPreprocessingError) {
  WriteTestData();

  base::RunLoop loop;
  store()->ReadAllDataAndPreprocess(
      base::BindLambdaForTesting(
          [&](std::unique_ptr<ModelTypeStore::RecordList> record_list)
              -> base::Optional<ModelError> {
            return ModelError(FROM_HERE, "Preprocessing error");
          }),
      base::BindLambdaForTesting([&](const base::Optional<ModelError>& error) {
        EXPECT_TRUE(error);
        loop.Quit();
      }));
  loop.Run();
}

// Test that records that DeleteAllDataAndMetadata() deletes everything.
TEST_F(ModelTypeStoreImplTest, WriteThenDeleteAll) {
  WriteTestData();

  {
    std::unique_ptr<ModelTypeStore::RecordList> data_records;
    std::unique_ptr<MetadataBatch> metadata_batch;
    ReadStoreContents(store(), &data_records, &metadata_batch);
    ASSERT_THAT(*data_records, SizeIs(2));
    ASSERT_THAT(metadata_batch, Not(IsEmptyMetadataBatch()));
  }

  store()->DeleteAllDataAndMetadata(base::DoNothing());

  {
    std::unique_ptr<ModelTypeStore::RecordList> data_records;
    std::unique_ptr<MetadataBatch> metadata_batch;
    ReadStoreContents(store(), &data_records, &metadata_batch);
    EXPECT_THAT(*data_records, IsEmpty());
    EXPECT_THAT(metadata_batch, IsEmptyMetadataBatch());
  }
}

// Test that if ModelTypeState is not set then ReadAllMetadata still succeeds
// and returns entry metadata records.
TEST_F(ModelTypeStoreImplTest, MissingModelTypeState) {
  WriteTestData();

  base::Optional<ModelError> error;

  auto write_batch = store()->CreateWriteBatch();
  write_batch->GetMetadataChangeList()->ClearModelTypeState();
  store()->CommitWriteBatch(std::move(write_batch),
                            base::BindOnce(&CaptureError, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(error) << error->ToString();

  std::unique_ptr<MetadataBatch> metadata_batch;
  store()->ReadAllMetadata(
      base::BindOnce(&CaptureErrorAndMetadataBatch, &error, &metadata_batch));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(error) << error->ToString();
  VerifyMetadata(std::move(metadata_batch), sync_pb::ModelTypeState(),
                 {{"id1", CreateEntityMetadata("metadata1")}});
}

// Test that when reading data records by id, if one of the ids is missing
// operation still succeeds and missing id is returned in missing_id_list.
TEST_F(ModelTypeStoreImplTest, ReadMissingDataRecords) {
  WriteTestData();

  base::Optional<ModelError> error;

  ModelTypeStore::IdList id_list;
  id_list.push_back("id1");
  id_list.push_back("id3");

  std::unique_ptr<ModelTypeStore::RecordList> records;
  std::unique_ptr<ModelTypeStore::IdList> missing_id_list;

  store()->ReadData(
      id_list, base::BindOnce(&CaptureErrorRecordsAndIdList, &error, &records,
                              &missing_id_list));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(error) << error->ToString();
  ASSERT_THAT(*records,
              testing::UnorderedElementsAre(RecordMatches("id1", "data1")));
  ASSERT_THAT(*missing_id_list, testing::UnorderedElementsAre("id3"));
}

// Test that stores for different types that share the same backend don't
// interfere with each other's records.
TEST(ModelTypeStoreImplWithTwoStoreTest, TwoStoresWithSharedBackend) {
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<ModelTypeStore> store_1 =
      ModelTypeStoreTestUtil::CreateInMemoryStoreForTest(AUTOFILL);
  std::unique_ptr<ModelTypeStore> store_2 =
      ModelTypeStoreTestUtil::CreateInMemoryStoreForTest(BOOKMARKS);

  const sync_pb::EntityMetadata metadata1 = CreateEntityMetadata("metadata1");
  const sync_pb::EntityMetadata metadata2 = CreateEntityMetadata("metadata2");
  const sync_pb::ModelTypeState state1 = CreateModelTypeState("state1");
  const sync_pb::ModelTypeState state2 = CreateModelTypeState("state2");

  WriteData(store_1.get(), "key", "data1");
  WriteMetadata(store_1.get(), "key", metadata1);
  WriteModelTypeState(store_1.get(), state1);

  WriteData(store_2.get(), "key", "data2");
  WriteMetadata(store_2.get(), "key", metadata2);
  WriteModelTypeState(store_2.get(), state2);

  std::unique_ptr<ModelTypeStore::RecordList> data_records;
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
TEST(ModelTypeStoreImplWithTwoStoreTest, DeleteAllWithSharedBackend) {
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<ModelTypeStore> store_1 =
      ModelTypeStoreTestUtil::CreateInMemoryStoreForTest(AUTOFILL);
  std::unique_ptr<ModelTypeStore> store_2 =
      ModelTypeStoreTestUtil::CreateInMemoryStoreForTest(BOOKMARKS);

  const sync_pb::EntityMetadata metadata1 = CreateEntityMetadata("metadata1");
  const sync_pb::EntityMetadata metadata2 = CreateEntityMetadata("metadata2");

  WriteData(store_1.get(), "key", "data1");
  WriteMetadata(store_1.get(), "key", metadata1);

  WriteData(store_2.get(), "key", "data2");
  WriteMetadata(store_2.get(), "key", metadata2);

  {
    std::unique_ptr<ModelTypeStore::RecordList> data_records;
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
    std::unique_ptr<ModelTypeStore::RecordList> data_records;
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
