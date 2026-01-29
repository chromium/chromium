// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite/local_storage_sqlite.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/sqlite/sqlite_database_utils.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

namespace {
constexpr const char kFirstUrlString[] = "https://a-fake.test/";
constexpr const char kSecondUrlString[] = "https://b-fake.test/";
constexpr const char kThirdUrlString[] = "https://c-fake.test/";
}  // namespace

class LocalStorageSqliteTest : public testing::Test {
 protected:
  LocalStorageSqliteTest();
  ~LocalStorageSqliteTest() override;

  // Creates a path to the SQLite database file under `temp_dir_`.  Creates
  // `temp_dir_` when necessary.
  void GetDatabasePath(base::FilePath* result);

  // Returns the `PassKey` required to create and open `LocalStorageSqlite`.
  base::PassKey<DomStorageDatabaseFactory> GetPassKey();

  void OpenOnDisk(std::unique_ptr<LocalStorageSqlite>* result);

  void OpenInMemory(std::unique_ptr<LocalStorageSqlite>* result);

  const blink::StorageKey kFirstStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kFirstUrlString);

  const blink::StorageKey kSecondStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kSecondUrlString);

  const blink::StorageKey kThirdStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kThirdUrlString);

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

LocalStorageSqliteTest::LocalStorageSqliteTest() {
  scoped_feature_list_.InitAndEnableFeature(kDomStorageSqlite);
}

LocalStorageSqliteTest::~LocalStorageSqliteTest() = default;

void LocalStorageSqliteTest::GetDatabasePath(base::FilePath* result) {
  if (!temp_dir_.IsValid()) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  *result = DomStorageDatabase::GetPath(StorageType::kLocalStorage,
                                        temp_dir_.GetPath());
}

base::PassKey<DomStorageDatabaseFactory> LocalStorageSqliteTest::GetPassKey() {
  return DomStorageDatabaseFactory::CreatePassKeyForTesting();
}

void LocalStorageSqliteTest::OpenOnDisk(
    std::unique_ptr<LocalStorageSqlite>* result) {
  base::FilePath database_path;
  ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));

  std::unique_ptr<LocalStorageSqlite> instance =
      std::make_unique<LocalStorageSqlite>(GetPassKey());

  DbStatus status = instance->Open(GetPassKey(),
                                   /*database_path=*/database_path,
                                   /*memory_dump_id=*/std::nullopt);

  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(instance);
}

void LocalStorageSqliteTest::OpenInMemory(
    std::unique_ptr<LocalStorageSqlite>* result) {
  std::unique_ptr<LocalStorageSqlite> instance =
      std::make_unique<LocalStorageSqlite>(GetPassKey());

  DbStatus status = instance->Open(GetPassKey(),
                                   /*database_path=*/base::FilePath(),
                                   /*memory_dump_id=*/std::nullopt);

  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(instance);
}

TEST_F(LocalStorageSqliteTest, OpenInMemory) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));
}

TEST_F(LocalStorageSqliteTest, OpenThenDestroyOnDisk) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));
  database.reset();

  base::FilePath database_path;
  ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));
  EXPECT_TRUE(base::PathExists(database_path));

  DbStatus status = sqlite::DestroyDatabase(database_path);
  EXPECT_TRUE(status.ok()) << status.ToString();
  EXPECT_FALSE(base::PathExists(database_path));
}

TEST_F(LocalStorageSqliteTest, VersionTooNew) {
  base::FilePath database_path;
  ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));

  // Write the wrong version to the database
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));
  database->PutVersionForTesting(9999);
  database.reset();

  // Opening the database with the wrong version must fail.
  database = std::make_unique<LocalStorageSqlite>(GetPassKey());
  DbStatus status = database->Open(GetPassKey(),
                                   /*database_path=*/database_path,
                                   /*memory_dump_id=*/std::nullopt);
  EXPECT_TRUE(status.IsNotFound());
}

// Verifies that reading metadata from an empty database returns default values:
// `next_map_id` should be `std::nullopt` and `map_metadata` should be empty.
TEST_F(LocalStorageSqliteTest, ReadAllMetadataWithEmpty) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata metadata,
                       database->ReadAllMetadata());

  EXPECT_EQ(metadata.next_map_id, std::nullopt);
  EXPECT_EQ(metadata.map_metadata.size(), 0u);
}

// Verifies that `PutMetadata()` correctly persists map metadata for a single
// storage key, and that subsequent calls with the same storage key update the
// existing row rather than creating a new one. Also verifies that `COALESCE`
// preserves existing field values when new values are `NULL`, allowing partial
// updates (e.g., updating `last_modified` without overwriting `last_accessed`).
TEST_F(LocalStorageSqliteTest, PutMetadataThenUpdate) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  const DomStorageDatabase::MapMetadata kInitialMapMetadata[] = {
      {
          .map_locator{kLocalStorageSessionId, kFirstStorageKey, /*map_id=*/1},
          .last_accessed = base::Time::UnixEpoch() + base::Days(1),
      },
  };
  std::vector<DomStorageDatabase::MapMetadata> expected_map_metadata =
      CloneMapMetadataVector(kInitialMapMetadata);

  DomStorageDatabase::Metadata initial_metadata(
      CloneMapMetadataVector(expected_map_metadata));

  DbStatus status = database->PutMetadata(std::move(initial_metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the initial metadata was persisted.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());

  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              expected_map_metadata);
  EXPECT_EQ(read_metadata.next_map_id, std::nullopt);

  // Update the metadata with `last_modified` and `total_size` (without
  // `last_accessed`). This should preserve `last_accessed` due to `COALESCE`.
  expected_map_metadata[0].last_modified =
      base::Time::UnixEpoch() + base::Days(20);
  expected_map_metadata[0].total_size = base::ByteSize(500u);

  DomStorageDatabase::Metadata update_metadata(
      CloneMapMetadataVector(expected_map_metadata));

  status = database->PutMetadata(std::move(update_metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the update preserved existing fields and added new ones.
  ASSERT_OK_AND_ASSIGN(read_metadata, database->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              expected_map_metadata);
  EXPECT_EQ(read_metadata.next_map_id, std::nullopt);
}

// Verifies that `PutMetadata()` correctly handles multiple storage keys in a
// single call, each with different combinations of optional fields set. Also
// verifies that each storage key gets a unique `row_id` (used as `map_id`).
TEST_F(LocalStorageSqliteTest, PutMetadataWithMultipleMaps) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Insert metadata for multiple storage keys in a single call.
  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kLocalStorageSessionId, kFirstStorageKey,
                       /*map_id=*/1},
          .last_accessed = base::Time::UnixEpoch() + base::Days(1),
          .last_modified = base::Time::UnixEpoch() + base::Days(2),
          .total_size = base::ByteSize(100u),
      },
      {
          .map_locator{kLocalStorageSessionId, kSecondStorageKey, /*map_id=*/2},
          .last_accessed = base::Time::UnixEpoch() + base::Days(10),
          .last_modified = base::Time::UnixEpoch() + base::Days(20),
          .total_size = base::ByteSize(500u),
      },
      {
          .map_locator{kLocalStorageSessionId, kThirdStorageKey, /*map_id=*/3},
          .last_accessed = base::Time::UnixEpoch() + base::Days(100),
      },
  };
  DomStorageDatabase::Metadata metadata_to_write(
      CloneMapMetadataVector(kExpectedMapMetadata));

  DbStatus status = database->PutMetadata(std::move(metadata_to_write));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify all metadata was persisted.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());

  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata, kExpectedMapMetadata);
  EXPECT_EQ(read_metadata.next_map_id, std::nullopt);
}

// Verifies that metadata written to the database is persisted across database
// close and reopen operations.
TEST_F(LocalStorageSqliteTest, MetadataPersistence) {
  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kLocalStorageSessionId, kFirstStorageKey, /*map_id=*/1},
          .last_accessed = base::Time::UnixEpoch() + base::Days(1),
          .last_modified = base::Time::UnixEpoch() + base::Days(2),
          .total_size = base::ByteSize(100u),
      },
      {
          .map_locator{kLocalStorageSessionId, kSecondStorageKey, /*map_id=*/2},
          .last_accessed = base::Time::UnixEpoch() + base::Days(10),
      },
  };

  // Open database and write metadata.
  {
    std::unique_ptr<LocalStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    DomStorageDatabase::Metadata metadata(
        CloneMapMetadataVector(kExpectedMapMetadata));

    DbStatus status = database->PutMetadata(std::move(metadata));
    EXPECT_TRUE(status.ok()) << status.ToString();

    // Verify metadata was written correctly before closing.
    ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                         database->ReadAllMetadata());

    ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                                kExpectedMapMetadata);
    EXPECT_EQ(read_metadata.next_map_id, std::nullopt);
  }

  // Re-open the database and verify metadata persisted.
  {
    std::unique_ptr<LocalStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                         database->ReadAllMetadata());

    ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                                kExpectedMapMetadata);
    EXPECT_EQ(read_metadata.next_map_id, std::nullopt);
  }
}

// Verifies that `ReadAllMetadata()` returns a corruption error when the
// database contains a storage key that cannot be deserialized.
TEST_F(LocalStorageSqliteTest, ReadAllMetadataWithInvalidStorageKey) {
  // Write valid metadata to ensure the table exists and works.
  {
    std::unique_ptr<LocalStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    DomStorageDatabase::Metadata valid_metadata;
    valid_metadata.map_metadata.push_back({
        .map_locator{kLocalStorageSessionId, kFirstStorageKey,
                     /*map_id=*/1},
        .last_accessed = base::Time::UnixEpoch() + base::Days(1),
    });
    DbStatus status = database->PutMetadata(std::move(valid_metadata));
    EXPECT_TRUE(status.ok()) << status.ToString();
  }

  // Use `sql::Database` directly to insert an invalid storage key.
  {
    base::FilePath database_path;
    ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));

    sql::Database database(sql::test::kTestTag);
    EXPECT_TRUE(database.Open(database_path));

    static constexpr char kInsertInvalidStorageKey[] =
        "INSERT INTO maps (storage_key, last_accessed) VALUES (?, ?)";

    sql::Statement statement(
        database.GetUniqueStatement(kInsertInvalidStorageKey));
    statement.BindBlob(
        0, /*storage_key=*/std::vector<uint8_t>{0xFF, 0xFE, 0x00, 0x01});
    statement.BindTime(1, /*last_accessed=*/base::Time::UnixEpoch());

    EXPECT_TRUE(statement.Run());
  }

  // Re-open the database and verify that `ReadAllMetadata()` returns a
  // corruption error due to the invalid storage key.
  {
    std::unique_ptr<LocalStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    StatusOr<DomStorageDatabase::Metadata> result = database->ReadAllMetadata();
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().IsCorruption());
  }
}

// Verifies that `ReadAllMetadata()` returns a corruption error when the
// database contains a `total_size` value that cannot be represented as a
// `base::ByteSize` (e.g., a negative value).
TEST_F(LocalStorageSqliteTest, ReadAllMetadataWithInvalidTotalSize) {
  // Write valid metadata to ensure the table exists and works.
  {
    std::unique_ptr<LocalStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    DomStorageDatabase::Metadata valid_metadata;
    valid_metadata.map_metadata.push_back({
        .map_locator{kLocalStorageSessionId, kFirstStorageKey,
                     /*map_id=*/1},
        .last_accessed = base::Time::UnixEpoch() + base::Days(1),
    });
    DbStatus status = database->PutMetadata(std::move(valid_metadata));
    EXPECT_TRUE(status.ok()) << status.ToString();
  }

  // Use `sql::Database` directly to insert a row with a negative `total_size`.
  {
    base::FilePath database_path;
    ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));

    sql::Database database(sql::test::kTestTag);
    EXPECT_TRUE(database.Open(database_path));

    static constexpr char kInsertInvalidTotalSize[] =
        "INSERT INTO maps (storage_key, last_accessed, total_size) "
        "VALUES (?, ?, ?)";

    sql::Statement statement(
        database.GetUniqueStatement(kInsertInvalidTotalSize));
    statement.BindBlob(0, kSecondStorageKey.Serialize());
    statement.BindTime(1, /*last_accessed=*/base::Time::Now());
    statement.BindInt64(2, /*total_size=*/-100);

    EXPECT_TRUE(statement.Run());
  }

  // Re-open the database and verify that `ReadAllMetadata()` returns a
  // corruption error due to the invalid total_size.
  {
    std::unique_ptr<LocalStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    StatusOr<DomStorageDatabase::Metadata> result = database->ReadAllMetadata();
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().IsCorruption());
  }
}

}  // namespace storage
