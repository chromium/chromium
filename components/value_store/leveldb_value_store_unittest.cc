// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/leveldb_value_store.h"

#include <stddef.h>

#include <memory>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/value_store/value_store_test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace value_store {

namespace {

const char kDatabaseUMAClientName[] = "Test";

// Exposes the protected LazyLevelDb state needed to assert that the underlying
// database is left closed after an unrecoverable corruption.
class TestLeveldbValueStore : public LeveldbValueStore {
 public:
  using LeveldbValueStore::LeveldbValueStore;

  bool HasOpenDb() { return db() != nullptr; }
};

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

// Regression test for a ClusterFuzz null-dereference reached via the public
// LeveldbValueStore::Remove() entry point. This drives the exact crash stack
// from the report: Remove() -> EnsureDbIsOpen() -> Read() -> db_->Get().
//
// If the initial open reports corruption and the recovery reopen then fails,
// EnsureDbIsOpen() must not report success while leaving the database closed.
// Before the fix it returned OK with db_ == nullptr, so Remove() proceeded to
// call Read(), which dereferenced the null db_ and crashed.
TEST_F(LeveldbValueStoreUnitTest, RemoveDoesNotCrashWhenRecoveryReopenFails) {
  // Force the initial open to report corruption, and every subsequent reopen
  // (attempted during corruption recovery) to fail. This drives
  // FixCorruption() down the path where the corrupt db is deleted but the
  // reopen fails, so it reports DB_RESTORE_DELETE_FAILURE and leaves
  // db_ == nullptr; EnsureDbIsOpen() must then not report success.
  int open_call_count = 0;
  leveldb_env::SetDBFactoryForTesting(base::BindLambdaForTesting(
      [&open_call_count](
          const leveldb_env::Options& options, const std::string& name,
          std::unique_ptr<leveldb::DB>* dbptr) -> leveldb::Status {
        if (open_call_count++ == 0) {
          return leveldb::Status::Corruption("injected corruption");
        }
        return leveldb::Status::IOError("injected reopen failure");
      }));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  TestLeveldbValueStore store(kDatabaseUMAClientName, temp_dir.GetPath());

  // Drive the real crash path. On unfixed code this segfaults inside
  // LazyLevelDb::Read() at `db_->Get(...)`; with the fix it returns gracefully.
  ValueStore::WriteResult result = store.Remove("some_key");

  // Reset the override before any later teardown work opens a database.
  leveldb_env::SetDBFactoryForTesting(leveldb_env::DBFactoryMethod());

  // The recovery reopen failed, so the database must still be closed...
  EXPECT_FALSE(store.HasOpenDb());
  // ...and Remove() must surface the failure instead of null-dereferencing db_.
  EXPECT_FALSE(result.status().ok())
      << "Remove() reported success but the database is closed";
}

TEST_F(LeveldbValueStoreUnitTest, GetBytesInUseAndLazyIteratorCalculation) {
  base::test::ScopedFeatureList feature_list(
      kOptimizeValueStoreQuotaEnforcement);
  base::Value val1("test_value");  // JSON: "\"test_value\"" (12 bytes)
  base::Value val2(42);            // JSON: "42" (2 bytes)

  EXPECT_EQ(0u, store()->GetBytesInUse());
  EXPECT_EQ(0u, store()->GetBytesInUse("k1"));

  ASSERT_TRUE(store()->Set(ValueStore::DEFAULTS, "k1", val1).status().ok());
  ASSERT_TRUE(store()->Set(ValueStore::DEFAULTS, "k2", val2).status().ok());

  // "k1" (2) + "\"test_value\"" (12) = 14
  // "k2" (2) + "42" (2) = 4
  EXPECT_EQ(14u, store()->GetBytesInUse("k1"));
  EXPECT_EQ(4u, store()->GetBytesInUse("k2"));
  EXPECT_EQ(18u, store()->GetBytesInUse(std::vector<std::string>{"k1", "k2"}));
  EXPECT_EQ(18u, store()->GetBytesInUse());

  // Reopen the store to verify LazyCalculateUsage computes byte usage directly
  // from the raw LevelDB iterator on disk.
  CloseStore();
  CreateStore();

  EXPECT_EQ(14u, store()->GetBytesInUse("k1"));
  EXPECT_EQ(4u, store()->GetBytesInUse("k2"));
  EXPECT_EQ(18u, store()->GetBytesInUse());

  // Removing a key updates byte usage.
  ASSERT_TRUE(store()->Remove("k1").status().ok());
  EXPECT_EQ(0u, store()->GetBytesInUse("k1"));
  EXPECT_EQ(4u, store()->GetBytesInUse());
}

TEST_F(LeveldbValueStoreUnitTest, QuotaEnforcementSingleAndBatchSet) {
  for (bool optimize_quota : {false, true}) {
    SCOPED_TRACE(optimize_quota);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(kOptimizeValueStoreQuotaEnforcement,
                                      optimize_quota);
    ASSERT_TRUE(store()->Clear().status().ok());

    QuotaEnforcer::Limits limits;
    limits.quota_bytes = 25;
    limits.quota_bytes_per_item = 15;
    limits.max_items = 2;
    store()->SetQuotaLimits(limits);

    // "k1" (2) + "\"val1\"" (6) = 8 bytes.
    base::Value val1("val1");
    EXPECT_TRUE(store()->Set(ValueStore::DEFAULTS, "k1", val1).status().ok());
    EXPECT_EQ(8u, store()->GetBytesInUse());

    // Item exceeding quota_bytes_per_item (15) is rejected without modifying
    // DB.
    base::Value large_item("this_item_is_too_long");
    ValueStore::WriteResult per_item_result =
        store()->Set(ValueStore::DEFAULTS, "k2", large_item);
    EXPECT_FALSE(per_item_result.status().ok());
    EXPECT_EQ(ValueStore::QUOTA_EXCEEDED, per_item_result.status().code);
    EXPECT_EQ(8u, store()->GetBytesInUse());
    EXPECT_TRUE(store()->Get("k2").settings().empty());

    // Second valid item: "k2" (2) + "\"val2\"" (6) = 8 bytes (total 16 <= 25).
    base::Value val2("val2");
    EXPECT_TRUE(store()->Set(ValueStore::DEFAULTS, "k2", val2).status().ok());
    EXPECT_EQ(16u, store()->GetBytesInUse());

    // Third item exceeds max_items (2).
    base::Value val3("v");
    ValueStore::WriteResult max_items_result =
        store()->Set(ValueStore::DEFAULTS, "k3", val3);
    EXPECT_FALSE(max_items_result.status().ok());
    EXPECT_EQ(ValueStore::QUOTA_EXCEEDED, max_items_result.status().code);
    EXPECT_EQ(16u, store()->GetBytesInUse());

    // Updating existing keys to exceed total quota_bytes (25) is rejected.
    base::DictValue batch_over_total;
    batch_over_total.Set("k1", "ten_bytes!");  // 2 + 12 = 14
    batch_over_total.Set("k2", "ten_bytes!");  // 2 + 12 = 14 (total 28 > 25)
    ValueStore::WriteResult total_result =
        store()->Set(ValueStore::DEFAULTS, batch_over_total);
    EXPECT_FALSE(total_result.status().ok());
    EXPECT_EQ(ValueStore::QUOTA_EXCEEDED, total_result.status().code);
    EXPECT_EQ(16u, store()->GetBytesInUse());
    EXPECT_EQ("val1", *store()->Get("k1").settings().FindString("k1"));

    // JSON serialization failure mid-batch discards earlier staged changes.
    base::DictValue batch_unserializable;
    batch_unserializable.Set("k1", "v");
    batch_unserializable.Set("k2", base::Value(base::Value::Type::BINARY));
    EXPECT_EQ(
        ValueStore::OTHER_ERROR,
        store()->Set(ValueStore::DEFAULTS, batch_unserializable).status().code);
    EXPECT_EQ(8u, store()->GetBytesInUse("k1"));
    EXPECT_EQ(16u, store()->GetBytesInUse());

    // Multi-key batch that grows one key ("k1": 8 -> 14) and shrinks another
    // ("k2": 8 -> 5) succeeds because Check() runs after all items are staged.
    base::DictValue batch_grow_and_shrink;
    batch_grow_and_shrink.Set("k1", "ten_bytes!");  // 2 + 12 = 14
    batch_grow_and_shrink.Set("k2", "v");           // 2 + 3 = 5
    EXPECT_TRUE(store()
                    ->Set(ValueStore::DEFAULTS, batch_grow_and_shrink)
                    .status()
                    .ok());
    EXPECT_EQ(19u, store()->GetBytesInUse());

    // IGNORE_QUOTA bypasses limits while still updating tracked byte usage.
    EXPECT_TRUE(
        store()->Set(ValueStore::IGNORE_QUOTA, batch_over_total).status().ok());
    EXPECT_EQ(28u, store()->GetBytesInUse());
  }
}

TEST_F(LeveldbValueStoreUnitTest, QuotaUsageAfterInlineCorruptionRecovery) {
  base::test::ScopedFeatureList feature_list(
      kOptimizeValueStoreQuotaEnforcement);
  store()->SetQuotaLimits(
      {.quota_bytes = 100, .quota_bytes_per_item = 100, .max_items = 2});

  // Insert a valid key: "good" (4) + "\"value\"" (7) = 11 bytes.
  ASSERT_TRUE(store()
                  ->Set(ValueStore::DEFAULTS, "good", base::Value("value"))
                  .status()
                  .ok());

  auto inject_corrupt_key = [&](const std::string& key) {
    leveldb::WriteBatch batch;
    batch.Put(key, "[{(.*+\"\'\\");
    ASSERT_TRUE(store()->WriteToDbForTest(&batch));
  };

  auto expect_corruption_recovery = [&](const std::string& key,
                                        size_t corrupt_bytes, auto&& op) {
    inject_corrupt_key(key);
    EXPECT_EQ(11u + corrupt_bytes, store()->GetBytesInUse());
    auto result = op();
    EXPECT_EQ(ValueStore::CORRUPTION, result.status().code);
    EXPECT_EQ(ValueStore::VALUE_RESTORE_DELETE_SUCCESS,
              result.status().restore_status);
    EXPECT_EQ(0u, store()->GetBytesInUse(key));
    EXPECT_EQ(11u, store()->GetBytesInUse("good"));
    EXPECT_EQ(11u, store()->GetBytesInUse());
  };

  // 1-3. Get(key), Get(keys), and Get() inline corruption recovery.
  expect_corruption_recovery("corrupt", 16u,
                             [&] { return store()->Get("corrupt"); });
  expect_corruption_recovery("corrupt", 16u, [&] {
    return store()->Get(std::vector<std::string>{"good", "corrupt"});
  });
  expect_corruption_recovery("corrupt", 16u, [&] { return store()->Get(); });

  // 4. Multi-key Remove(keys) mid-batch partial corruption: "good" is kept.
  expect_corruption_recovery("corrupt", 16u, [&] {
    return store()->Remove(std::vector<std::string>{"good", "corrupt"});
  });

  // 5. Multi-key Set() inline corruption recovery aborts the WriteBatch and
  // frees the corrupt item slot under max_items = 2.
  expect_corruption_recovery("z_corrupt", 18u, [&] {
    base::DictValue batch;
    batch.Set("good", "longer_value");
    batch.Set("z_corrupt", "fixed");
    return store()->Set(ValueStore::DEFAULTS, batch);
  });
  EXPECT_EQ("value", *store()->Get("good").settings().FindString("good"));
  EXPECT_TRUE(store()
                  ->Set(ValueStore::DEFAULTS, "new_key", base::Value("ok"))
                  .status()
                  .ok());
  ASSERT_TRUE(store()->Remove("new_key").status().ok());

  // 6. Set(NO_GENERATE_CHANGES) skips reading old_value and overwrites
  // directly.
  inject_corrupt_key("corrupt");
  EXPECT_EQ(27u, store()->GetBytesInUse());
  ValueStore::WriteResult no_changes_result = store()->Set(
      ValueStore::NO_GENERATE_CHANGES, "corrupt", base::Value("ok"));
  ASSERT_TRUE(no_changes_result.status().ok());
  EXPECT_TRUE(no_changes_result.changes().empty());
  EXPECT_EQ(11u, store()->GetBytesInUse("corrupt"));
  EXPECT_EQ(22u, store()->GetBytesInUse());

  // 7. Clear() emits removal changes only for valid keys, and will destroy the
  // DB, removing the corrupted entries.
  inject_corrupt_key("a_corrupt");
  inject_corrupt_key("z_corrupt");
  ValueStore::WriteResult clear_result = store()->Clear();
  ASSERT_TRUE(clear_result.status().ok());
  EXPECT_EQ(ValueStore::VALUE_RESTORE_DELETE_SUCCESS,
            clear_result.status().restore_status);
  ASSERT_EQ(2u, clear_result.changes().size());
  EXPECT_EQ("corrupt", clear_result.changes()[0].key);
  EXPECT_EQ("good", clear_result.changes()[1].key);
  EXPECT_EQ(0u, store()->GetBytesInUse());
}

TEST_F(LeveldbValueStoreUnitTest, ClearHealthyLeavesRestoreStatusNone) {
  ASSERT_TRUE(store()
                  ->Set(ValueStore::DEFAULTS, "k1", base::Value("v1"))
                  .status()
                  .ok());

  ValueStore::WriteResult clear_result = store()->Clear();
  ASSERT_TRUE(clear_result.status().ok());
  EXPECT_EQ(ValueStore::RESTORE_NONE, clear_result.status().restore_status);
  EXPECT_TRUE(clear_result.status().message.empty());
  ASSERT_EQ(1u, clear_result.changes().size());
  EXPECT_EQ("k1", clear_result.changes()[0].key);
}

TEST_F(LeveldbValueStoreUnitTest, OverQuotaSetRejectedBeforeReadingFromDb) {
  base::test::ScopedFeatureList feature_list(
      kOptimizeValueStoreQuotaEnforcement);
  store()->SetQuotaLimits(
      {.quota_bytes = 30, .quota_bytes_per_item = 100, .max_items = 2});

  // "k1" (8 bytes) + corrupt "k2" (11 bytes) = 19 bytes across 2 items.
  ASSERT_TRUE(store()
                  ->Set(ValueStore::DEFAULTS, "k1", base::Value("val1"))
                  .status()
                  .ok());
  leveldb::WriteBatch batch;
  batch.Put("k2", "[invalid!");
  ASSERT_TRUE(store()->WriteToDbForTest(&batch));
  ASSERT_EQ(19u, store()->GetBytesInUse());

  // Over-quota single and batch Set() calls must fail with QUOTA_EXCEEDED
  // before AddToBatch() reads corrupt "k2" from LevelDB.
  EXPECT_EQ(ValueStore::QUOTA_EXCEEDED,
            store()
                ->Set(ValueStore::DEFAULTS, "k2",
                      base::Value("0123456789012345678901234"))
                .status()
                .code);

  base::DictValue over_max_items;
  over_max_items.Set("k2", "ok");
  over_max_items.Set("k3", "ok");
  EXPECT_EQ(ValueStore::QUOTA_EXCEEDED,
            store()->Set(ValueStore::DEFAULTS, over_max_items).status().code);

  base::DictValue over_total_bytes;
  over_total_bytes.Set("k1", "ok");
  over_total_bytes.Set("k2", "0123456789012345678901234");
  EXPECT_EQ(ValueStore::QUOTA_EXCEEDED,
            store()->Set(ValueStore::DEFAULTS, over_total_bytes).status().code);
  EXPECT_EQ(19u, store()->GetBytesInUse());
}

}  // namespace value_store
