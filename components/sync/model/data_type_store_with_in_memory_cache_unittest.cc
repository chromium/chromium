// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/data_type_store_with_in_memory_cache.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/security_event_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using base::test::RunOnceCallback;
using StoreWithCache =
    DataTypeStoreWithInMemoryCache<sync_pb::SecurityEventSpecifics>;

void WriteToStore(DataTypeStore& store,
                  const std::string& storage_key,
                  sync_pb::SecurityEventSpecifics entry,
                  sync_pb::EntityMetadata metadata) {
  auto write_batch = store.CreateWriteBatch();
  write_batch->WriteData(storage_key, entry.SerializeAsString());

  auto mcl = std::make_unique<InMemoryMetadataChangeList>();
  mcl->UpdateMetadata(storage_key, metadata);
  write_batch->TakeMetadataChangesFrom(std::move(mcl));

  base::RunLoop run_loop;
  store.CommitWriteBatch(
      std::move(write_batch),
      base::BindLambdaForTesting(
          [&run_loop](const std::optional<ModelError>& error) {
            run_loop.Quit();
          }));
  run_loop.Run();
}

std::tuple<std::optional<ModelError>,
           std::unique_ptr<StoreWithCache>,
           std::unique_ptr<MetadataBatch>>
CreateAndLoadStoreWithCache(OnceDataTypeStoreFactory store_factory) {
  std::optional<ModelError> error;
  std::unique_ptr<StoreWithCache> store;
  std::unique_ptr<MetadataBatch> metadata_batch;

  base::RunLoop run_loop;
  base::MockCallback<StoreWithCache::CreateCallback> create_callback;
  EXPECT_CALL(create_callback, Run)
      .WillOnce([&](const std::optional<ModelError>& error_arg,
                    std::unique_ptr<StoreWithCache> store_arg,
                    std::unique_ptr<MetadataBatch> metadata_batch_arg) {
        error = error_arg;
        store = std::move(store_arg);
        metadata_batch = std::move(metadata_batch_arg);
        run_loop.Quit();
      });
  StoreWithCache::CreateAndLoad(std::move(store_factory),
                                DataType::SECURITY_EVENTS,
                                create_callback.Get());
  run_loop.Run();

  return std::make_tuple(std::move(error), std::move(store),
                         std::move(metadata_batch));
}

class DataTypeStoreWithInMemoryCacheTest : public ::testing::Test {
 public:
  DataTypeStoreWithInMemoryCacheTest() = default;
  ~DataTypeStoreWithInMemoryCacheTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DataTypeStoreWithInMemoryCacheTest, LoadsEmptyStore) {
  std::optional<ModelError> error;
  std::unique_ptr<StoreWithCache> store;
  std::unique_ptr<MetadataBatch> metadata_batch;
  std::tie(error, store, metadata_batch) = CreateAndLoadStoreWithCache(
      DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest());

  EXPECT_FALSE(error.has_value());
  ASSERT_TRUE(store);
  EXPECT_TRUE(store->in_memory_data().empty());
  ASSERT_TRUE(metadata_batch);
  EXPECT_TRUE(metadata_batch->GetAllMetadata().empty());
}

TEST_F(DataTypeStoreWithInMemoryCacheTest, LoadsPopulatedStore) {
  std::unique_ptr<DataTypeStore> underlying_store =
      DataTypeStoreTestUtil::CreateInMemoryStoreForTest(
          DataType::SECURITY_EVENTS);

  const std::string storage_key = "12345";
  sync_pb::SecurityEventSpecifics entry_in_store;
  entry_in_store.set_event_time_usec(12345);
  sync_pb::EntityMetadata metadata_in_store;
  metadata_in_store.set_creation_time(54321);
  WriteToStore(*underlying_store, storage_key, entry_in_store,
               metadata_in_store);

  OnceDataTypeStoreFactory store_factory =
      DataTypeStoreTestUtil::MoveStoreToFactory(std::move(underlying_store));

  std::optional<ModelError> error;
  std::unique_ptr<StoreWithCache> store;
  std::unique_ptr<MetadataBatch> metadata_batch;
  std::tie(error, store, metadata_batch) =
      CreateAndLoadStoreWithCache(std::move(store_factory));

  EXPECT_FALSE(error.has_value());

  ASSERT_TRUE(store);
  EXPECT_EQ(store->in_memory_data().size(), 1u);
  ASSERT_TRUE(store->in_memory_data().contains(storage_key));
  EXPECT_EQ(store->in_memory_data().at(storage_key).event_time_usec(),
            entry_in_store.event_time_usec());

  ASSERT_TRUE(metadata_batch);
  EXPECT_EQ(metadata_batch->GetAllMetadata().size(), 1u);
  ASSERT_TRUE(metadata_batch->GetAllMetadata().contains(storage_key));
  EXPECT_EQ(metadata_batch->GetAllMetadata().at(storage_key)->creation_time(),
            metadata_in_store.creation_time());
}

TEST_F(DataTypeStoreWithInMemoryCacheTest, HandlesStoreCreationError) {
  base::MockCallback<OnceDataTypeStoreFactory> store_factory;
  EXPECT_CALL(store_factory, Run)
      .WillOnce(RunOnceCallback<1>(
          ModelError(FROM_HERE, "Store creation error!"), nullptr));
  std::optional<ModelError> error;
  std::unique_ptr<StoreWithCache> store;
  std::unique_ptr<MetadataBatch> metadata_batch;
  std::tie(error, store, metadata_batch) =
      CreateAndLoadStoreWithCache(store_factory.Get());

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error->message(), "Store creation error!");
  EXPECT_FALSE(store);
  EXPECT_FALSE(metadata_batch);
}

TEST_F(DataTypeStoreWithInMemoryCacheTest, HandlesStoreLoadError) {
  auto underlying_store = std::make_unique<MockDataTypeStore>();
  MockDataTypeStore* underlying_store_raw = underlying_store.get();
  OnceDataTypeStoreFactory store_factory =
      DataTypeStoreTestUtil::MoveStoreToFactory(std::move(underlying_store));

  EXPECT_CALL(*underlying_store_raw, ReadAllDataAndMetadata)
      .WillOnce(RunOnceCallback<0>(ModelError(FROM_HERE, "Store load error!"),
                                   nullptr, nullptr));

  std::optional<ModelError> error;
  std::unique_ptr<StoreWithCache> store;
  std::unique_ptr<MetadataBatch> metadata_batch;
  std::tie(error, store, metadata_batch) =
      CreateAndLoadStoreWithCache(std::move(store_factory));

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error->message(), "Store load error!");
  EXPECT_FALSE(store);
  EXPECT_FALSE(metadata_batch);
}

}  // namespace
}  // namespace syncer
