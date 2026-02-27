// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/local_storage_leveldb.h"

#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/byte_size.h"
#include "base/containers/span.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/test_support/test_leveldb_utils.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

// Declare internal functions for unit testing that `local_storage_leveldb.cc`
// defines.
std::vector<uint8_t> ToBytes(std::string source);

std::optional<blink::StorageKey> TryExtractStorageKeyFromPrefixedKey(
    const DomStorageDatabase::Key& key,
    const DomStorageDatabase::KeyView& meta_data_prefix);

std::optional<DomStorageDatabase::MapMetadata> TryParseWriteMetadata(
    const DomStorageDatabase::KeyValuePair& leveldb_meta_entry);

std::optional<DomStorageDatabase::MapMetadata> TryParseAccessMetadata(
    const DomStorageDatabase::KeyValuePair& leveldb_meta_access_entry);

DomStorageDatabase::Key CreateAccessMetaDataKey(
    const blink::StorageKey& storage_key);

DomStorageDatabase::Key CreateWriteMetaDataKey(
    const blink::StorageKey& storage_key);

DomStorageDatabase::Value CreateAccessMetaDataValue(base::Time last_accessed);

DomStorageDatabase::Value CreateWriteMetaDataValue(base::Time last_modified,
                                                   base::ByteSize total_size);

DomStorageDatabase::Key GetMapPrefix(const blink::StorageKey& storage_key);

namespace {

// Define test constants used to populate the database.
constexpr const char kFakeUrlString[] = "https://a-fake.test";
constexpr const char kSecondFakeUrlString[] = "https://b-fake.test";
constexpr const char kThirdFakeUrlString[] = "https://c-fake.test";
constexpr const char kFourthFakeUrlString[] = "https://d-fake.test";

constexpr base::ByteSize kMapTotalSize{312};
constexpr base::ByteSize kSecondTotalSize{102454};
constexpr base::ByteSize kThirdTotalSize{50121524};

constexpr const uint8_t kExpectedVersion[] = {'1'};

void VerifyDatabaseVersionEntry(
    const DomStorageDatabase::KeyValuePair& version_entry) {
  EXPECT_EQ(version_entry.key, ToBytes(kLocalStorageLevelDBVersionKey));
  EXPECT_EQ(version_entry.value, ToBytes(kExpectedVersion));
}

// Return "_<storage key>\x00<script key>".
DomStorageDatabase::Key CreateMapEntryKey(const blink::StorageKey& storage_key,
                                          std::string script_key) {
  DomStorageDatabase::Key map_data_key = GetMapPrefix(storage_key);

  map_data_key.insert(map_data_key.end(), script_key.begin(), script_key.end());
  return map_data_key;
}

}  // namespace

class LocalStorageLevelDBTest : public testing::Test {
 protected:
  LocalStorageLevelDBTest();
  ~LocalStorageLevelDBTest() override = default;

  void OpenInMemory(std::unique_ptr<LocalStorageLevelDB>* result);

  // Uses `DomStorageDatabase::UpdateMaps()` to write `metadata_to_update` to
  // the database.  Afterwards, verifies with `ReadAllMetadata()`.
  void UpdateMapWithMetadata(
      LocalStorageLevelDB& database,
      const DomStorageDatabase::MapMetadata& metadata_to_update);

  base::test::TaskEnvironment task_environment_;

  const blink::StorageKey kFakeUrlStorageKey;
  const blink::StorageKey kSecondStorageKey;
  const blink::StorageKey kThirdStorageKey;
  const blink::StorageKey kFourthStorageKey;

  const base::Time kMapLastAccessed;
  const base::Time kSecondLastAccessed;
  const base::Time kThirdLastAccessed;
  const base::Time kFourthLastAccessed;

  const base::Time kMapLastModified;
  const base::Time kSecondLastModified;
  const base::Time kThirdLastModified;
};

LocalStorageLevelDBTest::LocalStorageLevelDBTest()
    : kFakeUrlStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kFakeUrlString)),
      kSecondStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kSecondFakeUrlString)),
      kThirdStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kThirdFakeUrlString)),
      kFourthStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kFourthFakeUrlString)),
      kMapLastAccessed(base::Time::Now() - base::Minutes(10)),
      kSecondLastAccessed(base::Time::Now() - base::Hours(10)),
      kThirdLastAccessed(base::Time::Now() - base::Hours(7)),
      kFourthLastAccessed(base::Time::Now() - base::Days(10)),
      kMapLastModified(base::Time::Now()),
      kSecondLastModified(base::Time::Now() - base::Hours(1)),
      kThirdLastModified(base::Time::Now() - base::Minutes(23)) {}

void LocalStorageLevelDBTest::OpenInMemory(
    std::unique_ptr<LocalStorageLevelDB>* result) {
  auto instance = std::make_unique<LocalStorageLevelDB>(
      DomStorageDatabaseFactory::CreatePassKeyForTesting());

  DbStatus status =
      instance->Open(DomStorageDatabaseFactory::CreatePassKeyForTesting(),
                     /*directory=*/base::FilePath(),
                     /*memory_dump_id=*/std::nullopt);

  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(instance);
}

void LocalStorageLevelDBTest::UpdateMapWithMetadata(
    LocalStorageLevelDB& database,
    const DomStorageDatabase::MapMetadata& metadata_to_update) {
  // Write the map usage metadata to the database.
  std::vector<DomStorageDatabase::MapBatchUpdate> map_update;
  map_update.emplace_back(metadata_to_update.map_locator.Clone());
  map_update.back().map_usage = DomStorageDatabase::MapBatchUpdate::Usage();

  if (metadata_to_update.last_accessed) {
    map_update.back().map_usage->SetLastAccessed(
        *metadata_to_update.last_accessed);
  }

  if (metadata_to_update.last_modified) {
    map_update.back().map_usage->SetLastModifiedAndTotalSize(
        *metadata_to_update.last_modified, *metadata_to_update.total_size);
  }

  DbStatus status = database.UpdateMaps(std::move(map_update));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Read back the map usage metadata from the database.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata all_metadata,
                       database.ReadAllMetadata());
  EXPECT_EQ(all_metadata.next_map_id, std::nullopt);
  ExpectEqualsMapMetadataSpan(all_metadata.map_metadata,
                              base::span_from_ref(metadata_to_update));
}

TEST_F(LocalStorageLevelDBTest, CreateAccessMetaDataKey) {
  DomStorageDatabase::Key access_metadata_key =
      CreateAccessMetaDataKey(kFakeUrlStorageKey);
  EXPECT_EQ(access_metadata_key,
            base::as_byte_span(std::string("METAACCESS:https://a-fake.test")));
}

TEST_F(LocalStorageLevelDBTest, CreateWriteMetaDataKey) {
  DomStorageDatabase::Key write_metadata_key =
      CreateWriteMetaDataKey(kFakeUrlStorageKey);
  EXPECT_EQ(write_metadata_key,
            base::as_byte_span(std::string("META:https://a-fake.test")));
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithEmptyPrefix) {
  DomStorageDatabase::Key write_metadata_key =
      CreateWriteMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(write_metadata_key,
                                          /*meta_data_prefix=*/{});
  EXPECT_EQ(storage_key, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithEmptyKey) {
  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(/*key=*/{}, kWriteMetaPrefix);
  EXPECT_EQ(storage_key, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithTooLongPrefix) {
  DomStorageDatabase::Key write_metadata_key =
      CreateWriteMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(
          write_metadata_key,
          base::as_byte_span(std::string("META:https://fake.test")));
  EXPECT_EQ(storage_key, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithInvalidStorageKey) {
  const DomStorageDatabase::Key kInvalidKey =
      ToBytes(std::string("META:invalid:\\\\fake.test"));

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(kInvalidKey, kWriteMetaPrefix);
  EXPECT_EQ(storage_key, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithWriteMetaData) {
  DomStorageDatabase::Key write_metadata_key =
      CreateWriteMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(write_metadata_key, kWriteMetaPrefix);
  EXPECT_EQ(storage_key, kFakeUrlStorageKey);
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithAccessMetaData) {
  DomStorageDatabase::Key access_metadata_key =
      CreateAccessMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(access_metadata_key,
                                          kAccessMetaPrefix);
  EXPECT_EQ(storage_key, kFakeUrlStorageKey);
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithWriteMetadataMismatch) {
  DomStorageDatabase::Key access_metadata_key =
      CreateAccessMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(access_metadata_key,
                                          kWriteMetaPrefix);
  EXPECT_EQ(storage_key, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest,
       TryExtractStorageKeyWithAccessMetadataMismatch) {
  DomStorageDatabase::Key write_metadata_key =
      CreateWriteMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(write_metadata_key,
                                          kAccessMetaPrefix);
  EXPECT_EQ(storage_key, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryParseAccessMetadataWithInvalidKey) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseAccessMetadata({
          CreateWriteMetaDataKey(kFakeUrlStorageKey),
          CreateAccessMetaDataValue(kMapLastAccessed),
      });
  EXPECT_EQ(map_metadata, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryParseAccessMetadataWithInvalidValue) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseAccessMetadata({
          CreateAccessMetaDataKey(kFakeUrlStorageKey),
          /*value=*/{0x1, 0x2, 0x3},
      });
  EXPECT_EQ(map_metadata, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryParseAccessMetadata) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseAccessMetadata({
          CreateAccessMetaDataKey(kFakeUrlStorageKey),
          CreateAccessMetaDataValue(kMapLastAccessed),
      });

  ASSERT_NE(map_metadata, std::nullopt);

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata{
      .map_locator{kFakeUrlStorageKey},
      .last_accessed{kMapLastAccessed},
  };
  ExpectEqualsMapMetadata(*map_metadata, kExpectedMapMetadata);
}

TEST_F(LocalStorageLevelDBTest, TryParseWriteMetadataWithInvalidKey) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseWriteMetadata({
          CreateAccessMetaDataKey(kFakeUrlStorageKey),
          CreateWriteMetaDataValue(kMapLastModified, kMapTotalSize),
      });
  EXPECT_EQ(map_metadata, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryParseWriteMetadataWithInvalidValue) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseWriteMetadata({
          CreateWriteMetaDataKey(kFakeUrlStorageKey),
          CreateAccessMetaDataValue(kMapLastAccessed),
      });
  EXPECT_EQ(map_metadata, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryWriteAccessMetadata) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseWriteMetadata({
          CreateWriteMetaDataKey(kFakeUrlStorageKey),
          CreateWriteMetaDataValue(kMapLastModified, kMapTotalSize),
      });

  ASSERT_NE(map_metadata, std::nullopt);

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata{
      .map_locator{kFakeUrlStorageKey},
      .last_modified{kMapLastModified},
      .total_size{kMapTotalSize},
  };
  ExpectEqualsMapMetadata(*map_metadata, kExpectedMapMetadata);
}

TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithEmpty) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata all_metadata,
                       local_storage_leveldb->ReadAllMetadata());
  EXPECT_EQ(all_metadata.map_metadata.size(), 0u);
  EXPECT_EQ(all_metadata.next_map_id, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithInvalid) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*local_storage_leveldb, {
                                               {
                                                   ToBytes("META:fake_key"),
                                                   ToBytes("fake_value"),
                                               },
                                           }));

  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata all_metadata,
                       local_storage_leveldb->ReadAllMetadata());
  EXPECT_EQ(all_metadata.map_metadata.size(), 0u);
  EXPECT_EQ(all_metadata.next_map_id, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithAccessMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*local_storage_leveldb,
                   {
                       {
                           CreateAccessMetaDataKey(kFakeUrlStorageKey),
                           CreateAccessMetaDataValue(kMapLastAccessed),
                       },
                   }));

  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata all_metadata,
                       local_storage_leveldb->ReadAllMetadata());

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kFakeUrlStorageKey},
          .last_accessed{kMapLastAccessed},
      },
  };
  ExpectEqualsMapMetadataSpan(all_metadata.map_metadata, kExpectedMapMetadata);
  EXPECT_EQ(all_metadata.next_map_id, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithWriteMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *local_storage_leveldb,
      {
          {
              CreateWriteMetaDataKey(kFakeUrlStorageKey),
              CreateWriteMetaDataValue(kMapLastModified, kMapTotalSize),
          },
      }));

  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata all_metadata,
                       local_storage_leveldb->ReadAllMetadata());

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[]{
      {
          .map_locator{kFakeUrlStorageKey},
          .last_modified{kMapLastModified},
          .total_size{kMapTotalSize},
      },
  };
  ExpectEqualsMapMetadataSpan(all_metadata.map_metadata, kExpectedMapMetadata);
  EXPECT_EQ(all_metadata.next_map_id, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithWriteAndAccessMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *local_storage_leveldb,
      {
          {
              CreateWriteMetaDataKey(kFakeUrlStorageKey),
              CreateWriteMetaDataValue(kMapLastModified, kMapTotalSize),
          },
          {
              CreateAccessMetaDataKey(kFakeUrlStorageKey),
              CreateAccessMetaDataValue(kMapLastAccessed),
          },
      }));

  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata all_metadata,
                       local_storage_leveldb->ReadAllMetadata());

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kFakeUrlStorageKey},
          .last_accessed{kMapLastAccessed},
          .last_modified{kMapLastModified},
          .total_size{kMapTotalSize},
      },
  };
  ExpectEqualsMapMetadataSpan(all_metadata.map_metadata, kExpectedMapMetadata);
  EXPECT_EQ(all_metadata.next_map_id, std::nullopt);
}

// Combine all previous tests into a single test using four storage
// keys for four different maps.
TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithMultipleStorageKeys) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *local_storage_leveldb,
      {
          {
              CreateWriteMetaDataKey(kThirdStorageKey),
              CreateWriteMetaDataValue(kThirdLastModified, kThirdTotalSize),
          },
          {
              CreateWriteMetaDataKey(kSecondStorageKey),
              CreateWriteMetaDataValue(kSecondLastModified, kSecondTotalSize),
          },
          {
              CreateAccessMetaDataKey(kFakeUrlStorageKey),
              CreateAccessMetaDataValue(kMapLastAccessed),
          },
          // Invalid entry.
          {
              ToBytes("METAACCESS:fake_key"),
              ToBytes("fake_value"),
          },
          {
              CreateAccessMetaDataKey(kFourthStorageKey),
              CreateAccessMetaDataValue(kFourthLastAccessed),
          },
          {
              CreateAccessMetaDataKey(kSecondStorageKey),
              CreateAccessMetaDataValue(kSecondLastAccessed),
          },
          {
              CreateWriteMetaDataKey(kFakeUrlStorageKey),
              CreateWriteMetaDataValue(kMapLastModified, kMapTotalSize),
          },
      }));

  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata all_metadata,
                       local_storage_leveldb->ReadAllMetadata());

  const DomStorageDatabase::MapMetadata kExpectedAllMapMetadata[] = {
      {
          .map_locator{kFakeUrlStorageKey},
          .last_accessed{kMapLastAccessed},
          .last_modified{kMapLastModified},
          .total_size{kMapTotalSize},
      },
      {
          .map_locator{kSecondStorageKey},
          .last_accessed{kSecondLastAccessed},
          .last_modified{kSecondLastModified},
          .total_size{kSecondTotalSize},
      },
      {
          .map_locator{kThirdStorageKey},
          .last_modified{kThirdLastModified},
          .total_size{kThirdTotalSize},
      },
      {
          .map_locator{kFourthStorageKey},
          .last_accessed{kFourthLastAccessed},
      },
  };
  ExpectEqualsMapMetadataSpan(all_metadata.map_metadata,
                              kExpectedAllMapMetadata);
  EXPECT_EQ(all_metadata.next_map_id, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, PutMetadataWithEmpty) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  // Write the metadata.
  DbStatus status = local_storage_leveldb->PutMetadata({});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which includes the "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 1u);

  VerifyDatabaseVersionEntry(all_entries[0]);
}

TEST_F(LocalStorageLevelDBTest, PutMetadataWithWriteMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  // Write the metadata.
  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata.push_back({
      .map_locator{kFakeUrlStorageKey},
      .last_modified{kMapLastModified},
      .total_size{kMapTotalSize},
  });

  DbStatus status = local_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which includes the "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 2u);

  // Verify "META:" entry.
  EXPECT_EQ(all_entries[0].key, CreateWriteMetaDataKey(kFakeUrlStorageKey));
  EXPECT_EQ(all_entries[0].value,
            CreateWriteMetaDataValue(kMapLastModified, kMapTotalSize));

  VerifyDatabaseVersionEntry(all_entries[1]);
}

TEST_F(LocalStorageLevelDBTest, PutMetadataWithAccessMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata.push_back({
      .map_locator{kFakeUrlStorageKey},
      .last_accessed{kMapLastAccessed},
  });

  // Write the metadata.
  DbStatus status = local_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which includes the "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 2u);

  // Verify "METAACCESS:" entry.
  EXPECT_EQ(all_entries[0].key, CreateAccessMetaDataKey(kFakeUrlStorageKey));
  EXPECT_EQ(all_entries[0].value, CreateAccessMetaDataValue(kMapLastAccessed));

  VerifyDatabaseVersionEntry(all_entries[1]);
}

TEST_F(LocalStorageLevelDBTest, PutMetadataWithAccessAndWriteMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata.push_back({
      .map_locator{kFakeUrlStorageKey},
      .last_accessed{kMapLastAccessed},
      .last_modified{kMapLastModified},
      .total_size{kMapTotalSize},
  });

  // Write the metadata.
  DbStatus status = local_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which includes the "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 3u);

  // Verify "META:" entry.
  EXPECT_EQ(all_entries[0].key, CreateWriteMetaDataKey(kFakeUrlStorageKey));
  EXPECT_EQ(all_entries[0].value,
            CreateWriteMetaDataValue(kMapLastModified, kMapTotalSize));

  // Verify "METAACCESS:" entry.
  EXPECT_EQ(all_entries[1].key, CreateAccessMetaDataKey(kFakeUrlStorageKey));
  EXPECT_EQ(all_entries[1].value, CreateAccessMetaDataValue(kMapLastAccessed));

  VerifyDatabaseVersionEntry(all_entries[2]);
}

TEST_F(LocalStorageLevelDBTest, PutMetadataWithMultipleMaps) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata.push_back({
      .map_locator{kFakeUrlStorageKey},
      .last_accessed{kMapLastAccessed},
      .last_modified{kMapLastModified},
      .total_size{kMapTotalSize},
  });
  metadata.map_metadata.push_back({
      .map_locator{kSecondStorageKey},
      .last_accessed{kSecondLastAccessed},
  });
  metadata.map_metadata.push_back({
      .map_locator{kThirdStorageKey},
      .last_modified{kThirdLastModified},
      .total_size{kThirdTotalSize},
  });

  // Write the metadata.
  DbStatus status = local_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which includes the "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 5u);

  // Verify "META:" entry for the first storage key.
  EXPECT_EQ(all_entries[0].key, CreateWriteMetaDataKey(kFakeUrlStorageKey));
  EXPECT_EQ(all_entries[0].value,
            CreateWriteMetaDataValue(kMapLastModified, kMapTotalSize));

  // Verify "META:" entry for the third storage key.
  EXPECT_EQ(all_entries[1].key, CreateWriteMetaDataKey(kThirdStorageKey));
  EXPECT_EQ(all_entries[1].value,
            CreateWriteMetaDataValue(kThirdLastModified, kThirdTotalSize));

  // Verify "METAACCESS:" entry for the first storage key.
  EXPECT_EQ(all_entries[2].key, CreateAccessMetaDataKey(kFakeUrlStorageKey));
  EXPECT_EQ(all_entries[2].value, CreateAccessMetaDataValue(kMapLastAccessed));

  // Verify "METAACCESS:" entry for the second storage key.
  EXPECT_EQ(all_entries[3].key, CreateAccessMetaDataKey(kSecondStorageKey));
  EXPECT_EQ(all_entries[3].value,
            CreateAccessMetaDataValue(kSecondLastAccessed));

  VerifyDatabaseVersionEntry(all_entries[4]);
}

TEST_F(LocalStorageLevelDBTest, GetMapPrefix) {
  std::string expected_prefix("_https://a-fake.test");
  expected_prefix.push_back(kLocalStorageKeyMapSeparator);

  EXPECT_EQ(GetMapPrefix(kFakeUrlStorageKey), ToBytes(expected_prefix));
}

TEST_F(LocalStorageLevelDBTest,
       DeleteStorageKeysFromSessionWithAccessMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*local_storage_leveldb,
                   {
                       {
                           CreateAccessMetaDataKey(kFakeUrlStorageKey),
                           CreateAccessMetaDataValue(kMapLastAccessed),
                       },
                   }));

  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kFakeUrlStorageKey);

  DbStatus status = local_storage_leveldb->DeleteStorageKeysFromSession(
      /*session_id=*/std::string(), /*metadata_to_delete=*/{kFakeUrlStorageKey},
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should only include the
  // "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 1u);

  VerifyDatabaseVersionEntry(all_entries[0]);
}

TEST_F(LocalStorageLevelDBTest, DeleteStorageKeysFromSessionWithWriteMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *local_storage_leveldb,
      {
          {
              CreateWriteMetaDataKey(kFakeUrlStorageKey),
              CreateWriteMetaDataValue(kMapLastModified, kMapTotalSize),
          },
      }));

  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kFakeUrlStorageKey);

  DbStatus status = local_storage_leveldb->DeleteStorageKeysFromSession(
      /*session_id=*/std::string(), /*metadata_to_delete=*/{kFakeUrlStorageKey},
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should only include the
  // "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 1u);

  VerifyDatabaseVersionEntry(all_entries[0]);
}

TEST_F(LocalStorageLevelDBTest, DeleteStorageKeysFromSessionWithMapKeyValues) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*local_storage_leveldb,
                   {
                       {
                           CreateMapEntryKey(kFakeUrlStorageKey, "key_1"),
                           ToBytes("value_1"),
                       },
                       {
                           CreateMapEntryKey(kFakeUrlStorageKey, "key_2"),
                           ToBytes("value_2"),
                       },
                   }));

  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kFakeUrlStorageKey);

  DbStatus status = local_storage_leveldb->DeleteStorageKeysFromSession(
      /*session_id=*/std::string(), /*metadata_to_delete=*/{kFakeUrlStorageKey},
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should only include the
  // "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 1u);

  VerifyDatabaseVersionEntry(all_entries[0]);
}

TEST_F(LocalStorageLevelDBTest,
       DeleteStorageKeysFromSessionWithMultipleStorageKeys) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  // Add three different storage keys to the database.
  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *local_storage_leveldb,
      {
          // Add map key value pairs.
          {
              CreateMapEntryKey(kFakeUrlStorageKey, "key_1"),
              ToBytes("value_1"),
          },
          {
              CreateMapEntryKey(kFakeUrlStorageKey, "key_2"),
              ToBytes("value_2"),
          },
          {
              CreateMapEntryKey(kSecondStorageKey, "key_3"),
              ToBytes("value_3"),
          },
          {
              CreateMapEntryKey(kThirdStorageKey, "key_1"),
              ToBytes("value_4"),
          },
          {
              CreateMapEntryKey(kThirdStorageKey, "key_2"),
              ToBytes("value_5"),
          },
          {
              CreateMapEntryKey(kThirdStorageKey, "key_3"),
              ToBytes("value_5"),
          },
          // Add "METAACCESS:" entries.
          {
              CreateAccessMetaDataKey(kFakeUrlStorageKey),
              CreateAccessMetaDataValue(kMapLastAccessed),
          },
          {
              CreateAccessMetaDataKey(kSecondStorageKey),
              CreateAccessMetaDataValue(kSecondLastAccessed),
          },
          {
              CreateAccessMetaDataKey(kThirdStorageKey),
              CreateAccessMetaDataValue(kThirdLastAccessed),
          },
          // Add "META:" entries.
          {
              CreateWriteMetaDataKey(kFakeUrlStorageKey),
              CreateWriteMetaDataValue(kMapLastModified, kMapTotalSize),
          },
          {
              CreateWriteMetaDataKey(kSecondStorageKey),
              CreateWriteMetaDataValue(kSecondLastModified, kSecondTotalSize),
          },
          {
              CreateWriteMetaDataKey(kThirdStorageKey),
              CreateWriteMetaDataValue(kThirdLastModified, kThirdTotalSize),
          },
      }));

  // Erase the first and third storage keys.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kFakeUrlStorageKey);
  maps_to_delete.emplace_back(kThirdStorageKey);

  DbStatus status = local_storage_leveldb->DeleteStorageKeysFromSession(
      /*session_id=*/std::string(),
      /*metadata_to_delete=*/{kFakeUrlStorageKey, kThirdStorageKey},
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should include the second
  // storage key entries and the "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 4u);

  // Verify "META:" entry for the second storage key.
  EXPECT_EQ(all_entries[0].key, CreateWriteMetaDataKey(kSecondStorageKey));
  EXPECT_EQ(all_entries[0].value,
            CreateWriteMetaDataValue(kSecondLastModified, kSecondTotalSize));

  // Verify "METAACCESS:" entry for the second storage key.
  EXPECT_EQ(all_entries[1].key, CreateAccessMetaDataKey(kSecondStorageKey));
  EXPECT_EQ(all_entries[1].value,
            CreateAccessMetaDataValue(kSecondLastAccessed));

  VerifyDatabaseVersionEntry(all_entries[2]);

  // Verify the map key/value paris for the second storage key.
  EXPECT_EQ(all_entries[3].key, CreateMapEntryKey(kSecondStorageKey, "key_3"));
  EXPECT_EQ(all_entries[3].value, ToBytes("value_3"));

  // Erase all the storage keys.
  maps_to_delete.clear();
  maps_to_delete.emplace_back(kFakeUrlStorageKey);
  maps_to_delete.emplace_back(kSecondStorageKey);
  maps_to_delete.emplace_back(kThirdStorageKey);
  maps_to_delete.emplace_back(kFourthStorageKey);

  status = local_storage_leveldb->DeleteStorageKeysFromSession(
      /*session_id=*/std::string(),
      /*metadata_to_delete=*/
      {
          kFakeUrlStorageKey,
          kSecondStorageKey,
          kThirdStorageKey,
          kFourthStorageKey,
      },
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should only include the
  // "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      all_entries,
      local_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 1u);

  VerifyDatabaseVersionEntry(all_entries[0]);
}

TEST_F(LocalStorageLevelDBTest, ReadMapKeyValuesWithEmpty) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  // An empty database must have no key/value pairs.
  DomStorageDatabase::MapLocator map_locator{kFakeUrlStorageKey};
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> entries),
      local_storage_leveldb->ReadMapKeyValues(std::move(map_locator)));
  EXPECT_EQ(entries.size(), 0u);
}

TEST_F(LocalStorageLevelDBTest, ReadMapKeyValues) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  // Add two key/value pairs to a single map.
  constexpr const char kScriptKey1[] = "key_1";
  constexpr const char kScriptKey2[] = "key_2";
  const DomStorageDatabase::Value kValue1 = ToBytes("value_1");
  const DomStorageDatabase::Value kValue2 = ToBytes("value_2");

  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*local_storage_leveldb,
                   {
                       {
                           CreateMapEntryKey(kFakeUrlStorageKey, kScriptKey1),
                           kValue1,
                       },
                       {
                           CreateMapEntryKey(kFakeUrlStorageKey, kScriptKey2),
                           kValue2,
                       },
                   }));

  // Read the two key/value pairs from the database.
  DomStorageDatabase::MapLocator map_locator{kFakeUrlStorageKey};
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> entries),
      local_storage_leveldb->ReadMapKeyValues(std::move(map_locator)));

  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[ToBytes(kScriptKey1)], kValue1);
  EXPECT_EQ(entries[ToBytes(kScriptKey2)], kValue2);
}

TEST_F(LocalStorageLevelDBTest, ReadMapKeyValuesWithMultipleMaps) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  // Create two maps, adding a key/value pair to each map.
  constexpr const char kScriptKey1[] = "key_1";
  constexpr const char kScriptKey2[] = "key_2";
  const DomStorageDatabase::Value kValue1 = ToBytes("value_1");
  const DomStorageDatabase::Value kValue2 = ToBytes("value_2");

  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*local_storage_leveldb,
                   {
                       {
                           CreateMapEntryKey(kFakeUrlStorageKey, kScriptKey1),
                           kValue1,
                       },
                       {
                           CreateMapEntryKey(kSecondStorageKey, kScriptKey2),
                           kValue2,
                       },
                   }));

  // Read the first map's key/value pair.
  DomStorageDatabase::MapLocator map_locator{kFakeUrlStorageKey};
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> entries),
      local_storage_leveldb->ReadMapKeyValues(std::move(map_locator)));

  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[ToBytes(kScriptKey1)], kValue1);

  // Read the second map's key/value pair.
  DomStorageDatabase::MapLocator other_map_locator{kSecondStorageKey};
  ASSERT_OK_AND_ASSIGN(entries, local_storage_leveldb->ReadMapKeyValues(
                                    std::move(other_map_locator)));

  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[ToBytes(kScriptKey2)], kValue2);
}

TEST_F(LocalStorageLevelDBTest, UpdateMaps) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  DomStorageDatabase::MapLocator map1_locator{kFakeUrlStorageKey};

  DomStorageDatabase::MapLocator map2_locator{kSecondStorageKey};

  ASSERT_NO_FATAL_FAILURE(
      TestUpdateMaps(*local_storage_leveldb, map1_locator, map2_locator));
}

TEST_F(LocalStorageLevelDBTest, UpdateMapsWithAccessMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata{
      .map_locator{kFakeUrlStorageKey},
      .last_accessed{kMapLastAccessed},
  };
  ASSERT_NO_FATAL_FAILURE(
      UpdateMapWithMetadata(*local_storage_leveldb, kExpectedMapMetadata));
}

TEST_F(LocalStorageLevelDBTest, UpdateMapsWithWriteMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata{
      .map_locator{kFakeUrlStorageKey},
      .last_modified{kMapLastModified},
      .total_size{kMapTotalSize},
  };
  ASSERT_NO_FATAL_FAILURE(
      UpdateMapWithMetadata(*local_storage_leveldb, kExpectedMapMetadata));
}

TEST_F(LocalStorageLevelDBTest, UpdateMapsClearsMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata{
      .map_locator{kFakeUrlStorageKey},
      .last_accessed{kMapLastAccessed},
      .last_modified{kMapLastModified},
      .total_size{kMapTotalSize},
  };
  ASSERT_NO_FATAL_FAILURE(
      UpdateMapWithMetadata(*local_storage_leveldb, kExpectedMapMetadata));

  // Use `UpdateMaps()` to delete the map metadata from the database.
  std::vector<DomStorageDatabase::MapBatchUpdate> delete_metadata_update;
  delete_metadata_update.emplace_back(kExpectedMapMetadata.map_locator.Clone());

  delete_metadata_update.back().map_usage =
      DomStorageDatabase::MapBatchUpdate::Usage();
  delete_metadata_update.back().map_usage->DeleteAllUsage();

  DbStatus status =
      local_storage_leveldb->UpdateMaps(std::move(delete_metadata_update));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify no metadata exists.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata all_metadata,
                       local_storage_leveldb->ReadAllMetadata());

  EXPECT_EQ(all_metadata.next_map_id, std::nullopt);
  EXPECT_EQ(all_metadata.map_metadata.size(), 0u);
}

}  // namespace storage
