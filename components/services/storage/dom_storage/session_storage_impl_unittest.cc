// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_impl.h"

#include <stdint.h>

#include <memory>
#include <string_view>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "base/uuid.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "components/services/storage/dom_storage/test_support/storage_area_test_util.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

namespace {

std::vector<uint8_t> StringViewToUint8Vector(std::string_view s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

class SessionStorageImplTest : public base::test::WithFeatureOverride,
                               public testing::Test {
 public:
  SessionStorageImplTest()
      : base::test::WithFeatureOverride(kDomStorageSqlite) {
    CHECK(temp_dir_.CreateUniqueTempDir());
  }

  SessionStorageImplTest(const SessionStorageImplTest&) = delete;
  SessionStorageImplTest& operator=(const SessionStorageImplTest&) = delete;

  ~SessionStorageImplTest() override {
    // Flush all tasks to make sure the database is fully closed.
    RunUntilIdle();
    EXPECT_TRUE(temp_dir_.Delete());
  }

  void SetUp() override {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &SessionStorageImplTest::OnBadMessage, base::Unretained(this)));
  }

  void TearDown() override {
    if (session_storage_)
      ShutDownSessionStorage();
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  bool IsSqliteEnabled() const { return GetParam(); }

  void OnBadMessage(const std::string& reason) { bad_message_called_ = true; }

  void SetBackingMode(SessionStorageImpl::BackingMode backing_mode) {
    DCHECK(!session_storage_);
    backing_mode_ = backing_mode;
  }

  SessionStorageImpl* session_storage_impl() {
    if (!session_storage_) {
      remote_session_storage_.reset();
      session_storage_ = std::make_unique<SessionStorageImpl>(
          temp_path(), backing_mode_, base::DoNothing(),
          remote_session_storage_.BindNewPipeAndPassReceiver());
    }
    return session_storage_.get();
  }

  mojom::SessionStorageControl* session_storage() {
    session_storage_impl();
    return remote_session_storage_.get();
  }

  void ShutDownSessionStorage() {
    remote_session_storage_.FlushForTesting();
    session_storage_.reset();
  }

  void DoTestPut(const std::string& namespace_id,
                 const blink::StorageKey& storage_key,
                 std::string_view key,
                 std::string_view value,
                 const std::string& source,
                 bool should_persist) {
    session_storage()->CreateNamespace(namespace_id);
    mojo::Remote<blink::mojom::StorageArea> area;
    session_storage()->BindStorageArea(storage_key, namespace_id,
                                       area.BindNewPipeAndPassReceiver());
    EXPECT_TRUE(test::PutSync(area.get(), StringViewToUint8Vector(key),
                              StringViewToUint8Vector(value), std::nullopt,
                              source));
    session_storage()->DeleteNamespace(namespace_id, should_persist);
  }

  std::optional<std::vector<uint8_t>> DoTestGet(
      const std::string& namespace_id,
      const blink::StorageKey& storage_key,
      std::string_view key) {
    session_storage()->CreateNamespace(namespace_id);
    mojo::Remote<blink::mojom::StorageArea> area;
    session_storage()->BindStorageArea(storage_key, namespace_id,
                                       area.BindNewPipeAndPassReceiver());

    // Use the GetAll interface because Gets are being removed.
    std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area.get());
    session_storage()->DeleteNamespace(namespace_id, true);

    std::vector<uint8_t> key_as_bytes = StringViewToUint8Vector(key);
    for (const auto& key_value : data) {
      if (key_value->key == key_as_bytes) {
        return key_value->value;
      }
    }
    return std::nullopt;
  }

 protected:
  const base::FilePath& temp_path() const { return temp_dir_.GetPath(); }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  void FlushMojo() { remote_session_storage_.FlushForTesting(); }

  // Ensures the database connection is fully established. As a result,
  // subsequent Mojo calls won't be deferred via RunWhenConnected.
  void EnsureDatabaseOpen() {
    base::RunLoop loop;
    session_storage_impl()->SetDatabaseOpenCallbackForTesting(
        loop.QuitClosure());
    loop.Run();
  }

  // Waits for all pending tasks on the database thread to complete.
  void WaitForDatabaseTasks() {
    base::RunLoop loop;
    session_storage_impl()
        ->GetDatabaseForTesting()
        ->database()
        .PostTaskWithThisObject(base::BindLambdaForTesting(
            [&](DomStorageDatabase*) { loop.Quit(); }));
    loop.Run();
  }

  void TestInvalidVersionOnDisk(std::string invalid_version_string);

  bool bad_message_called_ = false;

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  SessionStorageImpl::BackingMode backing_mode_ =
      SessionStorageImpl::BackingMode::kRestoreDiskState;
  std::unique_ptr<SessionStorageImpl> session_storage_;
  mojo::Remote<mojom::SessionStorageControl> remote_session_storage_;
};

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    SessionStorageImplTest,
    testing::Bool(),
    /*name_generator=*/
    [](const testing::TestParamInfo<SessionStorageImplTest::ParamType>& info) {
      return info.param ? "SQLite" : "LevelDB";
    });

TEST_P(SessionStorageImplTest, CommitRecordsUpdateMapsHistogram) {
  std::string namespace_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");
  DomStorageDatabase::Key key = StringViewToUint8Vector("key");
  DomStorageDatabase::Value value = StringViewToUint8Vector("value");

  session_storage()->CreateNamespace(namespace_id);

  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->BindStorageArea(storage_key, namespace_id,
                                     area.BindNewPipeAndPassReceiver());

  // Verify no data initially.
  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area.get());
  EXPECT_EQ(0ul, data.size());

  // To filter out unrelated events from initialization, wait for database setup
  // to complete before starting histogram recording.
  WaitForDatabaseTasks();
  base::HistogramTester histograms;

  // Put a value and flush to ensure we queue committing it to disk.
  EXPECT_TRUE(test::PutSync(area.get(), key, value, std::nullopt, "source"));
  session_storage_impl()->FlushAreaForTesting(namespace_id, storage_key);

  // Verify the key/value pair is present.
  data = test::GetAllSync(area.get());
  ASSERT_EQ(1ul, data.size());
  EXPECT_EQ(key, data[0]->key);
  EXPECT_EQ(value, data[0]->value);
  area.reset();

  // Wait for the commit to complete, then verify it succeeded (sample 0 = kOk)
  // via histogram.
  WaitForDatabaseTasks();
  histograms.ExpectUniqueSample("Storage.SessionStorage.UpdateMaps.OnDisk", 0,
                                1);
}

TEST_P(SessionStorageImplTest, StartupShutdownSave) {
  base::HistogramTester histograms;

  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver());

  // Verify no data.
  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area_n1.get());
  EXPECT_EQ(0ul, data.size());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  // Verify data is there.
  data = test::GetAllSync(area_n1.get());
  EXPECT_EQ(1ul, data.size());
  area_n1.reset();

  // Delete the namespace and shutdown Session Storage, BUT persist the
  // namespace so it can be loaded again.
  session_storage()->DeleteNamespace(namespace_id1, true);
  ShutDownSessionStorage();

  // This will re-initialize Session Storage and load the persisted namespace.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver());

  // The data from before should be here.
  data = test::GetAllSync(area_n1.get());
  EXPECT_EQ(1ul, data.size());
  area_n1.reset();

  // Delete the namespace, shut down Session Storage, and do not persist the
  // data.
  session_storage()->DeleteNamespace(namespace_id1, false);
  ShutDownSessionStorage();

  // This will re-initialize Session Storage and the namespace should be empty.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver());

  // The data from before should not be here.
  data = test::GetAllSync(area_n1.get());
  EXPECT_EQ(0ul, data.size());

  // Sample value of 0 denotes DbStatus::Type::kOk. The database was opened 3
  // times: initial open, after first shutdown, and after second shutdown.
  histograms.ExpectUniqueSample("Storage.SessionStorage.OpenDatabase.OnDisk",
                                /*sample=*/0, 3);
  // ReadAllMetadata is called once per database open.
  histograms.ExpectUniqueSample("Storage.SessionStorage.ReadAllMetadata.OnDisk",
                                /*sample=*/0, 3);
  // Each BindStorageArea triggers an async PutMetadata on the DB thread.
  // Wait for those to complete before asserting histogram counts.
  WaitForDatabaseTasks();
  histograms.ExpectUniqueSample("Storage.SessionStorage.PutMetadata.OnDisk",
                                /*sample=*/0, 2);
  // Only the GetAllSync after the first restart reads from disk; the others
  // operate on in-memory maps or empty namespaces.
  histograms.ExpectUniqueSample(
      "Storage.SessionStorage.ReadMapKeyValues.OnDisk",
      /*sample=*/0, 1);
}

TEST_P(SessionStorageImplTest, CloneBeforeBrowserClone) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  session_storage()->BindNamespace(namespace_id1,
                                   ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  ss_namespace1->Clone(namespace_id2);
  area_n1.FlushForTesting();

  // Do the browser-side clone afterwards.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver());

  // The data should be in namespace 2.
  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area_n2.get());
  EXPECT_EQ(1ul, data.size());
}

TEST_P(SessionStorageImplTest, Cloning) {
  base::HistogramTester histograms;

  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  session_storage()->BindNamespace(namespace_id1,
                                   ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver());

  // Internally triggered clone before the put. The clone doesn't actually count
  // until a clone comes from the namespace.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  ss_namespace1->Clone(namespace_id2);
  area_n1.FlushForTesting();
  session_storage_impl()->FlushAreaForTesting(namespace_id1, storage_key1);
  area_n1.reset();
  ss_namespace1.reset();

  // Wait for the UpdateMaps triggered by FlushAreaForTesting to complete.
  WaitForDatabaseTasks();
  histograms.ExpectUniqueSample("Storage.SessionStorage.UpdateMaps.OnDisk", 0,
                                1);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver());

  // Delete the namespace and shut down Session Storage, BUT persist the
  // namespace so it can be loaded again. This tests the case where our cloning
  // works even though the namespace is deleted (but persisted on disk).
  session_storage()->DeleteNamespace(namespace_id1, true);

  // The data from before should be in namespace 2.
  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area_n2.get());
  EXPECT_EQ(1ul, data.size());

  // Put some data in namespace 2.
  EXPECT_TRUE(test::PutSync(area_n2.get(), StringViewToUint8Vector("key2"),
                            StringViewToUint8Vector("value2"), std::nullopt,
                            "source1"));
  data = test::GetAllSync(area_n2.get());
  EXPECT_EQ(2ul, data.size());

  // Re-open namespace 1, check that we don't have the extra data.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver());

  // We should only have the first value.
  data = test::GetAllSync(area_n1.get());
  EXPECT_EQ(1ul, data.size());

  // Wait for async CloneMap to complete.
  WaitForDatabaseTasks();
  histograms.ExpectUniqueSample("Storage.SessionStorage.CloneMap.OnDisk",
                                /*sample=*/0, 1);
}

TEST_P(SessionStorageImplTest, ImmediateCloning) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  session_storage()->BindNamespace(namespace_id1,
                                   ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver());

  // Immediate clone.
  session_storage()->CloneNamespace(namespace_id1, namespace_id2,
                                    mojom::SessionStorageCloneType::kImmediate);

  // Open the second namespace, ensure empty.
  {
    mojo::Remote<blink::mojom::StorageArea> area_n2;
    session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                       area_n2.BindNewPipeAndPassReceiver());
    std::vector<blink::mojom::KeyValuePtr> data =
        test::GetAllSync(area_n2.get());
    EXPECT_EQ(0ul, data.size());
  }

  // Delete that namespace, copy again after a put.
  session_storage()->DeleteNamespace(namespace_id2, false);
  FlushMojo();

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value2"), std::nullopt,
                            "source1"));

  session_storage()->CloneNamespace(namespace_id1, namespace_id2,
                                    mojom::SessionStorageCloneType::kImmediate);

  // Open the second namespace, ensure populated
  {
    mojo::Remote<blink::mojom::StorageArea> area_n2;
    session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                       area_n2.BindNewPipeAndPassReceiver());
    std::vector<blink::mojom::KeyValuePtr> data =
        test::GetAllSync(area_n2.get());
    EXPECT_EQ(1ul, data.size());
  }

  session_storage()->DeleteNamespace(namespace_id2, false);
  FlushMojo();

  // Verify that cloning from the namespace object will result in a bad message.
  session_storage()->CloneNamespace(namespace_id1, namespace_id2,
                                    mojom::SessionStorageCloneType::kImmediate);

  // This should cause a bad message.
  ss_namespace1->Clone(namespace_id2);
  ss_namespace1.FlushForTesting();

  EXPECT_TRUE(bad_message_called_);
}

TEST_P(SessionStorageImplTest, Scavenging) {
  // Create our namespace, shut down Session Storage, and leave that namespace
  // on disk; then verify that it is scavenged if we re-initialize Session
  // Storage without calling CreateNamespace.

  // Create, verify we have no data.
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  // This scavenge call should NOT delete the namespace, as we just created it.
  // Cause the connection to start loading, so we start scavenging mid-load.
  session_storage()->Flush();
  session_storage()->ScavengeUnusedNamespaces();
  FlushMojo();

  // Restart Session Storage.
  ShutDownSessionStorage();
  session_storage()->CreateNamespace(namespace_id1);

  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));
  area_n1.reset();

  // This scavenge call should NOT delete the namespace, as we never called
  // delete.
  session_storage()->ScavengeUnusedNamespaces();

  // Restart Session Storage.
  ShutDownSessionStorage();
  session_storage()->CreateNamespace(namespace_id1);

  // Delete the namespace and shut down Session Storage, BUT persist the
  // namespace so it can be loaded again.
  session_storage()->DeleteNamespace(namespace_id1, true);

  // This scavenge call should NOT delete the namespace, as we explicitly
  // persisted the namespace.
  session_storage()->ScavengeUnusedNamespaces();
  FlushMojo();

  ShutDownSessionStorage();

  // Re-initialize Session Storage, load the persisted namespace, and verify
  // we still have data.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver());
  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area_n1.get());
  EXPECT_EQ(1ul, data.size());
  area_n1.reset();

  // Shutting down Session Storage without an explicit DeleteNamespace
  // should leave the data on disk.
  ShutDownSessionStorage();

  // Re-initialize Session Storage. Scavenge should now remove the namespace
  // as there has been no call to CreateNamespace. Check that the data is
  // empty.
  session_storage()->ScavengeUnusedNamespaces();
  FlushMojo();

  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver());
  data = test::GetAllSync(area_n1.get());
  EXPECT_EQ(0ul, data.size());
}

void SessionStorageImplTest::TestInvalidVersionOnDisk(
    std::string invalid_version_string) {
  std::string namespace_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");

  // Initialize Session Storage, add some data to it, and check that it's there.
  DoTestPut(namespace_id, storage_key, "key", "value", "source",
            /*should_persist=*/true);
  std::optional<std::vector<uint8_t>> opt_value =
      DoTestGet(namespace_id, storage_key, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(StringViewToUint8Vector("value"), opt_value.value());

  ShutDownSessionStorage();
  {
    // Re-open the database.
    base::RunLoop open_db_run_loop;
    DbStatus status;
    base::FilePath database_path =
        DomStorageDatabase::GetPath(StorageType::kSessionStorage, temp_path());

    std::unique_ptr<AsyncDomStorageDatabase> database =
        AsyncDomStorageDatabase::Open(
            StorageType::kSessionStorage, database_path,
            /*memory_dump_id=*/std::nullopt,
            base::BindLambdaForTesting([&](DbStatus callback_status) {
              status = callback_status;
              open_db_run_loop.Quit();
            }));

    open_db_run_loop.Run();
    ASSERT_TRUE(status.ok()) << status.ToString();

    // Mess up version number in database.
    PutVersionForTesting(*database, 654654);
  }

  opt_value = DoTestGet(namespace_id, storage_key, "key");
  EXPECT_FALSE(opt_value);

  // Write data again.
  DoTestPut(namespace_id, storage_key, "key", "value", "source",
            /*should_persist=*/true);

  ShutDownSessionStorage();

  // Data should have been preserved now.
  opt_value = DoTestGet(namespace_id, storage_key, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(StringViewToUint8Vector("value"), opt_value.value());
  ShutDownSessionStorage();
}

TEST_P(SessionStorageImplTest, InvalidVersionOnDisk) {
  ASSERT_NO_FATAL_FAILURE(TestInvalidVersionOnDisk("argh"));
}

TEST_P(SessionStorageImplTest, WrongVersionOnDisk) {
  ASSERT_NO_FATAL_FAILURE(TestInvalidVersionOnDisk("2"));
}

TEST_P(SessionStorageImplTest, CorruptionOnDisk) {
  base::HistogramTester histograms;

  std::string namespace_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");

  // Initialize Session Storage, add some data to it, and check that it's there.
  DoTestPut(namespace_id, storage_key, "key", "value", "source",
            /*should_persist=*/true);
  std::optional<std::vector<uint8_t>> opt_value =
      DoTestGet(namespace_id, storage_key, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(StringViewToUint8Vector("value"), opt_value.value());

  ShutDownSessionStorage();

  // Also flush Task Scheduler tasks to make sure the database is fully closed.
  RunUntilIdle();

  base::FilePath db_path =
      DomStorageDatabase::GetPath(StorageType::kSessionStorage, temp_path());
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

  opt_value = DoTestGet(namespace_id, storage_key, "key");
  EXPECT_FALSE(opt_value);

  // Write data again.
  DoTestPut(namespace_id, storage_key, "key", "value", "source",
            /*should_persist=*/true);

  ShutDownSessionStorage();

  // Data should have been preserved now.
  opt_value = DoTestGet(namespace_id, storage_key, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(StringViewToUint8Vector("value"), opt_value.value());
  ShutDownSessionStorage();

  // LevelDB reports corruption as an IO error. The SQLiteResultCode maps to a
  // DbStatus::Type::kCorruption error.
  uint8_t sample = IsSqliteEnabled() ? /*kCorruption=*/2 : /*kIoError=*/5;
  histograms.ExpectBucketCount("Storage.SessionStorage.OpenDatabase.OnDisk",
                               sample, 1);
}

TEST_P(SessionStorageImplTest, RecreateOnCommitFailure) {
  base::HistogramTester histograms;

  std::string namespace_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://asf.com");
  blink::StorageKey storage_key3 =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");

  std::optional<base::RunLoop> open_loop;
  size_t num_database_open_requests = 0;
  size_t num_databases_destroyed = 0;
  session_storage_impl()->SetDatabaseOpenCallbackForTesting(
      base::BindLambdaForTesting([&] {
        ++num_database_open_requests;
        open_loop->Quit();
      }));

  open_loop.emplace();

  // Open three connections to the database.
  mojo::Remote<blink::mojom::StorageArea> area_o1;
  mojo::Remote<blink::mojom::StorageArea> area_o2;
  mojo::Remote<blink::mojom::StorageArea> area_o3;
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace;
  session_storage()->CreateNamespace(namespace_id);
  session_storage()->BindNamespace(namespace_id,
                                   ss_namespace.BindNewPipeAndPassReceiver());
  session_storage()->BindStorageArea(storage_key1, namespace_id,
                                     area_o1.BindNewPipeAndPassReceiver());
  session_storage()->BindStorageArea(storage_key2, namespace_id,
                                     area_o2.BindNewPipeAndPassReceiver());
  session_storage()->BindStorageArea(storage_key3, namespace_id,
                                     area_o3.BindNewPipeAndPassReceiver());
  open_loop->Run();

  // Ensure that the first opened database always fails to write data.
  session_storage_impl()
      ->GetDatabaseForTesting()
      ->database()
      .PostTaskWithThisObject(
          base::BindLambdaForTesting([&](DomStorageDatabase* db) {
            db->MakeAllCommitsFailForTesting();
            db->SetDestructionCallbackForTesting(
                base::BindLambdaForTesting([&] { ++num_databases_destroyed; }));
          }));

  // Verify one attempt was made to open the database.
  ASSERT_EQ(1u, num_database_open_requests);

  // Setup a new RunLoop so we can wait until SessionStorageImpl tries to
  // reconnect to the database, which should happen after several commit
  // errors.
  open_loop.emplace();

  // Also prepare for another database connection, next time providing a
  // functioning database.
  session_storage_impl()->SetDatabaseOpenCallbackForTesting(
      base::BindLambdaForTesting([&] {
        ++num_database_open_requests;
        open_loop->Quit();
      }));

  // Start a put operation on the third connection before starting to commit
  // a lot of data on the first storage_key. This put operation should result in
  // a pending commit that will get cancelled when the database connection is
  // closed.
  auto value = StringViewToUint8Vector("avalue");
  area_o3->Put(StringViewToUint8Vector("w3key"), value, std::nullopt, "source",
               base::BindOnce([](bool success) { EXPECT_TRUE(success); }));

  // Repeatedly write data to the database, to trigger enough commit errors.
  while (area_o1.is_connected()) {
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    std::vector<uint8_t> old_value = value;
    value[0]++;
    area_o1->Put(StringViewToUint8Vector("key"), value, std::nullopt, "source",
                 base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
    area_o1.FlushForTesting();
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    session_storage_impl()->FlushAreaForTesting(namespace_id, storage_key1);
  }
  area_o1.reset();

  // Wait for a new database to be opened, which should happen after the first
  // database is destroyed due to failures.
  open_loop->Run();
  EXPECT_EQ(1u, num_databases_destroyed);
  EXPECT_EQ(2u, num_database_open_requests);

  // The connection to the second area should have closed as well.
  area_o2.FlushForTesting();
  ss_namespace.FlushForTesting();
  EXPECT_FALSE(area_o2.is_connected());
  EXPECT_FALSE(ss_namespace.is_connected());

  // Reconnect area_o1 to the new database, and try to read a value.
  ss_namespace.reset();
  session_storage()->BindStorageArea(storage_key1, namespace_id,
                                     area_o1.BindNewPipeAndPassReceiver());

  base::RunLoop delete_loop;
  test::MockStorageAreaObserver observer4;
  area_o1->AddObserver(observer4.Bind());
  area_o1->Delete(StringViewToUint8Vector("key"), std::nullopt, "source",
                  delete_loop.QuitClosure());

  // And deleting the value from the new area should have failed (as the
  // database is empty).
  delete_loop.Run();
  area_o1.reset();
  session_storage()->DeleteNamespace(namespace_id, true);

  {
    // Committing data should now work.
    DoTestPut(namespace_id, storage_key1, "key", "value", "source",
              /*should_persist=*/true);
    std::optional<std::vector<uint8_t>> opt_value =
        DoTestGet(namespace_id, storage_key1, "key");
    ASSERT_TRUE(opt_value);
    EXPECT_EQ(StringViewToUint8Vector("value"), opt_value.value());
  }

  // Verify that commit failures were recorded in the histogram.
  // Sum > 0 means at least one non-zero (failure) sample was recorded.
  EXPECT_GT(histograms.GetTotalSum("Storage.SessionStorage.UpdateMaps.OnDisk"),
            0);
}

TEST_P(SessionStorageImplTest, DontRecreateOnRepeatedCommitFailure) {
  std::string namespace_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");

  std::optional<base::RunLoop> open_loop;
  size_t num_database_open_requests = 0;
  size_t num_databases_destroyed = 0;
  session_storage_impl()->SetDatabaseOpenCallbackForTesting(
      base::BindLambdaForTesting([&] {
        ++num_database_open_requests;
        open_loop->Quit();
      }));
  open_loop.emplace();

  // Open three connections to the database.
  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->CreateNamespace(namespace_id);
  session_storage()->BindStorageArea(storage_key1, namespace_id,
                                     area.BindNewPipeAndPassReceiver());
  open_loop->Run();

  // Ensure that this database always fails to write data.
  session_storage_impl()
      ->GetDatabaseForTesting()
      ->database()
      .PostTaskWithThisObject(
          base::BindLambdaForTesting([&](DomStorageDatabase* db) {
            db->MakeAllCommitsFailForTesting();
            db->SetDestructionCallbackForTesting(
                base::BindLambdaForTesting([&] { ++num_databases_destroyed; }));
          }));

  // Verify one attempt was made to open the database.
  EXPECT_EQ(1u, num_database_open_requests);

  // Setup a new RunLoop so we can wait until SessionStorageImpl tries to
  // reconnect to the database, which should happen after several commit
  // errors.
  open_loop.emplace();
  session_storage_impl()->SetDatabaseOpenCallbackForTesting(
      base::BindLambdaForTesting([&] {
        ++num_database_open_requests;
        open_loop->Quit();

        // Ensure that this database also always fails to write data.
        session_storage_impl()->GetDatabaseForTesting()->database().AsyncCall(
            &DomStorageDatabase::MakeAllCommitsFailForTesting);
      }));

  // Repeatedly write data to the database, to trigger enough commit errors.
  auto value = StringViewToUint8Vector("avalue");
  std::optional<std::vector<uint8_t>> old_value = std::nullopt;
  while (area.is_connected()) {
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    area->Put(StringViewToUint8Vector("key"), value, old_value, "source",
              base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
    area.FlushForTesting();
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    session_storage_impl()->FlushAreaForTesting(namespace_id, storage_key1);

    old_value = value;
    value[0]++;
  }
  area.reset();

  // Wait for SessionStorageImpl to try to reconnect to the database, and
  // connect that new request with a database implementation that always fails
  // on write.
  open_loop->Run();

  EXPECT_EQ(2u, num_database_open_requests);
  EXPECT_EQ(1u, num_databases_destroyed);

  // Reconnect a area to the database, and repeatedly write data to it again.
  // This time all should just keep getting written, and commit errors are
  // getting ignored.
  session_storage()->BindStorageArea(storage_key1, namespace_id,
                                     area.BindNewPipeAndPassReceiver());

  old_value = std::nullopt;
  for (int i = 0; i < 64; ++i) {
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    area->Put(StringViewToUint8Vector("key"), value, old_value, "source",
              base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
    area.FlushForTesting();
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    session_storage_impl()->FlushAreaForTesting(namespace_id, storage_key1);

    old_value = value;
    value[0]++;
  }

  // Should still be connected after all that.
  RunUntilIdle();
  EXPECT_TRUE(area.is_connected());

  session_storage()->DeleteNamespace(namespace_id, false);
  ShutDownSessionStorage();
}

TEST_P(SessionStorageImplTest, GetUsage) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver());
  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  base::RunLoop loop;
  session_storage()->GetUsage(base::BindLambdaForTesting(
      [&](std::vector<mojom::SessionStorageUsageInfoPtr> usage) {
        loop.Quit();
        ASSERT_EQ(1u, usage.size());
        EXPECT_EQ(storage_key1, usage[0]->storage_key);
        EXPECT_EQ(namespace_id1, usage[0]->namespace_id);
      }));
  loop.Run();
}

TEST_P(SessionStorageImplTest, DeleteStorage) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");

  // First, test deleting data for a namespace that is open.
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  session_storage()->DeleteStorage(storage_key1, namespace_id1,
                                   base::DoNothing());

  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area.get());
  EXPECT_EQ(0ul, data.size());

  // Next, test that it deletes the data even if there isn't a namespace open.
  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));
  area.reset();

  // Delete the namespace and shutdown Session Storage, BUT persist the
  // namespace so it can be loaded again.
  session_storage()->DeleteNamespace(namespace_id1, true);
  ShutDownSessionStorage();

  // Ensure the database is fully open before calling methods.
  EnsureDatabaseOpen();

  base::HistogramTester histograms;
  session_storage()->DeleteStorage(storage_key1, namespace_id1,
                                   base::DoNothing());

  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver());
  data.clear();
  data = test::GetAllSync(area.get());
  EXPECT_EQ(0ul, data.size());

  WaitForDatabaseTasks();
  histograms.ExpectUniqueSample(
      "Storage.SessionStorage.DeleteStorageKeysFromSession.OnDisk", 0, 1);
}

TEST_P(SessionStorageImplTest, PurgeInactiveWrappers) {
  DomStorageDatabase::Key key = StringViewToUint8Vector("key1");
  DomStorageDatabase::Key value = StringViewToUint8Vector("value1");

  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");

  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver());

  // Write a key/value pair to the map in `area`.
  EXPECT_TRUE(test::PutSync(area.get(), key, value, std::nullopt, "source1"));
  session_storage_impl()->FlushAreaForTesting(namespace_id1, storage_key1);
  area.reset();

  // Find the `MapLocator` required to modify the map's key/values in the
  // database.
  const SessionStorageMetadata::NamespaceStorageKeyMap&
      namespace_storage_key_map = session_storage_impl()
                                      ->GetMetadataForTesting()
                                      .namespace_storage_key_map();
  auto namespace_it = namespace_storage_key_map.find(namespace_id1);
  ASSERT_NE(namespace_it, namespace_storage_key_map.end());

  const std::map<blink::StorageKey,
                 scoped_refptr<DomStorageDatabase::SharedMapLocator>>&
      storage_key_map = namespace_it->second;
  auto storage_key_it = storage_key_map.find(storage_key1);
  ASSERT_NE(storage_key_it, storage_key_map.end());

  // Verify the key/value pair exists in the database.
  const DomStorageDatabase::MapLocator& map_locator = *storage_key_it->second;
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> map_entries;
  ASSERT_NO_FATAL_FAILURE(
      ReadMapKeyValuesSync(*session_storage_impl()->GetDatabaseForTesting(),
                           map_locator.Clone(), &map_entries));
  EXPECT_EQ(map_entries.size(), 1u);
  EXPECT_EQ(map_entries[key], value);

  // Delete the key/value pair from the database.
  FakeCommitter committer(session_storage_impl()->GetDatabaseForTesting(),
                          map_locator.Clone());
  committer.ClearMapSync();

  // Verify the key/value pair no longer exists in the database.
  ASSERT_NO_FATAL_FAILURE(
      ReadMapKeyValuesSync(*session_storage_impl()->GetDatabaseForTesting(),
                           map_locator.Clone(), &map_entries));
  EXPECT_EQ(map_entries.size(), 0u);

  // Now open many new wrappers (for different storage_keys) to trigger clean
  // up.
  for (int i = 1; i <= 100; ++i) {
    blink::StorageKey storage_key =
        blink::StorageKey::CreateFromStringForTesting(
            base::StringPrintf("http://example.com:%d", i));
    session_storage()->BindStorageArea(storage_key, namespace_id1,
                                       area.BindNewPipeAndPassReceiver());
    RunUntilIdle();
    area.reset();
  }

  // And make sure caches were actually cleared.
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver());
  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area.get());
  EXPECT_EQ(0ul, data.size());
}

// TODO(crbug.com/40650136): Flakes when verifying no data found.
TEST_P(SessionStorageImplTest, ClearDiskState) {
  SetBackingMode(SessionStorageImpl::BackingMode::kClearDiskStateOnOpen);
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver());

  // Verify no data.
  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area.get());
  EXPECT_EQ(0ul, data.size());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));
  area.reset();

  // Delete the namespace and shut down Session Storage, BUT persist the
  // namespace on disk.
  session_storage()->DeleteNamespace(namespace_id1, true);
  ShutDownSessionStorage();

  // This will re-initialize Session Storage and load the persisted namespace,
  // but it should have been deleted due to our backing mode.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver());

  // The data from before should not be here, because SessionStorageImpl
  // clears disk space on open.
  data = test::GetAllSync(area.get());
  EXPECT_EQ(0ul, data.size());
}

TEST_P(SessionStorageImplTest, InterruptedCloneWithDelete) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->DeleteNamespace(namespace_id1, false);

  // Open the second namespace which should be initialized and empty.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver());

  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area_n2.get());
  EXPECT_EQ(0ul, data.size());
}

TEST_P(SessionStorageImplTest, InterruptedCloneChainWithDelete) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id2, namespace_id3,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->DeleteNamespace(namespace_id2, false);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n3;
  session_storage()->BindStorageArea(storage_key1, namespace_id3,
                                     area_n3.BindNewPipeAndPassReceiver());

  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area_n3.get());
  EXPECT_EQ(0ul, data.size());
}

TEST_P(SessionStorageImplTest, InterruptedTripleCloneChain) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id4 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id2, namespace_id3,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id3, namespace_id4,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->DeleteNamespace(namespace_id3, false);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n4;
  session_storage()->BindStorageArea(storage_key1, namespace_id4,
                                     area_n4.BindNewPipeAndPassReceiver());

  // Trigger the populated of namespace 2 by deleting namespace 1.
  session_storage()->DeleteNamespace(namespace_id1, false);

  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area_n4.get());
  EXPECT_EQ(0ul, data.size());
}

TEST_P(SessionStorageImplTest, TotalCloneChainDeletion) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id4 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id2, namespace_id3,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id3, namespace_id4,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->DeleteNamespace(namespace_id2, false);
  session_storage()->DeleteNamespace(namespace_id3, false);
  session_storage()->DeleteNamespace(namespace_id1, false);
  session_storage()->DeleteNamespace(namespace_id4, false);
}

}  // namespace

TEST_P(SessionStorageImplTest, PurgeMemoryDoesNotCrashOrHang) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");

  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver());

  session_storage()->CreateNamespace(namespace_id2);
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver());

  // Put some data in both.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));
  EXPECT_TRUE(test::PutSync(area_n2.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value2"), std::nullopt,
                            "source1"));

  session_storage_impl()->FlushAreaForTesting(namespace_id1, storage_key1);

  area_n2.reset();

  RunUntilIdle();

  // Verify this doesn't crash or hang.
  session_storage_impl()->PurgeMemory();

  size_t memory_used = session_storage_impl()
                           ->GetNamespaceForTesting(namespace_id1)
                           ->storage_key_areas_[storage_key1]
                           ->data_map()
                           ->storage_area()
                           ->memory_used();
  EXPECT_EQ(0ul, memory_used);

  // Test the values is still there.
  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area_n1.get());
  EXPECT_EQ(1ul, data.size());

  std::optional<std::vector<uint8_t>> opt_value2 =
      DoTestGet(namespace_id2, storage_key1, "key1");
  ASSERT_TRUE(opt_value2);
  EXPECT_EQ(StringViewToUint8Vector("value2"), opt_value2.value());
}

TEST_P(SessionStorageImplTest, DeleteWithPersistBeforeBrowserClone) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  // Delete the storage_key namespace, but save it.
  session_storage()->DeleteNamespace(namespace_id1, true);

  // Do the browser-side clone.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver());

  // The data should be in namespace 2.
  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area_n2.get());
  EXPECT_EQ(1ul, data.size());
}

TEST_P(SessionStorageImplTest, DeleteWithoutPersistBeforeBrowserClone) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  // Delete the storage_key namespace and don't save it.
  session_storage()->DeleteNamespace(namespace_id1, false);

  // Do the browser-side clone.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver());

  // The data should be gone, because the first namespace wasn't saved to disk.
  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area_n2.get());
  EXPECT_EQ(0ul, data.size());
}

TEST_P(SessionStorageImplTest, DeleteAfterCloneWithoutMojoClone) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  // Do the browser-side clone.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Delete the storage_key namespace and don't save it.
  session_storage()->DeleteNamespace(namespace_id1, false);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver());

  // The data should be there, as the namespace should clone to all pending
  // namespaces on destruction if it didn't get a 'Clone' from mojo.
  std::vector<blink::mojom::KeyValuePtr> data = test::GetAllSync(area_n2.get());
  EXPECT_EQ(1ul, data.size());
}

// Regression test for https://crbug.com/1128318
TEST_P(SessionStorageImplTest, Bug1128318) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  // Create two namespaces by cloning.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);
  session_storage()->CloneNamespace(
      namespace_id2, namespace_id3,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);
  // And delete both namespaces again. Since namespace_id2 has child namespaces
  // that are waiting for a clone (namespace_id3), the delete will not complete
  // until the database has been initialized.
  session_storage()->DeleteNamespace(namespace_id2, false);
  session_storage()->DeleteNamespace(namespace_id3, false);
  // Now recreate one of the namespaces. The previous delete should have fully
  // completed before the namespace is recreated to prevent any dangling
  // references.
  session_storage()->CloneNamespace(
      namespace_id2, namespace_id3,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);
  RunUntilIdle();

  // At this point `namespace_id3` should be alive. It should not exist in meta
  // data yet, as that would only be populated once the actual clone happens.
  // As such, the namespace_entry for the namespace should be null.
  auto* ns = session_storage_impl()->GetNamespaceForTesting(namespace_id3);
  EXPECT_TRUE(ns);
  EXPECT_FALSE(session_storage_impl()
                   ->GetMetadataForTesting()
                   .namespace_storage_key_map()
                   .contains(namespace_id3));
}

TEST_P(SessionStorageImplTest, DeleteSessionsHistogram) {
  base::HistogramTester histograms;
  std::string namespace_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");

  // Ensure the database is fully open before calling methods.
  EnsureDatabaseOpen();

  // Put some data, and delete it without persisting. Also Flush to ensure the
  // delete is dispatched. This should fire the histogram.
  DoTestPut(namespace_id, storage_key, "key", "value", "source",
            /*should_persist=*/false);
  FlushMojo();

  WaitForDatabaseTasks();
  histograms.ExpectUniqueSample("Storage.SessionStorage.DeleteSessions.OnDisk",
                                0, 1);
}

}  // namespace storage
