// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/local_storage_impl.h"

#include <string_view>
#include <tuple>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "components/services/storage/dom_storage/test_support/storage_area_test_util.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/leveldatabase/env_chromium.h"
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

class TestStorageAreaObserver : public blink::mojom::StorageAreaObserver {
 public:
  struct Observation {
    enum { kChange, kChangeFailed, kDelete, kDeleteAll } type;
    std::string key;
    std::optional<std::string> old_value;
    std::string new_value;
    std::string source;
  };

  TestStorageAreaObserver() = default;

  mojo::PendingRemote<blink::mojom::StorageAreaObserver> Bind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  const std::vector<Observation>& observations() { return observations_; }

  void FlushForTesting() { receiver_.FlushForTesting(); }

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

// Base test fixture for `LocalStorageImpl` tests. Provides common setup
// including database initialization, storage area binding, and helper methods
// for reading/writing map entries and metadata. Subclasses can parameterize
// tests to run on SQLite or LevelDB using `is_sqlite_enabled` when constructing
// `LocalStorageImplTestBase`.
class LocalStorageImplTestBase : public testing::Test {
 public:
  explicit LocalStorageImplTestBase(bool is_sqlite_enabled) {
    feature_list_.InitWithFeatureState(kDomStorageSqlite, is_sqlite_enabled);
    EXPECT_TRUE(temp_path_.CreateUniqueTempDir());
  }

  LocalStorageImplTestBase(const LocalStorageImplTestBase&) = delete;
  LocalStorageImplTestBase& operator=(const LocalStorageImplTestBase&) = delete;

  ~LocalStorageImplTestBase() override {
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
    storage_ =
        std::make_unique<LocalStorageImpl>(path, base::NullCallback(),
                                           /*receiver=*/mojo::NullReceiver());
  }

  // Resets `storage_` and waits for database shutdown tasks to finish.
  void ShutDownStorage() {
    if (!storage_) {
      return;
    }

    scoped_refptr<base::SequencedTaskRunner> db_task_runner;
    // If the database was never opened, no need to wait for it to close.
    if (context()->GetDatabaseForTesting()) {
      base::RunLoop loop;
      context()->GetDatabaseForTesting()->database().PostTaskWithThisObject(
          base::BindLambdaForTesting(
              [&](DomStorageDatabase* dom_storage_database) {
                db_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
                loop.Quit();
              }));
      loop.Run();
    }
    storage_.reset();
    if (db_task_runner) {
      base::RunLoop flush_db;
      db_task_runner->PostTask(FROM_HERE, flush_db.QuitClosure());
      flush_db.Run();
    }
  }

  void ResetStorage(const base::FilePath& path) {
    ShutDownStorage();
    InitializeStorage(path);
  }

  void WaitForDatabaseOpen() {
    base::RunLoop loop;
    context()->SetDatabaseOpenCallbackForTesting(loop.QuitClosure());
    loop.Run();
  }

  // Adds or updates a key/value pair in the map for `storage_key`.
  void PutMapKeyValue(const blink::StorageKey& storage_key,
                      DomStorageDatabase::Key key,
                      DomStorageDatabase::Key value) {
    FakeCommitter committer(context()->GetDatabaseForTesting(),
                            DomStorageDatabase::MapLocator(storage_key));
    committer.PutMapKeyValueSync(std::move(key), std::move(value));
  }

  // Use `AsyncDomStorageDatabase::DeleteStorageKeysFromSession()` to delete all
  // local storage key/value pairs and metadata from the database.
  void ClearDatabase() {
    AsyncDomStorageDatabase& database = *context()->GetDatabaseForTesting();

    // Enumerate all of the storage keys and maps to delete.
    DomStorageDatabase::Metadata all_metadata;
    ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(database, &all_metadata));

    std::vector<blink::StorageKey> storage_keys_to_delete;
    std::vector<DomStorageDatabase::MapLocator> maps_to_delete;

    for (const DomStorageDatabase::MapMetadata& map_metadata :
         all_metadata.map_metadata) {
      DomStorageDatabase::MapLocator map_to_delete =
          map_metadata.map_locator.Clone();
      storage_keys_to_delete.push_back(map_to_delete.storage_key());
      maps_to_delete.push_back(std::move(map_to_delete));
    }

    // Delete all of the storage keys and maps.
    ASSERT_NO_FATAL_FAILURE(DeleteStorageKeysFromSessionSync(
        database, /*session_id=*/std::string(),
        std::move(storage_keys_to_delete), std::move(maps_to_delete)));

    // Verify that no maps key/values or metadata exists in the database.
    DomStorageDatabase::Metadata empty_metadata;
    ReadAllMetadataSync(database, &empty_metadata);
    EXPECT_EQ(empty_metadata.map_metadata.size(), 0u);

    for (const DomStorageDatabase::MapMetadata& map_metadata :
         all_metadata.map_metadata) {
      std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
          empty_entries;
      ASSERT_NO_FATAL_FAILURE(ReadMapKeyValuesSync(
          database, map_metadata.map_locator.Clone(), &empty_entries));
      EXPECT_EQ(empty_entries.size(), 0u);
    }
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
    return test::GetSync(area.get(), key);
  }

  // Pumps both the main-thread sequence and the background database sequence
  // until both are idle. Prefer other means of waiting, such as `RunUntil` or
  // `TestFuture`.
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void DoTestPut(const std::vector<uint8_t>& key,
                 const std::vector<uint8_t>& value) {
    mojo::Remote<blink::mojom::StorageArea> area;
    context()->BindStorageArea(
        blink::StorageKey::CreateFromStringForTesting("http://foobar.com"),
        area.BindNewPipeAndPassReceiver());
    base::test::TestFuture<bool> success_future;
    area->Put(key, value, std::nullopt, "source", success_future.GetCallback());
    EXPECT_TRUE(success_future.Take());
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

  // Verifies a storage key's map in the database contains `expected_entries`.
  void ExpectMapEquals(blink::StorageKey storage_key,
                       std::map<DomStorageDatabase::Key,
                                DomStorageDatabase::Value> expected_entries) {
    DomStorageDatabase::MapLocator map_locator{storage_key};
    std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> actual_entries;

    ASSERT_NO_FATAL_FAILURE(
        ReadMapKeyValuesSync(*context()->GetDatabaseForTesting(),
                             std::move(map_locator), &actual_entries));
    EXPECT_EQ(actual_entries, expected_entries);
  }

  // Run until `expected_entries` exist in the database for storage key's map.
  void WaitForMapEntries(blink::StorageKey storage_key,
                         std::map<DomStorageDatabase::Key,
                                  DomStorageDatabase::Value> expected_entries) {
    DomStorageDatabase::MapLocator map_locator{storage_key};
    std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> actual_entries;

    EXPECT_TRUE(base::test::RunUntil([&]() {
      actual_entries.clear();
      ReadMapKeyValuesSync(*context()->GetDatabaseForTesting(),
                           std::move(map_locator), &actual_entries);
      return actual_entries.size() == expected_entries.size();
    }));

    EXPECT_EQ(actual_entries, expected_entries);
  }

  // Gets the map usage metadata for `storage_key` from the database.  `result`
  // is `std::nullopt` when no metadata for `storage_key` exists.
  void FindUsageMetadata(
      blink::StorageKey storage_key,
      std::optional<DomStorageDatabase::MapMetadata>* result) {
    *result = std::nullopt;

    DomStorageDatabase::Metadata all_metadata;
    ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(
        *context()->GetDatabaseForTesting(), &all_metadata));

    for (DomStorageDatabase::MapMetadata& usage_metadata :
         all_metadata.map_metadata) {
      if (usage_metadata.map_locator.storage_key() == storage_key) {
        *result = std::move(usage_metadata);
        break;
      }
    }
  }

  // Verifies map usage metadata for `storage_key` exists and is not null in the
  // database.
  void ExpectUsageMetadataExists(blink::StorageKey storage_key) {
    std::optional<DomStorageDatabase::MapMetadata> usage_metadata;
    ASSERT_NO_FATAL_FAILURE(FindUsageMetadata(storage_key, &usage_metadata));
    ASSERT_NE(usage_metadata, std::nullopt);

    EXPECT_NE(usage_metadata->last_accessed, std::nullopt);
    EXPECT_NE(usage_metadata->last_modified, std::nullopt);
    EXPECT_NE(usage_metadata->total_size, std::nullopt);
  }

  // Verifies the number storage keys with map usage metadata in the database.
  void ExpectUsageMetadataCount(size_t expected_count) {
    DomStorageDatabase::Metadata all_metadata;
    ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(
        *context()->GetDatabaseForTesting(), &all_metadata));
    EXPECT_EQ(all_metadata.map_metadata.size(), expected_count);
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

  // Enables or disables SQLite.
  base::test::ScopedFeatureList feature_list_;

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_path_;

  std::unique_ptr<LocalStorageImpl> storage_;
};

class LocalStorageImplTest
    : public testing::WithParamInterface</*is_sqlite_enabled=*/bool>,
      public LocalStorageImplTestBase {
 public:
  LocalStorageImplTest()
      : LocalStorageImplTestBase(/*is_sqlite_enabled=*/GetParam()) {}
  ~LocalStorageImplTest() override = default;

  bool IsSqliteEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    LocalStorageImplTest,
    testing::Bool(),
    /*name_generator=*/
    [](const testing::TestParamInfo<LocalStorageImplTest::ParamType>& info) {
      return info.param ? "SQLite" : "LevelDB";
    });

TEST_P(LocalStorageImplTest, Basic) {
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(storage_key, area.BindNewPipeAndPassReceiver());

  // Start histogram recording after setup to isolate the Put commit.
  base::HistogramTester histograms;

  base::test::TestFuture<bool> success_future;
  area->Put(key, value, std::nullopt, "source", success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  // This causes the changes to flush immediately rather than the default of 5
  // seconds.
  area.reset();

  // The database must contain the map's key/value pair and the usage metadata.
  ASSERT_NO_FATAL_FAILURE(
      WaitForMapEntries(storage_key, /*expected_entries=*/{{key, value}}));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(1u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key));

  // Verify the UpdateMaps histogram was recorded with success (sample 0 = kOk).
  histograms.ExpectUniqueSample("Storage.LocalStorage.UpdateMaps.OnDisk", 0, 1);
  // The Put and WaitForMapEntries each trigger a ReadMapKeyValues.
  histograms.ExpectUniqueSample("Storage.LocalStorage.ReadMapKeyValues.OnDisk",
                                0, 2);
}

TEST_P(LocalStorageImplTest, StorageKeysAreIndependent) {
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
  base::test::TestFuture<bool> success_future;
  area->Put(key2, value, std::nullopt, "source", success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());
  area.reset();

  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key1, /*expected_entries=*/{{key1, value}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key2, /*expected_entries=*/{{key2, value}}));

  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(2u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key1));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key2));
}

TEST_P(LocalStorageImplTest, WrapperOutlivesMojoConnection) {
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
  base::test::TestFuture<bool> success_future;
  area->Put(key, value, std::nullopt, "source", success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  area.reset();
  dummy_area.reset();

  // The database must contain the map's key/value pair and the usage metadata.
  ASSERT_NO_FATAL_FAILURE(
      WaitForMapEntries(storage_key, /*expected_entries=*/{{key, value}}));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(1u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key));

  // Clear all the data from the backing database.
  ASSERT_NO_FATAL_FAILURE(ClearDatabase());

  // Data should still be readable, because despite closing the area
  // connection above, the actual area instance should have been kept alive.
  EXPECT_EQ(value, DoTestGet(key));

  // Now purge memory.
  context()->PurgeMemory();

  // And make sure caches were actually cleared.
  EXPECT_EQ(std::nullopt, DoTestGet(key));
}

TEST_P(LocalStorageImplTest, OpeningWrappersPurgesInactiveWrappers) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");
  const blink::StorageKey storage_key(
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com"));

  // Write some data to the DB.
  mojo::Remote<blink::mojom::StorageArea> area;
  context()->BindStorageArea(storage_key, area.BindNewPipeAndPassReceiver());
  base::test::TestFuture<bool> success_future;
  area->Put(key, value, std::nullopt, "source", success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  area.reset();

  // The database must contain the map's key/value pair and the usage metadata.
  ASSERT_NO_FATAL_FAILURE(
      WaitForMapEntries(storage_key, /*expected_entries=*/{{key, value}}));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(1u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key));

  // Clear all the data from the backing database.
  ASSERT_NO_FATAL_FAILURE(ClearDatabase());

  // Now open many new areas (for different StorageKeys) to trigger clean up.
  for (int i = 1; i <= 100; ++i) {
    context()->BindStorageArea(
        blink::StorageKey::CreateFromStringForTesting(
            base::StringPrintf("http://example.com:%d", i)),
        area.BindNewPipeAndPassReceiver());
    area.reset();
  }

  // And make sure caches were actually cleared.
  EXPECT_TRUE(base::test::RunUntil([&]() { return !DoTestGet(key); }));
}

TEST_P(LocalStorageImplTest, ValidVersion) {
  DomStorageDatabase::Key key = StdStringToUint8Vector("key");
  DomStorageDatabase::Value value = StdStringToUint8Vector("value");

  WaitForDatabaseOpen();

  PutVersionForTesting(*context()->GetDatabaseForTesting(), 1);
  PutMapKeyValue(
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com"), key,
      value);

  ResetStorage(storage_path());
  EXPECT_EQ(value, DoTestGet(key));
}

TEST_P(LocalStorageImplTest, InvalidVersion) {
  DomStorageDatabase::Key key = StdStringToUint8Vector("key");
  DomStorageDatabase::Value value = StdStringToUint8Vector("value");

  WaitForDatabaseOpen();

  PutVersionForTesting(*context()->GetDatabaseForTesting(), 99999);
  PutMapKeyValue(
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com"), key,
      value);

  // Force the a reload of the database, which should fail due to invalid
  // version data.
  ResetStorage(storage_path());
  EXPECT_EQ(std::nullopt, DoTestGet(key));
}

TEST_P(LocalStorageImplTest, GetStorageUsage_NoData) {
  std::vector<mojom::StorageUsageInfoPtr> info = GetStorageUsageSync();
  EXPECT_EQ(0u, info.size());
}

TEST_P(LocalStorageImplTest, GetStorageUsage_Data) {
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
  base::test::TestFuture<bool> success_future;
  area->Put(key2, value, std::nullopt, "source", success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());
  area.reset();

  // Make sure all data gets committed to disk.
  ASSERT_NO_FATAL_FAILURE(ExpectMapEquals(
      storage_key1, /*expected_entries=*/{{key1, value}, {key2, value}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key2, /*expected_entries=*/{{key2, value}}));

  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(2u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key1));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key2));

  base::Time after_write = base::Time::Now();

  base::HistogramTester histograms;
  std::vector<mojom::StorageUsageInfoPtr> info = GetStorageUsageSync();
  ASSERT_EQ(2u, info.size());
  if (info[0]->storage_key == storage_key2) {
    std::swap(info[0], info[1]);
  }
  EXPECT_EQ(storage_key1, info[0]->storage_key);
  EXPECT_EQ(storage_key2, info[1]->storage_key);
  EXPECT_LE(before_write, info[0]->last_modified);
  EXPECT_LE(before_write, info[1]->last_modified);
  EXPECT_GE(after_write, info[0]->last_modified);
  EXPECT_GE(after_write, info[1]->last_modified);
  EXPECT_GT(info[0]->total_size_bytes, info[1]->total_size_bytes);

  // GetStorageUsageSync() results in a ReadAllMetadata call.
  histograms.ExpectUniqueSample("Storage.LocalStorage.ReadAllMetadata.OnDisk",
                                0, 1);
}

TEST_P(LocalStorageImplTest, CheckAccessMetaData) {
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

  // storage_key2 has content in its area.
  context()->BindStorageArea(storage_key2, area.BindNewPipeAndPassReceiver());
  area->Put(StdStringToUint8Vector("key"), StdStringToUint8Vector("value"),
            std::nullopt, "source", base::DoNothing());
  area.reset();

  // storage_key3 has content in its area but is purged on shutdown.
  context()->BindStorageArea(storage_key3, area.BindNewPipeAndPassReceiver());
  base::test::TestFuture<bool> success_future;
  area->Put(StdStringToUint8Vector("key"), StdStringToUint8Vector("value"),
            std::nullopt, "source", success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());
  area.reset();
  std::vector<mojom::StoragePolicyUpdatePtr> updates;
  updates.emplace_back(mojom::StoragePolicyUpdate::New(
      storage_key3.origin(), /*purge_on_shutdown=*/true));
  context()->ApplyPolicyUpdates(std::move(updates));

  // After shutdown, we should just see data for storage_key2.
  {
    base::HistogramTester purge_histograms;
    ResetStorage(storage_path());
    // Verify PurgeOrigins histogram is recorded during shutdown.
    purge_histograms.ExpectUniqueSample(
        "Storage.LocalStorage.PurgeOrigins.OnDisk", /*sample=*/0, 1);
  }
  base::Time after_metadata = base::Time::Now();

  WaitForDatabaseOpen();
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(1u));

  std::optional<DomStorageDatabase::MapMetadata> usage_metadata;
  ASSERT_NO_FATAL_FAILURE(FindUsageMetadata(storage_key2, &usage_metadata));
  ASSERT_NE(usage_metadata, std::nullopt);

  EXPECT_LE(before_metadata, usage_metadata->last_accessed.value());
  EXPECT_GE(after_metadata, usage_metadata->last_accessed.value());

  // If we re-bind storage_key2 and then shutdown, the last_accessed time should
  // be updated.
  before_metadata = base::Time::Now();
  context()->BindStorageArea(storage_key2, area.BindNewPipeAndPassReceiver());
  mojo::PendingRemote<blink::mojom::StorageAreaObserver> unused_observer;
  std::ignore = unused_observer.InitWithNewPipeAndPassReceiver();

  base::test::TestFuture<std::vector<blink::mojom::KeyValuePtr>> future;
  area->GetAll(std::move(unused_observer), future.GetCallback());
  EXPECT_TRUE(future.Wait());

  // Capture the PutMetadata histogram fired during ResetStorage() shutdown.
  base::HistogramTester histograms;
  ResetStorage(storage_path());
  after_metadata = base::Time::Now();

  WaitForDatabaseOpen();
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(1u));

  ASSERT_NO_FATAL_FAILURE(FindUsageMetadata(storage_key2, &usage_metadata));
  ASSERT_NE(usage_metadata, std::nullopt);

  EXPECT_LE(before_metadata, usage_metadata->last_accessed.value());
  EXPECT_GE(after_metadata, usage_metadata->last_accessed.value());

  // ResetStorage results in a PutMetadata call to update last_accessed.
  histograms.ExpectUniqueSample("Storage.LocalStorage.PutMetadata.OnDisk",
                                /*sample=*/0, 1);
}

TEST_P(LocalStorageImplTest, MetaDataClearedOnDelete) {
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
  base::test::TestFuture<void> delete_future;
  area->Delete(key, value, "source", delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Wait());
  area.reset();

  // Data from `storage_key2` should exist, including meta-data, but nothing
  // should exist for `storage_key1`.
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key1, /*expected_entries=*/{}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key2, /*expected_entries=*/{{key, value}}));

  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(1u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key2));
}

TEST_P(LocalStorageImplTest, MetaDataClearedOnDeleteAll) {
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
  base::test::TestFuture<void> delete_all_future;
  area->DeleteAll("source", mojo::NullRemote(),
                  delete_all_future.GetCallback());
  EXPECT_TRUE(delete_all_future.Wait());
  area.reset();

  // Data from `storage_key2` should exist, including meta-data, but nothing
  // should exist for `storage_key1`.
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key1, /*expected_entries=*/{}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key2, /*expected_entries=*/{{key, value}}));

  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(1u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key2));
}

TEST_P(LocalStorageImplTest, DeleteStorage) {
  WaitForDatabaseOpen();

  PutVersionForTesting(*context()->GetDatabaseForTesting(), 1);

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");

  PutMapKeyValue(storage_key, StdStringToUint8Vector("key"),
                 StdStringToUint8Vector("value"));

  base::HistogramTester histograms;
  ResetStorage(storage_path());
  base::RunLoop run_loop;
  context()->DeleteStorage(storage_key, run_loop.QuitClosure());
  run_loop.Run();

  // `storage_key` must not contain any key/values or usage metadata.
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key, /*expected_entries=*/{}));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(0u));

  histograms.ExpectUniqueSample(
      "Storage.LocalStorage.DeleteStorageKeysFromSession.OnDisk", 0, 1);
}

TEST_P(LocalStorageImplTest, DeleteStorageWithoutConnection) {
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
  base::test::TestFuture<bool> success_future;
  area->Put(key, value, std::nullopt, "source", success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());
  area.reset();

  // Make sure all data gets committed to disk.
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key1, /*expected_entries=*/{{key, value}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key2, /*expected_entries=*/{{key, value}}));

  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(2u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key1));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key2));

  base::RunLoop run_loop;
  context()->DeleteStorage(storage_key1, run_loop.QuitClosure());
  run_loop.Run();

  // Data from storage_key2 should exist, including meta-data, but nothing
  // should exist for storage_key1.
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key1, /*expected_entries=*/{}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key2, /*expected_entries=*/{{key, value}}));

  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(1u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key2));
}

TEST_P(LocalStorageImplTest, DeleteStorageNotifiesWrapper) {
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
  base::test::TestFuture<bool> success_future;
  area->Put(key, value, std::nullopt, "source", success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());
  area.reset();

  // Make sure all data gets committed to disk.
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key1, /*expected_entries=*/{{key, value}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key2, /*expected_entries=*/{{key, value}}));

  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(2u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key1));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key2));

  TestStorageAreaObserver observer;
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());
  area->AddObserver(observer.Bind());
  observer.FlushForTesting();

  base::RunLoop run_loop;
  context()->DeleteStorage(storage_key1, run_loop.QuitClosure());
  run_loop.Run();
  observer.FlushForTesting();

  ASSERT_EQ(1u, observer.observations().size());
  EXPECT_EQ(TestStorageAreaObserver::Observation::kDeleteAll,
            observer.observations()[0].type);

  // Data from storage_key2 should exist, including meta-data, but nothing
  // should exist for storage_key1.
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key1, /*expected_entries=*/{}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key2, /*expected_entries=*/{{key, value}}));

  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(1u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key2));
}

TEST_P(LocalStorageImplTest, DeleteStorageWithPendingWrites) {
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
  base::test::TestFuture<bool> success_future;
  area->Put(key, value, std::nullopt, "source", success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());
  area.reset();

  // Make sure all data gets committed to disk.
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key1, /*expected_entries=*/{{key, value}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key2, /*expected_entries=*/{{key, value}}));

  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(2u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key1));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key2));

  TestStorageAreaObserver observer;
  context()->BindStorageArea(storage_key1, area.BindNewPipeAndPassReceiver());
  area->AddObserver(observer.Bind());
  area->Put(StdStringToUint8Vector("key2"), value, std::nullopt, "source",
            success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());
  observer.FlushForTesting();

  base::RunLoop run_loop;
  context()->DeleteStorage(storage_key1, run_loop.QuitClosure());
  run_loop.Run();
  observer.FlushForTesting();

  ASSERT_EQ(2u, observer.observations().size());
  EXPECT_EQ(TestStorageAreaObserver::Observation::kChange,
            observer.observations()[0].type);
  EXPECT_EQ(TestStorageAreaObserver::Observation::kDeleteAll,
            observer.observations()[1].type);

  // Data from storage_key2 should exist, including meta-data, but nothing
  // should exist for storage_key1.
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key1, /*expected_entries=*/{}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key2, /*expected_entries=*/{{key, value}}));

  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(1u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key2));
}

TEST_P(LocalStorageImplTest, ShutdownClearsData) {
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
  base::test::TestFuture<bool> success_future;
  area->Put(key1, value, std::nullopt, "source", success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());
  area.reset();

  // Make sure data gets committed to disk.
  ASSERT_NO_FATAL_FAILURE(ExpectMapEquals(
      storage_key1, /*expected_entries=*/{{key1, value}, {key2, value}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key2, /*expected_entries=*/{{key2, value}}));
  ASSERT_NO_FATAL_FAILURE(ExpectMapEquals(
      storage_key1_third_party, /*expected_entries=*/{{key1, value}}));

  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(3u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key1));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key2));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key1_third_party));

  std::vector<mojom::StoragePolicyUpdatePtr> updates;
  updates.emplace_back(mojom::StoragePolicyUpdate::New(
      storage_key1.origin(), /*purge_on_shutdown=*/true));
  context()->ApplyPolicyUpdates(std::move(updates));

  // Data from storage_key2 should exist, including meta-data, but nothing
  // should exist for storage_key1.
  // Data from storage_key1_third_party should also be erased, since it is
  // a third party storage key, and its top_level_site matches the origin
  // of storage_key1, which is set to purge on shutdown.
  base::HistogramTester histograms;
  ResetStorage(storage_path());
  // Verify PurgeOrigins histogram is recorded during shutdown.
  histograms.ExpectUniqueSample("Storage.LocalStorage.PurgeOrigins.OnDisk",
                                /*sample=*/0, 1);

  WaitForDatabaseOpen();

  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key1, /*expected_entries=*/{}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key2, /*expected_entries=*/{{key2, value}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key1_third_party, /*expected_entries=*/{}));

  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(1u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key2));
}

TEST_P(LocalStorageImplTest, InMemory) {
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

  base::FilePath database_path = GetLocalStorageDatabasePath(storage_path());
  EXPECT_FALSE(base::PathExists(database_path));

  // Re-opening should get fresh data.
  InitializeStorage(base::FilePath());
  EXPECT_FALSE(DoTestGet(key, &result));
}

TEST_P(LocalStorageImplTest, InMemoryInvalidPath) {
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
  base::FilePath database_path = GetLocalStorageDatabasePath(storage_path());
  EXPECT_FALSE(base::PathExists(database_path));
}

TEST_P(LocalStorageImplTest, OnDisk) {
  base::HistogramTester histograms;
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  DoTestPut(key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(key, &result));
  EXPECT_EQ(value, result);

  ShutDownStorage();

  // Writing map entries must create the database on disk.
  base::FilePath database_path = GetLocalStorageDatabasePath(storage_path());
  EXPECT_TRUE(base::PathExists(database_path));

  // Should be able to re-open.
  InitializeStorage(storage_path());
  EXPECT_TRUE(DoTestGet(key, &result));
  EXPECT_EQ(value, result);
  // Sample value of 0 denotes DbStatus::Type::kOk.
  histograms.ExpectUniqueSample("Storage.LocalStorage.OpenDatabase.OnDisk",
                                /*sample=*/0, 2);
}

TEST_P(LocalStorageImplTest, InvalidVersionOnDisk) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  DoTestPut(key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(key, &result));
  EXPECT_EQ(value, result);

  ShutDownStorage();

  {
    // Re-open the database.
    base::FilePath db_path = GetLocalStorageDatabasePath(storage_path());
    base::RunLoop open_db_run_loop;
    DbStatus status;

    std::unique_ptr<AsyncDomStorageDatabase> database =
        AsyncDomStorageDatabase::Open(
            StorageType::kLocalStorage, db_path,
            /*memory_dump_id*/ std::nullopt,
            base::BindLambdaForTesting([&](DbStatus callback_status) {
              status = callback_status;
              open_db_run_loop.Quit();
            }));

    open_db_run_loop.Run();
    ASSERT_TRUE(status.ok()) << status.ToString();

    // Mess up version number in database.
    PutVersionForTesting(*database, 987897897);
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

TEST_P(LocalStorageImplTest, CorruptionOnDisk) {
  base::HistogramTester histograms;
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  DoTestPut(key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(key, &result));
  EXPECT_EQ(value, result);

  ShutDownStorage();

  base::FilePath db_path = GetLocalStorageDatabasePath(storage_path());
  if (IsSqliteEnabled()) {
    // Replace the SQLite database file with plain text.
    ASSERT_TRUE(base::WriteFile(db_path, "Corrupt database"));
  } else {
    // Delete manifest files to mess up opening DB.
    base::FileEnumerator file_enum(db_path, true, base::FileEnumerator::FILES,
                                   FILE_PATH_LITERAL("MANIFEST*"));
    for (base::FilePath name = file_enum.Next(); !name.empty();
         name = file_enum.Next()) {
      base::DeleteFile(name);
    }
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

  // LevelDB reports corruption as an IO error. The SQLiteResultCode maps to a
  // DbStatus::Type::kCorruption error.
  uint8_t sample = IsSqliteEnabled() ? /*kCorruption=*/2 : /*kIoError=*/5;
  histograms.ExpectBucketCount("Storage.LocalStorage.OpenDatabase.OnDisk",
                               sample, 1);
}

TEST_P(LocalStorageImplTest, RecreateOnCommitFailure) {
  base::HistogramTester histograms;

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
  TestStorageAreaObserver observer1;
  area1->AddObserver(observer1.Bind());
  TestStorageAreaObserver observer2;
  area2->AddObserver(observer2.Bind());

  // Verify one attempt was made to open the database.
  ASSERT_EQ(1u, num_database_open_requests);

  // This loop will be Quit if and when the current database instance is
  // destroyed, which should happen after many commit failures.
  destruction_loop.emplace();

  bool first_database_destroyed = false;
  context()->GetDatabaseForTesting()->database().PostTaskWithThisObject(
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
    // change to commit.
    value[0]++;
    area1->Put(key, value, std::nullopt, "source",
               base::BindLambdaForTesting([&](bool success) {
                 EXPECT_TRUE(success);
                 values_written++;
               }));
    area1.FlushForTesting();
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
  TestStorageAreaObserver observer3;
  area1->AddObserver(observer3.Bind());
  area1->Delete(key, std::nullopt, "source", delete_loop.QuitClosure());

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
    EXPECT_EQ(TestStorageAreaObserver::Observation::kChange,
              observer2.observations()[i].type);
    EXPECT_EQ(Uint8VectorToStdString(key), observer2.observations()[i].key);
  }

  // Verify that commit failures were recorded in the histogram.
  // Sum > 0 means at least one non-zero (failure) sample was recorded.
  EXPECT_GT(histograms.GetTotalSum("Storage.LocalStorage.UpdateMaps.OnDisk"),
            0);
}

TEST_P(LocalStorageImplTest, DontRecreateOnRepeatedCommitFailure) {
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
  context()->GetDatabaseForTesting()->database().PostTaskWithThisObject(
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
    area.FlushForTesting();
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
  context()->GetDatabaseForTesting()->database().PostTaskWithThisObject(
      base::BindOnce(
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
    base::test::TestFuture<bool> success_future;
    area->Put(key, value, old_value, "source", success_future.GetCallback());
    EXPECT_TRUE(success_future.Take());
    old_value = value;
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context()->FlushStorageKeyForTesting(blink::StorageKey(
        blink::StorageKey::CreateFromStringForTesting("http://foobar.com")));
  }

  // Should still be connected after all that.
  area.FlushForTesting();
  EXPECT_TRUE(area.is_connected());
}

class LocalStorageImplStaleDeletionTest
    : public testing::WithParamInterface</*is_sqlite_enabled=*/bool>,
      public LocalStorageImplTestBase {
 public:
  LocalStorageImplStaleDeletionTest() : LocalStorageImplTestBase(GetParam()) {}
  ~LocalStorageImplStaleDeletionTest() override = default;

  void UpdateAccessMetaData(const blink::StorageKey& storage_key,
                            const base::Time& last_accessed) {
    DomStorageDatabase::Metadata access_metadata;
    access_metadata.map_metadata.push_back({
        .map_locator{storage_key},
        .last_accessed{last_accessed},
    });

    PutMetadataSync(*context()->GetDatabaseForTesting(),
                    std::move(access_metadata));
  }

  void UpdateWriteMetaData(const blink::StorageKey& storage_key,
                           const base::Time& last_modified,
                           uint64_t size_bytes) {
    DomStorageDatabase::Metadata write_metadata;
    write_metadata.map_metadata.push_back({
        .map_locator{storage_key},
        .last_modified{last_modified},
        .total_size{size_bytes},
    });

    PutMetadataSync(*context()->GetDatabaseForTesting(),
                    std::move(write_metadata));
  }
};

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    LocalStorageImplStaleDeletionTest,
    testing::Bool(),
    /*name_generator=*/
    [](const testing::TestParamInfo<
        LocalStorageImplStaleDeletionTest::ParamType>& info) {
      return info.param ? "SQLite" : "LevelDB";
    });

TEST_P(LocalStorageImplStaleDeletionTest, StaleStorageAreaDeletion) {
  DomStorageDatabase::Key key = StdStringToUint8Vector("key");
  DomStorageDatabase::Value value = StdStringToUint8Vector("value");

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
  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();
  context()->BindStorageArea(storage_key2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();
  context()->BindStorageArea(storage_key3, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();
  context()->BindStorageArea(storage_key4, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, std::nullopt, "source", base::DoNothing());
  area.reset();
  context()->BindStorageArea(storage_key5, area.BindNewPipeAndPassReceiver());
  base::test::TestFuture<bool> success_future;
  area->Put(key, value, std::nullopt, "source", success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());
  area.reset();

  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key1, /*expected_entries=*/{{key, value}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key2, /*expected_entries=*/{{key, value}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key3, /*expected_entries=*/{{key, value}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key4, /*expected_entries=*/{{key, value}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key5, /*expected_entries=*/{{key, value}}));

  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(5u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key1));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key2));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key3));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key4));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key5));

  // Backdate metadata accessed and modified times so that storage_key3 and
  // storage_key4 should be purged, while storage_key1 and storage_key2 should
  // not. storage_key5 is left alone to test the default codepath.
  UpdateAccessMetaData(storage_key1, base::Time::Now() - base::Days(401));
  UpdateWriteMetaData(storage_key2, base::Time::Now() - base::Days(401), 0);
  UpdateAccessMetaData(storage_key3, base::Time::Now() - base::Days(401));
  UpdateWriteMetaData(storage_key3, base::Time::Now() - base::Days(401), 0);
  UpdateAccessMetaData(storage_key4, base::Time::Now() - base::Days(401));
  UpdateWriteMetaData(storage_key4, base::Time::Now() - base::Days(401), 0);

  // Restart local storage, force bind area for storage_key3, and trigger stale
  // storage area purging.
  ResetStorage(storage_path());
  context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
  context()->ForceFakeOpenStorageAreaForTesting(storage_key3);
  WaitForDatabaseOpen();
  RunUntilIdle();

  // We should see that only the data for storage_key4 was cleared.
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key1, /*expected_entries=*/{{key, value}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key2, /*expected_entries=*/{{key, value}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key3, /*expected_entries=*/{{key, value}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key4, /*expected_entries=*/{}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(storage_key5, /*expected_entries=*/{{key, value}}));

  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(4u));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key1));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key2));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key3));
  ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(storage_key5));
}

TEST_P(LocalStorageImplStaleDeletionTest, Orphan) {
  DomStorageDatabase::Key key = StdStringToUint8Vector("key");
  DomStorageDatabase::Value value = StdStringToUint8Vector("value");

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
  area->Put(key, value, std::nullopt, "source", base::DoNothing());
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

    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(first_party_key, /*expected_entries=*/{{key, value}}));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(1u));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(first_party_key));

    UpdateAccessMetaData(first_party_key, base::Time::Now() - base::Days(2));
    UpdateWriteMetaData(first_party_key, base::Time::Now() - base::Days(2), 0);
    context()->FlushStorageKeyForTesting(first_party_key);
    ResetStorage(storage_path());
    context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
    WaitForDatabaseOpen();
    RunUntilIdle();
    EXPECT_EQ(0, histograms.GetTotalSum(
                     "LocalStorage.OrphanStorageAreasOnStartupCount"));

    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(first_party_key, /*expected_entries=*/{{key, value}}));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(1u));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(first_party_key));
  }

  // First party nonce bucket does qualify, but only if it's old.
  const auto first_party_nonce_key = blink::StorageKey::CreateWithNonce(
      url::Origin::Create(GURL("http://firstpartynonce/")),
      base::UnguessableToken::Create());
  context()->BindStorageArea(first_party_nonce_key,
                             area.BindNewPipeAndPassReceiver());
  area->Put(key, value, std::nullopt, "source", base::DoNothing());
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

    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(first_party_key, /*expected_entries=*/{{key, value}}));
    ASSERT_NO_FATAL_FAILURE(ExpectMapEquals(
        first_party_nonce_key, /*expected_entries=*/{{key, value}}));

    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(2u));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(first_party_key));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(first_party_nonce_key));

    UpdateAccessMetaData(first_party_nonce_key,
                         base::Time::Now() - base::Days(2));
    UpdateWriteMetaData(first_party_nonce_key,
                        base::Time::Now() - base::Days(2), 0);
    context()->FlushStorageKeyForTesting(first_party_nonce_key);
    ResetStorage(storage_path());
    context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
    WaitForDatabaseOpen();
    RunUntilIdle();
    EXPECT_EQ(1, histograms.GetTotalSum(
                     "LocalStorage.OrphanStorageAreasOnStartupCount"));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(first_party_key, /*expected_entries=*/{{key, value}}));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(first_party_nonce_key, /*expected_entries=*/{}));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(1u));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(first_party_key));
  }

  // Third party bucket doesn't qualify, even if it's old.
  const auto third_party_key = blink::StorageKey::Create(
      url::Origin::Create(GURL("https://thirdparty/")),
      net::SchemefulSite(GURL("https://thirdparty2/")),
      blink::mojom::AncestorChainBit::kCrossSite);
  context()->BindStorageArea(third_party_key,
                             area.BindNewPipeAndPassReceiver());
  area->Put(key, value, std::nullopt, "source", base::DoNothing());
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

    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(first_party_key, /*expected_entries=*/{{key, value}}));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(first_party_nonce_key, /*expected_entries=*/{}));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(third_party_key, /*expected_entries=*/{{key, value}}));

    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(2u));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(first_party_key));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(third_party_key));

    UpdateAccessMetaData(third_party_key, base::Time::Now() - base::Days(2));
    UpdateWriteMetaData(third_party_key, base::Time::Now() - base::Days(2), 0);
    context()->FlushStorageKeyForTesting(third_party_key);
    ResetStorage(storage_path());
    context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
    WaitForDatabaseOpen();
    RunUntilIdle();
    EXPECT_EQ(0, histograms.GetTotalSum(
                     "LocalStorage.OrphanStorageAreasOnStartupCount"));

    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(first_party_key, /*expected_entries=*/{{key, value}}));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(first_party_nonce_key, /*expected_entries=*/{}));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(third_party_key, /*expected_entries=*/{{key, value}}));

    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(2u));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(first_party_key));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(third_party_key));
  }

  // Third party nonce bucket does qualify, but only if it's old.
  const auto third_party_nonce_key = blink::StorageKey::Create(
      url::Origin::Create(GURL("https://thirdparty/")),
      net::SchemefulSite(url::Origin::Create(GURL("http://thirdparty2/"))
                             .DeriveNewOpaqueOrigin()),
      blink::mojom::AncestorChainBit::kCrossSite);
  context()->BindStorageArea(third_party_nonce_key,
                             area.BindNewPipeAndPassReceiver());
  area->Put(key, value, std::nullopt, "source", base::DoNothing());
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

    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(first_party_key, /*expected_entries=*/{{key, value}}));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(first_party_nonce_key, /*expected_entries=*/{}));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(third_party_key, /*expected_entries=*/{{key, value}}));
    ASSERT_NO_FATAL_FAILURE(ExpectMapEquals(
        third_party_nonce_key, /*expected_entries=*/{{key, value}}));

    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(3u));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(first_party_key));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(third_party_key));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(third_party_nonce_key));

    UpdateAccessMetaData(third_party_nonce_key,
                         base::Time::Now() - base::Days(2));
    UpdateWriteMetaData(third_party_nonce_key,
                        base::Time::Now() - base::Days(2), 0);
    context()->FlushStorageKeyForTesting(third_party_nonce_key);
    ResetStorage(storage_path());
    context()->OverrideDeleteStaleStorageAreasDelayForTesting(base::Days(0));
    WaitForDatabaseOpen();
    RunUntilIdle();
    EXPECT_EQ(1, histograms.GetTotalSum(
                     "LocalStorage.OrphanStorageAreasOnStartupCount"));

    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(first_party_key, /*expected_entries=*/{{key, value}}));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(first_party_nonce_key, /*expected_entries=*/{}));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(third_party_key, /*expected_entries=*/{{key, value}}));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMapEquals(third_party_nonce_key, /*expected_entries=*/{}));

    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataCount(2u));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(first_party_key));
    ASSERT_NO_FATAL_FAILURE(ExpectUsageMetadataExists(third_party_key));
  }
}

}  // namespace storage
