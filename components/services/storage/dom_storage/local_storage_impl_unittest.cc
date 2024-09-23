// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/local_storage_impl.h"

#include <string_view>
#include <tuple>

#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/local_storage_database.pb.h"
#include "components/services/storage/dom_storage/storage_area_test_util.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "url/gurl.h"

namespace storage {

namespace {

std::vector<uint8_t> StdStringToUint8Vector(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::string Uint8VectorToStdString(const std::vector<uint8_t>& v) {
  return std::string(v.begin(), v.end());
}

void GetStorageUsageCallback(
    const base::RepeatingClosure& callback,
    std::vector<mojom::StorageUsageInfoPtr>* out_result,
    std::vector<mojom::StorageUsageInfoPtr> result) {
  *out_result = std::move(result);
  callback.Run();
}

class TestLevelDBObserver : public blink::mojom::StorageAreaObserver {
 public:
  struct Observation {
    enum { kChange, kChangeFailed, kDelete, kDeleteAll } type;
    std::string key;
    std::optional<std::string> old_value;
    std::string new_value;
    std::string source;
  };

  TestLevelDBObserver() = default;

  mojo::PendingRemote<blink::mojom::StorageAreaObserver> Bind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  const std::vector<Observation>& observations() { return observations_; }

 private:
  void KeyChanged(const std::vector<uint8_t>& key,
                  const std::vector<uint8_t>& new_value,
                  const std::optional<std::vector<uint8_t>>& old_value,
                  const std::string& source) override {
    observations_.push_back(
        {Observation::kChange, Uint8VectorToStdString(key),
         old_value ? std::make_optional(Uint8VectorToStdString(*old_value))
                   : std::nullopt,
         Uint8VectorToStdString(new_value), source});
  }
  void KeyChangeFailed(const std::vector<uint8_t>& key,
                       const std::string& source) override {
    observations_.push_back({Observation::kChangeFailed,
                             Uint8VectorToStdString(key), "", "", source});
  }
  void KeyDeleted(const std::vector<uint8_t>& key,
                  const std::optional<std::vector<uint8_t>>& old_value,
                  const std::string& source) override {
    observations_.push_back(
        {Observation::kDelete, Uint8VectorToStdString(key),
         old_value ? std::make_optional(Uint8VectorToStdString(*old_value))
                   : std::nullopt,
         "", source});
  }
  void AllDeleted(bool was_nonempty, const std::string& source) override {
    observations_.push_back({Observation::kDeleteAll, "", "", "", source});
  }
  void ShouldSendOldValueOnMutations(bool value) override {}

  std::vector<Observation> observations_;
  mojo::Receiver<blink::mojom::StorageAreaObserver> receiver_{this};
};

}  // namespace

class LocalStorageImplTest : public testing::Test {
 public:
  LocalStorageImplTest() { EXPECT_TRUE(temp_path_.CreateUniqueTempDir()); }

  LocalStorageImplTest(const LocalStorageImplTest&) = delete;
  LocalStorageImplTest& operator=(const LocalStorageImplTest&) = delete;

  ~LocalStorageImplTest() override {
    if (storage_)
      ShutDownStorage();

    EXPECT_TRUE(temp_path_.Delete());
  }

  const base::FilePath& storage_path() const { return temp_path_.GetPath(); }

  LocalStorageImpl* context() {
    DCHECK(storage_);
    return storage_.get();
  }

  void InitializeStorage(const base::FilePath& path) {
    DCHECK(!storage_);
    storage_ = std::make_unique<LocalStorageImpl>(
        path, base::SingleThreadTaskRunner::GetCurrentDefault(),
        /*receiver=*/mojo::NullReceiver());
  }

  void ShutDownStorage() {
    DCHECK(storage_);
    base::RunLoop loop;
    storage_->ShutDown(loop.QuitClosure());
    loop.Run();
    storage_.reset();
  }

  void ResetStorage(const base::FilePath& path) {
    if (storage_)
      ShutDownStorage();
    InitializeStorage(path);
  }

  void WaitForDatabaseOpen() {
    base::RunLoop loop;
    context()->SetDatabaseOpenCallbackForTesting(loop.QuitClosure());
    loop.Run();
  }

  void SetDatabaseEntry(std::string_view key, std::string_view value) {
    WaitForDatabaseOpen();
    base::RunLoop loop;
    context()->GetDatabaseForTesting().PostTaskWithThisObject(
        base::BindLambdaForTesting([&](const DomStorageDatabase& db) {
          leveldb::Status status =
              db.Put(base::as_bytes(base::make_span(key)),
                     base::as_bytes(base::make_span(value)));
          ASSERT_TRUE(status.ok());
          loop.Quit();
        }));
    loop.Run();
  }

  void ClearDatabase() {
    WaitForDatabaseOpen();
    base::RunLoop loop;
    context()->GetDatabaseForTesting().PostTaskWithThisObject(
        base::BindLambdaForTesting([&](const DomStorageDatabase& db) {
          leveldb::WriteBatch batch;
          leveldb::Status status = db.DeletePrefixed({}, &batch);
          ASSERT_TRUE(status.ok());
          status = db.Commit(&batch);
          ASSERT_TRUE(status.ok());
          loop.Quit();
        }));
    loop.Run();
  }

  std::map<std::string, std::string> GetDatabaseContents() {
    std::vector<DomStorageDatabase::KeyValuePair> entries;
    WaitForDatabaseOpen();
    base::RunLoop loop;
    context()->GetDatabaseForTesting().PostTaskWithThisObject(
        base::BindLambdaForTesting([&](const DomStorageDatabase& db) {
          leveldb::Status status = db.GetPrefixed({}, &entries);
          ASSERT_TRUE(status.ok());
          loop.Quit();
        }));
    loop.Run();

    std::map<std::string, std::string> contents;
    for (auto& entry : entries) {
      contents.emplace(std::string(entry.key.begin(), entry.key.end()),
                       std::string(entry.value.begin(), entry.value.end()));
    }

    return contents;
  }

  std::vector<mojom::StorageUsageInfoPtr> GetStorageUsageSync() {
    base::RunLoop run_loop;
    std::vector<mojom::StorageUsageInfoPtr> result;
    context()->GetUsage(base::BindOnce(&GetStorageUsageCallback,
                                       run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  std::optional<std::vector<uint8_t>> DoTestGet(
      const std::vector<uint8_t>& key) {
    const blink::StorageKey storage_key =
        blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
    mojo::Remote<blink::mojom::StorageArea> area;
    mojo::Remote<blink::mojom::StorageArea>
        dummy_area;  // To make sure values are cached.
    context()->BindStorageArea(storage_key, area.BindNewPipeAndPassReceiver());
    context()->BindStorageArea(storage_key,
                               dummy_area.BindNewPipeAndPassReceiver());
    std::vector<uint8_t> result;
    bool success = test::GetSync(area.get(), key, &result);
    return success ? std::optional<std::vector<uint8_t>>(result) : std::nullopt;
  }

  // Pumps both the main-thread sequence and the background database sequence
  // until both are idle.
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void DoTestPut(const std::vector<uint8_t>& key,
                 const std::vector<uint8_t>& value) {
    mojo::Remote<blink::mojom::StorageArea> area;
    bool success = false;
    base::RunLoop run_loop;
    context()->BindStorageArea(
        blink::StorageKey::CreateFromStringForTesting("http://foobar.com"),
        area.BindNewPipeAndPassReceiver());
    area->Put(key, value, std::nullopt, "source",
              test::MakeSuccessCallback(run_loop.QuitClosure(), &success));
    run_loop.Run();
    EXPECT_TRUE(success);
    area.reset();
    RunUntilIdle();
  }

  bool DoTestGet(const std::vector<uint8_t>& key,
                 std::vector<uint8_t>* result) {
    mojo::Remote<blink::mojom::StorageArea> area;
    context()->BindStorageArea(
        blink::StorageKey::CreateFromStringForTesting("http://foobar.com"),
        area.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    std::vector<blink::mojom::KeyValuePtr> data;
    mojo::PendingRemote<blink::mojom::StorageAreaObserver> unused_observer;
    std::ignore = unused_observer.InitWithNewPipeAndPassReceiver();
    area->GetAll(std::move(unused_observer),
                 test::MakeGetAllCallback(run_loop.QuitClosure(), &data));
    run_loop.Run();

    for (auto& entry : data) {
      if (key == entry->key) {
        *result = std::move(entry->value);
        return true;
      }
    }
    result->clear();
    return false;
  }

  base::FilePath FirstEntryInDir() {
    base::FileEnumerator enumerator(
        storage_path(), false /* recursive */,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    return enumerator.Next();
  }

 private:
  // testing::Test:
  void SetUp() override { InitializeStorage(storage_path()); }

  void TearDown() override {
    // Some of these tests close message pipes which serve as master interfaces
    // to other associated interfaces; this in turn schedules tasks to invoke
    // the associated interfaces' error handlers, and local storage code relies
    // on those handlers running in order to avoid memory leaks at shutdown.
    RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_path_;

  std::unique_ptr<LocalStorageImpl> storage_;
};

TEST_F(LocalStorageImplTest, Basic) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com"),
      area.BindNewPipeAndPassReceiver());

  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();

  RunUntilIdle();

  // Should have four rows of data, one for the version, one for the actual
  // data and two for metadata.
  EXPECT_EQ(4u, GetDatabaseContents().size());
}

TEST_F(LocalStorageImplTest, StorageKeysAreIndependent) {
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com:123");
  blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com:1234");
  auto key1 = StdStringToUint8Vector("4key");
  auto key2 = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());

  area->Put(key1, value, std::nullopt, "source", base::DoNothing());
  area.reset();

  context()->BindStorageArea(storage_key2, area.BindNewPipeAndPassReceiver());
  area->Put(key2, value, std::nullopt, "source", base::DoNothing());
  area.reset();

  RunUntilIdle();
  EXPECT_EQ(7u, GetDatabaseContents().size());
}

TEST_F(LocalStorageImplTest, WrapperOutlivesMojoConnection) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  // Write some data to the DB.
  mojo::Remote<blink::mojom::StorageArea> area;
  mojo::Remote<blink::mojom::StorageArea>
      dummy_area;  // To make sure values are cached.
  const blink::StorageKey storage_key(
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com"));
  context()->BindStorageArea(storage_key, area.BindNewPipeAndPassReceiver());
  context()->BindStorageArea(storage_key,
                             dummy_area.BindNewPipeAndPassReceiver());
  area->Put(key, value, std::nullopt, "source", base::DoNothing());

  area.reset();
  dummy_area.reset();
  RunUntilIdle();

  // Clear all the data from the backing database.
  EXPECT_FALSE(GetDatabaseContents().empty());
  ClearDatabase();

  // Data should still be readable, because despite closing the area
  // connection above, the actual area instance should have been kept alive.
  EXPECT_EQ(value, DoTestGet(key));

  // Now purge memory.
  context()->PurgeMemory();

  // And make sure caches were actually cleared.
  EXPECT_EQ(std::nullopt, DoTestGet(key));
}

TEST_F(LocalStorageImplTest, OpeningWrappersPurgesInactiveWrappers) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  // Write some data to the DB.
  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com"),
      area.BindNewPipeAndPassReceiver());
  area->Put(key, value, std::nullopt, "source", base::DoNothing());

  area.reset();
  RunUntilIdle();

  // Clear all the data from the backing database.
  EXPECT_FALSE(GetDatabaseContents().empty());
  ClearDatabase();

  // Now open many new areas (for different StorageKeys) to trigger clean up.
  for (int i = 1; i <= 100; ++i) {
    context()->BindStorageArea(
        blink::StorageKey::CreateFromStringForTesting(
            base::StringPrintf("http://example.com:%d", i)),
        area.BindNewPipeAndPassReceiver());
    area.reset();
  }

  RunUntilIdle();

  // And make sure caches were actually cleared.
  EXPECT_EQ(std::nullopt, DoTestGet(key));
}

TEST_F(LocalStorageImplTest, ValidVersion) {
  SetDatabaseEntry("VERSION", "1");
  SetDatabaseEntry(std::string("_http://foobar.com") + '\x00' + "key", "value");

  ResetStorage(storage_path());
  EXPECT_EQ(StdStringToUint8Vector("value"),
            DoTestGet(StdStringToUint8Vector("key")));
}

TEST_F(LocalStorageImplTest, InvalidVersion) {
  SetDatabaseEntry("VERSION", "foobar");
  SetDatabaseEntry(std::string("_http://foobar.com") + '\x00' + "key", "value");

  // Force the a reload of the database, which should fail due to invalid
  // version data.
  ResetStorage(storage_path());
  EXPECT_EQ(std::nullopt, DoTestGet(StdStringToUint8Vector("key")));
}

TEST_F(LocalStorageImplTest, VersionOnlyWrittenOnCommit) {
  EXPECT_EQ(std::nullopt, DoTestGet(StdStringToUint8Vector("key")));

  RunUntilIdle();
  EXPECT_TRUE(GetDatabaseContents().empty());
}

TEST_F(LocalStorageImplTest, GetStorageUsage_NoData) {
  std::vector<mojom::StorageUsageInfoPtr> info = GetStorageUsageSync();
  EXPECT_EQ(0u, info.size());
}

TEST_F(LocalStorageImplTest, GetStorageUsage_Data) {
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");
  auto key1 = StdStringToUint8Vector("key1");
  auto key2 = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  base::Time before_write = base::Time::Now();

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());

  area->Put(key1, value, std::nullopt, "source", base::DoNothing());
  area->Put(key2, value, std::nullopt, "source", base::DoNothing());
  area.reset();

  context()->BindStorageArea(storage_key2, area.BindNewPipeAndPassReceiver());
  area->Put(key2, value, std::nullopt, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  RunUntilIdle();

  base::Time after_write = base::Time::Now();

  std::vector<mojom::StorageUsageInfoPtr> info = GetStorageUsageSync();
  ASSERT_EQ(2u, info.size());
  if (info[0]->storage_key == storage_key2)
    std::swap(info[0], info[1]);
  EXPECT_EQ(storage_key1, info[0]->storage_key);
  EXPECT_EQ(storage_key2, info[1]->storage_key);
  EXPECT_LE(before_write, info[0]->last_modified);
  EXPECT_LE(before_write, info[1]->last_modified);
  EXPECT_GE(after_write, info[0]->last_modified);
  EXPECT_GE(after_write, info[1]->last_modified);
  EXPECT_GT(info[0]->total_size_bytes, info[1]->total_size_bytes);
}

TEST_F(LocalStorageImplTest, CheckAccessMetaData) {
  base::Time before_metadata = base::Time::Now();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foo.com");
  blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://bar.com");
  blink::StorageKey storage_key3 =
      blink::StorageKey::CreateFromStringForTesting("http://qux.com");
  mojo::Remote<blink::mojom::StorageArea> area;

  // storage_key1 has no content in its area.
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());
  area.reset();
  RunUntilIdle();

  // storage_key2 has content in its area.
  context()->BindStorageArea(storage_key2, area.BindNewPipeAndPassReceiver());
  area->Put(StdStringToUint8Vector("key"), StdStringToUint8Vector("value"),
            std::nullopt, "source", base::DoNothing());
  area.reset();
  RunUntilIdle();

  // storage_key3 has content in its area but is purged on shutdown.
  context()->BindStorageArea(storage_key3, area.BindNewPipeAndPassReceiver());
  area->Put(StdStringToUint8Vector("key"), StdStringToUint8Vector("value"),
            std::nullopt, "source", base::DoNothing());
  area.reset();
  std::vector<mojom::StoragePolicyUpdatePtr> updates;
  updates.emplace_back(mojom::StoragePolicyUpdate::New(
      storage_key3.origin(), /*purge_on_shutdown=*/true));
  context()->ApplyPolicyUpdates(std::move(updates));
  RunUntilIdle();

  // After shutdown, we should just see data for storage_key2.
  ResetStorage(storage_path());
  RunUntilIdle();
  base::Time after_metadata = base::Time::Now();
  auto contents = GetDatabaseContents();
  EXPECT_EQ(4u, contents.size());
  bool did_see_access_metadata = false;
  for (const auto& entry : contents) {
    if (entry.first.find("ACCESS") != std::string::npos &&
        entry.first.find(storage_key2.origin().Serialize()) !=
            std::string::npos) {
      storage::LocalStorageAreaAccessMetaData metadata;
      if (metadata.ParseFromArray(entry.second.data(), entry.second.size())) {
        base::Time last_accessed =
            base::Time::FromInternalValue(metadata.last_accessed());
        EXPECT_LE(before_metadata, last_accessed);
        EXPECT_GE(after_metadata, last_accessed);
        did_see_access_metadata = true;
      }
    }
  }
  EXPECT_TRUE(did_see_access_metadata);

  // If we re-bind storage_key2 and then shutdown, the last_accessed time should
  // be updated.
  before_metadata = base::Time::Now();
  context()->BindStorageArea(storage_key2, area.BindNewPipeAndPassReceiver());
  mojo::PendingRemote<blink::mojom::StorageAreaObserver> unused_observer;
  std::ignore = unused_observer.InitWithNewPipeAndPassReceiver();
  area->GetAll(std::move(unused_observer), base::DoNothing());
  area.reset();
  RunUntilIdle();
  ResetStorage(storage_path());
  RunUntilIdle();
  after_metadata = base::Time::Now();
  contents = GetDatabaseContents();
  EXPECT_EQ(4u, contents.size());
  did_see_access_metadata = false;
  for (const auto& entry : contents) {
    if (entry.first.find("ACCESS") != std::string::npos &&
        entry.first.find(storage_key2.origin().Serialize()) !=
            std::string::npos) {
      storage::LocalStorageAreaAccessMetaData metadata;
      if (metadata.ParseFromArray(entry.second.data(), entry.second.size())) {
        base::Time last_accessed =
            base::Time::FromInternalValue(metadata.last_accessed());
        EXPECT_LE(before_metadata, last_accessed);
        EXPECT_GE(after_metadata, last_accessed);
        did_see_access_metadata = true;
      }
    }
  }
  EXPECT_TRUE(did_see_access_metadata);
}

TEST_F(LocalStorageImplTest, MetaDataClearedOnDelete) {
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());

  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();
  context()->BindStorageArea(storage_key2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());
  area->Delete(key, value, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  RunUntilIdle();

  // Data from storage_key2 should exist, including meta-data, but nothing
  // should exist for storage_key1.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(4u, contents.size());
  for (const auto& entry : contents) {
    if (entry.first == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos,
              entry.first.find(storage_key1.origin().Serialize()));
    EXPECT_NE(std::string::npos,
              entry.first.find(storage_key2.origin().Serialize()));
  }
}

TEST_F(LocalStorageImplTest, MetaDataClearedOnDeleteAll) {
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());

  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();
  context()->BindStorageArea(storage_key2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();

  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());
  area->DeleteAll("source", mojo::NullRemote(), base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  RunUntilIdle();

  // Data from storage_key2 should exist, including meta-data, but nothing
  // should exist for storage_key1.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(4u, contents.size());
  for (const auto& entry : contents) {
    if (entry.first == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos,
              entry.first.find(storage_key1.origin().Serialize()));
    EXPECT_NE(std::string::npos,
              entry.first.find(storage_key2.origin().Serialize()));
  }
}

TEST_F(LocalStorageImplTest, DeleteStorage) {
  SetDatabaseEntry("VERSION", "1");
  SetDatabaseEntry(std::string("_http://foobar.com") + '\x00' + "key", "value");

  ResetStorage(storage_path());
  base::RunLoop run_loop;
  context()->DeleteStorage(
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com"),
      run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(1u, GetDatabaseContents().size());
}

TEST_F(LocalStorageImplTest, DeleteStorageWithoutConnection) {
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());

  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();

  context()->BindStorageArea(storage_key2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  RunUntilIdle();
  EXPECT_FALSE(GetDatabaseContents().empty());

  context()->DeleteStorage(storage_key1, base::DoNothing());
  RunUntilIdle();

  // Data from storage_key2 should exist, including meta-data, but nothing
  // should exist for storage_key1.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(4u, contents.size());
  for (const auto& entry : contents) {
    if (entry.first == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos,
              entry.first.find(storage_key1.origin().Serialize()));
    EXPECT_NE(std::string::npos,
              entry.first.find(storage_key2.origin().Serialize()));
  }
}

TEST_F(LocalStorageImplTest, DeleteStorageNotifiesWrapper) {
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());

  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();

  context()->BindStorageArea(storage_key2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  RunUntilIdle();
  EXPECT_FALSE(GetDatabaseContents().empty());

  TestLevelDBObserver observer;
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());
  area->AddObserver(observer.Bind());
  RunUntilIdle();

  context()->DeleteStorage(storage_key1, base::DoNothing());
  RunUntilIdle();

  ASSERT_EQ(1u, observer.observations().size());
  EXPECT_EQ(TestLevelDBObserver::Observation::kDeleteAll,
            observer.observations()[0].type);

  // Data from storage_key2 should exist, including meta-data, but nothing
  // should exist for storage_key1.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(4u, contents.size());
  for (const auto& entry : contents) {
    if (entry.first == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos,
              entry.first.find(storage_key1.origin().Serialize()));
    EXPECT_NE(std::string::npos,
              entry.first.find(storage_key2.origin().Serialize()));
  }
}

TEST_F(LocalStorageImplTest, DeleteStorageWithPendingWrites) {
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());

  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();

  context()->BindStorageArea(storage_key2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  RunUntilIdle();
  EXPECT_FALSE(GetDatabaseContents().empty());

  TestLevelDBObserver observer;
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());
  area->AddObserver(observer.Bind());
  area->Put(StdStringToUint8Vector("key2"), value, std::nullopt, "source",
            base::DoNothing());
  RunUntilIdle();

  context()->DeleteStorage(storage_key1, base::DoNothing());
  RunUntilIdle();

  ASSERT_EQ(2u, observer.observations().size());
  EXPECT_EQ(TestLevelDBObserver::Observation::kChange,
            observer.observations()[0].type);
  EXPECT_EQ(TestLevelDBObserver::Observation::kDeleteAll,
            observer.observations()[1].type);

  // Data from storage_key2 should exist, including meta-data, but nothing
  // should exist for storage_key1.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(4u, contents.size());
  for (const auto& entry : contents) {
    if (entry.first == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos,
              entry.first.find(storage_key1.origin().Serialize()));
    EXPECT_NE(std::string::npos,
              entry.first.find(storage_key2.origin().Serialize()));
  }
}

TEST_F(LocalStorageImplTest, ShutdownClearsData) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");
  blink::StorageKey storage_key1_third_party = blink::StorageKey::Create(
      url::Origin::Create(GURL("http://example1.com")),
      net::SchemefulSite(GURL("http://foobar.com")),
      blink::mojom::AncestorChainBit::kCrossSite);
  auto key1 = StdStringToUint8Vector("key1");
  auto key2 = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());

  area->Put(key1, value, std::nullopt, "source", base::DoNothing());
  area->Put(key2, value, std::nullopt, "source", base::DoNothing());
  area.reset();

  context()->BindStorageArea(storage_key2, area.BindNewPipeAndPassReceiver());
  area->Put(key2, value, std::nullopt, "source", base::DoNothing());
  area.reset();

  context()->BindStorageArea(storage_key1_third_party,
                             area.BindNewPipeAndPassReceiver());
  area->Put(key1, value, std::nullopt, "source", base::DoNothing());

  // Make sure all data gets committed to the DB.
  RunUntilIdle();

  std::vector<mojom::StoragePolicyUpdatePtr> updates;
  updates.emplace_back(mojom::StoragePolicyUpdate::New(
      storage_key1.origin(), /*purge_on_shutdown=*/true));
  context()->ApplyPolicyUpdates(std::move(updates));

  // Data from storage_key2 should exist, including meta-data, but nothing
  // should exist for storage_key1.
  // Data from storage_key1_third_party should also be erased, since it is
  // a third party storage key, and its top_level_site matches the origin
  // of storage_key1, which is set to purge on shutdown.
  ResetStorage(storage_path());
  auto contents = GetDatabaseContents();
  EXPECT_EQ(4u, contents.size());
  for (const auto& entry : contents) {
    if (entry.first == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos,
              entry.first.find(storage_key1.origin().Serialize()));
    EXPECT_EQ(std::string::npos,
              entry.first.find(storage_key1_third_party.origin().Serialize()));
    EXPECT_NE(std::string::npos,
              entry.first.find(storage_key2.origin().Serialize()));
  }
}

TEST_F(LocalStorageImplTest, InMemory) {
  ResetStorage(base::FilePath());
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com"),
      area.BindNewPipeAndPassReceiver());
  DoTestPut(key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(key, &result));
  EXPECT_EQ(value, result);

  // Should not have created any files.
  ShutDownStorage();
  EXPECT_TRUE(FirstEntryInDir().empty());

  // Re-opening should get fresh data.
  InitializeStorage(base::FilePath());
  EXPECT_FALSE(DoTestGet(key, &result));
}

TEST_F(LocalStorageImplTest, InMemoryInvalidPath) {
  ResetStorage(base::FilePath(FILE_PATH_LITERAL("../../")));
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com"),
      area.BindNewPipeAndPassReceiver());

  DoTestPut(key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(key, &result));
  EXPECT_EQ(value, result);

  ShutDownStorage();

  // Should not have created any files.
  EXPECT_TRUE(FirstEntryInDir().empty());
}

TEST_F(LocalStorageImplTest, OnDisk) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  DoTestPut(key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(key, &result));
  EXPECT_EQ(value, result);

  ShutDownStorage();

  // Should have created files.
  EXPECT_EQ(base::FilePath(kLocalStoragePath), FirstEntryInDir().BaseName());

  // Should be able to re-open.
  InitializeStorage(storage_path());
  EXPECT_TRUE(DoTestGet(key, &result));
  EXPECT_EQ(value, result);
}

TEST_F(LocalStorageImplTest, InvalidVersionOnDisk) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  DoTestPut(key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(key, &result));
  EXPECT_EQ(value, result);

  ShutDownStorage();

  {
    // Mess up version number in database.
    leveldb_env::ChromiumEnv env;
    std::unique_ptr<leveldb::DB> db;
    leveldb_env::Options options;
    options.env = &env;
    base::FilePath db_path = storage_path()
                                 .Append(kLocalStoragePath)
                                 .AppendASCII(kLocalStorageLeveldbName);
    ASSERT_TRUE(leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db).ok());
    ASSERT_TRUE(db->Put(leveldb::WriteOptions(), "VERSION", "argh").ok());
  }

  // Make sure data is gone.
  InitializeStorage(storage_path());
  EXPECT_FALSE(DoTestGet(key, &result));

  // Write data again.
  DoTestPut(key, value);

  // Data should have been preserved now.
  ResetStorage(storage_path());
  EXPECT_TRUE(DoTestGet(key, &result));
  EXPECT_EQ(value, result);
}

TEST_F(LocalStorageImplTest, CorruptionOnDisk) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  DoTestPut(key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(key, &result));
  EXPECT_EQ(value, result);

  ShutDownStorage();

  // Delete manifest files to mess up opening DB.
  base::FilePath db_path = storage_path()
                               .Append(kLocalStoragePath)
                               .AppendASCII(kLocalStorageLeveldbName);
  base::FileEnumerator file_enum(db_path, true, base::FileEnumerator::FILES,
                                 FILE_PATH_LITERAL("MANIFEST*"));
  for (base::FilePath name = file_enum.Next(); !name.empty();
       name = file_enum.Next()) {
    base::DeleteFile(name);
  }

  // Make sure data is gone.
  InitializeStorage(storage_path());
  EXPECT_FALSE(DoTestGet(key, &result));

  // Write data again.
  DoTestPut(key, value);

  // Data should have been preserved now.
  ResetStorage(storage_path());
  EXPECT_TRUE(DoTestGet(key, &result));
  EXPECT_EQ(value, result);
}

TEST_F(LocalStorageImplTest, RecreateOnCommitFailure) {
  std::optional<base::RunLoop> open_loop;
  std::optional<base::RunLoop> destruction_loop;
  size_t num_database_open_requests = 0;
  context()->SetDatabaseOpenCallbackForTesting(base::BindLambdaForTesting([&] {
    ++num_database_open_requests;
    open_loop->Quit();
  }));

  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  open_loop.emplace();

  // Open three connections to the database. Two to the same StorageKey, and a
  // third to a different StorageKey.
  mojo::Remote<blink::mojom::StorageArea> area1;
  mojo::Remote<blink::mojom::StorageArea> area2;
  mojo::Remote<blink::mojom::StorageArea> area3;

  context()->BindStorageArea(
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com"),
      area1.BindNewPipeAndPassReceiver());
  context()->BindStorageArea(
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com"),
      area2.BindNewPipeAndPassReceiver());
  context()->BindStorageArea(
      blink::StorageKey::CreateFromStringForTesting("http://example.com"),
      area3.BindNewPipeAndPassReceiver());
  open_loop->Run();

  // Add observers to the first two connections.
  TestLevelDBObserver observer1;
  area1->AddObserver(observer1.Bind());
  TestLevelDBObserver observer2;
  area2->AddObserver(observer2.Bind());

  // Verify one attempt was made to open the database.
  ASSERT_EQ(1u, num_database_open_requests);

  // This loop will be Quit if and when the current database instance is
  // destroyed, which should happen after many commit failures.
  destruction_loop.emplace();

  bool first_database_destroyed = false;
  context()->GetDatabaseForTesting().PostTaskWithThisObject(
      base::BindLambdaForTesting([&](DomStorageDatabase* db) {
        db->MakeAllCommitsFailForTesting();
        db->SetDestructionCallbackForTesting(base::BindLambdaForTesting([&] {
          first_database_destroyed = true;
          destruction_loop->Quit();
        }));
      }));

  // Also prepare for another database connection, this time without making
  // commits fail.
  open_loop.emplace();
  num_database_open_requests = 0;
  context()->SetDatabaseOpenCallbackForTesting(base::BindLambdaForTesting([&] {
    ++num_database_open_requests;
    open_loop->Quit();
  }));

  // Start a put operation on the third connection before starting to commit
  // a lot of data on the first StorageKey. This put operation should result in
  // a pending commit that will get cancelled when the database is destroyed.
  area3->Put(key, value, std::nullopt, "source",
             base::BindOnce([](bool success) { EXPECT_TRUE(success); }));

  // Repeatedly write data to the database, to trigger enough commit errors.
  size_t values_written = 0;
  while (area1.is_connected()) {
    // Every write needs to be different to make sure there actually is a
    value[0]++;
    area1->Put(key, value, std::nullopt, "source",
               base::BindLambdaForTesting([&](bool success) {
                 EXPECT_TRUE(success);
                 values_written++;
               }));
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context()->FlushStorageKeyForTesting(blink::StorageKey(
        blink::StorageKey::CreateFromStringForTesting("http://foobar.com")));
  }
  area1.reset();

  // Wait for LocalStorageImpl to try to reconnect to the database, and
  // Enough commit failures should happen during this loop to cause the database
  // to be destroyed.
  destruction_loop->Run();
  EXPECT_TRUE(first_database_destroyed);

  // The connection to the second area should end up closed as well.
  area2.FlushForTesting();
  EXPECT_FALSE(area2.is_connected());

  // Reconnect |area1| to the database, and try to read a value.
  context()->BindStorageArea(
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com"),
      area1.BindNewPipeAndPassReceiver());
  base::RunLoop delete_loop;
  bool success = true;
  TestLevelDBObserver observer3;
  area1->AddObserver(observer3.Bind());
  area1->Delete(key, std::nullopt, "source",
                base::BindLambdaForTesting([&](bool success_in) {
                  success = success_in;
                  delete_loop.Quit();
                }));

  // The new database should be ready to go.
  open_loop->Run();
  ASSERT_EQ(1u, num_database_open_requests);

  delete_loop.Run();
  area1.reset();

  {
    // Committing data should now work.
    DoTestPut(key, value);
    std::vector<uint8_t> result;
    EXPECT_TRUE(DoTestGet(key, &result));
    EXPECT_EQ(value, result);
  }

  // Observers should have seen one Add event and a number of Change events for
  // all commits until the connection was closed.
  ASSERT_EQ(values_written, observer2.observations().size());
  for (size_t i = 0; i < values_written; ++i) {
    EXPECT_EQ(TestLevelDBObserver::Observation::kChange,
              observer2.observations()[i].type);
    EXPECT_EQ(Uint8VectorToStdString(key), observer2.observations()[i].key);
  }
}

TEST_F(LocalStorageImplTest, DontRecreateOnRepeatedCommitFailure) {
  // Ensure that the opened database always fails on write.
  std::optional<base::RunLoop> open_loop;
  size_t num_database_open_requests = 0;
  size_t num_databases_destroyed = 0;
  context()->SetDatabaseOpenCallbackForTesting(base::BindLambdaForTesting([&] {
    ++num_database_open_requests;
    open_loop->Quit();
  }));
  open_loop.emplace();

  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  // Open a connection to the database.
  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com"),
      area.BindNewPipeAndPassReceiver());
  open_loop->Run();

  // Ensure that all commits fail on the database, and that we observe its
  // destruction.
  context()->GetDatabaseForTesting().PostTaskWithThisObject(
      base::BindLambdaForTesting([&](DomStorageDatabase* db) {
        db->MakeAllCommitsFailForTesting();
        db->SetDestructionCallbackForTesting(
            base::BindLambdaForTesting([&] { ++num_databases_destroyed; }));
      }));

  // Verify one attempt was made to open the database.
  ASSERT_EQ(1u, num_database_open_requests);

  // Setup a new RunLoop so we can wait until LocalStorageImpl tries to
  // reconnect to the database, which should happen after several commit
  // errors.
  open_loop.emplace();

  // Repeatedly write data to the database, to trigger enough commit errors.
  std::optional<std::vector<uint8_t>> old_value;
  while (area.is_connected()) {
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    value[0]++;
    area->Put(key, value, old_value, "source",
              base::BindLambdaForTesting(
                  [&](bool success) { EXPECT_TRUE(success); }));
    old_value = std::vector<uint8_t>(value);
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context()->FlushStorageKeyForTesting(blink::StorageKey(
        blink::StorageKey::CreateFromStringForTesting("http://foobar.com")));
  }
  area.reset();

  // Wait for LocalStorageImpl to try to reconnect to the database, and
  // connect that new request with a database implementation that always fails
  // on write.
  context()->SetDatabaseOpenCallbackForTesting(base::BindLambdaForTesting([&] {
    ++num_database_open_requests;
    open_loop->Quit();
  }));
  open_loop->Run();
  EXPECT_EQ(2u, num_database_open_requests);
  EXPECT_EQ(1u, num_databases_destroyed);
  context()->GetDatabaseForTesting().PostTaskWithThisObject(base::BindOnce(
      [](DomStorageDatabase* db) { db->MakeAllCommitsFailForTesting(); }));

  // Reconnect a area to the database, and repeatedly write data to it again.
  // This time all should just keep getting written, and commit errors are
  // getting ignored.
  context()->BindStorageArea(
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com"),
      area.BindNewPipeAndPassReceiver());
  old_value = std::nullopt;
  for (int i = 0; i < 64; ++i) {
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    value[0]++;
    area->Put(key, value, old_value, "source",
              base::BindLambdaForTesting(
                  [&](bool success) { EXPECT_TRUE(success); }));
    RunUntilIdle();
    old_value = value;
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context()->FlushStorageKeyForTesting(blink::StorageKey(
        blink::StorageKey::CreateFromStringForTesting("http://foobar.com")));
  }

  // Should still be connected after all that.
  RunUntilIdle();
  EXPECT_TRUE(area.is_connected());
}

class LocalStorageImplStaleDeletionTest
    : public LocalStorageImplTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  LocalStorageImplStaleDeletionTest() {
    feature_list_.InitWithFeatureStates(
        {{kDeleteStaleLocalStorageOnStartup,
          ShouldDeleteStaleLocalStorageOnStartup()},
         {kDeleteOrphanLocalStorageOnStartup,
          ShouldDeleteOrphanLocalStorageOnStartup()}});
  }

  bool ShouldDeleteStaleLocalStorageOnStartup() {
    return std::get<0>(GetParam());
  }

  bool ShouldDeleteOrphanLocalStorageOnStartup() {
    return std::get<1>(GetParam());
  }

  void UpdateAccessMetaData(const blink::StorageKey& storage_key,
                            const base::Time& last_accessed) {
    storage::LocalStorageAreaAccessMetaData data;
    data.set_last_accessed(last_accessed.ToInternalValue());
    SetDatabaseEntry("METAACCESS:" + storage_key.SerializeForLocalStorage(),
                     data.SerializeAsString());
  }

  void UpdateWriteMetaData(const blink::StorageKey& storage_key,
                           const base::Time& last_modified,
                           uint64_t size_bytes) {
    storage::LocalStorageAreaWriteMetaData data;
    data.set_last_modified(last_modified.ToInternalValue());
    data.set_size_bytes(size_bytes);
    SetDatabaseEntry("META:" + storage_key.SerializeForLocalStorage(),
                     data.SerializeAsString());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    LocalStorageImplStaleDeletionTest,
    testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(LocalStorageImplStaleDeletionTest, StaleStorageAreaDeletion) {
  const auto storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foo.com");
  const auto storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://bar.com");
  const auto storage_key3 =
      blink::StorageKey::CreateFromStringForTesting("http://baz.com");
  const auto storage_key4 =
      blink::StorageKey::CreateFromStringForTesting("http://qux.com");
  const auto storage_key5 =
      blink::StorageKey::CreateFromStringForTesting("http://cor.com");
  mojo::Remote<blink::mojom::StorageArea> area;

  // Load data into all storage areas.
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());
  area->Put(StdStringToUint8Vector("key"), StdStringToUint8Vector("value"),
            std::nullopt, "source", base::DoNothing());
  area.reset();
  context()->BindStorageArea(storage_key2, area.BindNewPipeAndPassReceiver());
  area->Put(StdStringToUint8Vector("key"), StdStringToUint8Vector("value"),
            std::nullopt, "source", base::DoNothing());
  area.reset();
  context()->BindStorageArea(storage_key3, area.BindNewPipeAndPassReceiver());
  area->Put(StdStringToUint8Vector("key"), StdStringToUint8Vector("value"),
            std::nullopt, "source", base::DoNothing());
  area.reset();
  context()->BindStorageArea(storage_key4, area.BindNewPipeAndPassReceiver());
  area->Put(StdStringToUint8Vector("key"), StdStringToUint8Vector("value"),
            std::nullopt, "source", base::DoNothing());
  area.reset();
  context()->BindStorageArea(storage_key5, area.BindNewPipeAndPassReceiver());
  area->Put(StdStringToUint8Vector("key"), StdStringToUint8Vector("value"),
            std::nullopt, "source", base::DoNothing());
  area.reset();
  RunUntilIdle();
  auto contents = GetDatabaseContents();
  EXPECT_EQ(16u, contents.size());

  // Backdate metadata accessed and modified times so that storage_key3 and
  // storage_key4 should be purged, while storage_key1 and storage_key2 should
  // not. storage_key5 is left alone to test the default codepath.
  UpdateAccessMetaData(storage_key1, base::Time::Now() - base::Days(401));
  UpdateWriteMetaData(storage_key2, base::Time::Now() - base::Days(401), 0);
  UpdateAccessMetaData(storage_key3, base::Time::Now() - base::Days(401));
  UpdateWriteMetaData(storage_key3, base::Time::Now() - base::Days(401), 0);
  UpdateAccessMetaData(storage_key4, base::Time::Now() - base::Days(401));
  UpdateWriteMetaData(storage_key4, base::Time::Now() - base::Days(401), 0);
  RunUntilIdle();

  // Restart local storage, force bind area for storage_key3, and trigger stale
  // storage area purging.
  ResetStorage(storage_path());
  context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
  context()->ForceFakeOpenStorageAreaForTesting(storage_key3);
  WaitForDatabaseOpen();
  RunUntilIdle();

  // We should see that only the data for storage_key4 was cleared.
  contents = GetDatabaseContents();
  if (ShouldDeleteStaleLocalStorageOnStartup()) {
    EXPECT_EQ(13u, contents.size());
    for (const auto& entry : contents) {
      EXPECT_EQ(entry.first.find(storage_key4.origin().Serialize()),
                std::string::npos);
    }
  } else {
    EXPECT_EQ(16u, contents.size());
  }
}

TEST_P(LocalStorageImplStaleDeletionTest, Orphan) {
  // Nothing should be orphaned initially.
  mojo::Remote<blink::mojom::StorageArea> area;
  {
    base::HistogramTester histograms;
    ResetStorage(storage_path());
    context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
    WaitForDatabaseOpen();
    RunUntilIdle();
    EXPECT_EQ(0, histograms.GetTotalSum(
                     "LocalStorage.OrphanStorageAreasOnStartupCount"));
  }

  // First party bucket doesn't qualify, even if it's old.
  const auto first_party_key =
      blink::StorageKey::CreateFromStringForTesting("http://firstparty/");
  context()->BindStorageArea(first_party_key,
                             area.BindNewPipeAndPassReceiver());
  area->Put(StdStringToUint8Vector("key"), StdStringToUint8Vector("value"),
            std::nullopt, "source", base::DoNothing());
  area.FlushForTesting();
  area.reset();
  RunUntilIdle();
  {
    base::HistogramTester histograms;
    ResetStorage(storage_path());
    context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
    WaitForDatabaseOpen();
    RunUntilIdle();
    EXPECT_EQ(0, histograms.GetTotalSum(
                     "LocalStorage.OrphanStorageAreasOnStartupCount"));
    EXPECT_EQ(4u, GetDatabaseContents().size());

    UpdateAccessMetaData(first_party_key, base::Time::Now() - base::Days(2));
    UpdateWriteMetaData(first_party_key, base::Time::Now() - base::Days(2), 0);
    context()->FlushStorageKeyForTesting(first_party_key);
    ResetStorage(storage_path());
    context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
    WaitForDatabaseOpen();
    RunUntilIdle();
    EXPECT_EQ(0, histograms.GetTotalSum(
                     "LocalStorage.OrphanStorageAreasOnStartupCount"));
    EXPECT_EQ(4u, GetDatabaseContents().size());
  }

  // First party nonce bucket does qualify, but only if it's old.
  const auto first_party_nonce_key = blink::StorageKey::CreateWithNonce(
      url::Origin::Create(GURL("http://firstpartynonce/")),
      base::UnguessableToken::Create());
  context()->BindStorageArea(first_party_nonce_key,
                             area.BindNewPipeAndPassReceiver());
  area->Put(StdStringToUint8Vector("key"), StdStringToUint8Vector("value"),
            std::nullopt, "source", base::DoNothing());
  area.FlushForTesting();
  area.reset();
  RunUntilIdle();
  {
    base::HistogramTester histograms;
    ResetStorage(storage_path());
    context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
    WaitForDatabaseOpen();
    RunUntilIdle();
    EXPECT_EQ(0, histograms.GetTotalSum(
                     "LocalStorage.OrphanStorageAreasOnStartupCount"));
    EXPECT_EQ(7u, GetDatabaseContents().size());

    UpdateAccessMetaData(first_party_nonce_key,
                         base::Time::Now() - base::Days(2));
    UpdateWriteMetaData(first_party_nonce_key,
                        base::Time::Now() - base::Days(2), 0);
    context()->FlushStorageKeyForTesting(first_party_nonce_key);
    ResetStorage(storage_path());
    context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
    WaitForDatabaseOpen();
    RunUntilIdle();
    EXPECT_EQ((ShouldDeleteStaleLocalStorageOnStartup() &&
               ShouldDeleteOrphanLocalStorageOnStartup())
                  ? 1
                  : 0,
              histograms.GetTotalSum(
                  "LocalStorage.OrphanStorageAreasOnStartupCount"));
    EXPECT_EQ((ShouldDeleteStaleLocalStorageOnStartup() &&
               ShouldDeleteOrphanLocalStorageOnStartup())
                  ? 4u
                  : 7u,
              GetDatabaseContents().size());
  }

  // Third party bucket doesn't qualify, even if it's old.
  const auto third_party_key = blink::StorageKey::Create(
      url::Origin::Create(GURL("https://thirdparty/")),
      net::SchemefulSite(GURL("https://thirdparty2/")),
      blink::mojom::AncestorChainBit::kCrossSite);
  context()->BindStorageArea(third_party_key,
                             area.BindNewPipeAndPassReceiver());
  area->Put(StdStringToUint8Vector("key"), StdStringToUint8Vector("value"),
            std::nullopt, "source", base::DoNothing());
  area.FlushForTesting();
  area.reset();
  RunUntilIdle();
  {
    base::HistogramTester histograms;
    ResetStorage(storage_path());
    context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
    WaitForDatabaseOpen();
    RunUntilIdle();
    EXPECT_EQ(0, histograms.GetTotalSum(
                     "LocalStorage.OrphanStorageAreasOnStartupCount"));
    EXPECT_EQ((ShouldDeleteStaleLocalStorageOnStartup() &&
               ShouldDeleteOrphanLocalStorageOnStartup())
                  ? 7u
                  : 10u,
              GetDatabaseContents().size());

    UpdateAccessMetaData(third_party_key, base::Time::Now() - base::Days(2));
    UpdateWriteMetaData(third_party_key, base::Time::Now() - base::Days(2), 0);
    context()->FlushStorageKeyForTesting(third_party_key);
    ResetStorage(storage_path());
    context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
    WaitForDatabaseOpen();
    RunUntilIdle();
    EXPECT_EQ(0, histograms.GetTotalSum(
                     "LocalStorage.OrphanStorageAreasOnStartupCount"));
    EXPECT_EQ((ShouldDeleteStaleLocalStorageOnStartup() &&
               ShouldDeleteOrphanLocalStorageOnStartup())
                  ? 7u
                  : 10u,
              GetDatabaseContents().size());
  }

  // Third party nonce bucket does qualify, but only if it's old.
  const auto third_party_nonce_key = blink::StorageKey::Create(
      url::Origin::Create(GURL("https://thirdparty/")),
      net::SchemefulSite(url::Origin::Create(GURL("http://thirdparty2/"))
                             .DeriveNewOpaqueOrigin()),
      blink::mojom::AncestorChainBit::kCrossSite);
  context()->BindStorageArea(third_party_nonce_key,
                             area.BindNewPipeAndPassReceiver());
  area->Put(StdStringToUint8Vector("key"), StdStringToUint8Vector("value"),
            std::nullopt, "source", base::DoNothing());
  area.FlushForTesting();
  area.reset();
  RunUntilIdle();
  {
    base::HistogramTester histograms;
    ResetStorage(storage_path());
    context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
    WaitForDatabaseOpen();
    RunUntilIdle();
    EXPECT_EQ(0, histograms.GetTotalSum(
                     "LocalStorage.OrphanStorageAreasOnStartupCount"));
    EXPECT_EQ((ShouldDeleteStaleLocalStorageOnStartup() &&
               ShouldDeleteOrphanLocalStorageOnStartup())
                  ? 10u
                  : 13u,
              GetDatabaseContents().size());

    UpdateAccessMetaData(third_party_nonce_key,
                         base::Time::Now() - base::Days(2));
    UpdateWriteMetaData(third_party_nonce_key,
                        base::Time::Now() - base::Days(2), 0);
    context()->FlushStorageKeyForTesting(third_party_nonce_key);
    ResetStorage(storage_path());
    context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
    WaitForDatabaseOpen();
    RunUntilIdle();
    EXPECT_EQ((ShouldDeleteStaleLocalStorageOnStartup() &&
               ShouldDeleteOrphanLocalStorageOnStartup())
                  ? 1
                  : 0,
              histograms.GetTotalSum(
                  "LocalStorage.OrphanStorageAreasOnStartupCount"));
    EXPECT_EQ((ShouldDeleteStaleLocalStorageOnStartup() &&
               ShouldDeleteOrphanLocalStorageOnStartup())
                  ? 7u
                  : 13u,
              GetDatabaseContents().size());
  }
}

}  // namespace storage
