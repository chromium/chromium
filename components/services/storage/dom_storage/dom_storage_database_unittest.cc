// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/dom_storage_database.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

using ::testing::UnorderedElementsAreArray;

// Helper to make Status checks a little more legible in test failures.
#define EXPECT_STATUS(expectation, value)                                 \
  ([](auto s) {                                                           \
    EXPECT_TRUE(s.expectation()) << "Actual status is: " << s.ToString(); \
  }(value))

#define EXPECT_STATUS_OK(value) EXPECT_STATUS(ok, value)

// Helper to make database value expectation checks and failures more legible
#define EXPECT_VALUE_EQ(expectation, value) \
  EXPECT_EQ(std::string(expectation), std::string(value.begin(), value.end()))

namespace storage {

std::ostream& operator<<(std::ostream& os,
                         const DomStorageDatabase::KeyValuePair& kvp) {
  os << "<\"" << std::string(kvp.key.begin(), kvp.key.end()) << "\", \""
     << std::string(kvp.value.begin(), kvp.value.end()) << "\">";
  return os;
}

namespace {

// Helper for tests to create a |uint8_t| span from a std::string_view.
base::span<const uint8_t> MakeBytes(std::string_view s) {
  return base::as_bytes(base::make_span(s));
}

DomStorageDatabase::KeyValuePair MakeKeyValuePair(std::string_view key,
                                                  std::string_view value) {
  return {DomStorageDatabase::Key(key.begin(), key.end()),
          DomStorageDatabase::Value(value.begin(), value.end())};
}

std::string MakePrefixedKey(std::string_view prefix, std::string_view key) {
  return std::string(prefix) + std::string(key);
}

class StorageServiceDomStorageDatabaseTest : public testing::Test {
 public:
  StorageServiceDomStorageDatabaseTest()
      : blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

  StorageServiceDomStorageDatabaseTest(
      const StorageServiceDomStorageDatabaseTest&) = delete;
  StorageServiceDomStorageDatabaseTest& operator=(
      const StorageServiceDomStorageDatabaseTest&) = delete;

 protected:
  // Helper for tests to block on the result of an OpenInMemory call.
  base::SequenceBound<DomStorageDatabase> OpenInMemorySync(
      const std::string& db_name) {
    base::SequenceBound<DomStorageDatabase> result;
    base::RunLoop loop;
    DomStorageDatabase::OpenInMemory(
        db_name, /*memory_dump_id=*/std::nullopt, blocking_task_runner_,
        base::BindLambdaForTesting(
            [&](base::SequenceBound<DomStorageDatabase> database,
                leveldb::Status status) {
              result = std::move(database);
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  // Helper for tests to block on the result of an OpenDirectory call.
  base::SequenceBound<DomStorageDatabase> OpenDirectorySync(
      const base::FilePath& directory,
      const std::string& db_name) {
    base::SequenceBound<DomStorageDatabase> result;
    base::RunLoop loop;
    DomStorageDatabase::OpenDirectory(
        directory, db_name, /*memory_dump_id=*/std::nullopt,
        blocking_task_runner_,
        base::BindLambdaForTesting(
            [&](base::SequenceBound<DomStorageDatabase> database,
                leveldb::Status status) {
              result = std::move(database);
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  // Helper for tests to block on the result of a Destroy call.
  leveldb::Status DestroySync(const base::FilePath& directory,
                              const std::string& db_name) {
    leveldb::Status result;
    base::RunLoop loop;
    DomStorageDatabase::Destroy(
        directory, db_name, blocking_task_runner_,
        base::BindLambdaForTesting([&](leveldb::Status status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  // Helper to run an async operation on a DomStorageDatabase and wait for it to
  // finish.
  template <typename Func>
  static void DoSync(const base::SequenceBound<DomStorageDatabase>& database,
                     Func operation) {
    base::RunLoop loop;
    database.PostTaskWithThisObject(
        base::BindLambdaForTesting([&](const DomStorageDatabase& database) {
          operation(database);
          loop.Quit();
        }));
    loop.Run();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
};

}  // namespace

TEST_F(StorageServiceDomStorageDatabaseTest, BasicOpenInMemory) {
  // Basic smoke test to verify that we can successfully create and destroy an
  // in-memory database with no problems.
  base::SequenceBound<DomStorageDatabase> database =
      OpenInMemorySync("test_db");
  EXPECT_TRUE(database);
}

TEST_F(StorageServiceDomStorageDatabaseTest, BasicOperations) {
  // Exercises basic Put, Get, Delete.

  base::SequenceBound<DomStorageDatabase> database =
      OpenInMemorySync("test_db");
  ASSERT_TRUE(database);

  // Write a key and read it back.
  const char kTestKey[] = "test_key";
  const char kTestValue[] = "test_value";
  DoSync(database, [&](const DomStorageDatabase& db) {
    EXPECT_STATUS_OK(db.Put(MakeBytes(kTestKey), MakeBytes(kTestValue)));

    DomStorageDatabase::Value value;
    EXPECT_STATUS_OK(db.Get(MakeBytes(kTestKey), &value));
    EXPECT_VALUE_EQ(kTestValue, value);
  });
}

TEST_F(StorageServiceDomStorageDatabaseTest, Reopen) {
  // Verifies that if we Put() something into a persistent database, we can
  // Get() it back out when we re-open the same database later. Also verifies
  // that this is not possible if the database is deleted.

  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  const char kTestDbName[] = "test_db";
  const char kTestKey[] = "test_key";
  const char kTestValue[] = "test_value";

  base::SequenceBound<DomStorageDatabase> database =
      OpenDirectorySync(temp_dir.GetPath(), kTestDbName);
  ASSERT_TRUE(database);
  DoSync(database, [&](const DomStorageDatabase& db) {
    EXPECT_STATUS_OK(db.Put(MakeBytes(kTestKey), MakeBytes(kTestValue)));
  });
  database.Reset();

  // Re-open and verify that we can read what was written above.
  database = OpenDirectorySync(temp_dir.GetPath(), kTestDbName);
  ASSERT_TRUE(database);
  DoSync(database, [&](const DomStorageDatabase& db) {
    DomStorageDatabase::Value value;
    EXPECT_STATUS_OK(db.Get(MakeBytes(kTestKey), &value));
    EXPECT_VALUE_EQ(kTestValue, value);
  });
  database.Reset();

  // Destroy the database. Note that this should be safe to call immediately
  // after |Reset()| as long as the same TaskRunner is used to open and destroy
  // the database.
  //
  // Because the database owns filesystem artifacts in the temp directory, we
  // will wait for the DomStorageDatabase instance to actually be destroyed
  // before completing the test.
  EXPECT_STATUS_OK(DestroySync(temp_dir.GetPath(), kTestDbName));

  // Verify that the database was destroyed (open again and verify it's a blank
  // slate).
  database = OpenDirectorySync(temp_dir.GetPath(), kTestDbName);
  ASSERT_TRUE(database);
  DoSync(database, [&](const DomStorageDatabase& db) {
    DomStorageDatabase::Value value;
    EXPECT_TRUE(db.Get(MakeBytes(kTestKey), &value).IsNotFound());
  });

  // Because the database owns filesystem artifacts in the temp directory, block
  // scope teardown until the DomStorageDatabase instance is actually destroyed
  // on its background sequence.
  database.SynchronouslyResetForTest();
}

TEST_F(StorageServiceDomStorageDatabaseTest, GetPrefixed) {
  // Verifies basic prefixed reading behavior.

  base::SequenceBound<DomStorageDatabase> database =
      OpenInMemorySync("test_db");
  ASSERT_TRUE(database);

  const char kTestPrefix1[] = "prefix";
  const char kTestPrefix2[] = "something_completely_different";
  const char kTestUnprefixedKey[] = "moot!";
  const char kTestKeyBase1[] = "key1";
  const char kTestKeyBase2[] = "key2";
  auto kTestPrefix1Key1 = MakePrefixedKey(kTestPrefix1, kTestKeyBase1);
  auto kTestPrefix1Key2 = MakePrefixedKey(kTestPrefix1, kTestKeyBase2);
  auto kTestPrefix2Key1 = MakePrefixedKey(kTestPrefix2, kTestKeyBase1);
  auto kTestPrefix2Key2 = MakePrefixedKey(kTestPrefix2, kTestKeyBase2);
  DoSync(database, [&](const DomStorageDatabase& db) {
    std::vector<DomStorageDatabase::KeyValuePair> entries;

    // No keys, so GetPrefixed should return nothing.
    EXPECT_STATUS_OK(db.GetPrefixed(MakeBytes(kTestPrefix1), &entries));
    EXPECT_TRUE(entries.empty());
    EXPECT_STATUS_OK(db.GetPrefixed(MakeBytes(kTestPrefix2), &entries));
    EXPECT_TRUE(entries.empty());

    // Insert a key which matches neither test prefix. GetPrefixed should still
    // return nothing.
    EXPECT_STATUS_OK(db.Put(MakeBytes(kTestUnprefixedKey), MakeBytes("meh")));
    EXPECT_STATUS_OK(db.GetPrefixed(MakeBytes(kTestPrefix1), &entries));
    EXPECT_TRUE(entries.empty());
    EXPECT_STATUS_OK(db.GetPrefixed(MakeBytes(kTestPrefix2), &entries));
    EXPECT_TRUE(entries.empty());

    // Insert a single prefixed key. GetPrefixed should return it when called
    // with kTestPrefix1.
    const char kTestValue1[] = "beep beep";
    EXPECT_STATUS_OK(
        db.Put(MakeBytes(kTestPrefix1Key1), MakeBytes(kTestValue1)));
    EXPECT_STATUS_OK(db.GetPrefixed(MakeBytes(kTestPrefix1), &entries));
    EXPECT_THAT(entries, UnorderedElementsAreArray({MakeKeyValuePair(
                             kTestPrefix1Key1, kTestValue1)}));

    // But not when called with kTestPrefix2.
    entries.clear();
    EXPECT_STATUS_OK(db.GetPrefixed(MakeBytes(kTestPrefix2), &entries));
    EXPECT_TRUE(entries.empty());

    // Insert a second prefixed key with kTestPrefix1, and also insert some
    // keys with kTestPrefix2.
    const char kTestValue2[] = "beep bop boop";
    const char kTestValue3[] = "vroom vroom";
    const char kTestValue4[] = "this data is lit fam";
    EXPECT_STATUS_OK(
        db.Put(MakeBytes(kTestPrefix1Key2), MakeBytes(kTestValue2)));
    EXPECT_STATUS_OK(
        db.Put(MakeBytes(kTestPrefix2Key1), MakeBytes(kTestValue3)));
    EXPECT_STATUS_OK(
        db.Put(MakeBytes(kTestPrefix2Key2), MakeBytes(kTestValue4)));

    // Verify that getting each prefix yields only the expected results.
    EXPECT_STATUS_OK(db.GetPrefixed(MakeBytes(kTestPrefix1), &entries));
    EXPECT_THAT(entries,
                UnorderedElementsAreArray(
                    {MakeKeyValuePair(kTestPrefix1Key1, kTestValue1),
                     MakeKeyValuePair(kTestPrefix1Key2, kTestValue2)}));
    entries.clear();

    EXPECT_STATUS_OK(db.GetPrefixed(MakeBytes(kTestPrefix2), &entries));
    EXPECT_THAT(entries,
                UnorderedElementsAreArray(
                    {MakeKeyValuePair(kTestPrefix2Key1, kTestValue3),
                     MakeKeyValuePair(kTestPrefix2Key2, kTestValue4)}));
  });
}

TEST_F(StorageServiceDomStorageDatabaseTest, DeletePrefixed) {
  // Verifies basic prefixed deletion behavior.

  base::SequenceBound<DomStorageDatabase> database =
      OpenInMemorySync("test_db");
  ASSERT_TRUE(database);

  const char kTestPrefix1[] = "prefix";
  const char kTestPrefix2[] = "something_completely_different";
  const char kTestUnprefixedKey[] = "moot!";
  const char kTestKeyBase1[] = "key1";
  const char kTestKeyBase2[] = "key2";
  auto kTestPrefix1Key1 = MakePrefixedKey(kTestPrefix1, kTestKeyBase1);
  auto kTestPrefix1Key2 = MakePrefixedKey(kTestPrefix1, kTestKeyBase2);
  auto kTestPrefix2Key1 = MakePrefixedKey(kTestPrefix2, kTestKeyBase1);
  auto kTestPrefix2Key2 = MakePrefixedKey(kTestPrefix2, kTestKeyBase2);
  DoSync(database, [&](const DomStorageDatabase& db) {
    // Insert a bunch of entries. One unprefixed, two with one prefix, and two
    // with another prefix.
    const char kTestValue1[] = "meh";
    const char kTestValue2[] = "bah";
    const char kTestValue3[] = "doh";
    EXPECT_STATUS_OK(
        db.Put(MakeBytes(kTestUnprefixedKey), MakeBytes(kTestValue1)));
    EXPECT_STATUS_OK(db.Put(MakeBytes(kTestPrefix1Key1), MakeBytes("x")));
    EXPECT_STATUS_OK(db.Put(MakeBytes(kTestPrefix1Key2), MakeBytes("x")));
    EXPECT_STATUS_OK(
        db.Put(MakeBytes(kTestPrefix2Key1), MakeBytes(kTestValue2)));
    EXPECT_STATUS_OK(
        db.Put(MakeBytes(kTestPrefix2Key2), MakeBytes(kTestValue3)));

    // Wipe out the first prefix. We should still see the second prefix.
    std::vector<DomStorageDatabase::KeyValuePair> entries;
    leveldb::WriteBatch batch;
    EXPECT_STATUS_OK(db.DeletePrefixed(MakeBytes(kTestPrefix1), &batch));
    EXPECT_STATUS_OK(db.Commit(&batch));
    EXPECT_STATUS_OK(db.GetPrefixed(MakeBytes(kTestPrefix1), &entries));
    EXPECT_TRUE(entries.empty());
    EXPECT_STATUS_OK(db.GetPrefixed(MakeBytes(kTestPrefix2), &entries));
    EXPECT_THAT(entries,
                UnorderedElementsAreArray(
                    {MakeKeyValuePair(kTestPrefix2Key1, kTestValue2),
                     MakeKeyValuePair(kTestPrefix2Key2, kTestValue3)}));

    // Wipe out the second prefix.
    batch.Clear();
    EXPECT_STATUS_OK(db.DeletePrefixed(MakeBytes(kTestPrefix2), &batch));
    EXPECT_STATUS_OK(db.Commit(&batch));
    EXPECT_STATUS_OK(db.GetPrefixed(MakeBytes(kTestPrefix2), &entries));

    // The lone unprefixed value should still exist.
    DomStorageDatabase::Value value;
    EXPECT_STATUS_OK(db.Get(MakeBytes(kTestUnprefixedKey), &value));
    EXPECT_VALUE_EQ(kTestValue1, value);
  });
}

TEST_F(StorageServiceDomStorageDatabaseTest, CopyPrefixed) {
  // Verifies basic prefixed copying behavior.

  base::SequenceBound<DomStorageDatabase> database =
      OpenInMemorySync("test_db");
  ASSERT_TRUE(database);

  const char kTestUnprefixedKey[] = "moot!";
  const char kTestPrefix1[] = "prefix";
  const char kTestPrefix2[] = "something_completely_different";
  const char kTestKeyBase1[] = "key1";
  const char kTestKeyBase2[] = "key2";
  auto kTestPrefix1Key1 = MakePrefixedKey(kTestPrefix1, kTestKeyBase1);
  auto kTestPrefix1Key2 = MakePrefixedKey(kTestPrefix1, kTestKeyBase2);
  auto kTestPrefix2Key1 = MakePrefixedKey(kTestPrefix2, kTestKeyBase1);
  auto kTestPrefix2Key2 = MakePrefixedKey(kTestPrefix2, kTestKeyBase2);
  const char kTestValue1[] = "a value";
  const char kTestValue2[] = "another value";
  const char kTestValue3[] = "the only other value in the world";

  DoSync(database, [&](const DomStorageDatabase& db) {
    // Populate the database with one unprefixed entry, and two values with
    // a key prefix of |kTestPrefix1|.
    EXPECT_STATUS_OK(
        db.Put(MakeBytes(kTestUnprefixedKey), MakeBytes(kTestValue1)));
    EXPECT_STATUS_OK(
        db.Put(MakeBytes(kTestPrefix1Key1), MakeBytes(kTestValue2)));
    EXPECT_STATUS_OK(
        db.Put(MakeBytes(kTestPrefix1Key2), MakeBytes(kTestValue3)));

    // Copy the prefixed entries to |kTestPrefix2| and verify that we have the
    // expected entries.
    leveldb::WriteBatch batch;
    EXPECT_STATUS_OK(db.CopyPrefixed(MakeBytes(kTestPrefix1),
                                     MakeBytes(kTestPrefix2), &batch));
    EXPECT_STATUS_OK(db.Commit(&batch));

    std::vector<DomStorageDatabase::KeyValuePair> entries;
    EXPECT_STATUS_OK(db.GetPrefixed(MakeBytes(kTestPrefix2), &entries));
    EXPECT_THAT(entries,
                UnorderedElementsAreArray(
                    {MakeKeyValuePair(kTestPrefix2Key1, kTestValue2),
                     MakeKeyValuePair(kTestPrefix2Key2, kTestValue3)}));

    // The original prefixed values should still be there too.
    entries.clear();
    EXPECT_STATUS_OK(db.GetPrefixed(MakeBytes(kTestPrefix1), &entries));
    EXPECT_THAT(entries,
                UnorderedElementsAreArray(
                    {MakeKeyValuePair(kTestPrefix1Key1, kTestValue2),
                     MakeKeyValuePair(kTestPrefix1Key2, kTestValue3)}));
  });
}

}  // namespace storage
