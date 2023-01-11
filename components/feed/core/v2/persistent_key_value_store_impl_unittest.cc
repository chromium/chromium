// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/persistent_key_value_store_impl.h"

#include <map>
#include <set>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/feed/core/proto/v2/keyvalue_store.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/public/persistent_key_value_store.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {
using ::feed::internal::kMaxEntriesInMemory;

int hash_int(int v) {
  return static_cast<int>(base::PersistentHash(base::NumberToString(v)));
}

class PersistentKeyValueStoreTest : public testing::Test {
 public:
  void SetUp() override {
    // Disable automatic cleanup for deterministic testing.
    Config config = GetFeedConfig();
    config.persistent_kv_store_cleanup_interval_in_written_bytes = 0;
    SetFeedConfigForTesting(config);
    MakeStore();
  }

  void TearDown() override {
    if (store_) {
      ASSERT_FALSE(store_->IsTaskRunningForTesting());
    }
    // ProtoDatabase requires PostTask to clean up.
    store_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void MakeStore() {
    store_ = std::make_unique<PersistentKeyValueStoreImpl>(
        leveldb_proto::ProtoDatabaseProvider::GetUniqueDB<feedkvstore::Entry>(
            leveldb_proto::ProtoDbType::FEED_STREAM_DATABASE,
            /*db_dir=*/{}, task_environment_.GetMainThreadTaskRunner()));
  }

  void SetMaxSizeBeforeEviction(int size_in_bytes) {
    Config config = GetFeedConfig();
    config.persistent_kv_store_maximum_size_before_eviction = size_in_bytes;
    SetFeedConfigForTesting(config);
  }

  void Put(const std::string& key, const std::string& value) {
    CallbackReceiver<PersistentKeyValueStore::Result> callback;
    store_->Put(key, value, callback.Bind());
    ASSERT_TRUE(callback.RunAndGetResult().success);
  }

  std::string Get(const std::string& key) {
    CallbackReceiver<PersistentKeyValueStore::Result> callback;
    store_->Get(key, callback.Bind());
    return callback.RunAndGetResult().get_result.value_or("<not-found>");
  }

  std::map<std::string, std::string> GetAllEntries() {
    // Make sure any queued tasks are complete.
    base::RunLoop().RunUntilIdle();
    std::map<std::string, std::string> result;
    auto callback =
        [&](bool ok,
            std::unique_ptr<std::map<std::string, feedkvstore::Entry>> data) {
          CHECK(ok);
          for (auto& entry : *data) {
            result.emplace(entry.first, entry.second.value());
          }
        };
    store_->GetDatabase()->LoadKeysAndEntries(
        base::BindLambdaForTesting(callback));

    base::RunLoop().RunUntilIdle();
    return result;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<PersistentKeyValueStoreImpl> store_;
};

TEST_F(PersistentKeyValueStoreTest, Put) {
  CallbackReceiver<PersistentKeyValueStore::Result> callback;
  store_->Put("x", "y", callback.Bind());

  ASSERT_TRUE(callback.RunAndGetResult().success);
  EXPECT_EQ((std::map<std::string, std::string>{{"x", "y"}}), GetAllEntries());
}

TEST_F(PersistentKeyValueStoreTest, GetEmptyKey) {
  CallbackReceiver<PersistentKeyValueStore::Result> callback;
  store_->Get("", callback.Bind());

  EXPECT_TRUE(callback.RunAndGetResult().success);
  EXPECT_FALSE(callback.GetResult()->get_result);
}

TEST_F(PersistentKeyValueStoreTest, GetKeyNotPresent) {
  CallbackReceiver<PersistentKeyValueStore::Result> callback;
  store_->Get("x", callback.Bind());

  EXPECT_TRUE(callback.RunAndGetResult().success);
  EXPECT_FALSE(callback.GetResult()->get_result);
}

TEST_F(PersistentKeyValueStoreTest, GetKeyPresent) {
  Put("x", "y");

  CallbackReceiver<PersistentKeyValueStore::Result> callback;
  store_->Get("x", callback.Bind());
  EXPECT_TRUE(callback.RunAndGetResult().success);
  EXPECT_EQ("y", callback.RunAndGetResult().get_result);
}

TEST_F(PersistentKeyValueStoreTest, Delete) {
  Put("x", "y");

  CallbackReceiver<PersistentKeyValueStore::Result> callback;
  store_->Delete("x", callback.Bind());
  EXPECT_TRUE(callback.RunAndGetResult().success);
  EXPECT_EQ("<not-found>", Get("x"));
}

TEST_F(PersistentKeyValueStoreTest, DeleteNotPresent) {
  Put("x", "y");

  CallbackReceiver<PersistentKeyValueStore::Result> callback;
  store_->Delete("y", callback.Bind());
  EXPECT_TRUE(callback.RunAndGetResult().success);
  EXPECT_EQ("y", Get("x"));
}

TEST_F(PersistentKeyValueStoreTest, ClearAll) {
  Put("x", "y");
  Put("a", "b");

  CallbackReceiver<PersistentKeyValueStore::Result> callback;
  store_->ClearAll(callback.Bind());
  EXPECT_TRUE(callback.RunAndGetResult().success);

  EXPECT_EQ((std::map<std::string, std::string>{}), GetAllEntries());
}

TEST_F(PersistentKeyValueStoreTest, EvictOldEntriesOnEmptyDatabaseDoesntCrash) {
  CallbackReceiver<PersistentKeyValueStore::Result> callback;
  store_->EvictOldEntries(callback.Bind());
  EXPECT_TRUE(callback.RunAndGetResult().success);
}

TEST_F(PersistentKeyValueStoreTest, EvictOldEntriesBelowSizeLimit) {
  Put("x", "12345");

  // Set config db size limit to equal size of 'x'.
  SetMaxSizeBeforeEviction(5);
  CallbackReceiver<PersistentKeyValueStore::Result> callback;
  store_->EvictOldEntries(callback.Bind());
  EXPECT_TRUE(callback.RunAndGetResult().success);

  EXPECT_EQ((std::map<std::string, std::string>{{"x", "12345"}}),
            GetAllEntries());
}

TEST_F(PersistentKeyValueStoreTest, EvictOldEntriesAboveSizeLimit) {
  Put("x", "12345");

  // Set config db size limit to just below size of 'x'.
  SetMaxSizeBeforeEviction(4);
  CallbackReceiver<PersistentKeyValueStore::Result> callback;
  store_->EvictOldEntries(callback.Bind());
  EXPECT_TRUE(callback.RunAndGetResult().success);

  EXPECT_EQ((std::map<std::string, std::string>{}), GetAllEntries());
}

TEST_F(PersistentKeyValueStoreTest, PutAndGetAreQueuedWhileEvicting) {
  SetMaxSizeBeforeEviction(0);
  std::vector<std::string> calls;
  auto record_call = base::BindLambdaForTesting(
      [&](std::string label, PersistentKeyValueStore::Result) {
        calls.push_back(label);
      });
  store_->Put("x", "12345", base::BindOnce(record_call, "put1"));
  store_->EvictOldEntries(base::BindOnce(record_call, "evict"));
  store_->Put("y", "123456", base::BindOnce(record_call, "put2"));
  std::string get_result = Get("y");

  EXPECT_EQ(std::vector<std::string>({"put1", "evict", "put2"}), calls);
  EXPECT_EQ((std::map<std::string, std::string>{
                {"y", "123456"},
            }),
            GetAllEntries());
  EXPECT_EQ("123456", get_result);
}

TEST_F(PersistentKeyValueStoreTest, EvictOldEntriesDeletesOldEntriesFirst) {
  Put("1", "x");
  task_environment_.FastForwardBy(base::Seconds(1));
  Put("2", "x");

  SetMaxSizeBeforeEviction(1);
  store_->EvictOldEntries(base::DoNothing());

  EXPECT_EQ((std::map<std::string, std::string>{{"2", "x"}}), GetAllEntries());
}

TEST_F(PersistentKeyValueStoreTest,
       EvictOldEntriesDeletesOldEntriesFirstReverseKeys) {
  Put("2", "x");
  task_environment_.FastForwardBy(base::Seconds(1));
  Put("1", "x");

  SetMaxSizeBeforeEviction(1);
  store_->EvictOldEntries(base::DoNothing());

  EXPECT_EQ((std::map<std::string, std::string>{{"1", "x"}}), GetAllEntries());
}

TEST_F(PersistentKeyValueStoreTest, EvictOldEntriesDeleteFutureEntriesFirst) {
  // Insert two entries manually. The second entry has a modification time in
  // the future, so it will be evicted preferentially.
  {
    // Trigger and wait for db initialization.
    Get("foo");

    auto entries_to_save = std::make_unique<
        leveldb_proto::ProtoDatabase<feedkvstore::Entry>::KeyEntryVector>();
    {
      feedkvstore::Entry new_entry;
      new_entry.set_value("1");
      new_entry.set_modification_time(
          (base::Time::Now().ToDeltaSinceWindowsEpoch()).InMilliseconds());
      entries_to_save->emplace_back("key1", std::move(new_entry));
    }
    {
      feedkvstore::Entry new_entry;
      new_entry.set_value("2");
      new_entry.set_modification_time(
          (base::Time::Now().ToDeltaSinceWindowsEpoch() + base::Minutes(1))
              .InMilliseconds());
      entries_to_save->emplace_back("key2", std::move(new_entry));
    }

    CallbackReceiver<bool> callback;
    store_->GetDatabase()->UpdateEntries(
        std::move(entries_to_save),
        /*keys_to_remove=*/std::make_unique<std::vector<std::string>>(),
        callback.Bind());
    ASSERT_TRUE(callback.RunAndGetResult());
  }

  SetMaxSizeBeforeEviction(1);
  store_->EvictOldEntries(base::DoNothing());

  EXPECT_EQ((std::map<std::string, std::string>{{"key1", "1"}}),
            GetAllEntries());
}

TEST_F(PersistentKeyValueStoreTest, EvictOldEntriesManyEntries) {
  const int kFinalEntryCount = kMaxEntriesInMemory * 2;
  const int kInitialEntryCount = kFinalEntryCount + kMaxEntriesInMemory / 2;

  for (int i = 0; i < kInitialEntryCount; ++i) {
    // Make key order different than insertion order.
    int key = hash_int(i);
    Put(base::NumberToString(key), "x");
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  SetMaxSizeBeforeEviction(kFinalEntryCount);
  store_->EvictOldEntries(base::DoNothing());

  std::map<std::string, std::string> want_entries;
  for (int i = kInitialEntryCount - kFinalEntryCount; i < kInitialEntryCount;
       ++i) {
    want_entries[base::NumberToString(hash_int(i))] = "x";
  }
  EXPECT_EQ(want_entries, GetAllEntries());
}

TEST_F(PersistentKeyValueStoreTest, EvictOldEntriesExactlyMaxEntriesInMemory) {
  for (int i = 0; i < kMaxEntriesInMemory; ++i) {
    // Make key order different than insertion order.
    int key = hash_int(i);
    Put(base::NumberToString(key), "x");
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  SetMaxSizeBeforeEviction(kMaxEntriesInMemory - 1);
  store_->EvictOldEntries(base::DoNothing());

  std::map<std::string, std::string> want_entries;
  for (int i = 1; i < kMaxEntriesInMemory; ++i) {
    want_entries[base::NumberToString(hash_int(i))] = "x";
  }
  EXPECT_EQ(want_entries, GetAllEntries());
}

TEST_F(PersistentKeyValueStoreTest, EvictOldEntriesMaxEntriesInMemoryPlusOne) {
  for (int i = 0; i < kMaxEntriesInMemory + 1; ++i) {
    // Make key order different than insertion order.
    int key = hash_int(i);
    Put(base::NumberToString(key), "x");
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  SetMaxSizeBeforeEviction(kMaxEntriesInMemory + 1 - 1);
  store_->EvictOldEntries(base::DoNothing());

  std::map<std::string, std::string> want_entries;
  for (int i = 1; i < kMaxEntriesInMemory + 1; ++i) {
    want_entries[base::NumberToString(hash_int(i))] = "x";
  }
  EXPECT_EQ(want_entries, GetAllEntries());
}

void CallAfterNPostTasks(int post_task_count, base::OnceClosure done) {
  if (post_task_count == 0) {
    std::move(done).Run();
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(base::BindOnce(&CallAfterNPostTasks, post_task_count - 1,
                                      std::move(done))));
  }
}

// Test that `EvictOldEntries()` completes without crashing, even when the
// store is deleted between posted tasks.
TEST_F(PersistentKeyValueStoreTest, DeleteStoreWhileEvictOldEntriesIsRunning) {
  SetMaxSizeBeforeEviction(kMaxEntriesInMemory + 1);

  constexpr int kMaxPostTasks = 32;  // Today, must be at least 16.
  for (int post_tasks_before_delete = 0;
       post_tasks_before_delete < kMaxPostTasks; ++post_tasks_before_delete) {
    MakeStore();
    for (int i = 0; i < kMaxEntriesInMemory + 1; ++i) {
      Put(base::NumberToString(i), "x");
      task_environment_.FastForwardBy(base::Seconds(1));
    }
    // Call EvictOldEntries(), and then eventually delete the store while
    // EvictOldEntries() is running. If EvictOldEntries() completes first,
    // then exit the loop because we've tried all possible orderings.
    base::RunLoop run_loop;
    bool evict_complete = false, delete_complete = false;
    bool evict_complete_first = false;
    auto complete_func = [&](bool is_evict_call) {
      evict_complete |= is_evict_call;
      delete_complete |= !is_evict_call;
      if (evict_complete && delete_complete) {
        evict_complete_first = !is_evict_call;
        run_loop.QuitClosure().Run();
      }
    };
    store_->EvictOldEntries(base::BindLambdaForTesting(
        [&](PersistentKeyValueStore::Result) { complete_func(true); }));
    CallAfterNPostTasks(post_tasks_before_delete,
                        base::BindLambdaForTesting([&]() {
                          store_.reset();
                          complete_func(false);
                        }));
    run_loop.RunUntilIdle();
    if (evict_complete_first) {
      ASSERT_GT(post_tasks_before_delete, 2)
          << "EvictOldEntries completed with fewer post tasks than expected";
      return;
    }
  }
  ASSERT_TRUE(false)
      << "EvictOldEntries didn't complete after kMaxPostTasks post tasks?";
}

TEST_F(PersistentKeyValueStoreTest, DataStoreCleansOldDataAutomatically) {
  // Simulate use of the store by inserting 10 byte entries. On average, we
  // should perform eviction on every 10 Put() calls -- with a 1/10 chance on
  // each call. We have a negligible probability of ~1.0e-46 of failing to run
  // eviction after 1000 iterations.
  Config config = GetFeedConfig();
  config.persistent_kv_store_cleanup_interval_in_written_bytes = 100;
  config.persistent_kv_store_maximum_size_before_eviction = 10;
  SetFeedConfigForTesting(config);
  MakeStore();

  for (int i = 0;; ++i) {
    ASSERT_LT(i, 1000);
    Put(base::NumberToString(i), "1234567890");
    task_environment_.FastForwardBy(base::Seconds(1));
    if (Get("0") == "<not-found>")
      break;
  }
}

}  // namespace
}  // namespace feed
