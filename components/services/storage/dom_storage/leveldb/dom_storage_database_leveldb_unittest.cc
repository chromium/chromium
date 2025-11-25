// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"
#include "storage/common/database/db_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::ErrorIs;
using ::testing::IsTrue;
using ::testing::Property;
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

constexpr const char kTestDbName[] = "test_db";

// Use "test-version" for the schema version key.
constexpr const std::uint8_t kTestVersionKey[] = {'t', 'e', 's', 't', '-', 'v',
                                                  'e', 'r', 's', 'i', 'o', 'n'};
constexpr const int64_t kTestMinSupportedVersion = 2;
constexpr const int64_t kTestMaxSupportedVersion = 4;
constexpr const char kTestMaxSupportedVersionString[] = "4";

DomStorageDatabase::KeyValuePair MakeKeyValuePair(std::string_view key,
                                                  std::string_view value) {
  return {DomStorageDatabase::Key(key.begin(), key.end()),
          DomStorageDatabase::Value(value.begin(), value.end())};
}

std::string MakePrefixedKey(std::string_view prefix, std::string_view key) {
  return base::StrCat({prefix, key});
}

}  // namespace

class DomStorageDatabaseLevelDBTest : public testing::Test {
 public:
  DomStorageDatabaseLevelDBTest()
      : blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

  DomStorageDatabaseLevelDBTest(const DomStorageDatabaseLevelDBTest&) = delete;
  DomStorageDatabaseLevelDBTest& operator=(
      const DomStorageDatabaseLevelDBTest&) = delete;

 protected:
  // To create an in-memory database, provide an empty `directory`.  Asserts
  // success.
  void Open(const base::FilePath& directory,
            std::unique_ptr<DomStorageDatabaseLevelDB>* result) {
    StatusOr<std::unique_ptr<DomStorageDatabaseLevelDB>> database =
        DomStorageDatabaseLevelDB::Open(
            directory, kTestDbName, /*memory_dump_id=*/std::nullopt,
            kTestVersionKey, kTestMinSupportedVersion,
            kTestMaxSupportedVersion);

    ASSERT_TRUE(database.has_value()) << database.error().ToString();
    *result = *std::move(database);
  }

  // Helper for tests to block on the result of a Destroy call.
  DbStatus DestroySync(const base::FilePath& directory,
                       const std::string& db_name) {
    DbStatus result;
    base::RunLoop loop;
    DomStorageDatabaseLevelDB::Destroy(
        directory, db_name, blocking_task_runner_,
        base::BindLambdaForTesting([&](DbStatus status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  void TestInvalidVersion(DomStorageDatabase::ValueView invalid_version);

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
};

TEST_F(DomStorageDatabaseLevelDBTest, BasicOpenInMemory) {
  // Basic smoke test to verify that we can successfully create and destroy an
  // in-memory database with no problems.
  std::unique_ptr<DomStorageDatabaseLevelDB> database;
  ASSERT_NO_FATAL_FAILURE(Open(/*directory=*/base::FilePath(), &database));
}

TEST_F(DomStorageDatabaseLevelDBTest, BasicOperations) {
  // Exercises basic Put, Get, Delete.

  std::unique_ptr<DomStorageDatabaseLevelDB> db;
  ASSERT_NO_FATAL_FAILURE(Open(/*directory=*/base::FilePath(), &db));

  // Write a key and read it back.
  const char kTestKey[] = "test_key";
  const char kTestValue[] = "test_value";
  EXPECT_STATUS_OK(db->Put(base::byte_span_from_cstring(kTestKey),
                           base::byte_span_from_cstring(kTestValue)));

  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Value value,
                       db->Get(base::byte_span_from_cstring(kTestKey)));
  EXPECT_VALUE_EQ(kTestValue, value);
}

TEST_F(DomStorageDatabaseLevelDBTest, Reopen) {
  // Verifies that if we Put() something into a persistent database, we can
  // Get() it back out when we re-open the same database later. Also verifies
  // that this is not possible if the database is deleted.

  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  const char kTestKey[] = "test_key";
  const char kTestValue[] = "test_value";

  std::unique_ptr<DomStorageDatabaseLevelDB> db;
  ASSERT_NO_FATAL_FAILURE(Open(temp_dir.GetPath(), &db));
  EXPECT_STATUS_OK(db->Put(base::byte_span_from_cstring(kTestKey),
                           base::byte_span_from_cstring(kTestValue)));
  db.reset();

  // Re-open and verify that we can read what was written above.
  ASSERT_NO_FATAL_FAILURE(Open(temp_dir.GetPath(), &db));

  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Value value,
                       db->Get(base::byte_span_from_cstring(kTestKey)));
  EXPECT_VALUE_EQ(kTestValue, value);
  db.reset();

  // Destroy the database. Note that this should be safe to call immediately
  // after `reset()` as long as the same TaskRunner is used to open and destroy
  // the database.
  //
  // Because the database owns filesystem artifacts in the temp directory, we
  // will wait for the `DomStorageDatabaseLevelDB` instance to actually be
  // destroyed before completing the test.
  EXPECT_STATUS_OK(DestroySync(temp_dir.GetPath(), kTestDbName));

  // Verify that the database was destroyed (open again and verify it's a blank
  // slate).
  ASSERT_NO_FATAL_FAILURE(Open(temp_dir.GetPath(), &db));
  EXPECT_THAT(db->Get(base::byte_span_from_cstring(kTestKey)),
              ErrorIs(Property(&DbStatus::IsNotFound, IsTrue)));
  db.reset();
}

TEST_F(DomStorageDatabaseLevelDBTest, GetPrefixed) {
  // Verifies basic prefixed reading behavior.

  std::unique_ptr<DomStorageDatabaseLevelDB> db;
  ASSERT_NO_FATAL_FAILURE(Open(/*directory=*/base::FilePath(), &db));

  static constexpr char kTestPrefix1[] = "prefix";
  static constexpr char kTestPrefix2[] = "something_completely_different";
  static constexpr char kTestUnprefixedKey[] = "moot!";
  static constexpr char kTestKeyBase1[] = "key1";
  static constexpr char kTestKeyBase2[] = "key2";
  std::string kTestPrefix1Key1 = MakePrefixedKey(kTestPrefix1, kTestKeyBase1);
  std::string kTestPrefix1Key2 = MakePrefixedKey(kTestPrefix1, kTestKeyBase2);
  std::string kTestPrefix2Key1 = MakePrefixedKey(kTestPrefix2, kTestKeyBase1);
  std::string kTestPrefix2Key2 = MakePrefixedKey(kTestPrefix2, kTestKeyBase2);

  // No keys, so GetPrefixed should return nothing.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> entries,
      db->GetPrefixed(base::byte_span_from_cstring(kTestPrefix1)));
  EXPECT_TRUE(entries.empty());
  ASSERT_OK_AND_ASSIGN(
      entries, db->GetPrefixed(base::byte_span_from_cstring(kTestPrefix2)));
  EXPECT_TRUE(entries.empty());

  // Insert a key which matches neither test prefix. GetPrefixed should still
  // return nothing.
  EXPECT_STATUS_OK(db->Put(base::byte_span_from_cstring(kTestUnprefixedKey),
                           base::byte_span_from_cstring("meh")));
  ASSERT_OK_AND_ASSIGN(
      entries, db->GetPrefixed(base::byte_span_from_cstring(kTestPrefix1)));
  EXPECT_TRUE(entries.empty());
  ASSERT_OK_AND_ASSIGN(
      entries, db->GetPrefixed(base::byte_span_from_cstring(kTestPrefix2)));
  EXPECT_TRUE(entries.empty());

  // Insert a single prefixed key. GetPrefixed should return it when called
  // with kTestPrefix1.
  static constexpr char kTestValue1[] = "beep beep";
  EXPECT_STATUS_OK(db->Put(base::as_byte_span(kTestPrefix1Key1),
                           base::byte_span_from_cstring(kTestValue1)));
  ASSERT_OK_AND_ASSIGN(
      entries, db->GetPrefixed(base::byte_span_from_cstring(kTestPrefix1)));
  EXPECT_THAT(entries, UnorderedElementsAreArray(
                           {MakeKeyValuePair(kTestPrefix1Key1, kTestValue1)}));

  // But not when called with kTestPrefix2.
  ASSERT_OK_AND_ASSIGN(
      entries, db->GetPrefixed(base::byte_span_from_cstring(kTestPrefix2)));
  EXPECT_TRUE(entries.empty());

  // Insert a second prefixed key with kTestPrefix1, and also insert some
  // keys with kTestPrefix2.
  static constexpr char kTestValue2[] = "beep bop boop";
  static constexpr char kTestValue3[] = "vroom vroom";
  static constexpr char kTestValue4[] = "this data is lit fam";
  EXPECT_STATUS_OK(db->Put(base::as_byte_span(kTestPrefix1Key2),
                           base::byte_span_from_cstring(kTestValue2)));
  EXPECT_STATUS_OK(db->Put(base::as_byte_span(kTestPrefix2Key1),
                           base::byte_span_from_cstring(kTestValue3)));
  EXPECT_STATUS_OK(db->Put(base::as_byte_span(kTestPrefix2Key2),
                           base::byte_span_from_cstring(kTestValue4)));

  // Verify that getting each prefix yields only the expected results.
  ASSERT_OK_AND_ASSIGN(
      entries, db->GetPrefixed(base::byte_span_from_cstring(kTestPrefix1)));
  EXPECT_THAT(entries, UnorderedElementsAreArray(
                           {MakeKeyValuePair(kTestPrefix1Key1, kTestValue1),
                            MakeKeyValuePair(kTestPrefix1Key2, kTestValue2)}));

  ASSERT_OK_AND_ASSIGN(
      entries, db->GetPrefixed(base::byte_span_from_cstring(kTestPrefix2)));
  EXPECT_THAT(entries, UnorderedElementsAreArray(
                           {MakeKeyValuePair(kTestPrefix2Key1, kTestValue3),
                            MakeKeyValuePair(kTestPrefix2Key2, kTestValue4)}));
}

TEST_F(DomStorageDatabaseLevelDBTest, DeletePrefixed) {
  // Verifies basic prefixed deletion behavior.

  std::unique_ptr<DomStorageDatabaseLevelDB> db;
  ASSERT_NO_FATAL_FAILURE(Open(/*directory=*/base::FilePath(), &db));

  static constexpr char kTestPrefix1[] = "prefix";
  static constexpr char kTestPrefix2[] = "something_completely_different";
  static constexpr char kTestUnprefixedKey[] = "moot!";
  static constexpr char kTestKeyBase1[] = "key1";
  static constexpr char kTestKeyBase2[] = "key2";
  std::string kTestPrefix1Key1 = MakePrefixedKey(kTestPrefix1, kTestKeyBase1);
  std::string kTestPrefix1Key2 = MakePrefixedKey(kTestPrefix1, kTestKeyBase2);
  std::string kTestPrefix2Key1 = MakePrefixedKey(kTestPrefix2, kTestKeyBase1);
  std::string kTestPrefix2Key2 = MakePrefixedKey(kTestPrefix2, kTestKeyBase2);
  // Insert a bunch of entries. One unprefixed, two with one prefix, and two
  // with another prefix.
  static constexpr char kTestValue1[] = "meh";
  static constexpr char kTestValue2[] = "bah";
  static constexpr char kTestValue3[] = "doh";
  EXPECT_STATUS_OK(db->Put(base::byte_span_from_cstring(kTestUnprefixedKey),
                           base::byte_span_from_cstring(kTestValue1)));
  EXPECT_STATUS_OK(db->Put(base::as_byte_span(kTestPrefix1Key1),
                           base::byte_span_from_cstring("x")));
  EXPECT_STATUS_OK(db->Put(base::as_byte_span(kTestPrefix1Key2),
                           base::byte_span_from_cstring("x")));
  EXPECT_STATUS_OK(db->Put(base::as_byte_span(kTestPrefix2Key1),
                           base::byte_span_from_cstring(kTestValue2)));
  EXPECT_STATUS_OK(db->Put(base::as_byte_span(kTestPrefix2Key2),
                           base::byte_span_from_cstring(kTestValue3)));

  // Wipe out the first prefix. We should still see the second prefix.
  std::unique_ptr<DomStorageBatchOperationLevelDB> batch =
      db->CreateBatchOperation();
  EXPECT_STATUS_OK(
      batch->DeletePrefixed(base::byte_span_from_cstring(kTestPrefix1)));
  EXPECT_STATUS_OK(batch->Commit());
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> entries,
      db->GetPrefixed(base::byte_span_from_cstring(kTestPrefix1)));
  EXPECT_TRUE(entries.empty());
  ASSERT_OK_AND_ASSIGN(
      entries, db->GetPrefixed(base::byte_span_from_cstring(kTestPrefix2)));
  EXPECT_THAT(entries, UnorderedElementsAreArray(
                           {MakeKeyValuePair(kTestPrefix2Key1, kTestValue2),
                            MakeKeyValuePair(kTestPrefix2Key2, kTestValue3)}));

  // Wipe out the second prefix.
  batch = db->CreateBatchOperation();
  EXPECT_STATUS_OK(
      batch->DeletePrefixed(base::byte_span_from_cstring(kTestPrefix2)));
  EXPECT_STATUS_OK(batch->Commit());
  ASSERT_OK_AND_ASSIGN(
      entries, db->GetPrefixed(base::byte_span_from_cstring(kTestPrefix2)));

  // The lone unprefixed value should still exist.
  ASSERT_OK_AND_ASSIGN(
      DomStorageDatabase::Value value,
      db->Get(base::byte_span_from_cstring(kTestUnprefixedKey)));
  EXPECT_VALUE_EQ(kTestValue1, value);
}

TEST_F(DomStorageDatabaseLevelDBTest, CopyPrefixed) {
  // Verifies basic prefixed copying behavior.

  std::unique_ptr<DomStorageDatabaseLevelDB> db;
  ASSERT_NO_FATAL_FAILURE(Open(/*directory=*/base::FilePath(), &db));

  static constexpr char kTestUnprefixedKey[] = "moot!";
  static constexpr char kTestPrefix1[] = "prefix";
  static constexpr char kTestPrefix2[] = "something_completely_different";
  static constexpr char kTestKeyBase1[] = "key1";
  static constexpr char kTestKeyBase2[] = "key2";
  std::string kTestPrefix1Key1 = MakePrefixedKey(kTestPrefix1, kTestKeyBase1);
  std::string kTestPrefix1Key2 = MakePrefixedKey(kTestPrefix1, kTestKeyBase2);
  std::string kTestPrefix2Key1 = MakePrefixedKey(kTestPrefix2, kTestKeyBase1);
  std::string kTestPrefix2Key2 = MakePrefixedKey(kTestPrefix2, kTestKeyBase2);
  static constexpr char kTestValue1[] = "a value";
  static constexpr char kTestValue2[] = "another value";
  static constexpr char kTestValue3[] = "the only other value in the world ";

  // Populate the database with one unprefixed entry, and two values with
  // a key prefix of |kTestPrefix1|.
  EXPECT_STATUS_OK(db->Put(base::byte_span_from_cstring(kTestUnprefixedKey),
                           base::byte_span_from_cstring(kTestValue1)));
  EXPECT_STATUS_OK(db->Put(base::as_byte_span(kTestPrefix1Key1),
                           base::byte_span_from_cstring(kTestValue2)));
  EXPECT_STATUS_OK(db->Put(base::as_byte_span(kTestPrefix1Key2),
                           base::byte_span_from_cstring(kTestValue3)));

  // Copy the prefixed entries to |kTestPrefix2| and verify that we have the
  // expected entries.
  std::unique_ptr<DomStorageBatchOperationLevelDB> batch =
      db->CreateBatchOperation();
  EXPECT_STATUS_OK(
      batch->CopyPrefixed(base::byte_span_from_cstring(kTestPrefix1),
                          base::byte_span_from_cstring(kTestPrefix2)));
  EXPECT_STATUS_OK(batch->Commit());

  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> entries,
      db->GetPrefixed(base::byte_span_from_cstring(kTestPrefix2)));
  EXPECT_THAT(entries, UnorderedElementsAreArray(
                           {MakeKeyValuePair(kTestPrefix2Key1, kTestValue2),
                            MakeKeyValuePair(kTestPrefix2Key2, kTestValue3)}));

  // The original prefixed values should still be there too.
  ASSERT_OK_AND_ASSIGN(
      entries, db->GetPrefixed(base::byte_span_from_cstring(kTestPrefix1)));
  EXPECT_THAT(entries, UnorderedElementsAreArray(
                           {MakeKeyValuePair(kTestPrefix1Key1, kTestValue2),
                            MakeKeyValuePair(kTestPrefix1Key2, kTestValue3)}));
}

TEST_F(DomStorageDatabaseLevelDBTest, OpenWritesVersion) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::unique_ptr<DomStorageDatabaseLevelDB> db;
  ASSERT_NO_FATAL_FAILURE(Open(temp_dir.GetPath(), &db));

  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Value version_bytes,
                       db->Get(kTestVersionKey));
  EXPECT_EQ(version_bytes,
            base::as_byte_span(std::string(kTestMaxSupportedVersionString)));

  // Re-open the database. `EnsureVersion()` must read the existing value.
  db.reset();
  ASSERT_NO_FATAL_FAILURE(Open(temp_dir.GetPath(), &db));

  ASSERT_OK_AND_ASSIGN(version_bytes, db->Get(kTestVersionKey));
  EXPECT_EQ(version_bytes,
            base::as_byte_span(std::string(kTestMaxSupportedVersionString)));
}

void DomStorageDatabaseLevelDBTest::TestInvalidVersion(
    DomStorageDatabase::ValueView invalid_version) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::unique_ptr<DomStorageDatabaseLevelDB> db;
  ASSERT_NO_FATAL_FAILURE(Open(temp_dir.GetPath(), &db));

  // Write the invalid version in the database.
  DomStorageDatabase::Value version_string_bytes;
  DbStatus status = db->Put(kTestVersionKey, invalid_version);
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Re-open the database, which must fail due to the invalid version number.
  db.reset();
  StatusOr<std::unique_ptr<DomStorageDatabaseLevelDB>> reopened_database =
      DomStorageDatabaseLevelDB::Open(temp_dir.GetPath(), kTestDbName,
                                      /*memory_dump_id=*/std::nullopt,
                                      kTestVersionKey, kTestMinSupportedVersion,
                                      kTestMaxSupportedVersion);
  ASSERT_FALSE(reopened_database.has_value());
  EXPECT_TRUE(reopened_database.error().IsCorruption());
}

TEST_F(DomStorageDatabaseLevelDBTest, OpenFailsWhenVersionNotANumber) {
  // 'a' is not a valid version.
  ASSERT_NO_FATAL_FAILURE(TestInvalidVersion({'a'}));
}

TEST_F(DomStorageDatabaseLevelDBTest, OpenFailsWithVersionBelowMin) {
  // '1' is less than the minimum version.
  ASSERT_NO_FATAL_FAILURE(TestInvalidVersion({'1'}));
}

TEST_F(DomStorageDatabaseLevelDBTest, OpenFailsWithVersionAboveMax) {
  // '5' is less than the minimum version.
  ASSERT_NO_FATAL_FAILURE(TestInvalidVersion({'5'}));
}

}  // namespace storage
