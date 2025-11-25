// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/local_storage_leveldb.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/byte_size.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
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
DomStorageDatabase::Key CreateMapDataKey(const blink::StorageKey& storage_key,
                                         std::string script_key) {
  DomStorageDatabase::Key map_data_key =
      LocalStorageLevelDB::GetMapPrefix(storage_key);

  map_data_key.insert(map_data_key.end(), script_key.begin(), script_key.end());
  return map_data_key;
}

}  // namespace

class LocalStorageLevelDBTest : public testing::Test {
 protected:
  LocalStorageLevelDBTest();
  ~LocalStorageLevelDBTest() override = default;

  void OpenInMemory(std::unique_ptr<LocalStorageLevelDB>* result);

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
                     /*directory=*/base::FilePath(), "LocalStorageLevelDBTest",
                     /*memory_dump_id=*/std::nullopt);

  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(instance);
}

TEST_F(LocalStorageLevelDBTest, CreateAccessMetaDataKey) {
  DomStorageDatabase::Key access_metadata_key =
      LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey);
  EXPECT_EQ(access_metadata_key,
            base::as_byte_span(std::string("METAACCESS:https://a-fake.test")));
}

TEST_F(LocalStorageLevelDBTest, CreateWriteMetaDataKey) {
  DomStorageDatabase::Key write_metadata_key =
      LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey);
  EXPECT_EQ(write_metadata_key,
            base::as_byte_span(std::string("META:https://a-fake.test")));
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithEmptyPrefix) {
  DomStorageDatabase::Key write_metadata_key =
      LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey);

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
      LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey);

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
      LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(write_metadata_key, kWriteMetaPrefix);
  EXPECT_EQ(storage_key, kFakeUrlStorageKey);
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithAccessMetaData) {
  DomStorageDatabase::Key access_metadata_key =
      LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(access_metadata_key,
                                          kAccessMetaPrefix);
  EXPECT_EQ(storage_key, kFakeUrlStorageKey);
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithWriteMetadataMismatch) {
  DomStorageDatabase::Key access_metadata_key =
      LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(access_metadata_key,
                                          kWriteMetaPrefix);
  EXPECT_EQ(storage_key, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest,
       TryExtractStorageKeyWithAccessMetadataMismatch) {
  DomStorageDatabase::Key write_metadata_key =
      LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(write_metadata_key,
                                          kAccessMetaPrefix);
  EXPECT_EQ(storage_key, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryParseAccessMetadataWithInvalidKey) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseAccessMetadata({
          LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey),
          LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed),
      });
  EXPECT_EQ(map_metadata, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryParseAccessMetadataWithInvalidValue) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseAccessMetadata({
          LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey),
          /*value=*/{0x1, 0x2, 0x3},
      });
  EXPECT_EQ(map_metadata, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryParseAccessMetadata) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseAccessMetadata({
          LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey),
          LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed),
      });

  ASSERT_NE(map_metadata, std::nullopt);

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata{
      .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
      .last_accessed{kMapLastAccessed},
  };
  ExpectEqualsMapMetadata(*map_metadata, kExpectedMapMetadata);
}

TEST_F(LocalStorageLevelDBTest, TryParseWriteMetadataWithInvalidKey) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseWriteMetadata({
          LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey),
          LocalStorageLevelDB::CreateWriteMetaDataValue(kMapLastModified,
                                                        kMapTotalSize),
      });
  EXPECT_EQ(map_metadata, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryParseWriteMetadataWithInvalidValue) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseWriteMetadata({
          LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey),
          LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed),
      });
  EXPECT_EQ(map_metadata, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryWriteAccessMetadata) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseWriteMetadata({
          LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey),
          LocalStorageLevelDB::CreateWriteMetaDataValue(kMapLastModified,
                                                        kMapTotalSize),
      });

  ASSERT_NE(map_metadata, std::nullopt);

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata{
      .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
      .last_modified{kMapLastModified},
      .total_size{kMapTotalSize},
  };
  ExpectEqualsMapMetadata(*map_metadata, kExpectedMapMetadata);
}

TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithEmpty) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  StatusOr<DomStorageDatabase::Metadata> all_metadata =
      local_storage_leveldb->ReadAllMetadata();
  ASSERT_TRUE(all_metadata.has_value()) << all_metadata.error().ToString();

  EXPECT_EQ(all_metadata->map_metadata.size(), 0u);
  EXPECT_EQ(all_metadata->next_map_id, std::nullopt);
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

  StatusOr<DomStorageDatabase::Metadata> all_metadata =
      local_storage_leveldb->ReadAllMetadata();
  ASSERT_TRUE(all_metadata.has_value()) << all_metadata.error().ToString();

  EXPECT_EQ(all_metadata->map_metadata.size(), 0u);
  EXPECT_EQ(all_metadata->next_map_id, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithAccessMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *local_storage_leveldb,
      {
          {
              LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey),
              LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed),
          },
      }));

  StatusOr<DomStorageDatabase::Metadata> all_metadata =
      local_storage_leveldb->ReadAllMetadata();
  ASSERT_TRUE(all_metadata.has_value()) << all_metadata.error().ToString();

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
          .last_accessed{kMapLastAccessed},
      },
  };
  ExpectEqualsMapMetadataSpan(all_metadata->map_metadata, kExpectedMapMetadata);
  EXPECT_EQ(all_metadata->next_map_id, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithWriteMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *local_storage_leveldb,
      {
          {
              LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey),
              LocalStorageLevelDB::CreateWriteMetaDataValue(kMapLastModified,
                                                            kMapTotalSize),
          },
      }));

  StatusOr<DomStorageDatabase::Metadata> all_metadata =
      local_storage_leveldb->ReadAllMetadata();
  ASSERT_TRUE(all_metadata.has_value()) << all_metadata.error().ToString();

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[]{
      {
          .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
          .last_modified{kMapLastModified},
          .total_size{kMapTotalSize},
      },
  };
  ExpectEqualsMapMetadataSpan(all_metadata->map_metadata, kExpectedMapMetadata);
  EXPECT_EQ(all_metadata->next_map_id, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithWriteAndAccessMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *local_storage_leveldb,
      {
          {
              LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey),
              LocalStorageLevelDB::CreateWriteMetaDataValue(kMapLastModified,
                                                            kMapTotalSize),
          },
          {
              LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey),
              LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed),
          },
      }));

  StatusOr<DomStorageDatabase::Metadata> all_metadata =
      local_storage_leveldb->ReadAllMetadata();
  ASSERT_TRUE(all_metadata.has_value()) << all_metadata.error().ToString();

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
          .last_accessed{kMapLastAccessed},
          .last_modified{kMapLastModified},
          .total_size{kMapTotalSize},
      },
  };
  ExpectEqualsMapMetadataSpan(all_metadata->map_metadata, kExpectedMapMetadata);
  EXPECT_EQ(all_metadata->next_map_id, std::nullopt);
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
              LocalStorageLevelDB::CreateWriteMetaDataKey(kThirdStorageKey),
              LocalStorageLevelDB::CreateWriteMetaDataValue(kThirdLastModified,
                                                            kThirdTotalSize),
          },
          {
              LocalStorageLevelDB::CreateWriteMetaDataKey(kSecondStorageKey),
              LocalStorageLevelDB::CreateWriteMetaDataValue(kSecondLastModified,
                                                            kSecondTotalSize),
          },
          {
              LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey),
              LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed),
          },
          // Invalid entry.
          {
              ToBytes("METAACCESS:fake_key"),
              ToBytes("fake_value"),
          },
          {
              LocalStorageLevelDB::CreateAccessMetaDataKey(kFourthStorageKey),
              LocalStorageLevelDB::CreateAccessMetaDataValue(
                  kFourthLastAccessed),
          },
          {
              LocalStorageLevelDB::CreateAccessMetaDataKey(kSecondStorageKey),
              LocalStorageLevelDB::CreateAccessMetaDataValue(
                  kSecondLastAccessed),
          },
          {
              LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey),
              LocalStorageLevelDB::CreateWriteMetaDataValue(kMapLastModified,
                                                            kMapTotalSize),
          },
      }));

  StatusOr<DomStorageDatabase::Metadata> all_metadata =
      local_storage_leveldb->ReadAllMetadata();
  ASSERT_TRUE(all_metadata.has_value()) << all_metadata.error().ToString();

  const DomStorageDatabase::MapMetadata kExpectedAllMapMetadata[] = {
      {
          .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
          .last_accessed{kMapLastAccessed},
          .last_modified{kMapLastModified},
          .total_size{kMapTotalSize},
      },
      {
          .map_locator{kLocalStorageSessionId, kSecondStorageKey},
          .last_accessed{kSecondLastAccessed},
          .last_modified{kSecondLastModified},
          .total_size{kSecondTotalSize},
      },
      {
          .map_locator{kLocalStorageSessionId, kThirdStorageKey},
          .last_modified{kThirdLastModified},
          .total_size{kThirdTotalSize},
      },
      {
          .map_locator{kLocalStorageSessionId, kFourthStorageKey},
          .last_accessed{kFourthLastAccessed},
      },
  };
  ExpectEqualsMapMetadataSpan(all_metadata->map_metadata,
                              kExpectedAllMapMetadata);
  EXPECT_EQ(all_metadata->next_map_id, std::nullopt);
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
      local_storage_leveldb->GetLevelDB().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 1u);

  VerifyDatabaseVersionEntry(all_entries[0]);
}

TEST_F(LocalStorageLevelDBTest, PutMetadataWithNoUsage) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  // Write the metadata.
  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata.push_back({
      .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
  });
  DbStatus status = local_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which includes the "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDB().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 1u);

  VerifyDatabaseVersionEntry(all_entries[0]);
}

TEST_F(LocalStorageLevelDBTest, PutMetadataWithWriteMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  // Write the metadata.
  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata.push_back({
      .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
      .last_modified{kMapLastModified},
      .total_size{kMapTotalSize},
  });

  DbStatus status = local_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which includes the "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDB().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 2u);

  // Verify "META:" entry.
  EXPECT_EQ(all_entries[0].key,
            LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey));
  EXPECT_EQ(all_entries[0].value, LocalStorageLevelDB::CreateWriteMetaDataValue(
                                      kMapLastModified, kMapTotalSize));

  VerifyDatabaseVersionEntry(all_entries[1]);
}

TEST_F(LocalStorageLevelDBTest, PutMetadataWithAccessMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata.push_back({
      .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
      .last_accessed{kMapLastAccessed},
  });

  // Write the metadata.
  DbStatus status = local_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which includes the "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDB().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 2u);

  // Verify "METAACCESS:" entry.
  EXPECT_EQ(all_entries[0].key,
            LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey));
  EXPECT_EQ(all_entries[0].value,
            LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed));

  VerifyDatabaseVersionEntry(all_entries[1]);
}

TEST_F(LocalStorageLevelDBTest, PutMetadataWithAccessAndWriteMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata.push_back({
      .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
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
      local_storage_leveldb->GetLevelDB().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 3u);

  // Verify "META:" entry.
  EXPECT_EQ(all_entries[0].key,
            LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey));
  EXPECT_EQ(all_entries[0].value, LocalStorageLevelDB::CreateWriteMetaDataValue(
                                      kMapLastModified, kMapTotalSize));

  // Verify "METAACCESS:" entry.
  EXPECT_EQ(all_entries[1].key,
            LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey));
  EXPECT_EQ(all_entries[1].value,
            LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed));

  VerifyDatabaseVersionEntry(all_entries[2]);
}

TEST_F(LocalStorageLevelDBTest, PutMetadataWithMultipleMaps) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata.push_back({
      .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
      .last_accessed{kMapLastAccessed},
      .last_modified{kMapLastModified},
      .total_size{kMapTotalSize},
  });
  metadata.map_metadata.push_back({
      .map_locator{kLocalStorageSessionId, kSecondStorageKey},
      .last_accessed{kSecondLastAccessed},
  });
  metadata.map_metadata.push_back({
      .map_locator{kLocalStorageSessionId, kThirdStorageKey},
      .last_modified{kThirdLastModified},
      .total_size{kThirdTotalSize},
  });
  // Add a map with no usage metadata, which must not write anything to LevelDB.
  metadata.map_metadata.push_back({
      .map_locator{kLocalStorageSessionId, kFourthStorageKey},
  });

  // Write the metadata.
  DbStatus status = local_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which includes the "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDB().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 5u);

  // Verify "META:" entry for the first storage key.
  EXPECT_EQ(all_entries[0].key,
            LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey));
  EXPECT_EQ(all_entries[0].value, LocalStorageLevelDB::CreateWriteMetaDataValue(
                                      kMapLastModified, kMapTotalSize));

  // Verify "META:" entry for the third storage key.
  EXPECT_EQ(all_entries[1].key,
            LocalStorageLevelDB::CreateWriteMetaDataKey(kThirdStorageKey));
  EXPECT_EQ(all_entries[1].value, LocalStorageLevelDB::CreateWriteMetaDataValue(
                                      kThirdLastModified, kThirdTotalSize));

  // Verify "METAACCESS:" entry for the first storage key.
  EXPECT_EQ(all_entries[2].key,
            LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey));
  EXPECT_EQ(all_entries[2].value,
            LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed));

  // Verify "METAACCESS:" entry for the second storage key.
  EXPECT_EQ(all_entries[3].key,
            LocalStorageLevelDB::CreateAccessMetaDataKey(kSecondStorageKey));
  EXPECT_EQ(
      all_entries[3].value,
      LocalStorageLevelDB::CreateAccessMetaDataValue(kSecondLastAccessed));

  VerifyDatabaseVersionEntry(all_entries[4]);
}

TEST_F(LocalStorageLevelDBTest, GetMapPrefix) {
  std::string expected_prefix("_https://a-fake.test");
  expected_prefix.push_back(kLocalStorageKeyMapSeparator);

  EXPECT_EQ(LocalStorageLevelDB::GetMapPrefix(kFakeUrlStorageKey),
            ToBytes(expected_prefix));
}

TEST_F(LocalStorageLevelDBTest,
       DeleteStorageKeysFromSessionWithAccessMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *local_storage_leveldb,
      {
          {
              LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey),
              LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed),
          },
      }));

  DbStatus status = local_storage_leveldb->DeleteStorageKeysFromSession(
      kLocalStorageSessionId, {kFakeUrlStorageKey},
      /*excluded_cloned_map_ids=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should only include the
  // "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDB().GetPrefixed({}));
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
              LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey),
              LocalStorageLevelDB::CreateWriteMetaDataValue(kMapLastModified,
                                                            kMapTotalSize),
          },
      }));

  DbStatus status = local_storage_leveldb->DeleteStorageKeysFromSession(
      kLocalStorageSessionId, {kFakeUrlStorageKey},
      /*excluded_cloned_map_ids=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should only include the
  // "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDB().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 1u);

  VerifyDatabaseVersionEntry(all_entries[0]);
}

TEST_F(LocalStorageLevelDBTest,
       DeleteStorageKeysFromSessionWithWriteMapKeyValues) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*local_storage_leveldb,
                   {
                       {
                           CreateMapDataKey(kFakeUrlStorageKey, "key_1"),
                           ToBytes("value_1"),
                       },
                       {
                           CreateMapDataKey(kFakeUrlStorageKey, "key_2"),
                           ToBytes("value_2"),
                       },
                   }));

  DbStatus status = local_storage_leveldb->DeleteStorageKeysFromSession(
      kLocalStorageSessionId, {kFakeUrlStorageKey},
      /*excluded_cloned_map_ids=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should only include the
  // "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDB().GetPrefixed({}));
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
              CreateMapDataKey(kFakeUrlStorageKey, "key_1"),
              ToBytes("value_1"),
          },
          {
              CreateMapDataKey(kFakeUrlStorageKey, "key_2"),
              ToBytes("value_2"),
          },
          {
              CreateMapDataKey(kSecondStorageKey, "key_3"),
              ToBytes("value_3"),
          },
          {
              CreateMapDataKey(kThirdStorageKey, "key_1"),
              ToBytes("value_4"),
          },
          {
              CreateMapDataKey(kThirdStorageKey, "key_2"),
              ToBytes("value_5"),
          },
          {
              CreateMapDataKey(kThirdStorageKey, "key_3"),
              ToBytes("value_5"),
          },
          // Add "METAACCESS:" entries.
          {
              LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey),
              LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed),
          },
          {
              LocalStorageLevelDB::CreateAccessMetaDataKey(kSecondStorageKey),
              LocalStorageLevelDB::CreateAccessMetaDataValue(
                  kSecondLastAccessed),
          },
          {
              LocalStorageLevelDB::CreateAccessMetaDataKey(kThirdStorageKey),
              LocalStorageLevelDB::CreateAccessMetaDataValue(
                  kThirdLastAccessed),
          },
          // Add "META:" entries.
          {
              LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey),
              LocalStorageLevelDB::CreateWriteMetaDataValue(kMapLastModified,
                                                            kMapTotalSize),
          },
          {
              LocalStorageLevelDB::CreateWriteMetaDataKey(kSecondStorageKey),
              LocalStorageLevelDB::CreateWriteMetaDataValue(kSecondLastModified,
                                                            kSecondTotalSize),
          },
          {
              LocalStorageLevelDB::CreateWriteMetaDataKey(kThirdStorageKey),
              LocalStorageLevelDB::CreateWriteMetaDataValue(kThirdLastModified,
                                                            kThirdTotalSize),
          },
      }));

  // Erase the first and third storage keys.
  DbStatus status = local_storage_leveldb->DeleteStorageKeysFromSession(
      kLocalStorageSessionId, {kFakeUrlStorageKey, kThirdStorageKey},
      /*excluded_cloned_map_ids=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should include the second
  // storage key entries and the "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      local_storage_leveldb->GetLevelDB().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 4u);

  // Verify "META:" entry for the second storage key.
  EXPECT_EQ(all_entries[0].key,
            LocalStorageLevelDB::CreateWriteMetaDataKey(kSecondStorageKey));
  EXPECT_EQ(all_entries[0].value, LocalStorageLevelDB::CreateWriteMetaDataValue(
                                      kSecondLastModified, kSecondTotalSize));

  // Verify "METAACCESS:" entry for the second storage key.
  EXPECT_EQ(all_entries[1].key,
            LocalStorageLevelDB::CreateAccessMetaDataKey(kSecondStorageKey));
  EXPECT_EQ(
      all_entries[1].value,
      LocalStorageLevelDB::CreateAccessMetaDataValue(kSecondLastAccessed));

  VerifyDatabaseVersionEntry(all_entries[2]);

  // Verify the map key/value paris for the second storage key.
  EXPECT_EQ(all_entries[3].key, CreateMapDataKey(kSecondStorageKey, "key_3"));
  EXPECT_EQ(all_entries[3].value, ToBytes("value_3"));

  // Erase all the storage keys.
  status = local_storage_leveldb->DeleteStorageKeysFromSession(
      kLocalStorageSessionId,
      {
          kFakeUrlStorageKey,
          kSecondStorageKey,
          kThirdStorageKey,
          kFourthStorageKey,
      },
      /*excluded_cloned_map_ids=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should only include the
  // "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(all_entries,
                       local_storage_leveldb->GetLevelDB().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 1u);

  VerifyDatabaseVersionEntry(all_entries[0]);
}

}  // namespace storage
