// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/data_type_store_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using testing::Eq;
using testing::Le;
using testing::NotNull;
using testing::SizeIs;

std::unique_ptr<DataTypeStore> ExerciseStoreFactoryAndWait(
    const RepeatingDataTypeStoreFactory& store_factory,
    DataType data_type = DataType::PREFERENCES) {
  std::unique_ptr<DataTypeStore> result;
  base::RunLoop loop;
  store_factory.Run(data_type, base::BindLambdaForTesting(
                                   [&](const std::optional<ModelError>& error,
                                       std::unique_ptr<DataTypeStore> store) {
                                     EXPECT_FALSE(error.has_value());
                                     result = std::move(store);
                                     loop.Quit();
                                   }));
  loop.Run();
  return result;
}

void WriteDataAndWait(DataTypeStore* store,
                      const std::string& id,
                      const std::string& value) {
  std::unique_ptr<DataTypeStore::WriteBatch> batch = store->CreateWriteBatch();
  batch->WriteData(id, value);
  base::RunLoop loop;
  store->CommitWriteBatch(
      std::move(batch),
      base::BindLambdaForTesting(
          [&](const std::optional<ModelError>& error) { loop.Quit(); }));
  loop.Run();
}

std::optional<std::string> ReadDataAndWait(DataTypeStore* store,
                                           const std::string& id) {
  base::RunLoop loop;
  std::optional<std::string> read_value;
  store->ReadData(
      {id}, base::BindLambdaForTesting(
                [&](const std::optional<ModelError>& error,
                    std::unique_ptr<DataTypeStore::RecordList> data_records,
                    std::unique_ptr<DataTypeStore::IdList> missing_id_list) {
                  EXPECT_THAT(*data_records, SizeIs(Le(1u)));
                  if (data_records->size() == 1) {
                    read_value = data_records->front().value;
                  }
                  loop.Quit();
                }));
  loop.Run();
  return read_value;
}

class DataTypeStoreServiceImplTest : public testing::Test {
 public:
  DataTypeStoreServiceImplTest() {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::internal::kMigrateReadingListFromLocalToAccount, false);
#if BUILDFLAG(IS_ANDROID)
    pref_service_.registry()->RegisterBooleanPref(
        prefs::internal::kWipedWebAPkDataForMigration, false);
#endif  // BUILDFLAG(IS_ANDROID)
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
};

// Regression test for http://crbug.com/1190187.
TEST_F(DataTypeStoreServiceImplTest, ShouldSupportFactoryOutlivingService) {
  auto service = std::make_unique<DataTypeStoreServiceImpl>(
      base::CreateUniqueTempDirectoryScopedToTest(), &pref_service_);

  const RepeatingDataTypeStoreFactory store_factory =
      service->GetStoreFactory();
  ASSERT_TRUE(store_factory);

  // Destroy the service and wait until all backend cleanup work is done.
  service.reset();
  task_environment_.RunUntilIdle();

  // Verify that the factory continues to work, even if it outlives the service.
  EXPECT_THAT(ExerciseStoreFactoryAndWait(store_factory), NotNull());
}

TEST_F(DataTypeStoreServiceImplTest, ShouldUseIsolatedStorageTypes) {
  auto service = std::make_unique<DataTypeStoreServiceImpl>(
      base::CreateUniqueTempDirectoryScopedToTest(), &pref_service_);

  const RepeatingDataTypeStoreFactory default_store_factory =
      service->GetStoreFactory();
  const RepeatingDataTypeStoreFactory account_store_factory =
      service->GetStoreFactoryForAccountStorage();

  ASSERT_TRUE(default_store_factory);
  ASSERT_TRUE(account_store_factory);

  std::unique_ptr<DataTypeStore> default_store =
      ExerciseStoreFactoryAndWait(default_store_factory);
  std::unique_ptr<DataTypeStore> account_store =
      ExerciseStoreFactoryAndWait(account_store_factory);

  ASSERT_THAT(default_store, NotNull());
  ASSERT_THAT(account_store, NotNull());

  WriteDataAndWait(default_store.get(), "key", "A");
  WriteDataAndWait(account_store.get(), "key", "B");

  // Although they share key, the two values should remain independent.
  EXPECT_THAT(ReadDataAndWait(default_store.get(), "key"), Eq("A"));
  EXPECT_THAT(ReadDataAndWait(account_store.get(), "key"), Eq("B"));
}

TEST_F(DataTypeStoreServiceImplTest,
       ShouldTriggerReadingListMigrationIfPrefSet) {
  base::FilePath temp_path = base::CreateUniqueTempDirectoryScopedToTest();
  // Put some data into the default (local) store.
  {
    auto service =
        std::make_unique<DataTypeStoreServiceImpl>(temp_path, &pref_service_);
    const RepeatingDataTypeStoreFactory default_store_factory =
        service->GetStoreFactory();
    std::unique_ptr<DataTypeStore> default_store = ExerciseStoreFactoryAndWait(
        default_store_factory, DataType::READING_LIST);
    WriteDataAndWait(default_store.get(), "key", "data");
  }

  // Recreate the service *without* setting the migration pref. The data should
  // remain unchanged.
  ASSERT_FALSE(pref_service_.GetBoolean(
      prefs::internal::kMigrateReadingListFromLocalToAccount));
  {
    auto service =
        std::make_unique<DataTypeStoreServiceImpl>(temp_path, &pref_service_);

    // The item should still be in the default store.
    const RepeatingDataTypeStoreFactory default_store_factory =
        service->GetStoreFactory();
    std::unique_ptr<DataTypeStore> default_store = ExerciseStoreFactoryAndWait(
        default_store_factory, DataType::READING_LIST);
    EXPECT_THAT(ReadDataAndWait(default_store.get(), "key"), Eq("data"));

    // And not in the account store.
    const RepeatingDataTypeStoreFactory account_store_factory =
        service->GetStoreFactoryForAccountStorage();
    std::unique_ptr<DataTypeStore> account_store = ExerciseStoreFactoryAndWait(
        account_store_factory, DataType::READING_LIST);
    EXPECT_THAT(ReadDataAndWait(account_store.get(), "key"), Eq(std::nullopt));
  }

  // Set the migration pref and recreate the service again. The ReadingList data
  // should get moved to the account store.
  pref_service_.SetBoolean(
      prefs::internal::kMigrateReadingListFromLocalToAccount, true);
  {
    auto service =
        std::make_unique<DataTypeStoreServiceImpl>(temp_path, &pref_service_);

    // The item should not be in the default store anymore.
    const RepeatingDataTypeStoreFactory default_store_factory =
        service->GetStoreFactory();
    std::unique_ptr<DataTypeStore> default_store = ExerciseStoreFactoryAndWait(
        default_store_factory, DataType::READING_LIST);
    EXPECT_THAT(ReadDataAndWait(default_store.get(), "key"), std::nullopt);

    // It should be in the *account* store now.
    const RepeatingDataTypeStoreFactory account_store_factory =
        service->GetStoreFactoryForAccountStorage();
    std::unique_ptr<DataTypeStore> account_store = ExerciseStoreFactoryAndWait(
        account_store_factory, DataType::READING_LIST);
    EXPECT_THAT(ReadDataAndWait(account_store.get(), "key"), Eq("data"));

    // The migration pref should've been reset to false.
    EXPECT_FALSE(pref_service_.GetBoolean(
        prefs::internal::kMigrateReadingListFromLocalToAccount));
  }
}

}  // namespace
}  // namespace syncer
