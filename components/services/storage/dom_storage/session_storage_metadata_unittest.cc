// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_metadata.h"

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/uuid.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {
namespace {

std::vector<uint8_t> StdStringToUint8Vector(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

class SessionStorageMetadataTest : public base::test::WithFeatureOverride,
                                   public testing::Test {
 public:
  SessionStorageMetadataTest()
      : base::test::WithFeatureOverride(kDomStorageSqlite) {
    // Create an in-memory database.
    base::RunLoop loop;
    database_ = AsyncDomStorageDatabase::Open(
        StorageType::kSessionStorage,
        /*database_path=*/base::FilePath(),
        /*memory_dump_id=*/std::nullopt,
        base::BindLambdaForTesting([&](DbStatus) { loop.Quit(); }));
    loop.Run();
  }

  ~SessionStorageMetadataTest() override = default;

  void ReadMetadataFromDatabase(SessionStorageMetadata* metadata) {
    DomStorageDatabase::Metadata database_metadata;
    ASSERT_NO_FATAL_FAILURE(
        ReadAllMetadataSync(*database_, &database_metadata));
    metadata->Initialize(std::move(database_metadata));
  }

  void SetupTestData() {
    // Create two sessions in the database that each contain have two maps.
    // Clone the first map across both sessions.
    map1_locator_.AddSession(test_namespace2_id_);

    DomStorageDatabase::Metadata metadata;
    metadata.map_metadata.push_back({map1_locator_.Clone()});
    metadata.map_metadata.push_back({map3_locator_.Clone()});
    metadata.map_metadata.push_back({map4_locator_.Clone()});
    metadata.next_map_id = 5;

    ASSERT_NO_FATAL_FAILURE(PutMetadataSync(*database_, std::move(metadata)));

    // Add a key/value pair to each map in the database.
    FakeCommitter map1_committer(database_.get(), map1_locator_.Clone());
    map1_committer.PutMapKeyValueSync(kKey1, kValue1);

    FakeCommitter map3_committer(database_.get(), map3_locator_.Clone());
    map3_committer.PutMapKeyValueSync(kKey1, kValue3);

    FakeCommitter map4_committer(database_.get(), map4_locator_.Clone());
    map4_committer.PutMapKeyValueSync(kKey1, kValue4);
  }

  // Verifies a map in the database contains `expected_entries`.
  void ExpectMapEquals(const DomStorageDatabase::MapLocator& map_locator,
                       std::map<DomStorageDatabase::Key,
                                DomStorageDatabase::Value> expected_entries) {
    std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> actual_entries;
    ASSERT_NO_FATAL_FAILURE(
        ReadMapKeyValuesSync(*database_, map_locator.Clone(), &actual_entries));
    EXPECT_EQ(actual_entries, expected_entries);
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  const DomStorageDatabase::Key kKey1 = StdStringToUint8Vector("key1");
  const DomStorageDatabase::Value kValue1 = StdStringToUint8Vector("data1");
  const DomStorageDatabase::Value kValue3 = StdStringToUint8Vector("data3");
  const DomStorageDatabase::Value kValue4 = StdStringToUint8Vector("data4");

  const std::string test_namespace1_id_ =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string test_namespace2_id_ =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string test_namespace3_id_ =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  const blink::StorageKey test_storage_key1_ =
      blink::StorageKey::CreateFromStringForTesting("http://host1:1/");
  const blink::StorageKey test_storage_key2_ =
      blink::StorageKey::CreateFromStringForTesting("http://host2:2/");

  DomStorageDatabase::MapLocator map1_locator_{test_namespace1_id_,
                                               test_storage_key1_,
                                               /*map_id=*/1};

  DomStorageDatabase::MapLocator map3_locator_{test_namespace1_id_,
                                               test_storage_key2_,
                                               /*map_id=*/3};

  DomStorageDatabase::MapLocator map4_locator_{test_namespace2_id_,
                                               test_storage_key2_,
                                               /*map_id=*/4};

  std::unique_ptr<AsyncDomStorageDatabase> database_;
};

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    SessionStorageMetadataTest,
    testing::Bool(),
    /*name_generator=*/
    [](const testing::TestParamInfo<SessionStorageMetadataTest::ParamType>&
           info) { return info.param ? "SQLite" : "LevelDB"; });

TEST_P(SessionStorageMetadataTest, LoadingData) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  EXPECT_EQ(5, metadata.NextMapId());
  EXPECT_EQ(2ul, metadata.namespace_storage_key_map().size());

  // Namespace 1 should have 2 StorageKeys, referencing map 1 and 3. Map 1 is
  // shared between namespace 1 and namespace 2.
  auto entry = metadata.GetOrCreateNamespaceEntry(test_namespace1_id_);
  EXPECT_EQ(test_namespace1_id_, entry->first);
  EXPECT_EQ(2ul, entry->second.size());
  EXPECT_EQ(1, entry->second[test_storage_key1_]->map_id().value());
  EXPECT_EQ(2u, entry->second[test_storage_key1_]->session_ids().size());
  EXPECT_EQ(3, entry->second[test_storage_key2_]->map_id().value());
  EXPECT_EQ(1u, entry->second[test_storage_key2_]->session_ids().size());

  // Namespace 2 is the same, except the second StorageKey references map 4.
  entry = metadata.GetOrCreateNamespaceEntry(test_namespace2_id_);
  EXPECT_EQ(test_namespace2_id_, entry->first);
  EXPECT_EQ(2ul, entry->second.size());
  EXPECT_EQ(1, entry->second[test_storage_key1_]->map_id().value());
  EXPECT_EQ(2u, entry->second[test_storage_key1_]->session_ids().size());
  EXPECT_EQ(4, entry->second[test_storage_key2_]->map_id().value());
  EXPECT_EQ(1u, entry->second[test_storage_key2_]->session_ids().size());
}

TEST_P(SessionStorageMetadataTest, ShallowCopies) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  auto ns1_entry = metadata.GetOrCreateNamespaceEntry(test_namespace1_id_);
  auto ns3_entry = metadata.GetOrCreateNamespaceEntry(test_namespace3_id_);

  metadata.RegisterShallowClonedNamespace(ns1_entry, ns3_entry);

  ASSERT_NO_FATAL_FAILURE(PutMetadataSync(
      *database_, SessionStorageMetadata::ToDomStorageMetadata(ns3_entry)));

  // Verify in-memory metadata is correct.
  EXPECT_EQ(1, ns3_entry->second[test_storage_key1_]->map_id().value());
  EXPECT_EQ(3, ns3_entry->second[test_storage_key2_]->map_id().value());
  EXPECT_EQ(ns1_entry->second[test_storage_key1_].get(),
            ns3_entry->second[test_storage_key1_].get());
  EXPECT_EQ(ns1_entry->second[test_storage_key2_].get(),
            ns3_entry->second[test_storage_key2_].get());
  EXPECT_EQ(3u, ns3_entry->second[test_storage_key1_]->session_ids().size());
  EXPECT_EQ(2u, ns3_entry->second[test_storage_key2_]->session_ids().size());

  // Verify metadata was written to disk where `test_namespace3_id_` cloned map
  // 1 and map 3.
  DomStorageDatabase::Metadata all_metadata;
  ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database_, &all_metadata));

  EXPECT_EQ(all_metadata.next_map_id, 5);
  ASSERT_EQ(all_metadata.map_metadata.size(), 3u);

  DomStorageDatabase::MapMetadata expected_metadata[] = {
      {map1_locator_.Clone()},
      {map3_locator_.Clone()},
      {map4_locator_.Clone()},
  };
  expected_metadata[0].map_locator.AddSession(test_namespace3_id_);
  expected_metadata[1].map_locator.AddSession(test_namespace3_id_);

  ExpectEqualsMapMetadataSpan(all_metadata.map_metadata, expected_metadata);
}

TEST_P(SessionStorageMetadataTest, TakeNamespace) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  std::map<blink::StorageKey,
           scoped_refptr<DomStorageDatabase::SharedMapLocator>>
      namespace_to_delete = metadata.TakeNamespace(test_namespace1_id_);

  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  for (auto& [storage_key, map_locator] : namespace_to_delete) {
    if (map_locator->session_ids().empty()) {
      maps_to_delete.push_back(std::move(*map_locator));
    }
  }
  DeleteSessionsSync(*database_, {test_namespace1_id_},
                     std::move(maps_to_delete));

  EXPECT_FALSE(
      metadata.namespace_storage_key_map().contains(test_namespace1_id_));

  // Verify in-memory metadata is correct.
  auto ns2_entry = metadata.GetOrCreateNamespaceEntry(test_namespace2_id_);
  EXPECT_EQ(1u, ns2_entry->second[test_storage_key1_]->session_ids().size());
  EXPECT_EQ(1u, ns2_entry->second[test_storage_key2_]->session_ids().size());

  // Verify metadata and data was deleted from disk.
  DomStorageDatabase::Metadata all_metadata;
  ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database_, &all_metadata));

  EXPECT_EQ(all_metadata.next_map_id, 5);
  ASSERT_EQ(all_metadata.map_metadata.size(), 2u);

  // Two maps must remain in the database each used by session
  // `test_namespace2_id_`.
  DomStorageDatabase::MapMetadata expected_metadata[] = {
      {map1_locator_.Clone()},
      {map4_locator_.Clone()},
  };
  expected_metadata[0].map_locator.RemoveSession(test_namespace1_id_);

  ExpectEqualsMapMetadataSpan(all_metadata.map_metadata, expected_metadata);
  ASSERT_NO_FATAL_FAILURE(ExpectMapEquals(map3_locator_, {}));
  ASSERT_NO_FATAL_FAILURE(ExpectMapEquals(map1_locator_, {{kKey1, kValue1}}));
  ASSERT_NO_FATAL_FAILURE(ExpectMapEquals(map4_locator_, {{kKey1, kValue4}}));
}

TEST_P(SessionStorageMetadataTest, DeleteArea) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  // First delete an area with a shared map.
  scoped_refptr<DomStorageDatabase::SharedMapLocator> map_locator =
      metadata.TakeExistingMap(test_namespace1_id_, test_storage_key1_);
  EXPECT_EQ(map_locator->session_ids().size(), 1u);
  EXPECT_EQ(map_locator->session_ids()[0], test_namespace2_id_);

  DeleteStorageKeysFromSessionSync(*database_, test_namespace1_id_,
                                   {test_storage_key1_}, /*maps_to_delete=*/{});

  // Verify in-memory metadata is correct.
  auto ns1_entry = metadata.GetOrCreateNamespaceEntry(test_namespace1_id_);
  auto ns2_entry = metadata.GetOrCreateNamespaceEntry(test_namespace2_id_);
  EXPECT_FALSE(ns1_entry->second.contains(test_storage_key1_));
  EXPECT_EQ(1u, ns1_entry->second[test_storage_key2_]->session_ids().size());
  EXPECT_EQ(1u, ns2_entry->second[test_storage_key1_]->session_ids().size());
  EXPECT_EQ(1u, ns2_entry->second[test_storage_key2_]->session_ids().size());

  // Verify only the applicable data was deleted.
  DomStorageDatabase::Metadata all_metadata;
  ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database_, &all_metadata));

  EXPECT_EQ(all_metadata.next_map_id, 5);
  ASSERT_EQ(all_metadata.map_metadata.size(), 3u);

  // Three maps must remain in the database.  `test_namespace1_id_` and
  // `test_namespace2_id_` no longer share a clone of map 1.
  DomStorageDatabase::MapMetadata expected_metadata[] = {
      {map1_locator_.Clone()},
      {map3_locator_.Clone()},
      {map4_locator_.Clone()},
  };
  expected_metadata[0].map_locator.RemoveSession(test_namespace1_id_);

  ExpectEqualsMapMetadataSpan(all_metadata.map_metadata, expected_metadata);
  ASSERT_NO_FATAL_FAILURE(ExpectMapEquals(map3_locator_, {{kKey1, kValue3}}));
  ASSERT_NO_FATAL_FAILURE(ExpectMapEquals(map1_locator_, {{kKey1, kValue1}}));
  ASSERT_NO_FATAL_FAILURE(ExpectMapEquals(map4_locator_, {{kKey1, kValue4}}));

  // Now delete an area with a unique map.
  map_locator =
      metadata.TakeExistingMap(test_namespace2_id_, test_storage_key2_);
  EXPECT_EQ(map_locator->session_ids().size(), 0u);

  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(std::move(*map_locator));

  DeleteStorageKeysFromSessionSync(*database_, test_namespace2_id_,
                                   {test_storage_key2_},
                                   std::move(maps_to_delete));

  // Verify in-memory metadata is correct.
  EXPECT_FALSE(ns1_entry->second.contains(test_storage_key1_));
  EXPECT_EQ(1u, ns1_entry->second[test_storage_key2_]->session_ids().size());
  EXPECT_EQ(1u, ns2_entry->second[test_storage_key1_]->session_ids().size());
  EXPECT_FALSE(ns2_entry->second.contains(test_storage_key2_));

  // Verify only the applicable data was deleted, which must delete map 4 from
  // the database.
  ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database_, &all_metadata));

  EXPECT_EQ(all_metadata.next_map_id, 5);
  ASSERT_EQ(all_metadata.map_metadata.size(), 2u);

  ExpectEqualsMapMetadataSpan(all_metadata.map_metadata,
                              base::span(expected_metadata).first(2u));

  ASSERT_NO_FATAL_FAILURE(ExpectMapEquals(map3_locator_, {{kKey1, kValue3}}));
  ASSERT_NO_FATAL_FAILURE(ExpectMapEquals(map1_locator_, {{kKey1, kValue1}}));
  ASSERT_NO_FATAL_FAILURE(ExpectMapEquals(map4_locator_, {}));
}

TEST_P(SessionStorageMetadataTest, InitializesNamespacesEmpty) {
  DomStorageDatabase::Metadata source;
  source.next_map_id = 0;

  SessionStorageMetadata metadata;
  metadata.Initialize(std::move(source));
  EXPECT_EQ(metadata.namespace_storage_key_map().size(), 0u);
}

TEST_P(SessionStorageMetadataTest, InitializeNamespaces) {
  DomStorageDatabase::Metadata source;
  source.map_metadata.push_back({
      .map_locator{test_namespace3_id_, test_storage_key1_, /*map_id=*/1},
      .last_accessed{base::Time::Now()},
  });
  source.next_map_id = 2;

  SessionStorageMetadata metadata;
  metadata.Initialize(std::move(source));

  const SessionStorageMetadata::NamespaceStorageKeyMap& parsed_namespaces =
      metadata.namespace_storage_key_map();
  EXPECT_EQ(parsed_namespaces.size(), 1u);

  auto namespace_it = parsed_namespaces.find(test_namespace3_id_);
  ASSERT_TRUE(namespace_it != parsed_namespaces.end());

  const auto& storage_key_maps = namespace_it->second;
  EXPECT_EQ(storage_key_maps.size(), 1u);

  auto storage_key_it = storage_key_maps.find(test_storage_key1_);
  EXPECT_TRUE(storage_key_it != storage_key_maps.end());
}

}  // namespace

}  // namespace storage
