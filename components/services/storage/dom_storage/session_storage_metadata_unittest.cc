// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_metadata.h"

#include <string>

#include "base/containers/contains.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/uuid.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "storage/common/database/db_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"

namespace storage {
namespace {

std::vector<uint8_t> StdStringToUint8Vector(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

void ErrorCallback(DbStatus* status_out, DbStatus status) {
  *status_out = status;
}

class SessionStorageMetadataTest : public testing::Test {
 public:
  SessionStorageMetadataTest()
      : test_namespace1_id_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
        test_namespace2_id_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
        test_namespace3_id_(
            base::Uuid::GenerateRandomV4().AsLowercaseString()) {
    base::RunLoop loop;
    database_ = AsyncDomStorageDatabase::OpenInMemory(
        std::nullopt, "SessionStorageMetadataTest",
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
        base::BindLambdaForTesting([&](DbStatus) { loop.Quit(); }));
    loop.Run();

    next_map_id_key_ = std::vector<uint8_t>(
        std::begin(SessionStorageMetadata::kNextMapIdKeyBytes),
        std::end(SessionStorageMetadata::kNextMapIdKeyBytes));
    database_version_key_ = std::vector<uint8_t>(
        std::begin(SessionStorageMetadata::kLevelDbSchemaVersionKeyBytes),
        std::end(SessionStorageMetadata::kLevelDbSchemaVersionKeyBytes));
    namespaces_prefix_key_ = std::vector<uint8_t>(
        std::begin(SessionStorageMetadata::kNamespacePrefixBytes),
        std::end(SessionStorageMetadata::kNamespacePrefixBytes));
  }
  ~SessionStorageMetadataTest() override = default;

  void ReadMetadataFromDatabase(SessionStorageMetadata* metadata) {
    std::vector<uint8_t> version_value;
    std::vector<uint8_t> next_map_id_value;
    std::vector<DomStorageDatabase::KeyValuePair> namespace_entries;

    base::RunLoop loop;
    database_->database().PostTaskWithThisObject(
        base::BindLambdaForTesting([&](const DomStorageDatabaseLevelDB& db) {
          EXPECT_TRUE(db.Get(database_version_key_, &version_value).ok());
          EXPECT_TRUE(db.Get(next_map_id_key_, &next_map_id_value).ok());
          EXPECT_TRUE(
              db.GetPrefixed(namespaces_prefix_key_, &namespace_entries).ok());
          loop.Quit();
        }));
    loop.Run();

    int64_t parsed_version;
    EXPECT_TRUE(metadata->ParseDatabaseVersion(version_value, &parsed_version));
    EXPECT_EQ(parsed_version, SessionStorageMetadata::kLevelDbSchemaVersion);

    metadata->ParseNextMapId(next_map_id_value);
    EXPECT_TRUE(metadata->ParseNamespaces(std::move(namespace_entries)));
  }

  void SetupTestData() {
    // | key                                    | value              |
    // |----------------------------------------|--------------------|
    // | map-1-key1                             | data1              |
    // | map-3-key1                             | data3              |
    // | map-4-key1                             | data4              |
    // | namespace-<guid 1>-http://host1:1/     | 1                  |
    // | namespace-<guid 1>-http://host2:2/     | 3                  |
    // | namespace-<guid 2>-http://host1:1/     | 1                  |
    // | namespace-<guid 2>-http://host2:2/     | 4                  |
    // | next-map-id                            | 5                  |
    // | version                                | 1                  |
    base::RunLoop loop;
    database_->database().PostTaskWithThisObject(
        base::BindLambdaForTesting([&](DomStorageDatabaseLevelDB* db) {
          db->Put(StdStringToUint8Vector(std::string("namespace-") +
                                         test_namespace1_id_ + "-" +
                                         test_storage_key1_.Serialize()),
                  StdStringToUint8Vector("1"));
          db->Put(StdStringToUint8Vector(std::string("namespace-") +
                                         test_namespace1_id_ + "-" +
                                         test_storage_key2_.Serialize()),
                  StdStringToUint8Vector("3"));
          db->Put(StdStringToUint8Vector(std::string("namespace-") +
                                         test_namespace2_id_ + "-" +
                                         test_storage_key1_.Serialize()),
                  StdStringToUint8Vector("1"));
          db->Put(StdStringToUint8Vector(std::string("namespace-") +
                                         test_namespace2_id_ + "-" +
                                         test_storage_key2_.Serialize()),
                  StdStringToUint8Vector("4"));

          db->Put(next_map_id_key_, StdStringToUint8Vector("5"));

          db->Put(StdStringToUint8Vector("map-1-key1"),
                  StdStringToUint8Vector("data1"));
          db->Put(StdStringToUint8Vector("map-3-key1"),
                  StdStringToUint8Vector("data3"));
          db->Put(StdStringToUint8Vector("map-4-key1"),
                  StdStringToUint8Vector("data4"));

          db->Put(database_version_key_, StdStringToUint8Vector("1"));
          loop.Quit();
        }));
    loop.Run();
  }

  std::map<std::vector<uint8_t>, std::vector<uint8_t>> GetDatabaseContents() {
    std::vector<DomStorageDatabase::KeyValuePair> entries;
    base::RunLoop loop;
    database_->database().PostTaskWithThisObject(
        base::BindLambdaForTesting([&](const DomStorageDatabaseLevelDB& db) {
          DbStatus status = db.GetPrefixed({}, &entries);
          ASSERT_TRUE(status.ok());
          loop.Quit();
        }));
    loop.Run();

    std::map<std::vector<uint8_t>, std::vector<uint8_t>> contents;
    for (auto& entry : entries)
      contents.emplace(entry.key, entry.value);
    return contents;
  }

  void RunBatch(std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> tasks,
                base::OnceCallback<void(DbStatus)> callback) {
    base::RunLoop loop;
    database_->RunBatchDatabaseTasks(
        RunBatchTasksContext::kTest, std::move(tasks),
        base::BindLambdaForTesting([&](DbStatus status) {
          std::move(callback).Run(status);
          loop.Quit();
        }));
    loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  const std::string test_namespace1_id_;
  const std::string test_namespace2_id_;
  const std::string test_namespace3_id_;
  const blink::StorageKey test_storage_key1_ =
      blink::StorageKey::CreateFromStringForTesting("http://host1:1/");
  const blink::StorageKey test_storage_key2_ =
      blink::StorageKey::CreateFromStringForTesting("http://host2:2/");
  std::unique_ptr<AsyncDomStorageDatabase> database_;

  std::vector<uint8_t> database_version_key_;
  std::vector<uint8_t> next_map_id_key_;
  std::vector<uint8_t> namespaces_prefix_key_;
};

TEST_F(SessionStorageMetadataTest, SaveNewMetadata) {
  SessionStorageMetadata metadata;
  std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> tasks =
      metadata.SetupNewDatabaseForTesting();

  DbStatus status;
  RunBatch(std::move(tasks), base::BindOnce(&ErrorCallback, &status));
  EXPECT_TRUE(status.ok());

  auto contents = GetDatabaseContents();
  EXPECT_EQ(StdStringToUint8Vector("1"), contents[database_version_key_]);
  EXPECT_EQ(StdStringToUint8Vector("0"), contents[next_map_id_key_]);
}

TEST_F(SessionStorageMetadataTest, LoadingData) {
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
  EXPECT_EQ(StdStringToUint8Vector("map-1-"),
            entry->second[test_storage_key1_]->KeyPrefix());
  EXPECT_EQ(2, entry->second[test_storage_key1_]->ReferenceCount());
  EXPECT_EQ(StdStringToUint8Vector("map-3-"),
            entry->second[test_storage_key2_]->KeyPrefix());
  EXPECT_EQ(1, entry->second[test_storage_key2_]->ReferenceCount());

  // Namespace 2 is the same, except the second StorageKey references map 4.
  entry = metadata.GetOrCreateNamespaceEntry(test_namespace2_id_);
  EXPECT_EQ(test_namespace2_id_, entry->first);
  EXPECT_EQ(2ul, entry->second.size());
  EXPECT_EQ(StdStringToUint8Vector("map-1-"),
            entry->second[test_storage_key1_]->KeyPrefix());
  EXPECT_EQ(2, entry->second[test_storage_key1_]->ReferenceCount());
  EXPECT_EQ(StdStringToUint8Vector("map-4-"),
            entry->second[test_storage_key2_]->KeyPrefix());
  EXPECT_EQ(1, entry->second[test_storage_key2_]->ReferenceCount());
}

TEST_F(SessionStorageMetadataTest, SaveNewMap) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> tasks;
  auto ns1_entry = metadata.GetOrCreateNamespaceEntry(test_namespace1_id_);
  auto map_data =
      metadata.RegisterNewMap(ns1_entry, test_storage_key1_, &tasks);
  ASSERT_TRUE(map_data);

  // Verify in-memory metadata is correct.
  EXPECT_EQ(StdStringToUint8Vector("map-5-"),
            ns1_entry->second[test_storage_key1_]->KeyPrefix());
  EXPECT_EQ(1, ns1_entry->second[test_storage_key1_]->ReferenceCount());
  EXPECT_EQ(1, metadata.GetOrCreateNamespaceEntry(test_namespace2_id_)
                   ->second[test_storage_key1_]
                   ->ReferenceCount());

  DbStatus status;
  RunBatch(std::move(tasks), base::BindOnce(&ErrorCallback, &status));
  EXPECT_TRUE(status.ok());

  // Verify metadata was written to disk.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(StdStringToUint8Vector("6"), contents[next_map_id_key_]);
  EXPECT_EQ(StdStringToUint8Vector("5"),
            contents[StdStringToUint8Vector(std::string("namespace-") +
                                            test_namespace1_id_ + "-" +
                                            test_storage_key1_.Serialize())]);
}

TEST_F(SessionStorageMetadataTest, ShallowCopies) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  auto ns1_entry = metadata.GetOrCreateNamespaceEntry(test_namespace1_id_);
  auto ns3_entry = metadata.GetOrCreateNamespaceEntry(test_namespace3_id_);

  std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> tasks;
  metadata.RegisterShallowClonedNamespace(ns1_entry, ns3_entry, &tasks);

  DbStatus status;
  RunBatch(std::move(tasks), base::BindOnce(&ErrorCallback, &status));
  EXPECT_TRUE(status.ok());

  // Verify in-memory metadata is correct.
  EXPECT_EQ(StdStringToUint8Vector("map-1-"),
            ns3_entry->second[test_storage_key1_]->KeyPrefix());
  EXPECT_EQ(StdStringToUint8Vector("map-3-"),
            ns3_entry->second[test_storage_key2_]->KeyPrefix());
  EXPECT_EQ(ns1_entry->second[test_storage_key1_].get(),
            ns3_entry->second[test_storage_key1_].get());
  EXPECT_EQ(ns1_entry->second[test_storage_key2_].get(),
            ns3_entry->second[test_storage_key2_].get());
  EXPECT_EQ(3, ns3_entry->second[test_storage_key1_]->ReferenceCount());
  EXPECT_EQ(2, ns3_entry->second[test_storage_key2_]->ReferenceCount());

  // Verify metadata was written to disk.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(StdStringToUint8Vector("1"),
            contents[StdStringToUint8Vector(std::string("namespace-") +
                                            test_namespace3_id_ + "-" +
                                            test_storage_key1_.Serialize())]);
  EXPECT_EQ(StdStringToUint8Vector("3"),
            contents[StdStringToUint8Vector(std::string("namespace-") +
                                            test_namespace3_id_ + "-" +
                                            test_storage_key2_.Serialize())]);
}

TEST_F(SessionStorageMetadataTest, DeleteNamespace) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> tasks;
  metadata.DeleteNamespace(test_namespace1_id_, &tasks);
  DbStatus status;
  RunBatch(std::move(tasks), base::BindOnce(&ErrorCallback, &status));
  EXPECT_TRUE(status.ok());

  EXPECT_FALSE(base::Contains(metadata.namespace_storage_key_map(),
                              test_namespace1_id_));

  // Verify in-memory metadata is correct.
  auto ns2_entry = metadata.GetOrCreateNamespaceEntry(test_namespace2_id_);
  EXPECT_EQ(1, ns2_entry->second[test_storage_key1_]->ReferenceCount());
  EXPECT_EQ(1, ns2_entry->second[test_storage_key2_]->ReferenceCount());

  // Verify metadata and data was deleted from disk.
  auto contents = GetDatabaseContents();
  EXPECT_FALSE(base::Contains(
      contents,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace1_id_ +
                             "-" + test_storage_key1_.Serialize())));
  EXPECT_FALSE(base::Contains(
      contents,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace1_id_ +
                             "-" + test_storage_key2_.Serialize())));
  EXPECT_FALSE(base::Contains(contents, StdStringToUint8Vector("map-3-key1")));
  EXPECT_TRUE(base::Contains(contents, StdStringToUint8Vector("map-1-key1")));
}

TEST_F(SessionStorageMetadataTest, DeleteArea) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  // First delete an area with a shared map.
  std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> tasks;
  metadata.DeleteArea(test_namespace1_id_, test_storage_key1_, &tasks);
  DbStatus status;
  RunBatch(std::move(tasks), base::BindOnce(&ErrorCallback, &status));
  EXPECT_TRUE(status.ok());

  // Verify in-memory metadata is correct.
  auto ns1_entry = metadata.GetOrCreateNamespaceEntry(test_namespace1_id_);
  auto ns2_entry = metadata.GetOrCreateNamespaceEntry(test_namespace2_id_);
  EXPECT_FALSE(base::Contains(ns1_entry->second, test_storage_key1_));
  EXPECT_EQ(1, ns1_entry->second[test_storage_key2_]->ReferenceCount());
  EXPECT_EQ(1, ns2_entry->second[test_storage_key1_]->ReferenceCount());
  EXPECT_EQ(1, ns2_entry->second[test_storage_key2_]->ReferenceCount());

  // Verify only the applicable data was deleted.
  auto contents = GetDatabaseContents();
  EXPECT_FALSE(base::Contains(
      contents,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace1_id_ +
                             "-" + test_storage_key1_.Serialize())));
  EXPECT_TRUE(base::Contains(
      contents,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace1_id_ +
                             "-" + test_storage_key2_.Serialize())));
  EXPECT_TRUE(base::Contains(contents, StdStringToUint8Vector("map-1-key1")));
  EXPECT_TRUE(base::Contains(contents, StdStringToUint8Vector("map-4-key1")));

  // Now delete an area with a unique map.
  tasks.clear();
  metadata.DeleteArea(test_namespace2_id_, test_storage_key2_, &tasks);
  RunBatch(std::move(tasks), base::BindOnce(&ErrorCallback, &status));
  EXPECT_TRUE(status.ok());

  // Verify in-memory metadata is correct.
  EXPECT_FALSE(base::Contains(ns1_entry->second, test_storage_key1_));
  EXPECT_EQ(1, ns1_entry->second[test_storage_key2_]->ReferenceCount());
  EXPECT_EQ(1, ns2_entry->second[test_storage_key1_]->ReferenceCount());
  EXPECT_FALSE(base::Contains(ns2_entry->second, test_storage_key2_));

  // Verify only the applicable data was deleted.
  contents = GetDatabaseContents();
  EXPECT_TRUE(base::Contains(
      contents,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace2_id_ +
                             "-" + test_storage_key1_.Serialize())));
  EXPECT_FALSE(base::Contains(
      contents,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace2_id_ +
                             "-" + test_storage_key2_.Serialize())));
  EXPECT_TRUE(base::Contains(contents, StdStringToUint8Vector("map-1-key1")));
  EXPECT_TRUE(base::Contains(contents, StdStringToUint8Vector("map-3-key1")));
  EXPECT_FALSE(base::Contains(contents, StdStringToUint8Vector("map-4-key1")));
}

TEST_F(SessionStorageMetadataTest, ParseDatabaseVersion) {
  SessionStorageMetadata metadata;
  int64_t parsed_version;

  // Parsing empty bytes must fail.
  EXPECT_FALSE(metadata.ParseDatabaseVersion({}, &parsed_version));

  // Parsing non-numeric text must fail.
  EXPECT_FALSE(metadata.ParseDatabaseVersion(
      {'i', 'n', 'v', 'a', 'l', 'i', 'd'}, &parsed_version));

  EXPECT_FALSE(metadata.ParseDatabaseVersion({'1', 'a'}, &parsed_version));

  // Parsing numeric text must succeed.
  EXPECT_TRUE(metadata.ParseDatabaseVersion({'6', '4', '4'}, &parsed_version));
  EXPECT_EQ(parsed_version, 644);

  EXPECT_TRUE(metadata.ParseDatabaseVersion({'1'}, &parsed_version));
  EXPECT_EQ(parsed_version, 1);
}

TEST_F(SessionStorageMetadataTest, ParseNamespacesEmpty) {
  // Parsing an empty vector must succeed.
  SessionStorageMetadata metadata;
  EXPECT_TRUE(metadata.ParseNamespaces(/*db_key_values=*/{}));
  EXPECT_EQ(metadata.namespace_storage_key_map().size(), 0u);
}

TEST_F(SessionStorageMetadataTest, ParseNamespacesInvalidId) {
  // Parsing an invalid namespace database key without an ID and storage key
  // must fail, i.e. "namespace-".
  std::vector<DomStorageDatabase::KeyValuePair> db_key_values{
      {
          StdStringToUint8Vector("namespace-"),
          StdStringToUint8Vector("1"),
      },
  };
  SessionStorageMetadata metadata;
  EXPECT_FALSE(metadata.ParseNamespaces(db_key_values));
  EXPECT_EQ(metadata.namespace_storage_key_map().size(), 0u);
}

TEST_F(SessionStorageMetadataTest, ParseNamespacesInvalidStorageKey) {
  // Parsing an invalid namespace database key without a storage key must fail,
  // i.e. "namespace-<guid>-".
  std::vector<DomStorageDatabase::KeyValuePair> db_key_values{
      {
          StdStringToUint8Vector("namespace-" + test_namespace3_id_ + "-"),
          StdStringToUint8Vector("1"),
      },
  };
  SessionStorageMetadata metadata;
  EXPECT_FALSE(metadata.ParseNamespaces(db_key_values));
  EXPECT_EQ(metadata.namespace_storage_key_map().size(), 0u);
}

TEST_F(SessionStorageMetadataTest, ParseNamespaces) {
  // Parsing a valid namespace database key must succeed.
  const std::string valid_namespace_key = std::string("namespace-") +
                                          test_namespace3_id_ + "-" +
                                          test_storage_key1_.Serialize();
  std::vector<DomStorageDatabase::KeyValuePair> db_key_values{
      {
          StdStringToUint8Vector(valid_namespace_key),
          StdStringToUint8Vector("1"),
      },
  };
  SessionStorageMetadata metadata;
  EXPECT_TRUE(metadata.ParseNamespaces(db_key_values));

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
