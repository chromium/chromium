// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_store_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "components/sync/base/model_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using testing::Eq;
using testing::NotNull;
using testing::SizeIs;

std::unique_ptr<ModelTypeStore> ExerciseStoreFactoryAndWait(
    const RepeatingModelTypeStoreFactory& store_factory) {
  std::unique_ptr<ModelTypeStore> result;
  base::RunLoop loop;
  store_factory.Run(
      syncer::PREFERENCES,
      base::BindLambdaForTesting([&](const absl::optional<ModelError>& error,
                                     std::unique_ptr<ModelTypeStore> store) {
        EXPECT_FALSE(error.has_value());
        result = std::move(store);
        loop.Quit();
      }));
  loop.Run();
  return result;
}

void WriteDataAndWait(ModelTypeStore* store,
                      const std::string& id,
                      const std::string& value) {
  std::unique_ptr<ModelTypeStore::WriteBatch> batch = store->CreateWriteBatch();
  batch->WriteData(id, value);
  base::RunLoop loop;
  store->CommitWriteBatch(
      std::move(batch),
      base::BindLambdaForTesting(
          [&](const absl::optional<ModelError>& error) { loop.Quit(); }));
  loop.Run();
}

std::string ReadDataAndWait(ModelTypeStore* store, const std::string& id) {
  base::RunLoop loop;
  std::string read_value;
  store->ReadData(
      {id}, base::BindLambdaForTesting(
                [&](const absl::optional<ModelError>& error,
                    std::unique_ptr<ModelTypeStore::RecordList> data_records,
                    std::unique_ptr<ModelTypeStore::IdList> missing_id_list) {
                  EXPECT_THAT(*data_records, SizeIs(1));
                  if (data_records->size() == 1) {
                    read_value = data_records->front().value;
                  }
                  loop.Quit();
                }));
  loop.Run();
  return read_value;
}

// Regression test for http://crbug.com/1190187.
TEST(ModelTypeStoreServiceImplTest, ShouldSupportFactoryOutlivingService) {
  base::test::TaskEnvironment task_environment;
  auto service = std::make_unique<ModelTypeStoreServiceImpl>(
      base::CreateUniqueTempDirectoryScopedToTest());

  const RepeatingModelTypeStoreFactory store_factory =
      service->GetStoreFactory();
  ASSERT_TRUE(store_factory);

  // Destroy the service and wait until all backend cleanup work is done.
  service.reset();
  task_environment.RunUntilIdle();

  // Verify that the factory continues to work, even if it outlives the service.
  EXPECT_THAT(ExerciseStoreFactoryAndWait(store_factory), NotNull());
}

TEST(ModelTypeStoreServiceImplTest, ShouldUseIsolatedStorageTypes) {
  base::test::TaskEnvironment task_environment;
  auto service = std::make_unique<ModelTypeStoreServiceImpl>(
      base::CreateUniqueTempDirectoryScopedToTest());

  const RepeatingModelTypeStoreFactory default_store_factory =
      service->GetStoreFactory();
  const RepeatingModelTypeStoreFactory account_store_factory =
      service->GetStoreFactoryForAccountStorage();

  ASSERT_TRUE(default_store_factory);
  ASSERT_TRUE(account_store_factory);

  std::unique_ptr<ModelTypeStore> default_store =
      ExerciseStoreFactoryAndWait(default_store_factory);
  std::unique_ptr<ModelTypeStore> account_store =
      ExerciseStoreFactoryAndWait(account_store_factory);

  ASSERT_THAT(default_store, NotNull());
  ASSERT_THAT(account_store, NotNull());

  WriteDataAndWait(default_store.get(), "key", "A");
  WriteDataAndWait(account_store.get(), "key", "B");

  // Although they share key, the two values should remain independent.
  EXPECT_THAT(ReadDataAndWait(default_store.get(), "key"), Eq("A"));
  EXPECT_THAT(ReadDataAndWait(account_store.get(), "key"), Eq("B"));
}

}  // namespace
}  // namespace syncer
