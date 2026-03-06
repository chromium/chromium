// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/async_dom_storage_database.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

namespace {
constexpr const char kFirstFakeUrlString[] = "https://a-fake-url.test";
constexpr const char kSecondFakeUrlString[] = "https://b-fake-url.test";
constexpr const char kThirdFakeUrlString[] = "https://c-fake-url.test";
constexpr const char kFourthFakeUrlString[] = "https://d-fake-url.test";
}  // namespace

class AsyncDomStorageDatabaseTest : public base::test::WithFeatureOverride,
                                    public testing::Test {
 public:
  AsyncDomStorageDatabaseTest();
  ~AsyncDomStorageDatabaseTest() override = default;

  bool IsSqliteEnabled() const { return GetParam(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  const blink::StorageKey kFirstStorageKey;
  const blink::StorageKey kSecondStorageKey;
  const blink::StorageKey kThirdStorageKey;
  const blink::StorageKey kFourthStorageKey;
};

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    AsyncDomStorageDatabaseTest,
    testing::Bool(),
    /*name_generator=*/
    [](const testing::TestParamInfo<AsyncDomStorageDatabaseTest::ParamType>&
           info) { return info.param ? "SQLite" : "LevelDB"; });

AsyncDomStorageDatabaseTest::AsyncDomStorageDatabaseTest()
    : base::test::WithFeatureOverride(kDomStorageSqlite),
      kFirstStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kFirstFakeUrlString)),
      kSecondStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kSecondFakeUrlString)),
      kThirdStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kThirdFakeUrlString)),
      kFourthStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kFourthFakeUrlString)) {
  // Match the state of `kDomStorageSqliteInMemory` to the top level
  // kDomStorageSqlite. That way in-memory databases will use the backend
  // expected by the param state.
  if (IsSqliteEnabled()) {
    feature_list_.InitAndEnableFeature(kDomStorageSqliteInMemory);
  } else {
    feature_list_.InitAndDisableFeature(kDomStorageSqliteInMemory);
  }
}

TEST_P(AsyncDomStorageDatabaseTest,
       WriteAndReadThenDeleteLocalStorageMetadata) {
  // Define test values to write to the database.
  const DomStorageDatabase::MapMetadata kInitialMapMetadataArray[] = {
      {
          .map_locator{kFirstStorageKey},
          .last_accessed{base::Time::Now() - base::Days(7)},
      },
      {
          .map_locator{kSecondStorageKey},
          .last_modified{base::Time::Now() - base::Seconds(12)},
          .total_size{104},
      },
      {
          .map_locator{kThirdStorageKey},
          .last_accessed{base::Time::Now() - base::Minutes(15)},
      },
      {
          .map_locator{kFourthStorageKey},
          .last_accessed{base::Time::Now() - base::Minutes(30)},
          .last_modified{base::Time::Now() - base::Seconds(47)},
          .total_size{211114},
      },
  };
  const base::span<const DomStorageDatabase::MapMetadata> kInitialMapMetadata =
      kInitialMapMetadataArray;

  // Open the database.
  std::unique_ptr<AsyncDomStorageDatabase> database;
  ASSERT_NO_FATAL_FAILURE(OpenAsyncDomStorageDatabaseInMemorySync(
      StorageType::kLocalStorage, &database));

  // Write each map's metadata to the database.
  for (size_t i = 0; i < kInitialMapMetadata.size(); ++i) {
    // Write the metadata for a single map.
    DomStorageDatabase::Metadata cloned_metadata;
    cloned_metadata.map_metadata =
        CloneMapMetadataVector(base::span_from_ref(kInitialMapMetadata[i]));

    ASSERT_NO_FATAL_FAILURE(
        PutMetadataSync(*database, std::move(cloned_metadata)));

    // Read the metadata from the database.
    DomStorageDatabase::Metadata read_metadata;
    ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database, &read_metadata));

    // Read back the metadata written so far.
    base::span<const DomStorageDatabase::MapMetadata> written_metadata_span =
        kInitialMapMetadata.first(/*count=*/i + 1);

    std::vector<DomStorageDatabase::MapMetadata> expected_map_metadata;
    if (IsSqliteEnabled()) {
      // Copy `written_metadata_span`, inserting the expected `map_id`.
      for (size_t j = 0u; j < written_metadata_span.size(); ++j) {
        const DomStorageDatabase::MapMetadata& written_metadata =
            written_metadata_span[j];

        expected_map_metadata.push_back({
            .map_locator{written_metadata.map_locator.storage_key(),
                         /*map_id=*/static_cast<int64_t>(j + 1)},
            .last_accessed = written_metadata.last_accessed,
            .last_modified = written_metadata.last_modified,
            .total_size = written_metadata.total_size,
        });
      }
    } else {
      // LevelDB does not create map IDs, associating maps by storage key only.
      expected_map_metadata = CloneMapMetadataVector(written_metadata_span);
    }

    ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                                expected_map_metadata);

    // Local storage does not store the next map id number.
    EXPECT_EQ(read_metadata.next_map_id, std::nullopt);
  }

  // Delete the first and third storage keys.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kFirstStorageKey);
  maps_to_delete.emplace_back(kThirdStorageKey);

  DeleteStorageKeysFromSessionSync(*database, /*session_id=*/std::string(),
                                   {kFirstStorageKey, kThirdStorageKey},
                                   std::move(maps_to_delete));

  DomStorageDatabase::Metadata read_metadata;
  ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database, &read_metadata));
  EXPECT_EQ(read_metadata.next_map_id, std::nullopt);

  // Add the second and fourth storage keys as expected.
  std::vector<DomStorageDatabase::MapMetadata> expected_metadata_after_delete;
  if (IsSqliteEnabled()) {
    // Copy the second and fourth `kInitialMapMetadata`, inserting the expected
    // `map_id`.
    expected_metadata_after_delete.push_back({
        .map_locator{kInitialMapMetadata[1].map_locator.storage_key(),
                     /*map_id=*/2},
        .last_accessed = kInitialMapMetadata[1].last_accessed,
        .last_modified = kInitialMapMetadata[1].last_modified,
        .total_size = kInitialMapMetadata[1].total_size,
    });

    expected_metadata_after_delete.push_back({
        .map_locator{kInitialMapMetadata[3].map_locator.storage_key(),
                     /*map_id=*/4},
        .last_accessed = kInitialMapMetadata[3].last_accessed,
        .last_modified = kInitialMapMetadata[3].last_modified,
        .total_size = kInitialMapMetadata[3].total_size,
    });
  } else {
    // LevelDB does not create map IDs, associating maps by storage key only.
    expected_metadata_after_delete.push_back(
        CloneMapMetadata(kInitialMapMetadata[1]));
    expected_metadata_after_delete.push_back(
        CloneMapMetadata(kInitialMapMetadata[3]));
  }

  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              expected_metadata_after_delete);
}

TEST_P(AsyncDomStorageDatabaseTest, MapLocatorToDebugStringWithoutSessions) {
  DomStorageDatabase::MapLocator map_locator{"session_id1", kFirstStorageKey,
                                             /*map_id=*/216};
  map_locator.RemoveSession("session_id1");
  EXPECT_EQ(map_locator.ToDebugString(),
            "sessions_ids:, storage_key:{ origin: https://a-fake-url.test, "
            "top-level site: https://a-fake-url.test, nonce: <null>, ancestor "
            "chain bit: Same-Site }, map_id:216");
}

TEST_P(AsyncDomStorageDatabaseTest,
       MapLocatorToDebugStringWithMultipleSessions) {
  DomStorageDatabase::MapLocator map_locator{"session_id1", kFirstStorageKey,
                                             /*map_id=*/216};
  map_locator.AddSession("session_id2");
  EXPECT_EQ(map_locator.ToDebugString(),
            "sessions_ids:session_id1:session_id2, storage_key:{ origin: "
            "https://a-fake-url.test, top-level site: https://a-fake-url.test, "
            "nonce: <null>, ancestor chain bit: Same-Site }, "
            "map_id:216");
}

TEST_P(AsyncDomStorageDatabaseTest, MapLocatorToDebugStringWithoutMapId) {
  DomStorageDatabase::MapLocator map_locator{"session_id1", kFirstStorageKey};
  EXPECT_EQ(map_locator.ToDebugString(),
            "sessions_ids:session_id1, storage_key:{ origin: "
            "https://a-fake-url.test, top-level site: https://a-fake-url.test, "
            "nonce: <null>, ancestor chain bit: Same-Site }, "
            "map_id:null");
}

}  // namespace storage
