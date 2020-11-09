// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/storage_area_impl.h"

#include "base/atomic_ref_count.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/storage_area_test_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

using test::MakeGetAllCallback;
using test::MakeSuccessCallback;
using CacheMode = StorageAreaImpl::CacheMode;

const char* kTestSource = "source";
const size_t kTestSizeLimit = 512;

std::string ToString(const std::vector<uint8_t>& input) {
  return std::string(input.begin(), input.end());
}

std::vector<uint8_t> ToBytes(base::StringPiece input) {
  return std::vector<uint8_t>(input.begin(), input.end());
}

class BarrierBuilder {
 public:
  class ContinuationRef : public base::RefCountedThreadSafe<ContinuationRef> {
   public:
    explicit ContinuationRef(base::OnceClosure continuation)
        : continuation_(std::move(continuation)) {}

   private:
    friend class base::RefCountedThreadSafe<ContinuationRef>;
    ~ContinuationRef() = default;

    base::ScopedClosureRunner continuation_;
  };

  explicit BarrierBuilder(base::OnceClosure continuation)
      : continuation_(
            base::MakeRefCounted<ContinuationRef>(std::move(continuation))) {}

  base::OnceClosure AddClosure() {
    return base::BindOnce([](scoped_refptr<ContinuationRef>) {}, continuation_);
  }

 private:
  const scoped_refptr<ContinuationRef> continuation_;

  DISALLOW_COPY_AND_ASSIGN(BarrierBuilder);
};

class MockDelegate : public StorageAreaImpl::Delegate {
 public:
  MockDelegate() {}
  ~MockDelegate() override {}

  void OnNoBindings() override {}
  void DidCommit(leveldb::Status status) override {
    if (!status.ok())
      LOG(ERROR) << "error committing!";
    if (committed_)
      std::move(committed_).Run();
  }
  void OnMapLoaded(leveldb::Status) override { map_load_count_++; }
  std::vector<StorageAreaImpl::Change> FixUpData(
      const StorageAreaImpl::ValueMap& data) override {
    return std::move(mock_changes_);
  }

  int map_load_count() const { return map_load_count_; }
  void set_mock_changes(std::vector<StorageAreaImpl::Change> changes) {
    mock_changes_ = std::move(changes);
  }

  void SetDidCommitCallback(base::OnceClosure committed) {
    committed_ = std::move(committed);
  }

 private:
  int map_load_count_ = 0;
  std::vector<StorageAreaImpl::Change> mock_changes_;
  base::OnceClosure committed_;
};

void GetCallback(base::OnceClosure callback,
                 bool* success_out,
                 std::vector<uint8_t>* value_out,
                 bool success,
                 const std::vector<uint8_t>& value) {
  *success_out = success;
  *value_out = value;
  std::move(callback).Run();
}

base::OnceCallback<void(bool, const std::vector<uint8_t>&)> MakeGetCallback(
    base::OnceClosure callback,
    bool* success_out,
    std::vector<uint8_t>* value_out) {
  return base::BindOnce(&GetCallback, std::move(callback), success_out,
                        value_out);
}

StorageAreaImpl::Options GetDefaultTestingOptions(CacheMode cache_mode) {
  StorageAreaImpl::Options options;
  options.max_size = kTestSizeLimit;
  options.default_commit_delay = base::TimeDelta::FromSeconds(5);
  options.max_bytes_per_hour = 10 * 1024 * 1024;
  options.max_commits_per_hour = 60;
  options.cache_mode = cache_mode;
  return options;
}

}  // namespace

class StorageAreaImplTest : public testing::Test,
                            public blink::mojom::StorageAreaObserver {
 public:
  struct Observation {
    enum { kChange, kChangeFailed, kDelete, kDeleteAll, kSendOldValue } type;
    std::string key;
    base::Optional<std::string> old_value;
    std::string new_value;
    std::string source;
    bool should_send_old_value;
  };

  StorageAreaImplTest() {
    base::RunLoop loop;
    db_ = AsyncDomStorageDatabase::OpenInMemory(
        base::nullopt, "StorageAreaImplTest",
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
        base::BindLambdaForTesting(
            [&](leveldb::Status status) { loop.Quit(); }));
    loop.Run();

    StorageAreaImpl::Options options =
        GetDefaultTestingOptions(CacheMode::KEYS_ONLY_WHEN_POSSIBLE);
    storage_area_ = std::make_unique<StorageAreaImpl>(db_.get(), test_prefix_,
                                                      &delegate_, options);

    SetDatabaseEntry(test_prefix_ + test_key1_, test_value1_);
    SetDatabaseEntry(test_prefix_ + test_key2_, test_value2_);
    SetDatabaseEntry("123", "baddata");

    storage_area_->Bind(storage_area_remote_.BindNewPipeAndPassReceiver());
    storage_area_remote_->AddObserver(
        observer_receiver_.BindNewPipeAndPassRemote());
  }

  ~StorageAreaImplTest() override { task_environment_.RunUntilIdle(); }

  void SetDatabaseEntry(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& value) {
    base::RunLoop loop;
    db_->database().PostTaskWithThisObject(
        FROM_HERE,
        base::BindLambdaForTesting([&](const DomStorageDatabase& db) {
          ASSERT_TRUE(db.Put(key, value).ok());
          loop.Quit();
        }));
    loop.Run();
  }

  void SetDatabaseEntry(base::StringPiece key, base::StringPiece value) {
    SetDatabaseEntry(ToBytes(key), ToBytes(value));
  }

  std::string GetDatabaseEntry(base::StringPiece key) {
    std::vector<uint8_t> value;
    base::RunLoop loop;
    db_->database().PostTaskWithThisObject(
        FROM_HERE,
        base::BindLambdaForTesting([&](const DomStorageDatabase& db) {
          ASSERT_TRUE(db.Get(ToBytes(key), &value).ok());
          loop.Quit();
        }));
    loop.Run();
    return std::string(value.begin(), value.end());
  }

  bool HasDatabaseEntry(base::StringPiece key) {
    base::RunLoop loop;
    leveldb::Status status;
    db_->database().PostTaskWithThisObject(
        FROM_HERE,
        base::BindLambdaForTesting([&](const DomStorageDatabase& db) {
          std::vector<uint8_t> value;
          status = db.Get(ToBytes(key), &value);
          loop.Quit();
        }));
    loop.Run();
    return status.ok();
  }

  void ClearDatabase() {
    base::RunLoop loop;
    db_->database().PostTaskWithThisObject(
        FROM_HERE,
        base::BindLambdaForTesting([&](const DomStorageDatabase& db) {
          leveldb::WriteBatch batch;
          ASSERT_TRUE(db.DeletePrefixed({}, &batch).ok());
          ASSERT_TRUE(db.Commit(&batch).ok());
          loop.Quit();
        }));
    loop.Run();
  }

  blink::mojom::StorageArea* storage_area() {
    return storage_area_remote_.get();
  }
  StorageAreaImpl* storage_area_impl() { return storage_area_.get(); }

  void FlushAreaBinding() { storage_area_remote_.FlushForTesting(); }

  bool GetSync(blink::mojom::StorageArea* area,
               const std::vector<uint8_t>& key,
               std::vector<uint8_t>* result) {
    bool success = false;
    base::RunLoop loop;
    area->Get(key, MakeGetCallback(loop.QuitClosure(), &success, result));
    loop.Run();
    return success;
  }

  bool DeleteSync(
      blink::mojom::StorageArea* area,
      const std::vector<uint8_t>& key,
      const base::Optional<std::vector<uint8_t>>& client_old_value) {
    return test::DeleteSync(area, key, client_old_value, test_source_);
  }

  bool DeleteAllSync(blink::mojom::StorageArea* area) {
    return test::DeleteAllSync(area, test_source_);
  }

  bool GetSync(const std::vector<uint8_t>& key, std::vector<uint8_t>* result) {
    return GetSync(storage_area(), key, result);
  }

  bool PutSync(const std::vector<uint8_t>& key,
               const std::vector<uint8_t>& value,
               const base::Optional<std::vector<uint8_t>>& client_old_value,
               std::string source = kTestSource) {
    return test::PutSync(storage_area(), key, value, client_old_value, source);
  }

  bool DeleteSync(
      const std::vector<uint8_t>& key,
      const base::Optional<std::vector<uint8_t>>& client_old_value) {
    return DeleteSync(storage_area(), key, client_old_value);
  }

  bool DeleteAllSync() { return DeleteAllSync(storage_area()); }

  std::string GetSyncStrUsingGetAll(StorageAreaImpl* area_impl,
                                    const std::string& key) {
    std::vector<blink::mojom::KeyValuePtr> data;
    bool success = test::GetAllSync(area_impl, &data);

    if (!success)
      return "";

    for (const auto& key_value : data) {
      if (key_value->key == ToBytes(key)) {
        return ToString(key_value->value);
      }
    }
    return "";
  }

  void BlockingCommit() { BlockingCommit(&delegate_, storage_area_.get()); }

  void BlockingCommit(MockDelegate* delegate, StorageAreaImpl* area) {
    while (area->has_pending_load_tasks() || area->has_changes_to_commit()) {
      base::RunLoop loop;
      delegate->SetDidCommitCallback(loop.QuitClosure());
      area->ScheduleImmediateCommit();
      loop.Run();
    }
  }

  const std::vector<Observation>& observations() { return observations_; }

  MockDelegate* delegate() { return &delegate_; }
  AsyncDomStorageDatabase* database() { return db_.get(); }

  void should_record_send_old_value_observations(bool value) {
    should_record_send_old_value_observations_ = value;
  }

 protected:
  const std::string test_prefix_ = "abc";
  const std::string test_key1_ = "def";
  const std::string test_key2_ = "123";
  const std::string test_value1_ = "defdata";
  const std::string test_value2_ = "123data";
  const std::string test_copy_prefix1_ = "www";
  const std::string test_copy_prefix2_ = "xxx";
  const std::string test_copy_prefix3_ = "yyy";
  const std::string test_source_ = kTestSource;

  const std::vector<uint8_t> test_prefix_bytes_ = ToBytes(test_prefix_);
  const std::vector<uint8_t> test_key1_bytes_ = ToBytes(test_key1_);
  const std::vector<uint8_t> test_key2_bytes_ = ToBytes(test_key2_);
  const std::vector<uint8_t> test_value1_bytes_ = ToBytes(test_value1_);
  const std::vector<uint8_t> test_value2_bytes_ = ToBytes(test_value2_);

 private:
  // blink::mojom::StorageAreaObserver:
  void KeyChanged(const std::vector<uint8_t>& key,
                  const std::vector<uint8_t>& new_value,
                  const base::Optional<std::vector<uint8_t>>& old_value,
                  const std::string& source) override {
    base::Optional<std::string> optional_old_value;
    if (old_value)
      optional_old_value = ToString(*old_value);
    observations_.push_back({Observation::kChange, ToString(key),
                             optional_old_value, ToString(new_value), source,
                             false});
  }
  void KeyChangeFailed(const std::vector<uint8_t>& key,
                       const std::string& source) override {
    observations_.push_back(
        {Observation::kChangeFailed, ToString(key), "", "", source, false});
  }
  void KeyDeleted(const std::vector<uint8_t>& key,
                  const base::Optional<std::vector<uint8_t>>& old_value,
                  const std::string& source) override {
    base::Optional<std::string> optional_old_value;
    if (old_value)
      optional_old_value = ToString(*old_value);
    observations_.push_back({Observation::kDelete, ToString(key),
                             optional_old_value, "", source, false});
  }
  void AllDeleted(bool was_nonempty, const std::string& source) override {
    observations_.push_back(
        {Observation::kDeleteAll, "", "", "", source, false});
  }
  void ShouldSendOldValueOnMutations(bool value) override {
    if (should_record_send_old_value_observations_) {
      observations_.push_back(
          {Observation::kSendOldValue, "", "", "", "", value});
    }
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AsyncDomStorageDatabase> db_;
  MockDelegate delegate_;
  std::unique_ptr<StorageAreaImpl> storage_area_;
  mojo::Remote<blink::mojom::StorageArea> storage_area_remote_;
  mojo::Receiver<blink::mojom::StorageAreaObserver> observer_receiver_{this};
  std::vector<Observation> observations_;
  bool should_record_send_old_value_observations_ = false;
};

class StorageAreaImplParamTest : public StorageAreaImplTest,
                                 public testing::WithParamInterface<CacheMode> {
 public:
  StorageAreaImplParamTest() {}
  ~StorageAreaImplParamTest() override {}
};

INSTANTIATE_TEST_SUITE_P(StorageAreaImplTest,
                         StorageAreaImplParamTest,
                         testing::Values(CacheMode::KEYS_ONLY_WHEN_POSSIBLE,
                                         CacheMode::KEYS_AND_VALUES));

TEST_F(StorageAreaImplTest, GetLoadedFromMap) {
  storage_area_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  std::vector<uint8_t> result;
  EXPECT_TRUE(GetSync(test_key2_bytes_, &result));
  EXPECT_EQ(test_value2_bytes_, result);

  EXPECT_FALSE(GetSync(ToBytes("x"), &result));
}

TEST_F(StorageAreaImplTest, NoDataCallsOnMapLoaded) {
  StorageAreaImpl::Options options =
      GetDefaultTestingOptions(CacheMode::KEYS_ONLY_WHEN_POSSIBLE);
  // Load an area that has no data inside, so the result will be empty and the
  // migration code is triggered.
  auto area = std::make_unique<StorageAreaImpl>(database(), test_copy_prefix2_,
                                                delegate(), options);
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_TRUE(data.empty());
  EXPECT_EQ(1, delegate()->map_load_count());
}

TEST_F(StorageAreaImplTest, GetFromPutOverwrite) {
  storage_area_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  std::vector<uint8_t> key = test_key2_bytes_;
  std::vector<uint8_t> value = ToBytes("foo");

  base::RunLoop loop;
  bool put_success = false;
  std::vector<uint8_t> result;
  bool get_success = false;
  {
    BarrierBuilder barrier(loop.QuitClosure());
    storage_area()->Put(
        key, value, test_value2_bytes_, test_source_,
        MakeSuccessCallback(barrier.AddClosure(), &put_success));
    storage_area()->Get(
        key, MakeGetCallback(barrier.AddClosure(), &get_success, &result));
  }

  loop.Run();
  EXPECT_TRUE(put_success);
  EXPECT_TRUE(get_success);

  EXPECT_EQ(value, result);
}

TEST_F(StorageAreaImplTest, GetFromPutNewKey) {
  storage_area_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  std::vector<uint8_t> key = ToBytes("newkey");
  std::vector<uint8_t> value = ToBytes("foo");

  EXPECT_TRUE(PutSync(key, value, base::nullopt));

  std::vector<uint8_t> result;
  EXPECT_TRUE(GetSync(key, &result));
  EXPECT_EQ(value, result);
}

TEST_F(StorageAreaImplTest, PutLoadsValuesAfterCacheModeUpgrade) {
  std::vector<uint8_t> key = ToBytes("newkey");
  std::vector<uint8_t> value1 = ToBytes("foo");
  std::vector<uint8_t> value2 = ToBytes("bar");

  ASSERT_EQ(CacheMode::KEYS_ONLY_WHEN_POSSIBLE,
            storage_area_impl()->cache_mode());

  // Do a put to load the key-only cache.
  EXPECT_TRUE(PutSync(key, value1, base::nullopt));
  BlockingCommit();
  EXPECT_EQ(StorageAreaImpl::MapState::LOADED_KEYS_ONLY,
            storage_area_impl()->map_state_);

  // Change cache mode.
  storage_area_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  // Loading new map isn't necessary yet.
  EXPECT_EQ(StorageAreaImpl::MapState::LOADED_KEYS_ONLY,
            storage_area_impl()->map_state_);

  // Do another put and check that the map has been upgraded
  EXPECT_TRUE(PutSync(key, value2, value1));
  EXPECT_EQ(StorageAreaImpl::MapState::LOADED_KEYS_AND_VALUES,
            storage_area_impl()->map_state_);
}

TEST_P(StorageAreaImplParamTest, GetAll) {
  storage_area_impl()->SetCacheModeForTesting(GetParam());

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(storage_area(), &data));
  EXPECT_EQ(2u, data.size());
}

TEST_P(StorageAreaImplParamTest, CommitPutToDB) {
  storage_area_impl()->SetCacheModeForTesting(GetParam());
  std::string key1 = test_key2_;
  std::string value1 = "foo";
  std::string key2 = test_prefix_;
  std::string value2 = "data abc";

  base::RunLoop loop;
  bool put_success1 = false;
  bool put_success2 = false;
  bool put_success3 = false;
  {
    BarrierBuilder barrier(loop.QuitClosure());

    storage_area()->Put(
        ToBytes(key1), ToBytes(value1), test_value2_bytes_, test_source_,
        MakeSuccessCallback(barrier.AddClosure(), &put_success1));
    storage_area()->Put(
        ToBytes(key2), ToBytes("old value"), base::nullopt, test_source_,
        MakeSuccessCallback(barrier.AddClosure(), &put_success2));
    storage_area()->Put(
        ToBytes(key2), ToBytes(value2), ToBytes("old value"), test_source_,
        MakeSuccessCallback(barrier.AddClosure(), &put_success3));
  }

  loop.Run();
  EXPECT_TRUE(put_success1);
  EXPECT_TRUE(put_success2);
  EXPECT_TRUE(put_success3);

  EXPECT_FALSE(HasDatabaseEntry(test_prefix_ + key2));

  BlockingCommit();
  EXPECT_TRUE(HasDatabaseEntry(test_prefix_ + key1));
  EXPECT_EQ(value1, GetDatabaseEntry(test_prefix_ + key1));
  EXPECT_TRUE(HasDatabaseEntry(test_prefix_ + key2));
  EXPECT_EQ(value2, GetDatabaseEntry(test_prefix_ + key2));
}

TEST_P(StorageAreaImplParamTest, PutObservations) {
  storage_area_impl()->SetCacheModeForTesting(GetParam());
  std::string key = "new_key";
  std::string value1 = "foo";
  std::string value2 = "data abc";
  std::string source1 = "source1";
  std::string source2 = "source2";

  EXPECT_TRUE(PutSync(ToBytes(key), ToBytes(value1), base::nullopt, source1));
  ASSERT_EQ(1u, observations().size());
  EXPECT_EQ(Observation::kChange, observations()[0].type);
  EXPECT_EQ(key, observations()[0].key);
  EXPECT_EQ(value1, observations()[0].new_value);
  EXPECT_EQ(base::nullopt, observations()[0].old_value);
  EXPECT_EQ(source1, observations()[0].source);

  EXPECT_TRUE(PutSync(ToBytes(key), ToBytes(value2), ToBytes(value1), source2));
  ASSERT_EQ(2u, observations().size());
  EXPECT_EQ(Observation::kChange, observations()[1].type);
  EXPECT_EQ(key, observations()[1].key);
  EXPECT_EQ(value1, *observations()[1].old_value);
  EXPECT_EQ(value2, observations()[1].new_value);
  EXPECT_EQ(source2, observations()[1].source);

  // Same put should cause another observation.
  EXPECT_TRUE(PutSync(ToBytes(key), ToBytes(value2), ToBytes(value2), source2));
  ASSERT_EQ(3u, observations().size());
  EXPECT_EQ(Observation::kChange, observations()[2].type);
  EXPECT_EQ(key, observations()[2].key);
  EXPECT_EQ(value2, *observations()[2].old_value);
  EXPECT_EQ(value2, observations()[2].new_value);
  EXPECT_EQ(source2, observations()[2].source);
}

TEST_P(StorageAreaImplParamTest, DeleteNonExistingKey) {
  storage_area_impl()->SetCacheModeForTesting(GetParam());
  EXPECT_TRUE(DeleteSync(ToBytes("doesn't exist"), std::vector<uint8_t>()));
  EXPECT_EQ(1u, observations().size());
}

TEST_P(StorageAreaImplParamTest, DeleteExistingKey) {
  storage_area_impl()->SetCacheModeForTesting(GetParam());
  std::string key = "newkey";
  std::string value = "foo";
  SetDatabaseEntry(test_prefix_ + key, value);

  EXPECT_TRUE(DeleteSync(ToBytes(key), ToBytes(value)));
  ASSERT_EQ(1u, observations().size());
  EXPECT_EQ(Observation::kDelete, observations()[0].type);
  EXPECT_EQ(key, observations()[0].key);
  EXPECT_EQ(value, *observations()[0].old_value);
  EXPECT_EQ(test_source_, observations()[0].source);

  EXPECT_TRUE(HasDatabaseEntry(test_prefix_ + key));

  BlockingCommit();
  EXPECT_FALSE(HasDatabaseEntry(test_prefix_ + key));
}

TEST_P(StorageAreaImplParamTest, DeleteAllWithoutLoadedMap) {
  storage_area_impl()->SetCacheModeForTesting(GetParam());
  std::string key = "newkey";
  std::string value = "foo";
  std::string dummy_key = "foobar";
  SetDatabaseEntry(dummy_key, value);
  SetDatabaseEntry(test_prefix_ + key, value);

  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(1u, observations().size());
  EXPECT_EQ(Observation::kDeleteAll, observations()[0].type);
  EXPECT_EQ(test_source_, observations()[0].source);

  EXPECT_TRUE(HasDatabaseEntry(test_prefix_ + key));
  EXPECT_TRUE(HasDatabaseEntry(dummy_key));

  BlockingCommit();
  EXPECT_FALSE(HasDatabaseEntry(test_prefix_ + key));
  EXPECT_TRUE(HasDatabaseEntry(dummy_key));

  // Deleting all again should still work, and still cause an observation.
  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(2u, observations().size());
  EXPECT_EQ(Observation::kDeleteAll, observations()[1].type);
  EXPECT_EQ(test_source_, observations()[1].source);

  // And now we've deleted all, writing something the quota size should work.
  EXPECT_TRUE(PutSync(std::vector<uint8_t>(kTestSizeLimit, 'b'),
                      std::vector<uint8_t>(), base::nullopt));
}

TEST_P(StorageAreaImplParamTest, DeleteAllWithLoadedMap) {
  storage_area_impl()->SetCacheModeForTesting(GetParam());
  std::string key = "newkey";
  std::string value = "foo";
  std::string dummy_key = "foobar";
  SetDatabaseEntry(dummy_key, value);

  EXPECT_TRUE(PutSync(ToBytes(key), ToBytes(value), base::nullopt));

  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(2u, observations().size());
  EXPECT_EQ(Observation::kDeleteAll, observations()[1].type);
  EXPECT_EQ(test_source_, observations()[1].source);

  EXPECT_TRUE(HasDatabaseEntry(dummy_key));

  BlockingCommit();
  EXPECT_FALSE(HasDatabaseEntry(test_prefix_ + key));
  EXPECT_TRUE(HasDatabaseEntry(dummy_key));
}

TEST_P(StorageAreaImplParamTest, DeleteAllWithPendingMapLoad) {
  storage_area_impl()->SetCacheModeForTesting(GetParam());
  std::string key = "newkey";
  std::string value = "foo";
  std::string dummy_key = "foobar";
  SetDatabaseEntry(dummy_key, value);

  storage_area()->Put(ToBytes(key), ToBytes(value), base::nullopt, kTestSource,
                      base::DoNothing());

  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(2u, observations().size());
  EXPECT_EQ(Observation::kDeleteAll, observations()[1].type);
  EXPECT_EQ(test_source_, observations()[1].source);

  EXPECT_TRUE(HasDatabaseEntry(dummy_key));

  BlockingCommit();
  EXPECT_FALSE(HasDatabaseEntry(test_prefix_ + key));
  EXPECT_TRUE(HasDatabaseEntry(dummy_key));
}

TEST_P(StorageAreaImplParamTest, DeleteAllWithoutLoadedEmptyMap) {
  storage_area_impl()->SetCacheModeForTesting(GetParam());
  ClearDatabase();

  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(1u, observations().size());
  EXPECT_EQ(Observation::kDeleteAll, observations()[0].type);
  EXPECT_EQ(test_source_, observations()[0].source);
}

TEST_F(StorageAreaImplParamTest, PutOverQuotaLargeValue) {
  storage_area_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  std::vector<uint8_t> key = ToBytes("newkey");
  std::vector<uint8_t> value(kTestSizeLimit, 4);

  EXPECT_FALSE(PutSync(key, value, base::nullopt));

  value.resize(kTestSizeLimit / 2);
  EXPECT_TRUE(PutSync(key, value, base::nullopt));
}

TEST_F(StorageAreaImplParamTest, PutOverQuotaLargeKey) {
  storage_area_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  std::vector<uint8_t> key(kTestSizeLimit, 'a');
  std::vector<uint8_t> value = ToBytes("newvalue");

  EXPECT_FALSE(PutSync(key, value, base::nullopt));

  key.resize(kTestSizeLimit / 2);
  EXPECT_TRUE(PutSync(key, value, base::nullopt));
}

TEST_F(StorageAreaImplParamTest, PutWhenAlreadyOverQuota) {
  storage_area_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  std::string key = "largedata";
  std::vector<uint8_t> value(kTestSizeLimit, 4);
  std::vector<uint8_t> old_value = value;

  SetDatabaseEntry(test_prefix_ + key, ToString(value));

  // Put with same data should succeed.
  EXPECT_TRUE(PutSync(ToBytes(key), value, base::nullopt));

  // Put with same data size should succeed.
  value[1] = 13;
  EXPECT_TRUE(PutSync(ToBytes(key), value, old_value));

  // Adding a new key when already over quota should not succeed.
  EXPECT_FALSE(PutSync(ToBytes("newkey"), {1, 2, 3}, base::nullopt));

  // Reducing size should also succeed.
  old_value = value;
  value.resize(kTestSizeLimit / 2);
  EXPECT_TRUE(PutSync(ToBytes(key), value, old_value));

  // Increasing size again should succeed, as still under the limit.
  old_value = value;
  value.resize(value.size() + 1);
  EXPECT_TRUE(PutSync(ToBytes(key), value, old_value));

  // But increasing back to original size should fail.
  old_value = value;
  value.resize(kTestSizeLimit);
  EXPECT_FALSE(PutSync(ToBytes(key), value, old_value));
}

TEST_F(StorageAreaImplParamTest, PutWhenAlreadyOverQuotaBecauseOfLargeKey) {
  storage_area_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  std::vector<uint8_t> key(kTestSizeLimit, 'x');
  std::vector<uint8_t> value = ToBytes("value");
  std::vector<uint8_t> old_value = value;

  SetDatabaseEntry(test_prefix_ + ToString(key), ToString(value));

  // Put with same data size should succeed.
  value[0] = 'X';
  EXPECT_TRUE(PutSync(key, value, old_value));

  // Reducing size should also succeed.
  old_value = value;
  value.clear();
  EXPECT_TRUE(PutSync(key, value, old_value));

  // Increasing size should fail.
  old_value = value;
  value.resize(1, 'a');
  EXPECT_FALSE(PutSync(key, value, old_value));
}

TEST_P(StorageAreaImplParamTest, PutAfterPurgeMemory) {
  storage_area_impl()->SetCacheModeForTesting(GetParam());
  std::vector<uint8_t> result;
  const auto key = test_key2_bytes_;
  const auto value = test_value2_bytes_;
  EXPECT_TRUE(PutSync(key, value, value));
  EXPECT_EQ(delegate()->map_load_count(), 1);

  // Adding again doesn't load map again.
  EXPECT_TRUE(PutSync(key, value, value));
  EXPECT_EQ(delegate()->map_load_count(), 1);

  storage_area_impl()->PurgeMemory();

  // Now adding should still work, and load map again.
  EXPECT_TRUE(PutSync(key, value, value));
  EXPECT_EQ(delegate()->map_load_count(), 2);
}

TEST_P(StorageAreaImplParamTest, PurgeMemoryWithPendingChanges) {
  storage_area_impl()->SetCacheModeForTesting(GetParam());
  std::vector<uint8_t> key = test_key2_bytes_;
  std::vector<uint8_t> value = ToBytes("foo");
  EXPECT_TRUE(PutSync(key, value, test_value2_bytes_));
  EXPECT_EQ(delegate()->map_load_count(), 1);

  // Purge memory, and read. Should not actually have purged, so should not have
  // triggered a load.
  storage_area_impl()->PurgeMemory();

  EXPECT_TRUE(PutSync(key, value, value));
  EXPECT_EQ(delegate()->map_load_count(), 1);
}

TEST_P(StorageAreaImplParamTest, FixUpData) {
  storage_area_impl()->SetCacheModeForTesting(GetParam());
  std::vector<StorageAreaImpl::Change> changes;
  changes.push_back(std::make_pair(test_key1_bytes_, ToBytes("foo")));
  changes.push_back(std::make_pair(test_key2_bytes_, base::nullopt));
  changes.push_back(std::make_pair(test_prefix_bytes_, ToBytes("bla")));
  delegate()->set_mock_changes(std::move(changes));

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(storage_area(), &data));

  ASSERT_EQ(2u, data.size());
  EXPECT_EQ(test_prefix_, ToString(data[0]->key));
  EXPECT_EQ("bla", ToString(data[0]->value));
  EXPECT_EQ(test_key1_, ToString(data[1]->key));
  EXPECT_EQ("foo", ToString(data[1]->value));

  EXPECT_FALSE(HasDatabaseEntry(test_prefix_ + test_key2_));
  EXPECT_EQ("foo", GetDatabaseEntry(test_prefix_ + test_key1_));
  EXPECT_EQ("bla", GetDatabaseEntry(test_prefix_ + test_prefix_));
}

TEST_F(StorageAreaImplTest, SetOnlyKeysWithoutDatabase) {
  std::vector<uint8_t> key = test_key2_bytes_;
  std::vector<uint8_t> value = ToBytes("foo");
  MockDelegate delegate;
  StorageAreaImpl storage_area(
      nullptr, test_prefix_, &delegate,
      GetDefaultTestingOptions(CacheMode::KEYS_ONLY_WHEN_POSSIBLE));
  mojo::Remote<blink::mojom::StorageArea> storage_area_remote;
  storage_area.Bind(storage_area_remote.BindNewPipeAndPassReceiver());
  // Setting only keys mode is noop.
  storage_area.SetCacheModeForTesting(CacheMode::KEYS_ONLY_WHEN_POSSIBLE);

  EXPECT_FALSE(storage_area.initialized());
  EXPECT_EQ(CacheMode::KEYS_AND_VALUES, storage_area.cache_mode());

  // Put and Get can work synchronously without reload.
  bool put_callback_called = false;
  storage_area.Put(key, value, base::nullopt, "source",
                   base::BindOnce(
                       [](bool* put_callback_called, bool success) {
                         EXPECT_TRUE(success);
                         *put_callback_called = true;
                       },
                       &put_callback_called));
  EXPECT_TRUE(put_callback_called);

  std::vector<uint8_t> expected_value;
  storage_area.Get(key,
                   base::BindOnce(
                       [](std::vector<uint8_t>* expected_value, bool success,
                          const std::vector<uint8_t>& value) {
                         EXPECT_TRUE(success);
                         *expected_value = value;
                       },
                       &expected_value));
  EXPECT_EQ(expected_value, value);
}

TEST_P(StorageAreaImplParamTest, CommitOnDifferentCacheModes) {
  storage_area_impl()->SetCacheModeForTesting(GetParam());
  std::vector<uint8_t> key = test_key2_bytes_;
  std::vector<uint8_t> value = ToBytes("foo");
  std::vector<uint8_t> value2 = ToBytes("foo2");
  std::vector<uint8_t> value3 = ToBytes("foobar");

  // The initial map always has values, so a nullopt is fine for the old value.
  ASSERT_TRUE(PutSync(key, value, base::nullopt));
  ASSERT_TRUE(storage_area_impl()->commit_batch_);

  // Area stays in CacheMode::KEYS_AND_VALUES until the first commit has
  // succeeded.
  EXPECT_TRUE(storage_area_impl()->commit_batch_->changed_values.empty());
  auto* changes = &storage_area_impl()->commit_batch_->changed_keys;
  ASSERT_EQ(1u, changes->size());
  EXPECT_EQ(key, *changes->begin());

  BlockingCommit();

  ASSERT_TRUE(PutSync(key, value2, value));
  ASSERT_TRUE(storage_area_impl()->commit_batch_);

  // Commit has occured, so the map type will diverge based on the cache mode.
  if (GetParam() == CacheMode::KEYS_AND_VALUES) {
    EXPECT_TRUE(storage_area_impl()->commit_batch_->changed_values.empty());
    auto* changes = &storage_area_impl()->commit_batch_->changed_keys;
    ASSERT_EQ(1u, changes->size());
    EXPECT_EQ(key, *changes->begin());
  } else {
    EXPECT_TRUE(storage_area_impl()->commit_batch_->changed_keys.empty());
    auto* changes = &storage_area_impl()->commit_batch_->changed_values;
    ASSERT_EQ(1u, changes->size());
    auto it = changes->begin();
    EXPECT_EQ(key, it->first);
    EXPECT_EQ(value2, it->second);
  }

  BlockingCommit();

  EXPECT_EQ("foo2", GetDatabaseEntry(test_prefix_ + test_key2_));
  if (GetParam() == CacheMode::KEYS_AND_VALUES)
    EXPECT_EQ(2u, storage_area_impl()->keys_values_map_.size());
  else
    EXPECT_EQ(2u, storage_area_impl()->keys_only_map_.size());
  ASSERT_TRUE(PutSync(key, value2, value2));
  EXPECT_FALSE(storage_area_impl()->commit_batch_);
  ASSERT_TRUE(PutSync(key, value3, value2));
  ASSERT_TRUE(storage_area_impl()->commit_batch_);

  if (GetParam() == CacheMode::KEYS_AND_VALUES) {
    auto* changes = &storage_area_impl()->commit_batch_->changed_keys;
    EXPECT_EQ(1u, changes->size());
    auto it = changes->find(key);
    ASSERT_NE(it, changes->end());
  } else {
    auto* changes = &storage_area_impl()->commit_batch_->changed_values;
    EXPECT_EQ(1u, changes->size());
    auto it = changes->find(key);
    ASSERT_NE(it, changes->end());
    EXPECT_EQ(value3, it->second);
  }

  ClearDatabase();
  EXPECT_TRUE(storage_area_impl()->has_changes_to_commit());
  BlockingCommit();
  EXPECT_EQ("foobar", GetDatabaseEntry(test_prefix_ + test_key2_));
  EXPECT_FALSE(storage_area_impl()->has_changes_to_commit());
}

TEST_F(StorageAreaImplTest, GetAllWhenCacheOnlyKeys) {
  std::vector<uint8_t> key = test_key2_bytes_;
  std::vector<uint8_t> value = ToBytes("foo");
  std::vector<uint8_t> value2 = ToBytes("foobar");

  // Go to load state only keys.
  ASSERT_TRUE(PutSync(key, value, base::nullopt));
  BlockingCommit();
  ASSERT_TRUE(PutSync(key, value2, value));
  EXPECT_TRUE(storage_area_impl()->has_changes_to_commit());

  std::vector<blink::mojom::KeyValuePtr> data;

  base::RunLoop loop;
  bool put_result1 = false;
  bool put_result2 = false;
  {
    BarrierBuilder barrier(loop.QuitClosure());

    storage_area()->Put(
        key, value, value2, test_source_,
        MakeSuccessCallback(barrier.AddClosure(), &put_result1));

    mojo::PendingRemote<blink::mojom::StorageAreaObserver> unused_observer;
    ignore_result(unused_observer.InitWithNewPipeAndPassReceiver());
    storage_area()->GetAll(std::move(unused_observer),
                           MakeGetAllCallback(barrier.AddClosure(), &data));
    storage_area()->Put(
        key, value2, value, test_source_,
        MakeSuccessCallback(barrier.AddClosure(), &put_result2));
    FlushAreaBinding();
  }

  // GetAll triggers a commit when it's switching map types.
  EXPECT_TRUE(put_result1);
  EXPECT_EQ("foo", GetDatabaseEntry(test_prefix_ + test_key2_));

  loop.Run();

  EXPECT_TRUE(put_result1);

  EXPECT_EQ(2u, data.size());
  EXPECT_TRUE(data[1]->Equals(
      blink::mojom::KeyValue(test_key1_bytes_, test_value1_bytes_)))
      << ToString(data[1]->value) << " vs expected " << test_value1_;
  EXPECT_TRUE(data[0]->Equals(blink::mojom::KeyValue(key, value)))
      << ToString(data[0]->value) << " vs expected " << ToString(value);

  // The last "put" isn't committed yet.
  EXPECT_EQ("foo", GetDatabaseEntry(test_prefix_ + test_key2_));

  ASSERT_TRUE(storage_area_impl()->has_changes_to_commit());
  BlockingCommit();

  EXPECT_EQ("foobar", GetDatabaseEntry(test_prefix_ + test_key2_));
}

TEST_F(StorageAreaImplTest, GetAllAfterSetCacheMode) {
  std::vector<uint8_t> key = test_key2_bytes_;
  std::vector<uint8_t> value = ToBytes("foo");
  std::vector<uint8_t> value2 = ToBytes("foobar");

  // Go to load state only keys.
  ASSERT_TRUE(PutSync(key, value, base::nullopt));
  BlockingCommit();
  EXPECT_TRUE(storage_area_impl()->map_state_ ==
              StorageAreaImpl::MapState::LOADED_KEYS_ONLY);
  ASSERT_TRUE(PutSync(key, value2, value));
  EXPECT_TRUE(storage_area_impl()->has_changes_to_commit());

  storage_area_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);

  // Cache isn't cleared when commit batch exists.
  EXPECT_TRUE(storage_area_impl()->map_state_ ==
              StorageAreaImpl::MapState::LOADED_KEYS_ONLY);

  base::RunLoop loop;

  bool put_success = false;
  std::vector<blink::mojom::KeyValuePtr> data;
  bool delete_success = false;
  {
    BarrierBuilder barrier(loop.QuitClosure());

    storage_area()->Put(
        key, value, value2, test_source_,
        MakeSuccessCallback(barrier.AddClosure(), &put_success));

    // Put task triggers database upgrade, so there should be another map load.
    base::RunLoop upgrade_loop;
    storage_area_impl()->SetOnLoadCallbackForTesting(
        upgrade_loop.QuitClosure());
    upgrade_loop.Run();

    mojo::PendingRemote<blink::mojom::StorageAreaObserver> unused_observer;
    ignore_result(unused_observer.InitWithNewPipeAndPassReceiver());
    storage_area()->GetAll(
        std::move(unused_observer),
        MakeGetAllCallback(upgrade_loop.QuitClosure(), &data));

    // This Delete() should not affect the value returned by GetAll().
    storage_area()->Delete(
        key, value, test_source_,
        MakeSuccessCallback(barrier.AddClosure(), &delete_success));
  }
  loop.Run();

  EXPECT_EQ(2u, data.size());
  EXPECT_TRUE(data[1]->Equals(
      blink::mojom::KeyValue(test_key1_bytes_, test_value1_bytes_)))
      << ToString(data[1]->value) << " vs expected " << test_value1_;
  EXPECT_TRUE(data[0]->Equals(blink::mojom::KeyValue(key, value)))
      << ToString(data[0]->value) << " vs expected " << ToString(value2);

  EXPECT_TRUE(put_success);
  EXPECT_TRUE(delete_success);

  // GetAll shouldn't trigger a commit before it runs now because the value
  // map should be loading.
  EXPECT_EQ("foobar", GetDatabaseEntry(test_prefix_ + test_key2_));

  ASSERT_TRUE(storage_area_impl()->has_changes_to_commit());
  BlockingCommit();

  EXPECT_FALSE(HasDatabaseEntry(test_prefix_ + test_key2_));
}

TEST_F(StorageAreaImplTest, SetCacheModeConsistent) {
  std::vector<uint8_t> key = test_key2_bytes_;
  std::vector<uint8_t> value = ToBytes("foo");
  std::vector<uint8_t> value2 = ToBytes("foobar");

  EXPECT_FALSE(storage_area_impl()->IsMapLoaded());
  EXPECT_TRUE(storage_area_impl()->cache_mode() ==
              CacheMode::KEYS_ONLY_WHEN_POSSIBLE);

  // Clear the database before the area loads data.
  ClearDatabase();

  EXPECT_TRUE(PutSync(key, value, base::nullopt));
  EXPECT_TRUE(storage_area_impl()->has_changes_to_commit());
  BlockingCommit();

  EXPECT_TRUE(PutSync(key, value2, value));
  EXPECT_TRUE(storage_area_impl()->has_changes_to_commit());

  // Setting cache mode does not reload the cache till it is required.
  storage_area_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  EXPECT_EQ(StorageAreaImpl::MapState::LOADED_KEYS_ONLY,
            storage_area_impl()->map_state_);

  // Put operation should change the mode.
  EXPECT_TRUE(PutSync(key, value, value2));
  EXPECT_TRUE(storage_area_impl()->has_changes_to_commit());
  EXPECT_EQ(StorageAreaImpl::MapState::LOADED_KEYS_AND_VALUES,
            storage_area_impl()->map_state_);
  std::vector<uint8_t> result;
  EXPECT_TRUE(GetSync(key, &result));
  EXPECT_EQ(value, result);
  EXPECT_EQ(StorageAreaImpl::MapState::LOADED_KEYS_AND_VALUES,
            storage_area_impl()->map_state_);

  BlockingCommit();

  // Test that the map will unload correctly
  EXPECT_TRUE(PutSync(key, value2, value));
  storage_area_impl()->SetCacheModeForTesting(
      CacheMode::KEYS_ONLY_WHEN_POSSIBLE);
  EXPECT_EQ(StorageAreaImpl::MapState::LOADED_KEYS_ONLY,
            storage_area_impl()->map_state_);
  BlockingCommit();
  EXPECT_EQ(StorageAreaImpl::MapState::LOADED_KEYS_ONLY,
            storage_area_impl()->map_state_);

  // Test the map will unload right away when there are no changes.
  storage_area_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  EXPECT_TRUE(GetSync(key, &result));
  EXPECT_EQ(StorageAreaImpl::MapState::LOADED_KEYS_AND_VALUES,
            storage_area_impl()->map_state_);
  storage_area_impl()->SetCacheModeForTesting(
      CacheMode::KEYS_ONLY_WHEN_POSSIBLE);
  EXPECT_EQ(StorageAreaImpl::MapState::LOADED_KEYS_ONLY,
            storage_area_impl()->map_state_);
}

TEST_F(StorageAreaImplTest, SendOldValueObservations) {
  ASSERT_EQ(0u, observations().size());
  should_record_send_old_value_observations(true);
  storage_area_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  // Flush tasks on mojo thread to observe callback.
  EXPECT_TRUE(DeleteSync(ToBytes("doesn't exist"), base::nullopt));
  storage_area_impl()->SetCacheModeForTesting(
      CacheMode::KEYS_ONLY_WHEN_POSSIBLE);
  // Flush tasks on mojo thread to observe callback.
  EXPECT_TRUE(DeleteSync(ToBytes("doesn't exist"), base::nullopt));

  ASSERT_EQ(4u, observations().size());
  EXPECT_EQ(Observation::kSendOldValue, observations()[0].type);
  EXPECT_FALSE(observations()[0].should_send_old_value);
  EXPECT_EQ(Observation::kSendOldValue, observations()[2].type);
  EXPECT_TRUE(observations()[2].should_send_old_value);
}

TEST_P(StorageAreaImplParamTest, PrefixForking) {
  std::string value3 = "value3";
  std::string value4 = "value4";
  std::string value5 = "value5";

  // In order to test the interaction between forking and mojo calls where
  // forking can happen in between a request and reply to the area mojo
  // service, all calls are done on the 'impl' object itself.

  // Operations in the same run cycle:
  // Fork 1 created from original
  // Put on fork 1
  // Fork 2 create from fork 1
  // Put on fork 1
  // Put on original
  // Fork 3 created from original
  std::unique_ptr<StorageAreaImpl> fork1;
  MockDelegate fork1_delegate;
  std::unique_ptr<StorageAreaImpl> fork2;
  MockDelegate fork2_delegate;
  std::unique_ptr<StorageAreaImpl> fork3;
  MockDelegate fork3_delegate;

  auto options = GetDefaultTestingOptions(GetParam());
  bool put_success1 = false;
  bool put_success2 = false;
  bool put_success3 = false;
  base::RunLoop loop;
  {
    BarrierBuilder barrier(loop.QuitClosure());

    // Create fork 1.
    fork1 = storage_area_impl()->ForkToNewPrefix(test_copy_prefix1_,
                                                 &fork1_delegate, options);

    // Do a put on fork 1 and create fork 2.
    // Note - these are 'skipping' the mojo layer, which is why the fork isn't
    // scheduled.
    fork1->Put(test_key2_bytes_, ToBytes(value4), test_value2_bytes_,
               test_source_,
               MakeSuccessCallback(barrier.AddClosure(), &put_success1));
    fork2 =
        fork1->ForkToNewPrefix(test_copy_prefix2_, &fork2_delegate, options);
    fork1->Put(test_key2_bytes_, ToBytes(value5), ToBytes(value4), test_source_,
               MakeSuccessCallback(barrier.AddClosure(), &put_success2));

    // Do a put on original and create fork 3, which is key-only.
    storage_area_impl()->Put(
        test_key1_bytes_, ToBytes(value3), test_value1_bytes_, test_source_,
        MakeSuccessCallback(barrier.AddClosure(), &put_success3));
    fork3 = storage_area_impl()->ForkToNewPrefix(
        test_copy_prefix3_, &fork3_delegate,
        GetDefaultTestingOptions(CacheMode::KEYS_ONLY_WHEN_POSSIBLE));
  }
  loop.Run();
  EXPECT_TRUE(put_success1);
  EXPECT_TRUE(put_success2);
  EXPECT_TRUE(fork2.get());
  EXPECT_TRUE(fork3.get());

  EXPECT_EQ(value3, GetSyncStrUsingGetAll(storage_area_impl(), test_key1_));
  EXPECT_EQ(test_value1_, GetSyncStrUsingGetAll(fork1.get(), test_key1_));
  EXPECT_EQ(test_value1_, GetSyncStrUsingGetAll(fork2.get(), test_key1_));
  EXPECT_EQ(value3, GetSyncStrUsingGetAll(fork3.get(), test_key1_));

  EXPECT_EQ(test_value2_,
            GetSyncStrUsingGetAll(storage_area_impl(), test_key2_));
  EXPECT_EQ(value5, GetSyncStrUsingGetAll(fork1.get(), test_key2_));
  EXPECT_EQ(value4, GetSyncStrUsingGetAll(fork2.get(), test_key2_));
  EXPECT_EQ(test_value2_, GetSyncStrUsingGetAll(fork3.get(), test_key2_));

  BlockingCommit(delegate(), storage_area_impl());
  BlockingCommit(&fork1_delegate, fork1.get());

  // test_key1_ values.
  EXPECT_EQ(value3, GetDatabaseEntry(test_prefix_ + test_key1_));
  EXPECT_EQ(test_value1_, GetDatabaseEntry(test_copy_prefix1_ + test_key1_));
  EXPECT_EQ(test_value1_, GetDatabaseEntry(test_copy_prefix2_ + test_key1_));
  EXPECT_EQ(value3, GetDatabaseEntry(test_copy_prefix3_ + test_key1_));

  // test_key2_ values.
  EXPECT_EQ(test_value2_, GetDatabaseEntry(test_prefix_ + test_key2_));
  EXPECT_EQ(value5, GetDatabaseEntry(test_copy_prefix1_ + test_key2_));
  EXPECT_EQ(value4, GetDatabaseEntry(test_copy_prefix2_ + test_key2_));
  EXPECT_EQ(test_value2_, GetDatabaseEntry(test_copy_prefix3_ + test_key2_));
}

TEST_P(StorageAreaImplParamTest, PrefixForkAfterLoad) {
  const std::string kValue = "foo";
  const std::vector<uint8_t> kValueVec = ToBytes(kValue);

  // Do a sync put so the map loads.
  EXPECT_TRUE(PutSync(test_key1_bytes_, kValueVec, base::nullopt));

  // Execute the fork.
  MockDelegate fork1_delegate;
  std::unique_ptr<StorageAreaImpl> fork1 = storage_area_impl()->ForkToNewPrefix(
      test_copy_prefix1_, &fork1_delegate,
      GetDefaultTestingOptions(GetParam()));

  // Check our forked state.
  EXPECT_EQ(kValue, GetSyncStrUsingGetAll(fork1.get(), test_key1_));

  BlockingCommit(delegate(), storage_area_impl());

  EXPECT_EQ(kValue, GetDatabaseEntry(test_copy_prefix1_ + test_key1_));
}

namespace {
std::string GetNewPrefix(int* i) {
  std::string prefix = "prefix-" + base::NumberToString(*i) + "-";
  (*i)++;
  return prefix;
}

struct FuzzState {
  base::Optional<std::vector<uint8_t>> val1;
  base::Optional<std::vector<uint8_t>> val2;
};
}  // namespace

TEST_F(StorageAreaImplTest, PrefixForkingPsuedoFuzzer) {
  const std::string kKey1 = "key1";
  const std::vector<uint8_t> kKey1Vec = ToBytes(kKey1);
  const std::string kKey2 = "key2";
  const std::vector<uint8_t> kKey2Vec = ToBytes(kKey2);
  const int kTotalAreas = 1000;

  // This tests tries to throw all possible enumartions of operations and
  // forking at areas. The purpose is to hit all edge cases possible to
  // expose any loading bugs.

  std::vector<FuzzState> states(kTotalAreas);
  std::vector<std::unique_ptr<StorageAreaImpl>> areas(kTotalAreas);
  std::vector<MockDelegate> delegates(kTotalAreas);
  std::list<bool> successes;
  int curr_prefix = 0;

  base::RunLoop loop;
  {
    BarrierBuilder barrier(loop.QuitClosure());
    for (int64_t i = 0; i < kTotalAreas; i++) {
      FuzzState& state = states[i];
      if (!areas[i]) {
        areas[i] = storage_area_impl()->ForkToNewPrefix(
            GetNewPrefix(&curr_prefix), &delegates[i],
            GetDefaultTestingOptions(CacheMode::KEYS_ONLY_WHEN_POSSIBLE));
      }
      int64_t forks = i;
      if ((i % 5 == 0 || i % 6 == 0) && forks + 1 < kTotalAreas) {
        forks++;
        states[forks] = state;
        areas[forks] = areas[i]->ForkToNewPrefix(
            GetNewPrefix(&curr_prefix), &delegates[forks],
            GetDefaultTestingOptions(CacheMode::KEYS_AND_VALUES));
      }
      if (i % 13 == 0) {
        FuzzState old_state = state;
        state.val1 = base::nullopt;
        successes.push_back(false);
        areas[i]->Delete(
            kKey1Vec, old_state.val1, test_source_,
            MakeSuccessCallback(barrier.AddClosure(), &successes.back()));
      }
      if (i % 4 == 0) {
        FuzzState old_state = state;
        state.val2 = base::make_optional<std::vector<uint8_t>>(
            {static_cast<uint8_t>(i)});
        successes.push_back(false);
        areas[i]->Put(
            kKey2Vec, state.val2.value(), old_state.val2, test_source_,
            MakeSuccessCallback(barrier.AddClosure(), &successes.back()));
      }
      if (i % 3 == 0) {
        FuzzState old_state = state;
        state.val1 = base::make_optional<std::vector<uint8_t>>(
            {static_cast<uint8_t>(i + 5)});
        successes.push_back(false);
        areas[i]->Put(
            kKey1Vec, state.val1.value(), old_state.val1, test_source_,
            MakeSuccessCallback(barrier.AddClosure(), &successes.back()));
      }
      if (i % 11 == 0) {
        state.val1 = base::nullopt;
        state.val2 = base::nullopt;
        successes.push_back(false);
        areas[i]->DeleteAll(
            test_source_, mojo::NullRemote(),
            MakeSuccessCallback(barrier.AddClosure(), &successes.back()));
      }
      if (i % 2 == 0 && forks + 1 < kTotalAreas) {
        CacheMode mode = i % 3 == 0 ? CacheMode::KEYS_AND_VALUES
                                    : CacheMode::KEYS_ONLY_WHEN_POSSIBLE;
        forks++;
        states[forks] = state;
        areas[forks] = areas[i]->ForkToNewPrefix(
            GetNewPrefix(&curr_prefix), &delegates[forks],
            GetDefaultTestingOptions(mode));
      }
      if (i % 3 == 0) {
        FuzzState old_state = state;
        state.val1 = base::make_optional<std::vector<uint8_t>>(
            {static_cast<uint8_t>(i + 9)});
        successes.push_back(false);
        areas[i]->Put(
            kKey1Vec, state.val1.value(), old_state.val1, test_source_,
            MakeSuccessCallback(barrier.AddClosure(), &successes.back()));
      }
    }
  }
  loop.Run();

  // This section checks that we get the correct values when we query the
  // areas (which may or may not be maintaining their own cache).
  for (size_t i = 0; i < kTotalAreas; i++) {
    FuzzState& state = states[i];
    std::vector<uint8_t> result;

    // Note: this will cause all keys-only areas to commit.
    std::string result1 = GetSyncStrUsingGetAll(areas[i].get(), kKey1);
    std::string result2 = GetSyncStrUsingGetAll(areas[i].get(), kKey2);
    EXPECT_EQ(!!state.val1, !result1.empty()) << i;
    if (state.val1)
      EXPECT_EQ(state.val1.value(), ToBytes(result1));
    EXPECT_EQ(!!state.val2, !result2.empty()) << i;
    if (state.val2)
      EXPECT_EQ(state.val2.value(), ToBytes(result2)) << i;
  }

  // This section verifies that all areas have committed their changes to
  // the database.
  ASSERT_EQ(areas.size(), delegates.size());
  size_t half = kTotalAreas / 2;
  for (size_t i = 0; i < half; i++) {
    BlockingCommit(&delegates[i], areas[i].get());
  }

  for (size_t i = kTotalAreas - 1; i >= half; i--) {
    BlockingCommit(&delegates[i], areas[i].get());
  }

  // This section checks the data in the database itself to verify all areas
  // committed changes correctly.
  for (size_t i = 0; i < kTotalAreas; ++i) {
    FuzzState& state = states[i];

    std::vector<uint8_t> prefix = areas[i]->prefix();
    std::string key1 = ToString(prefix) + kKey1;
    std::string key2 = ToString(prefix) + kKey2;
    EXPECT_EQ(!!state.val1, HasDatabaseEntry(key1));
    if (state.val1)
      EXPECT_EQ(ToString(state.val1.value()), GetDatabaseEntry(key1));
    EXPECT_EQ(!!state.val2, HasDatabaseEntry(key2));
    if (state.val2)
      EXPECT_EQ(ToString(state.val2.value()), GetDatabaseEntry(key2));

    EXPECT_FALSE(areas[i]->has_pending_load_tasks()) << i;
  }
}

TEST_P(StorageAreaImplParamTest, EmptyMapIgnoresDisk) {
  const std::string kValue = "foo";
  const std::vector<uint8_t> kValueVec = ToBytes(kValue);

  // Set fake data to ensure that our shortcut doesn't read it.
  SetDatabaseEntry(test_copy_prefix1_ + test_key1_, kValue);

  // Create an empty map that will have no data in it.
  StorageAreaImpl::Options options =
      GetDefaultTestingOptions(CacheMode::KEYS_ONLY_WHEN_POSSIBLE);
  auto empty_storage_area = std::make_unique<StorageAreaImpl>(
      database(), test_copy_prefix1_, delegate(), options);
  empty_storage_area->InitializeAsEmpty();

  // Check the forked state, which should be empty.
  EXPECT_EQ("", GetSyncStrUsingGetAll(empty_storage_area.get(), test_key1_));
}

TEST_P(StorageAreaImplParamTest, ForkFromEmptyMap) {
  const std::string kValue = "foo";
  const std::vector<uint8_t> kValueVec = ToBytes(kValue);

  // Set fake data to ensure that our shortcut doesn't read it.
  SetDatabaseEntry(test_copy_prefix1_ + test_key1_, kValue);

  // Create an empty map that will have no data in it.
  StorageAreaImpl::Options options =
      GetDefaultTestingOptions(CacheMode::KEYS_ONLY_WHEN_POSSIBLE);
  auto empty_storage_area = std::make_unique<StorageAreaImpl>(
      database(), test_copy_prefix1_, delegate(), options);
  empty_storage_area->InitializeAsEmpty();

  // Execute the fork, which should shortcut disk and just be empty.
  MockDelegate fork1_delegate;
  std::unique_ptr<StorageAreaImpl> fork =
      empty_storage_area->ForkToNewPrefix(test_copy_prefix1_, &fork1_delegate,
                                          GetDefaultTestingOptions(GetParam()));

  // Check the forked state, which should be empty.
  EXPECT_EQ("", GetSyncStrUsingGetAll(fork.get(), test_key1_));
}

}  // namespace storage
