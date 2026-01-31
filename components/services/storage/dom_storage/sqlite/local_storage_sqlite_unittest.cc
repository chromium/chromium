// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite/local_storage_sqlite.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/sqlite/sqlite_database_utils.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

class LocalStorageSqliteTest : public testing::Test {
 protected:
  LocalStorageSqliteTest();
  ~LocalStorageSqliteTest() override;

  // Creates a path to the SQLite database file under `temp_dir_`.  Creates
  // `temp_dir_` when necessary.
  void GetDatabasePath(base::FilePath* result);

  // Returns the `PassKey` required to create and open `LocalStorageSqlite`.
  base::PassKey<DomStorageDatabaseFactory> GetPassKey();

  void OpenOnDisk(std::unique_ptr<LocalStorageSqlite>* result);

  void OpenInMemory(std::unique_ptr<LocalStorageSqlite>* result);

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

LocalStorageSqliteTest::LocalStorageSqliteTest() {
  scoped_feature_list_.InitAndEnableFeature(kDomStorageSqlite);
}

LocalStorageSqliteTest::~LocalStorageSqliteTest() = default;

void LocalStorageSqliteTest::GetDatabasePath(base::FilePath* result) {
  if (!temp_dir_.IsValid()) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  *result = DomStorageDatabase::GetPath(StorageType::kLocalStorage,
                                        temp_dir_.GetPath());
}

base::PassKey<DomStorageDatabaseFactory> LocalStorageSqliteTest::GetPassKey() {
  return DomStorageDatabaseFactory::CreatePassKeyForTesting();
}

void LocalStorageSqliteTest::OpenOnDisk(
    std::unique_ptr<LocalStorageSqlite>* result) {
  base::FilePath database_path;
  ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));

  std::unique_ptr<LocalStorageSqlite> instance =
      std::make_unique<LocalStorageSqlite>(GetPassKey());

  DbStatus status = instance->Open(GetPassKey(),
                                   /*database_path=*/database_path,
                                   /*memory_dump_id=*/std::nullopt);

  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(instance);
}

void LocalStorageSqliteTest::OpenInMemory(
    std::unique_ptr<LocalStorageSqlite>* result) {
  std::unique_ptr<LocalStorageSqlite> instance =
      std::make_unique<LocalStorageSqlite>(GetPassKey());

  DbStatus status = instance->Open(GetPassKey(),
                                   /*database_path=*/base::FilePath(),
                                   /*memory_dump_id=*/std::nullopt);

  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(instance);
}

TEST_F(LocalStorageSqliteTest, OpenInMemory) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));
}

TEST_F(LocalStorageSqliteTest, OpenThenDestroyOnDisk) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));
  database.reset();

  base::FilePath database_path;
  ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));
  EXPECT_TRUE(base::PathExists(database_path));

  DbStatus status = sqlite::DestroyDatabase(database_path);
  EXPECT_TRUE(status.ok()) << status.ToString();
  EXPECT_FALSE(base::PathExists(database_path));
}

TEST_F(LocalStorageSqliteTest, VersionTooNew) {
  base::FilePath database_path;
  ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));

  // Write the wrong version to the database
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));
  database->PutVersionForTesting(9999);
  database.reset();

  // Opening the database with the wrong version must fail.
  database = std::make_unique<LocalStorageSqlite>(GetPassKey());
  DbStatus status = database->Open(GetPassKey(),
                                   /*database_path=*/database_path,
                                   /*memory_dump_id=*/std::nullopt);
  EXPECT_TRUE(status.IsNotFound());
}

}  // namespace storage
