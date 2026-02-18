// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite/map_entries_table.h"

#include <map>
#include <memory>
#include <string_view>
#include <vector>

#include "base/test/gmock_expected_support.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/public/cpp/compression.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {
namespace {

std::vector<uint8_t> ToBytes(std::string_view str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

}  // namespace

class MapEntriesTableTest : public testing::Test {
 protected:
  MapEntriesTableTest();

  void SetUp() override;

  // Applies a batch update to the map entries table within a transaction.
  // The update may include adding, replacing, or deleting key/value pairs.
  void UpdateMap(DomStorageDatabase::MapBatchUpdate map_update);

  // Helper to insert key/value pairs into a map using `UpdateMap()`.
  void InsertMapEntries(const DomStorageDatabase::MapLocator& map_locator,
                        const std::map<DomStorageDatabase::Key,
                                       DomStorageDatabase::Value>& entries);

  // Deletes all entries for the specified map within a transaction.
  void DeleteMap(int64_t map_id);

  // Clones all entries from source map to target map within a transaction.
  void CloneMap(int64_t source_map_id, int64_t target_map_id);

  DomStorageDatabase::MapLocator kFirstMapLocator{
      /*session_id=*/"36356e0b_1627_4492_a474_db76a8996bed",
      blink::StorageKey::CreateFromStringForTesting("https://a-fake.test"),
      /*map_id=*/1};

  DomStorageDatabase::MapLocator kSecondMapLocator{
      /*session_id=*/"5fe0e896_c6d8_4d2b_8b3c_d26f47832125",
      blink::StorageKey::CreateFromStringForTesting("https://b-fake.test"),
      /*map_id=*/2};

  sql::Database database_;
  std::unique_ptr<MapEntriesTable> map_entries_table_;
};

MapEntriesTableTest::MapEntriesTableTest()
    : database_(sql::DatabaseOptions()
                    .set_wal_mode(true)
                    .set_mmap_enabled(false),
                sql::test::kTestTag) {}

void MapEntriesTableTest::SetUp() {
  ASSERT_TRUE(database_.OpenInMemory());

  // Create the `map_entries` table schema.
  DbStatus status = MapEntriesTable::CreateSchema(database_);
  ASSERT_TRUE(status.ok()) << status.ToString();

  map_entries_table_ = std::make_unique<MapEntriesTable>(database_);
}

void MapEntriesTableTest::UpdateMap(
    DomStorageDatabase::MapBatchUpdate map_update) {
  sql::Transaction transaction(&database_);
  EXPECT_TRUE(transaction.Begin());

  DbStatus status = map_entries_table_->UpdateMap(std::move(map_update));
  EXPECT_TRUE(status.ok()) << status.ToString();

  EXPECT_TRUE(transaction.Commit());
}

void MapEntriesTableTest::DeleteMap(int64_t map_id) {
  sql::Transaction transaction(&database_);
  EXPECT_TRUE(transaction.Begin());

  DbStatus status = map_entries_table_->DeleteMap(map_id);
  EXPECT_TRUE(status.ok()) << status.ToString();

  EXPECT_TRUE(transaction.Commit());
}

void MapEntriesTableTest::CloneMap(int64_t source_map_id,
                                   int64_t target_map_id) {
  sql::Transaction transaction(&database_);
  EXPECT_TRUE(transaction.Begin());

  DbStatus status = map_entries_table_->CloneMap(source_map_id, target_map_id);
  EXPECT_TRUE(status.ok()) << status.ToString();

  EXPECT_TRUE(transaction.Commit());
}

void MapEntriesTableTest::InsertMapEntries(
    const DomStorageDatabase::MapLocator& map_locator,
    const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>&
        entries) {
  DomStorageDatabase::MapBatchUpdate map_update(map_locator.Clone());
  for (const auto& entry : entries) {
    map_update.entries_to_add.emplace_back(entry.first, entry.second);
  }
  UpdateMap(std::move(map_update));
}

// Verifies that reading key/value pairs from an empty map returns an empty
// result without errors.
TEST_F(MapEntriesTableTest, GetMapKeyValuesWithEmpty) {
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> entries),
      map_entries_table_->GetMapKeyValues(kFirstMapLocator.map_id().value()));

  EXPECT_TRUE(entries.empty());
}

// Verifies the complete lifecycle of key/value pairs: inserting initial
// entries, reading them back, replacing an existing value, adding a new key,
// and confirming all changes are persisted correctly.
TEST_F(MapEntriesTableTest, InsertReadAndReplaceKeyValues) {
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> expected_entries{
      {ToBytes("key_1"), ToBytes("value_1")},
      {ToBytes("key_2"), ToBytes("value_2")},
  };

  // Insert initial key/value pairs.
  InsertMapEntries(kFirstMapLocator, expected_entries);

  // Read and verify the inserted values.
  {
    ASSERT_OK_AND_ASSIGN(
        (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
             actual_entries),
        map_entries_table_->GetMapKeyValues(kFirstMapLocator.map_id().value()));

    EXPECT_EQ(actual_entries, expected_entries);
  }

  // Replace one value and add a new key.
  {
    DomStorageDatabase::MapBatchUpdate update(kFirstMapLocator.Clone());
    // Replace key1's value.
    update.entries_to_add.emplace_back(ToBytes("key1"), ToBytes("new_value"));
    expected_entries[ToBytes("key1")] = ToBytes("new_value");

    // Add a new key3.
    update.entries_to_add.emplace_back(ToBytes("key3"), ToBytes("value3"));
    expected_entries[ToBytes("key3")] = ToBytes("value3");

    UpdateMap(std::move(update));
  }

  // Read and verify the updated values.
  {
    ASSERT_OK_AND_ASSIGN(
        (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
             actual_entries),
        map_entries_table_->GetMapKeyValues(kFirstMapLocator.map_id().value()));

    EXPECT_EQ(actual_entries, expected_entries);
  }
}

// Verifies that individual keys can be deleted from a map while leaving other
// keys intact.
TEST_F(MapEntriesTableTest, DeleteKeys) {
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> expected_entries{
      {ToBytes("a"), ToBytes("1")},
      {ToBytes("b"), ToBytes("2")},
      {ToBytes("c"), ToBytes("3")},
  };

  // Insert initial key/value pairs.
  InsertMapEntries(kFirstMapLocator, expected_entries);

  // Delete one key: "b".
  {
    DomStorageDatabase::MapBatchUpdate update(kFirstMapLocator.Clone());
    update.keys_to_delete.push_back(ToBytes("b"));
    expected_entries.erase(ToBytes("b"));

    UpdateMap(std::move(update));
  }

  // Verify deletion.
  {
    ASSERT_OK_AND_ASSIGN(
        (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
             actual_entries),
        map_entries_table_->GetMapKeyValues(kFirstMapLocator.map_id().value()));

    EXPECT_EQ(actual_entries, expected_entries);
  }
}

// Verifies that the `clear_all_first` flag correctly removes all existing
// entries from a map before adding new entries, effectively replacing the
// entire map contents.
TEST_F(MapEntriesTableTest, ClearAllFirst) {
  // Insert initial key/value pairs.
  {
    DomStorageDatabase::MapBatchUpdate update(kFirstMapLocator.Clone());
    update.entries_to_add.emplace_back(ToBytes("x"), ToBytes("1"));
    update.entries_to_add.emplace_back(ToBytes("y"), ToBytes("2"));

    UpdateMap(std::move(update));
  }

  // Clear all and add new entries.
  {
    DomStorageDatabase::MapBatchUpdate update(kFirstMapLocator.Clone());
    update.clear_all_first = true;
    update.entries_to_add.emplace_back(ToBytes("z"), ToBytes("3"));

    UpdateMap(std::move(update));
  }

  // Verify only the new entry exists.
  {
    ASSERT_OK_AND_ASSIGN(
        (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> entries),
        map_entries_table_->GetMapKeyValues(kFirstMapLocator.map_id().value()));

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[ToBytes("z")], ToBytes("3"));
  }
}

// Verifies that deleting a map removes all its key/value pairs while leaving
// key/value pairs in other maps unaffected.
TEST_F(MapEntriesTableTest, DeleteMap) {
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kFirstMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
          {ToBytes("key_3"), ToBytes("value_3")},
      };
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kSecondMapEntries{
          {ToBytes("key_4"), ToBytes("value_4")},
          {ToBytes("key_5"), ToBytes("value_5")},
      };

  // Insert entries into the first and second maps.
  InsertMapEntries(kFirstMapLocator, kFirstMapEntries);
  InsertMapEntries(kSecondMapLocator, kSecondMapEntries);

  // Verify the first map has 3 entries.
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
           actual_entries),
      map_entries_table_->GetMapKeyValues(kFirstMapLocator.map_id().value()));
  EXPECT_EQ(actual_entries, kFirstMapEntries);

  // Delete the first map.
  DeleteMap(kFirstMapLocator.map_id().value());

  // Verify the first map is now empty.
  ASSERT_OK_AND_ASSIGN(actual_entries, map_entries_table_->GetMapKeyValues(
                                           kFirstMapLocator.map_id().value()));
  EXPECT_EQ(actual_entries.size(), 0u);

  // Verify the second map is unaffected.
  ASSERT_OK_AND_ASSIGN(actual_entries, map_entries_table_->GetMapKeyValues(
                                           kSecondMapLocator.map_id().value()));
  EXPECT_EQ(actual_entries, kSecondMapEntries);
}

// Verifies that cloning a map copies all key/value pairs from the source map
// to the target map while leaving the source map unchanged.
TEST_F(MapEntriesTableTest, CloneMap) {
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kSourceMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
          {ToBytes("key_3"), ToBytes("value_3")},
      };

  // Insert entries into the source map.
  InsertMapEntries(kFirstMapLocator, kSourceMapEntries);

  // Verify the source map has the expected entries.
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
           source_entries),
      map_entries_table_->GetMapKeyValues(kFirstMapLocator.map_id().value()));
  EXPECT_EQ(source_entries, kSourceMapEntries);

  // Clone the source map to the target map.
  CloneMap(/*source_map_id=*/kFirstMapLocator.map_id().value(),
           /*target_map_id=*/kSecondMapLocator.map_id().value());

  // Verify the target map now contains the same entries as the source map.
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
           target_entries),
      map_entries_table_->GetMapKeyValues(kSecondMapLocator.map_id().value()));
  EXPECT_EQ(target_entries, kSourceMapEntries);

  // Verify the source map is unchanged.
  ASSERT_OK_AND_ASSIGN(source_entries, map_entries_table_->GetMapKeyValues(
                                           kFirstMapLocator.map_id().value()));
  EXPECT_EQ(source_entries, kSourceMapEntries);
}

// Verifies that cloning an empty source map results in an empty target map.
TEST_F(MapEntriesTableTest, CloneEmptyMap) {
  // Verify the source map is empty.
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
           source_entries),
      map_entries_table_->GetMapKeyValues(kFirstMapLocator.map_id().value()));
  EXPECT_TRUE(source_entries.empty());

  // Clone the empty source map to the target map.
  CloneMap(/*source_map_id=*/kFirstMapLocator.map_id().value(),
           /*target_map_id=*/kSecondMapLocator.map_id().value());

  // Verify the target map is also empty.
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
           target_entries),
      map_entries_table_->GetMapKeyValues(kSecondMapLocator.map_id().value()));
  EXPECT_TRUE(target_entries.empty());
}

// Verifies that a large value is compressed when stored and decompressed
// correctly when read back.
TEST_F(MapEntriesTableTest, LargeValueStoredCompressed) {
  std::vector<uint8_t> large_value(1024, 'A');
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> expected_entries{
      {ToBytes("key"), large_value},
  };
  InsertMapEntries(kFirstMapLocator, expected_entries);

  // Verify the value was actually compressed (compression_type != 0).
  {
    sql::Statement statement(database_.GetUniqueStatement(
        "SELECT value_compression_type FROM map_entries WHERE map_id = 1"));
    ASSERT_TRUE(statement.Step());
    EXPECT_NE(static_cast<CompressionType>(statement.ColumnInt(0)),
              CompressionType::kUncompressed);
  }

  // Verify the decompressed value matches the original.
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
           actual_entries),
      map_entries_table_->GetMapKeyValues(kFirstMapLocator.map_id().value()));
  EXPECT_EQ(actual_entries, expected_entries);
}

// Verifies that small values below the compression threshold are stored
// uncompressed.
TEST_F(MapEntriesTableTest, SmallValueStoredUncompressed) {
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> expected_entries{
      {ToBytes("key"), ToBytes("small")},
  };
  InsertMapEntries(kFirstMapLocator, expected_entries);

  // Verify `value_compression_type` is `kUncompressed`.
  {
    sql::Statement statement(database_.GetUniqueStatement(
        "SELECT value_compression_type FROM map_entries WHERE map_id = 1"));
    ASSERT_TRUE(statement.Step());
    EXPECT_EQ(static_cast<CompressionType>(statement.ColumnInt(0)),
              CompressionType::kUncompressed);
  }

  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
           actual_entries),
      map_entries_table_->GetMapKeyValues(kFirstMapLocator.map_id().value()));
  EXPECT_EQ(actual_entries, expected_entries);
}

// Verifies that a large value that doesn't compress well is stored
// uncompressed.
TEST_F(MapEntriesTableTest, LargeIncompressibleValueStoredUncompressed) {
  // Create a value that doesn't compress well (sequential byte values).
  std::vector<uint8_t> incompressible_value(256);
  for (size_t i = 0; i < incompressible_value.size(); ++i) {
    incompressible_value[i] = static_cast<uint8_t>(i);
  }
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> expected_entries{
      {ToBytes("key"), incompressible_value},
  };
  InsertMapEntries(kFirstMapLocator, expected_entries);

  // Verify `value_compression_type` is `kUncompressed`.
  {
    sql::Statement statement(database_.GetUniqueStatement(
        "SELECT value_compression_type FROM map_entries WHERE map_id = 1"));
    ASSERT_TRUE(statement.Step());
    EXPECT_EQ(static_cast<CompressionType>(statement.ColumnInt(0)),
              CompressionType::kUncompressed);
  }

  // Verify the value reads back correctly regardless of compression decision.
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
           actual_entries),
      map_entries_table_->GetMapKeyValues(kFirstMapLocator.map_id().value()));
  EXPECT_EQ(actual_entries, expected_entries);
}

// Verifies that `GetMapKeyValues()` returns a corruption error when the
// database contains a corrupt compressed value.
TEST_F(MapEntriesTableTest, CorruptCompressedDataReturnsError) {
  std::vector<uint8_t> garbage = ToBytes("this is not valid compressed value!");

  // Insert `garbage` directly with compression type `kZstd`.
  {
    sql::Statement insert(database_.GetUniqueStatement(
        "INSERT INTO map_entries(map_id, key, value, value_compression_type) "
        "VALUES(?, ?, ?, ?)"));
    insert.BindInt64(0, kFirstMapLocator.map_id().value());
    insert.BindBlob(1, ToBytes("key"));
    insert.BindBlob(2, garbage);
    insert.BindInt(3, static_cast<int>(CompressionType::kZstd));
    ASSERT_TRUE(insert.Run());
  }

  StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>
      result = map_entries_table_->GetMapKeyValues(
          kFirstMapLocator.map_id().value());
  ASSERT_FALSE(result.has_value());
  EXPECT_TRUE(result.error().IsCorruption());
}

}  // namespace storage
