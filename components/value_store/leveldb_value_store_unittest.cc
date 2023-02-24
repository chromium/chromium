// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/value_store/leveldb_value_store.h"
#include "components/value_store/value_store_test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace value_store {

namespace {

const char kDatabaseUMAClientName[] = "Test";

ValueStore* Param(const base::FilePath& file_path) {
  return new LeveldbValueStore(kDatabaseUMAClientName, file_path);
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(LeveldbValueStore,
                         ValueStoreTestSuite,
                         testing::Values(&Param));

class LeveldbValueStoreUnitTest : public testing::Test {
 public:
  LeveldbValueStoreUnitTest() = default;
  ~LeveldbValueStoreUnitTest() override = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    CreateStore();
    ASSERT_TRUE(store_->Get().status().ok());
  }

  void TearDown() override {
    if (!store_)
      return;
    store_->Clear();
    CloseStore();
  }

  void CloseStore() { store_.reset(); }

  void CreateStore() {
    store_ = std::make_unique<LeveldbValueStore>(kDatabaseUMAClientName,
                                                 database_path());
  }

  LeveldbValueStore* store() { return store_.get(); }
  const base::FilePath& database_path() { return database_dir_.GetPath(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<LeveldbValueStore> store_;
  base::ScopedTempDir database_dir_;
};

// Check that we can restore a single corrupted key in the LeveldbValueStore.
TEST_F(LeveldbValueStoreUnitTest, RestoreKeyTest) {
  const char kNotCorruptKey[] = "not-corrupt";
  const char kValue[] = "value";

  // Insert a valid pair.
  std::unique_ptr<base::Value> value(new base::Value(kValue));
  ASSERT_TRUE(
      store()->Set(ValueStore::DEFAULTS, kNotCorruptKey, *value).status().ok());

  // Insert a corrupt pair.
  const char kCorruptKey[] = "corrupt";
  leveldb::WriteBatch batch;
  batch.Put(kCorruptKey, "[{(.*+\"\'\\");
  ASSERT_TRUE(store()->WriteToDbForTest(&batch));

  // Verify corruption (the first Get will return corruption).
  ValueStore::ReadResult result = store()->Get(kCorruptKey);
  ASSERT_FALSE(result.status().ok());
  ASSERT_EQ(ValueStore::CORRUPTION, result.status().code);

  // Verify restored (was deleted in the first Get).
  result = store()->Get(kCorruptKey);
  EXPECT_TRUE(result.status().ok())
      << "Get result not OK: " << result.status().message;
  EXPECT_TRUE(result.settings().empty());

  // Verify that the valid pair is still present.
  result = store()->Get(kNotCorruptKey);
  EXPECT_TRUE(result.status().ok());
  const std::string* value_string =
      result.settings().FindString(kNotCorruptKey);
  ASSERT_TRUE(value_string);
  EXPECT_EQ(kValue, *value_string);
}

// Test that the Restore() method does not just delete the entire database
// (unless absolutely necessary), and instead only removes corrupted keys.
TEST_F(LeveldbValueStoreUnitTest, RestoreDoesMinimumNecessary) {
  const char* kNotCorruptKeys[] = {"a", "n", "z"};
  const char kCorruptKey1[] = "f";
  const char kCorruptKey2[] = "s";
  const char kValue[] = "value";
  const char kCorruptValue[] = "[{(.*+\"\'\\";

  // Insert a collection of non-corrupted pairs.
  std::unique_ptr<base::Value> value(new base::Value(kValue));
  for (auto* kNotCorruptKey : kNotCorruptKeys) {
    ASSERT_TRUE(store()
                    ->Set(ValueStore::DEFAULTS, kNotCorruptKey, *value)
                    .status()
                    .ok());
  }

  // Insert a few corrupted pairs.
  leveldb::WriteBatch batch;
  batch.Put(kCorruptKey1, kCorruptValue);
  batch.Put(kCorruptKey2, kCorruptValue);
  ASSERT_TRUE(store()->WriteToDbForTest(&batch));

  // Verify that we broke it and that it was repaired by the value store.
  ValueStore::ReadResult result = store()->Get();
  ASSERT_FALSE(result.status().ok());
  ASSERT_EQ(ValueStore::CORRUPTION, result.status().code);
  ASSERT_EQ(ValueStore::VALUE_RESTORE_DELETE_SUCCESS,
            result.status().restore_status);

  // We should still have all valid pairs present in the database.
  std::string* value_string;
  for (auto* kNotCorruptKey : kNotCorruptKeys) {
    result = store()->Get(kNotCorruptKey);
    EXPECT_TRUE(result.status().ok());
    ASSERT_EQ(ValueStore::RESTORE_NONE, result.status().restore_status);
    value_string = result.settings().FindString(kNotCorruptKey);
    ASSERT_TRUE(value_string);
    EXPECT_EQ(kValue, *value_string);
  }
}

// Test that the LeveldbValueStore can recover in the case of a CATastrophic
// failure and we have total corruption. In this case, the database is plagued
// by LolCats.
// Full corruption has been known to happen occasionally in strange edge cases,
// such as after users use Windows Restore. We can't prevent it, but we need to
// be able to handle it smoothly.
TEST_F(LeveldbValueStoreUnitTest, RestoreFullDatabase) {
  const std::string kLolCats("I can haz leveldb filez?");
  const char* kNotCorruptKeys[] = {"a", "n", "z"};
  const char kValue[] = "value";

  // Generate a database.
  std::unique_ptr<base::Value> value(new base::Value(kValue));
  for (auto* kNotCorruptKey : kNotCorruptKeys) {
    ASSERT_TRUE(store()
                    ->Set(ValueStore::DEFAULTS, kNotCorruptKey, *value)
                    .status()
                    .ok());
  }

  // Close it (so we remove the lock), and replace all files with LolCats.
  CloseStore();
  base::FileEnumerator enumerator(database_path(), true /* recursive */,
                                  base::FileEnumerator::FILES);
  for (base::FilePath file = enumerator.Next(); !file.empty();
       file = enumerator.Next()) {
    // WriteFile() failure is a result of -1.
    ASSERT_TRUE(base::WriteFile(file, kLolCats));
  }
  CreateStore();

  // We couldn't recover anything, but we should be in a sane state again.
  ValueStore::ReadResult result = store()->Get();
  ASSERT_EQ(ValueStore::DB_RESTORE_REPAIR_SUCCESS,
            result.status().restore_status);
  EXPECT_TRUE(result.status().ok());
  EXPECT_EQ(0u, result.settings().size());
}

}  // namespace value_store
