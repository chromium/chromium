// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_transaction.h"

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scope.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_test_utils.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_iterator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"

namespace content::indexed_db {

namespace {
const size_t kTestingMaxOpenCursors = 3;

class TransactionalLevelDBTransactionTest : public LevelDBScopesTestBase {
 public:
  TransactionalLevelDBTransactionTest() = default;
  TransactionalLevelDBTransactionTest(
      const TransactionalLevelDBTransactionTest&) = delete;
  TransactionalLevelDBTransactionTest& operator=(
      const TransactionalLevelDBTransactionTest&) = delete;
  ~TransactionalLevelDBTransactionTest() override = default;

  void TearDown() override {
    leveldb_database_.reset();
    LevelDBScopesTestBase::TearDown();
  }

 protected:
  void SetupLevelDBDatabase() {
    ASSERT_TRUE(leveldb_);
    std::unique_ptr<LevelDBScopes> scopes_system =
        std::make_unique<LevelDBScopes>(
            metadata_prefix_, kWriteBatchSizeForTesting, leveldb_,
            &lock_manager_,
            base::BindLambdaForTesting(
                [this](leveldb::Status s) { this->failure_status_ = s; }));
    leveldb::Status s = scopes_system->Initialize();
    ASSERT_TRUE(s.ok()) << s.ToString();
    scopes_system->StartRecoveryAndCleanupTasks();
    leveldb_database_ = transactional_leveldb_factory_.CreateLevelDBDatabase(
        leveldb_, std::move(scopes_system),
        base::SequencedTaskRunner::GetCurrentDefault(), kTestingMaxOpenCursors);
  }

  std::vector<PartitionedLock> AcquireLocksSync(
      PartitionedLockManager* lock_manager,
      base::flat_set<PartitionedLockManager::PartitionedLockRequest>
          lock_requests) {
    base::RunLoop loop;
    PartitionedLockHolder locks_receiver;
    lock_manager->AcquireLocks(
        lock_requests, locks_receiver,
        base::BindLambdaForTesting([&loop]() { loop.Quit(); }));
    loop.Run();
    return std::move(locks_receiver.locks);
  }

  // Convenience methods to access the database outside any
  // transaction to cut down on boilerplate around calls.
  void Put(std::string_view key, std::string value) {
    std::string put_value = value;
    leveldb::Status s = leveldb_database_->Put(key, &put_value);
    ASSERT_TRUE(s.ok());
  }

  void Get(std::string_view key, std::string* value, bool* found) {
    leveldb::Status s = leveldb_database_->Get(key, value, found);
    ASSERT_TRUE(s.ok());
  }

  bool Has(std::string_view key) {
    bool found;
    std::string value;
    leveldb::Status s = leveldb_database_->Get(key, &value, &found);
    EXPECT_TRUE(s.ok());
    return found;
  }

  // Convenience wrappers for LevelDBTransaction operations to
  // avoid boilerplate in tests.
  bool TransactionHas(TransactionalLevelDBTransaction* transaction,
                      std::string_view key) {
    std::string value;
    bool found;
    leveldb::Status s = transaction->Get(key, &value, &found);
    EXPECT_TRUE(s.ok());
    return found;
  }

  void TransactionPut(TransactionalLevelDBTransaction* transaction,
                      std::string_view key,
                      const std::string& value) {
    std::string put_value = value;
    leveldb::Status s = transaction->Put(key, &put_value);
    EXPECT_TRUE(s.ok());
  }

  void TransactionRemove(TransactionalLevelDBTransaction* transaction,
                         std::string_view key) {
    leveldb::Status s = transaction->Remove(key);
    ASSERT_TRUE(s.ok());
  }

  int Compare(std::string_view a, std::string_view b) const {
    return leveldb_database_->leveldb_state()->comparator()->Compare(
        leveldb_env::MakeSlice(a), leveldb_env::MakeSlice(b));
  }

  bool KeysEqual(std::string_view a, std::string_view b) const {
    return Compare(a, b) == 0;
  }

  TransactionalLevelDBDatabase* db() { return leveldb_database_.get(); }

  std::unique_ptr<TransactionalLevelDBTransaction> CreateTransaction() {
    return transactional_leveldb_factory_.CreateLevelDBTransaction(
        db(), db()->scopes()->CreateScope(AcquireLocksSync(
                  &lock_manager_, {CreateSimpleSharedLock()})));
  }

  leveldb::Status failure_status_;

 private:
  DefaultTransactionalLevelDBFactory transactional_leveldb_factory_;
  std::unique_ptr<TransactionalLevelDBDatabase> leveldb_database_;
  PartitionedLockManager lock_manager_;
};

TEST_F(TransactionalLevelDBTransactionTest, GetPutDelete) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();
  leveldb::Status status;

  const std::string key("key");
  std::string got_value;

  const std::string value("value");
  Put(key, value);

  bool found = false;
  Get(key, &got_value, &found);
  EXPECT_TRUE(found);
  EXPECT_EQ(Compare(got_value, value), 0);

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();

  status = transaction->Get(key, &got_value, &found);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(found);
  EXPECT_EQ(Compare(got_value, value), 0);

  const std::string added_key("b-added key");
  const std::string added_value("b-added value");
  Put(added_key, added_value);

  Get(added_key, &got_value, &found);
  EXPECT_TRUE(found);
  EXPECT_EQ(Compare(got_value, added_value), 0);

  EXPECT_TRUE(TransactionHas(transaction.get(), added_key));

  const std::string another_key("b-another key");
  const std::string another_value("b-another value");
  TransactionPut(transaction.get(), another_key, another_value);

  status = transaction->Get(another_key, &got_value, &found);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(found);
  EXPECT_EQ(Compare(got_value, another_value), 0);

  TransactionRemove(transaction.get(), another_key);

  status = transaction->Get(another_key, &got_value, &found);
  EXPECT_FALSE(found);
}

TEST_F(TransactionalLevelDBTransactionTest, Iterator) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();
  const std::string key1("b-key1");
  const std::string value1("value1");
  const std::string key2("b-key2");
  const std::string value2("value2");

  Put(key1, value1);
  Put(key2, value2);

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();

  leveldb::Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());

  it->Seek(std::string("b"));

  ASSERT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(it->Key(), key1), 0) << it->Key() << ", " << key1;
  EXPECT_EQ(Compare(it->Value(), value1), 0);

  it->Next();

  ASSERT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(it->Key(), key2), 0);
  EXPECT_EQ(Compare(it->Value(), value2), 0);

  it->Next();

  EXPECT_FALSE(it->IsValid());
}

TEST_F(TransactionalLevelDBTransactionTest, Commit) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();
  const std::string key1("b-key1");
  const std::string key2("b-key2");
  const std::string value1("value1");
  const std::string value2("value2");
  const std::string value3("value3");

  std::string got_value;
  bool found;

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();

  TransactionPut(transaction.get(), key1, value1);
  TransactionPut(transaction.get(), key2, value2);
  TransactionPut(transaction.get(), key2, value3);

  leveldb::Status status = transaction->Commit(/*sync_on_commit=*/false);
  EXPECT_TRUE(status.ok());

  Get(key1, &got_value, &found);
  EXPECT_TRUE(found);
  EXPECT_EQ(value1, got_value);

  Get(key2, &got_value, &found);
  EXPECT_TRUE(found);
  EXPECT_EQ(value3, got_value);
}

TEST_F(TransactionalLevelDBTransactionTest, IterationWithEvictedCursors) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();
  leveldb::Status status;

  Put("b-key1", "value1");
  Put("b-key2", "value2");
  Put("b-key3", "value3");

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();

  std::unique_ptr<TransactionalLevelDBIterator> evicted_normal_location =
      transaction->CreateIterator(status);
  ASSERT_TRUE(status.ok());

  std::unique_ptr<TransactionalLevelDBIterator> evicted_before_start =
      transaction->CreateIterator(status);
  ASSERT_TRUE(status.ok());

  std::unique_ptr<TransactionalLevelDBIterator> evicted_after_end =
      transaction->CreateIterator(status);
  ASSERT_TRUE(status.ok());

  std::unique_ptr<TransactionalLevelDBIterator> it1 =
      transaction->CreateIterator(status);
  ASSERT_TRUE(status.ok());
  std::unique_ptr<TransactionalLevelDBIterator> it2 =
      transaction->CreateIterator(status);
  ASSERT_TRUE(status.ok());
  std::unique_ptr<TransactionalLevelDBIterator> it3 =
      transaction->CreateIterator(status);
  ASSERT_TRUE(status.ok());

  evicted_normal_location->Seek("b-key1");
  evicted_before_start->Seek("b-key1");
  evicted_before_start->Prev();
  evicted_after_end->SeekToLast();
  evicted_after_end->Next();

  EXPECT_FALSE(evicted_before_start->IsValid());
  EXPECT_TRUE(evicted_normal_location->IsValid());
  EXPECT_FALSE(evicted_after_end->IsValid());

  // Nothing is purged, as we just have 3 iterators used.
  EXPECT_FALSE(evicted_normal_location->IsEvicted());
  EXPECT_FALSE(evicted_before_start->IsEvicted());
  EXPECT_FALSE(evicted_after_end->IsEvicted());

  // Should purge all of our earlier iterators.
  status = it1->Seek("b-key1");
  EXPECT_TRUE(status.ok());
  status = it2->Seek("b-key2");
  EXPECT_TRUE(status.ok());
  status = it3->Seek("b-key3");
  EXPECT_TRUE(status.ok());

  EXPECT_TRUE(evicted_normal_location->IsEvicted());
  EXPECT_TRUE(evicted_before_start->IsEvicted());
  EXPECT_TRUE(evicted_after_end->IsEvicted());

  EXPECT_FALSE(evicted_before_start->IsValid());
  EXPECT_TRUE(evicted_normal_location->IsValid());
  EXPECT_FALSE(evicted_after_end->IsValid());

  // Check we don't need to reload for just the key.
  EXPECT_EQ("b-key1", evicted_normal_location->Key());
  EXPECT_TRUE(evicted_normal_location->IsEvicted());

  // Make sure iterators are reloaded correctly.
  EXPECT_TRUE(evicted_normal_location->IsValid());
  EXPECT_EQ("value1", evicted_normal_location->Value());
  // The iterator isn't reloaded because it caches the value.
  EXPECT_TRUE(evicted_normal_location->IsEvicted());
  status = evicted_normal_location->Next();
  EXPECT_TRUE(status.ok());
  EXPECT_FALSE(evicted_normal_location->IsEvicted());
  EXPECT_FALSE(evicted_before_start->IsValid());
  EXPECT_FALSE(evicted_after_end->IsValid());

  // And our |Value()| call purged the earlier iterator.
  EXPECT_TRUE(it1->IsEvicted());
}

TEST_F(TransactionalLevelDBTransactionTest, IteratorReloadingNext) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();
  const std::string key1("b-key1");
  const std::string key2("b-key2");
  const std::string key3("b-key3");
  const std::string key4("b-key4");
  const std::string value("value");

  Put(key1, value);
  Put(key2, value);
  Put(key3, value);

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();
  leveldb::Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());

  it->Seek(std::string("b"));
  ASSERT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(it->Key(), key1), 0) << it->Key() << ", " << key1;

  // Remove key2, so the next key should be key3.
  TransactionRemove(transaction.get(), key2);
  it->Next();

  ASSERT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(it->Key(), key3), 0) << it->Key() << ", " << key3;

  // Add key4.
  TransactionPut(transaction.get(), key4, value);
  it->Next();

  ASSERT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(it->Key(), key4), 0) << it->Key() << ", " << key4;
}

TEST_F(TransactionalLevelDBTransactionTest, IteratorReloadingPrev) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();
  const std::string key1("b-key1");
  const std::string key2("b-key2");
  const std::string key3("b-key3");
  const std::string key4("b-key4");
  const std::string value("value");

  Put(key2, value);
  Put(key3, value);
  Put(key4, value);

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();
  leveldb::Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());

  it->SeekToLast();
  ASSERT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(it->Key(), key4), 0) << it->Key() << ", " << key4;

  // Remove key3, so the prev key should be key2.
  TransactionRemove(transaction.get(), key3);
  it->Prev();

  ASSERT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(it->Key(), key2), 0) << it->Key() << ", " << key2;

  // Add key1.
  TransactionPut(transaction.get(), key1, value);
  it->Prev();

  ASSERT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(it->Key(), key1), 0) << it->Key() << ", " << key1;
}

TEST_F(TransactionalLevelDBTransactionTest, IteratorSkipsScopesMetadata) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();
  const std::string key1("b-key1");
  const std::string pre_key("0");
  const std::string value("value");

  Put(key1, value);

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();
  leveldb::Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());

  // Should skip metadata, and go to key1.
  it->Seek("");
  ASSERT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(it->Key(), key1), 0) << it->Key() << ", " << key1;

  // Should skip metadata, and go to the beginning.
  it->Prev();
  ASSERT_FALSE(it->IsValid());

  TransactionRemove(transaction.get(), key1);
  TransactionPut(transaction.get(), pre_key, value);

  it->SeekToLast();

  ASSERT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(it->Key(), pre_key), 0) << it->Key() << ", " << pre_key;
}

TEST_F(TransactionalLevelDBTransactionTest, IteratorReflectsInitialChanges) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();
  const std::string key1("b-key1");
  const std::string value("value");

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();

  TransactionPut(transaction.get(), key1, value);

  leveldb::Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());

  it->Seek("");
  ASSERT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(it->Key(), key1), 0) << it->Key() << ", " << key1;
}

namespace {
enum RangePrepareMode {
  DataInMemory,
  DataInDatabase,
  DataMixed,
};
}  // namespace

class LevelDBTransactionRangeTest
    : public TransactionalLevelDBTransactionTest,
      public testing::WithParamInterface<RangePrepareMode> {
 public:
  LevelDBTransactionRangeTest() = default;
  LevelDBTransactionRangeTest(const LevelDBTransactionRangeTest&) = delete;
  LevelDBTransactionRangeTest& operator=(const LevelDBTransactionRangeTest&) =
      delete;
  ~LevelDBTransactionRangeTest() override = default;

  void SetUp() override {
    TransactionalLevelDBTransactionTest::SetUp();
    SetUpRealDatabase();
    SetupLevelDBDatabase();

    switch (GetParam()) {
      case DataInMemory:
        transaction_ = CreateTransaction();

        TransactionPut(transaction_.get(), key_before_range_, value_);
        TransactionPut(transaction_.get(), range_start_, value_);
        TransactionPut(transaction_.get(), key_in_range1_, value_);
        TransactionPut(transaction_.get(), key_in_range2_, value_);
        TransactionPut(transaction_.get(), range_end_, value_);
        TransactionPut(transaction_.get(), key_after_range_, value_);
        break;

      case DataInDatabase:
        Put(key_before_range_, value_);
        Put(range_start_, value_);
        Put(key_in_range1_, value_);
        Put(key_in_range2_, value_);
        Put(range_end_, value_);
        Put(key_after_range_, value_);

        transaction_ = CreateTransaction();
        break;

      case DataMixed:
        Put(key_before_range_, value_);
        Put(key_in_range1_, value_);
        Put(range_end_, value_);

        transaction_ = CreateTransaction();

        TransactionPut(transaction_.get(), range_start_, value_);
        TransactionPut(transaction_.get(), key_in_range2_, value_);
        TransactionPut(transaction_.get(), key_after_range_, value_);
        break;
    }
  }

 protected:
  const std::string key_before_range_ = "b1";
  const std::string range_start_ = "b2";
  const std::string key_in_range1_ = "b3";
  const std::string key_in_range2_ = "b4";
  const std::string range_end_ = "b5";
  const std::string key_after_range_ = "b6";
  const std::string value_ = "value";

  std::unique_ptr<TransactionalLevelDBTransaction> transaction_;
};

TEST_P(LevelDBTransactionRangeTest, RemoveRangeUpperClosed) {
  leveldb::Status status;

  status = transaction_->RemoveRange(
      range_start_, range_end_,
      LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive);
  EXPECT_TRUE(status.ok());

  EXPECT_TRUE(TransactionHas(transaction_.get(), key_before_range_));
  EXPECT_FALSE(TransactionHas(transaction_.get(), range_start_));
  EXPECT_FALSE(TransactionHas(transaction_.get(), key_in_range1_));
  EXPECT_FALSE(TransactionHas(transaction_.get(), key_in_range2_));
  EXPECT_FALSE(TransactionHas(transaction_.get(), range_end_));
  EXPECT_TRUE(TransactionHas(transaction_.get(), key_after_range_));

  status = transaction_->Commit(/*sync_on_commit=*/false);
  EXPECT_TRUE(status.ok());

  EXPECT_TRUE(Has(key_before_range_));
  EXPECT_FALSE(Has(range_start_));
  EXPECT_FALSE(Has(key_in_range1_));
  EXPECT_FALSE(Has(key_in_range2_));
  EXPECT_FALSE(Has(range_end_));
  EXPECT_TRUE(Has(key_after_range_));
}

TEST_P(LevelDBTransactionRangeTest, RemoveRangeUpperOpen) {
  leveldb::Status status;

  status = transaction_->RemoveRange(
      range_start_, range_end_,
      LevelDBScopeDeletionMode::kImmediateWithRangeEndExclusive);
  EXPECT_TRUE(status.ok());

  EXPECT_TRUE(TransactionHas(transaction_.get(), key_before_range_));
  EXPECT_FALSE(TransactionHas(transaction_.get(), range_start_));
  EXPECT_FALSE(TransactionHas(transaction_.get(), key_in_range1_));
  EXPECT_FALSE(TransactionHas(transaction_.get(), key_in_range2_));
  EXPECT_TRUE(TransactionHas(transaction_.get(), range_end_));
  EXPECT_TRUE(TransactionHas(transaction_.get(), key_after_range_));

  status = transaction_->Commit(/*sync_on_commit=*/false);
  EXPECT_TRUE(status.ok());

  EXPECT_TRUE(Has(key_before_range_));
  EXPECT_FALSE(Has(range_start_));
  EXPECT_FALSE(Has(key_in_range1_));
  EXPECT_FALSE(Has(key_in_range2_));
  EXPECT_TRUE(Has(range_end_));
  EXPECT_TRUE(Has(key_after_range_));
}

TEST_P(LevelDBTransactionRangeTest, RemoveRangeIteratorRetainsKey) {
  leveldb::Status status;

  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction_->CreateIterator(status);
  ASSERT_TRUE(status.ok());
  status = it->Seek(key_in_range1_);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(key_in_range1_, it->Key()), 0);
  status = it->Next();
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(key_in_range2_, it->Key()), 0);

  status = transaction_->RemoveRange(
      range_start_, range_end_,
      LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive);
  EXPECT_TRUE(status.ok());

  ASSERT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(key_in_range2_, it->Key()), 0)
      << key_in_range2_ << " != " << it->Key();

  status = it->Next();
  EXPECT_TRUE(status.ok());
  ASSERT_TRUE(it->IsValid());
  EXPECT_EQ(Compare(key_after_range_, it->Key()), 0)
      << key_after_range_ << " != " << it->Key();

  status = transaction_->Commit(/*sync_on_commit=*/false);
  EXPECT_TRUE(status.ok());
}

INSTANTIATE_TEST_SUITE_P(LevelDBTransactionRangeTests,
                         LevelDBTransactionRangeTest,
                         ::testing::Values(DataInMemory,
                                           DataInDatabase,
                                           DataMixed));

TEST_F(TransactionalLevelDBTransactionTest, IteratorValueStaysTheSame) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();

  const std::string key1("b-key1");
  const std::string value1("value1");
  const std::string value2("value2");

  Put(key1, value1);

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();
  leveldb::Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());

  s = it->Seek(std::string("b-key1"));
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(s.ok());

  EXPECT_EQ(value1, it->Value());
  TransactionPut(transaction.get(), key1, value2);
  EXPECT_EQ(value1, it->Value());
  it->EvictLevelDBIterator();
  EXPECT_EQ(value1, it->Value());

  s = it->Seek(std::string("b-key1"));
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(value2, it->Value());
}

TEST_F(TransactionalLevelDBTransactionTest, IteratorPutInvalidation) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();

  // This tests edge cases of using 'Put' with an iterator from the same
  // transaction in all combinations of these edge cases:
  // * 'next' value changes, and we go next,
  // * the iterator was detached, and
  // * 'prev' value changes and we go back.

  const std::string key1("b-key1");
  const std::string key2("b-key2");
  const std::string key3("b-key3");
  const std::string value1("value1");
  const std::string value2("value2");

  Put(key1, value1);
  Put(key2, value1);
  Put(key3, value1);

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();
  leveldb::Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());

  s = it->Seek(std::string("b-key1"));
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(s.ok());

  // Change the 'next' value.
  TransactionPut(transaction.get(), key2, value2);
  s = it->Next();
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(KeysEqual(it->Key(), key2)) << it->Key() << ", " << key2;
  EXPECT_EQ(value2, it->Value());

  // Change the 'next' value and Detach.
  TransactionPut(transaction.get(), key3, value2);
  it->EvictLevelDBIterator();
  s = it->Next();
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(KeysEqual(it->Key(), key3)) << it->Key() << ", " << key3;
  EXPECT_EQ(value2, it->Value());

  // Seek past the end.
  s = it->Next();
  EXPECT_TRUE(s.ok());
  ASSERT_FALSE(it->IsValid());

  // Go to the last key (key3).
  s = it->SeekToLast();
  EXPECT_TRUE(s.ok());
  ASSERT_TRUE(it->IsValid());

  // Change the 'prev' value.
  TransactionPut(transaction.get(), key2, value1);
  s = it->Prev();
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(KeysEqual(it->Key(), key2)) << it->Key() << ", " << key2;
  EXPECT_EQ(value1, it->Value());

  // Change the 'prev' value and detach.
  TransactionPut(transaction.get(), key1, value1);
  it->EvictLevelDBIterator();
  s = it->Prev();
  EXPECT_TRUE(s.ok());
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(KeysEqual(it->Key(), key1)) << it->Key() << ", " << key1;
  EXPECT_EQ(value1, it->Value());

  s = it->Prev();
  EXPECT_TRUE(s.ok());
  ASSERT_FALSE(it->IsValid());
}

TEST_F(TransactionalLevelDBTransactionTest, IteratorRemoveInvalidation) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();

  // This tests edge cases of using 'Remove' with an iterator from the same
  // transaction in all combinations of these edge cases:
  // * 'next' key is removed, and we go next
  // * the iterator was detached
  // * 'prev' key was removed we go back

  const std::string key0("b-key0");
  const std::string key1("b-key1");
  const std::string key2("b-key2");
  const std::string key3("b-key3");
  const std::string key4("b-key4");
  const std::string key5("b-key5");
  const std::string value("value");

  Put(key0, value);
  Put(key1, value);
  Put(key2, value);
  Put(key3, value);
  Put(key4, value);
  Put(key5, value);

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();
  leveldb::Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());

  s = it->Seek(std::string("b-key1"));
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(s.ok());

  // Remove the 'next' value.
  TransactionRemove(transaction.get(), key2);
  s = it->Next();
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(KeysEqual(it->Key(), key3)) << it->Key() << ", " << key3;

  // Remove the 'next' value and detach.
  TransactionRemove(transaction.get(), key4);
  it->EvictLevelDBIterator();
  s = it->Next();
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(KeysEqual(it->Key(), key5)) << it->Key() << ", " << key5;

  // Seek past the end.
  s = it->Next();
  EXPECT_TRUE(s.ok());
  ASSERT_FALSE(it->IsValid());

  // Go to the last key (key5).
  s = it->SeekToLast();
  EXPECT_TRUE(s.ok());
  ASSERT_TRUE(it->IsValid());

  // Remove the 'prev' value.
  TransactionRemove(transaction.get(), key3);
  s = it->Prev();
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(KeysEqual(it->Key(), key1)) << it->Key() << ", " << key1;

  // Remove the 'prev' value and detach.
  TransactionRemove(transaction.get(), key1);
  it->EvictLevelDBIterator();
  s = it->Prev();
  EXPECT_TRUE(s.ok());
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(KeysEqual(it->Key(), key0)) << it->Key() << ", " << key0;

  s = it->Prev();
  EXPECT_TRUE(s.ok());
  ASSERT_FALSE(it->IsValid());
}

TEST_F(TransactionalLevelDBTransactionTest, IteratorGoesInvalidAfterRemove) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();

  // Tests that the iterator correctly goes 'next' or 'prev' to invalid past a
  // removed key, and tests that it does the same after being detached.

  const std::string key1("b-key1");
  const std::string key2("b-key2");
  const std::string value("value");

  Put(key1, value);
  Put(key2, value);

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();
  leveldb::Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());

  s = it->Seek(std::string("b-key1"));
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(s.ok());

  // Remove the 'next' value, and the iterator should go to the end.
  TransactionRemove(transaction.get(), key2);
  s = it->Next();
  EXPECT_TRUE(s.ok());
  ASSERT_FALSE(it->IsValid());

  // Put key2 back, seek to it.
  TransactionPut(transaction.get(), key2, value);
  s = it->SeekToLast();
  EXPECT_TRUE(s.ok());
  ASSERT_TRUE(it->IsValid());

  // Remove the 'prev' value.
  TransactionRemove(transaction.get(), key1);
  s = it->Prev();
  EXPECT_TRUE(s.ok());
  ASSERT_FALSE(it->IsValid());

  // Put key1 back, seek to it.
  TransactionPut(transaction.get(), key1, value);
  s = it->Seek(std::string("b-key1"));
  EXPECT_TRUE(s.ok());
  ASSERT_TRUE(it->IsValid());

  // Remove the 'next' value & detach the iterator.
  TransactionRemove(transaction.get(), key2);
  it->EvictLevelDBIterator();
  s = it->Next();
  EXPECT_TRUE(s.ok());
  ASSERT_FALSE(it->IsValid());

  // Put key2 back, seek to it.
  TransactionPut(transaction.get(), key2, value);
  s = it->SeekToLast();
  EXPECT_TRUE(s.ok());
  ASSERT_TRUE(it->IsValid());

  // Remove the 'prev' value and detach the iterator.
  TransactionRemove(transaction.get(), key1);
  it->EvictLevelDBIterator();
  s = it->Prev();
  EXPECT_TRUE(s.ok());
  ASSERT_FALSE(it->IsValid());
}

TEST_F(TransactionalLevelDBTransactionTest,
       IteratorNextAfterRemovingCurrentKey) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();

  // This tests that the iterator reloading logic correctly handles not calling
  // Next when it reloads after the current key was removed.

  const std::string key1("b-key1");
  const std::string key2("b-key2");
  const std::string key3("b-key3");
  const std::string value("value");

  Put(key1, value);
  Put(key2, value);
  Put(key3, value);

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();
  leveldb::Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());

  s = it->Seek(std::string("b-key1"));
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(s.ok());

  // Make sure the iterator is detached, and remove the current key.
  it->EvictLevelDBIterator();
  TransactionRemove(transaction.get(), key1);

  // This call reloads the iterator at key "b-key1".
  s = it->Next();
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(KeysEqual(it->Key(), key2)) << it->Key() << ", " << key2;
}

TEST_F(TransactionalLevelDBTransactionTest,
       IteratorPrevAfterRemovingCurrentKeyAtDatabaseEnd) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();

  // This tests that the iterator reloading logic correctly handles not calling
  // Next when it reloads after the current key was removed.

  const std::string key1("b-key1");
  const std::string key2("b-key2");
  const std::string value("value");

  Put(key1, value);
  Put(key2, value);

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();
  leveldb::Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());

  s = it->Seek(std::string("b-key2"));
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(s.ok());

  // Make sure the iterator is detached, and remove the current key.
  it->EvictLevelDBIterator();
  TransactionRemove(transaction.get(), key2);

  // This call reloads the iterator at key "b-key2", which is now deleted. It
  // should seek to the end of the database instead, which is "b-key1"
  s = it->Prev();
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(KeysEqual(it->Key(), key1)) << it->Key() << ", " << key1;
}

TEST_F(TransactionalLevelDBTransactionTest,
       IteratorPrevAfterRemovingCurrentKeyAtDatabaseStart) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();

  // This tests that the iterator reloading logic correctly handles not calling
  // Next when it reloads after the current key was removed.

  const std::string key1("b-key1");
  const std::string key2("b-key2");
  const std::string value("value");

  Put(key1, value);
  Put(key2, value);

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();
  leveldb::Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());

  s = it->Seek(std::string("b-key1"));
  ASSERT_TRUE(it->IsValid());
  EXPECT_TRUE(s.ok());

  // Make sure the iterator is detached, and remove the current key.
  it->EvictLevelDBIterator();
  TransactionRemove(transaction.get(), key1);

  // This call reloads the iterator at key "b-key1", which is now deleted. Since
  // there is no key before it, it should be invalid.
  s = it->Prev();
  ASSERT_FALSE(it->IsValid());
}

// Exercises the UAF in `TransactionalLevelDBIterator::Seek(other->Key())` at
// the normal cache size (`kTestingMaxOpenCursors = 3`). When the iterator LRU
// cache is full and `source_it` is the least-recently-used active iterator,
// calling `clone_it->Seek(source_it->Key())` evaluates `source_it->Key()`
// (returning a `std::string_view` into `source_it`'s underlying LevelDB buffer)
// before `clone_it->WillUseDBIterator()` inserts `clone_it` into the LRU and
// evicts `source_it`, freeing the buffer backing the `std::string_view`.
TEST_F(TransactionalLevelDBTransactionTest,
       SeekWithKeyFromEvictedIteratorAtNormalCacheSize) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();

  // Use 50-byte keys (whose internal keys are 58 bytes, placing both the
  // 50-byte user key allocation in `AppendInternalKey` and the 58-byte internal
  // key allocation in `Block::Iter::key_` in the 64-byte PartitionAlloc bucket)
  // and compact to an SSTable so that `source_it->Key()` is backed by a
  // heap-allocated buffer owned by `source_it`'s `leveldb::Iterator`.
  const std::string key1("b-long-key-exceeding-sso-buffer-capacity-000000001");
  const std::string key2("b-long-key-exceeding-sso-buffer-capacity-000000002");
  const std::string key3("b-long-key-exceeding-sso-buffer-capacity-000000003");
  const std::string value("value");

  Put(key1, value);
  Put(key2, value);
  Put(key3, value);
  db()->CompactAll();

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();
  leveldb::Status s;

  std::unique_ptr<TransactionalLevelDBIterator> source_it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());
  std::unique_ptr<TransactionalLevelDBIterator> other_it1 =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());
  std::unique_ptr<TransactionalLevelDBIterator> other_it2 =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());

  // Fill the LRU cache (capacity `kTestingMaxOpenCursors = 3`) so that
  // `source_it` is still active (`!IsEvicted()`) in reverse mode (`saved_key_`)
  // and sits at the LRU tail.
  ASSERT_TRUE(source_it->SeekToLast().ok());
  ASSERT_TRUE(source_it->IsValid());
  ASSERT_EQ(source_it->Key(), key3);
  ASSERT_TRUE(other_it1->Seek(key1).ok());
  ASSERT_TRUE(other_it2->Seek(key2).ok());
  ASSERT_FALSE(source_it->IsEvicted());

  // Clone `source_it` the same way `BackingStore::Cursor::CloneIterator` does:
  // `clone_it->Seek(std::string(source_it->Key()))` evicts `source_it` inside
  // `WillUseDBIterator()` before `iterator_->Seek()` consumes the key slice.
  std::unique_ptr<TransactionalLevelDBIterator> clone_it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());
  s = clone_it->Seek(std::string(source_it->Key()));
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(source_it->IsEvicted());
  ASSERT_TRUE(clone_it->IsValid());
  EXPECT_EQ(clone_it->Key(), key3);
}

class TransactionalLevelDBTransactionMemoryConsumerTest
    : public TransactionalLevelDBTransactionTest {
 protected:
  base::test::ScopedFeatureList feature_list_{base::kLRUCacheMemoryConsumer};
  base::TestMemoryConsumerRegistry test_registry_;
};

// Exercises the same scenario when `kLRUCacheMemoryConsumer` is enabled and
// `MemoryCoordinator` sets the memory limit to 0%. Because `LRUCacheBase`
// scales `max_size()` down to 1,
// `clone_it->Seek(std::string(source_it->Key()))` calls `ShrinkToSize(0)` and
// evicts `source_it` even when `source_it` is the only open, most-recently-used
// iterator.
TEST_F(TransactionalLevelDBTransactionMemoryConsumerTest,
       SeekWithKeyFromEvictedIteratorUnderZeroMemoryLimit) {
  SetUpRealDatabase();
  SetupLevelDBDatabase();

  // `NotifyUpdateMemoryLimitAsync` posts to the main thread behind
  // `iterator_lru_`'s async registration task and replies after the limit
  // update task has been queued on the consumer sequence, so `RunUntilQuit()`
  // deterministically waits until `iterator_lru_.max_size()` becomes 1.
  test_registry_.NotifyUpdateMemoryLimitAsync(base::MemoryLimit::FromPercent(0),
                                              task_env_.QuitClosure());
  task_env_.RunUntilQuit();

  const std::string key1("b-long-key-exceeding-sso-buffer-capacity-000000001");
  const std::string value("value");
  Put(key1, value);
  db()->CompactAll();

  std::unique_ptr<TransactionalLevelDBTransaction> transaction =
      CreateTransaction();
  leveldb::Status s;

  // Position `source_it` so it is active and at the MRU front of
  // `iterator_lru_`.
  std::unique_ptr<TransactionalLevelDBIterator> source_it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(source_it->SeekToLast().ok());
  ASSERT_TRUE(source_it->IsValid());
  ASSERT_EQ(source_it->Key(), key1);
  ASSERT_FALSE(source_it->IsEvicted());

  // With `max_size() == 1`, seeking a second iterator immediately evicts the
  // MRU `source_it` during `clone_it->Seek(std::string(source_it->Key()))`.
  std::unique_ptr<TransactionalLevelDBIterator> clone_it =
      transaction->CreateIterator(s);
  ASSERT_TRUE(s.ok());
  s = clone_it->Seek(std::string(source_it->Key()));
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(source_it->IsEvicted());
  ASSERT_TRUE(clone_it->IsValid());
  EXPECT_EQ(clone_it->Key(), key1);
}

}  // namespace
}  // namespace content::indexed_db
