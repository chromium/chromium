// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/shared_proto_database_client.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/internal/shared_proto_database.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "components/leveldb_proto/testing/proto/test_db.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace leveldb_proto {

class SharedProtoDatabaseClientTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_ = base::WrapRefCounted(
        new SharedProtoDatabase("client", temp_dir_.GetPath()));
  }

 protected:
  scoped_refptr<SharedProtoDatabase> db() { return db_; }
  base::ScopedTempDir* temp_dir() { return &temp_dir_; }

  LevelDB* GetLevelDB() const { return db_->GetLevelDBForTesting(); }

  std::unique_ptr<SharedProtoDatabaseClient> GetClient(
      ProtoDbType db_type,
      bool create_if_missing,
      Callbacks::InitStatusCallback callback,
      SharedDBMetadataProto::MigrationStatus expected_migration_status =
          SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED) {
    return db_->GetClientForTesting(
        db_type, create_if_missing,
        base::BindOnce(
            [](SharedDBMetadataProto::MigrationStatus expected_migration_status,
               Callbacks::InitStatusCallback callback, Enums::InitStatus status,
               SharedDBMetadataProto::MigrationStatus migration_status) {
              EXPECT_EQ(expected_migration_status, migration_status);
              std::move(callback).Run(status);
            },
            expected_migration_status, std::move(callback)));
  }

  std::unique_ptr<SharedProtoDatabaseClient> GetClientAndWait(
      ProtoDbType db_type,
      bool create_if_missing,
      Enums::InitStatus* status,
      SharedDBMetadataProto::MigrationStatus expected_migration_status =
          SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED) {
    base::RunLoop loop;
    auto client =
        GetClient(db_type, create_if_missing,
                  base::BindOnce(
                      [](Enums::InitStatus* status_out,
                         base::OnceClosure closure, Enums::InitStatus status) {
                        *status_out = status;
                        std::move(closure).Run();
                      },
                      status, loop.QuitClosure()),
                  expected_migration_status);
    loop.Run();
    return client;
  }

  bool ContainsKeys(const leveldb_proto::KeyVector& db_keys,
                    const leveldb_proto::KeyVector& keys,
                    ProtoDbType db_type) {
    std::set<std::string> key_set(db_keys.begin(), db_keys.end());
    for (auto& key : keys) {
      auto full_key =
          db_type == ProtoDbType::LAST
              ? key
              : SharedProtoDatabaseClient::PrefixForDatabase(db_type) + key;
      if (key_set.find(full_key) == key_set.end())
        return false;
    }
    return true;
  }

  bool ContainsEntries(const leveldb_proto::KeyVector& db_keys,
                       const ValueVector& entries,
                       ProtoDbType db_type) {
    std::set<std::string> entry_id_set;
    for (auto& entry : entries)
      entry_id_set.insert(entry);

    // Entry IDs don't include the full prefix, so we don't look for that here.
    auto prefix = SharedProtoDatabaseClient::PrefixForDatabase(db_type);
    for (auto& key : db_keys) {
      if (entry_id_set.find(prefix + key) == entry_id_set.end())
        return false;
    }
    return true;
  }

  void UpdateEntries(SharedProtoDatabaseClient* client,
                     const leveldb_proto::KeyVector& keys,
                     const leveldb_proto::KeyVector& keys_to_remove,
                     bool expect_success) {
    auto entries =
        std::make_unique<ProtoDatabase<std::string>::KeyEntryVector>();
    for (auto& key : keys) {
      std::string value;
      value = client->prefix_ + key;
      entries->emplace_back(std::make_pair(key, std::move(value)));
    }

    auto entries_to_remove = std::make_unique<leveldb_proto::KeyVector>();
    for (auto& key : keys_to_remove)
      entries_to_remove->push_back(key);

    base::RunLoop update_entries_loop;
    client->UpdateEntries(
        std::move(entries), std::move(entries_to_remove),
        base::BindOnce(
            [](base::OnceClosure signal, bool expect_success, bool success) {
              ASSERT_EQ(success, expect_success);
              std::move(signal).Run();
            },
            update_entries_loop.QuitClosure(), expect_success));
    update_entries_loop.Run();
  }

  void UpdateEntriesWithRemoveFilter(SharedProtoDatabaseClient* client,
                                     const leveldb_proto::KeyVector& keys,
                                     const KeyFilter& delete_key_filter,
                                     bool expect_success) {
    auto entries =
        std::make_unique<ProtoDatabase<std::string>::KeyEntryVector>();
    for (auto& key : keys) {
      std::string value;
      value = client->prefix_ + key;
      entries->emplace_back(std::make_pair(key, std::move(value)));
    }

    base::RunLoop update_entries_loop;
    client->UpdateEntriesWithRemoveFilter(
        std::move(entries), delete_key_filter,
        base::BindOnce(
            [](base::OnceClosure signal, bool expect_success, bool success) {
              ASSERT_EQ(success, expect_success);
              std::move(signal).Run();
            },
            update_entries_loop.QuitClosure(), expect_success));
    update_entries_loop.Run();
  }

  // This fills in for all the LoadEntriesX functions because they all call this
  // one.
  void LoadEntriesWithFilter(SharedProtoDatabaseClient* client,
                             const KeyFilter& key_filter,
                             const leveldb::ReadOptions& options,
                             const std::string& target_prefix,
                             bool expect_success,
                             std::unique_ptr<ValueVector>* entries) {
    base::RunLoop load_entries_loop;
    client->LoadEntriesWithFilter(
        key_filter, options, target_prefix,
        base::BindOnce(
            [](std::unique_ptr<ValueVector>* entries_ptr,
               base::OnceClosure signal, bool expect_success, bool success,
               std::unique_ptr<ValueVector> entries) {
              ASSERT_EQ(success, expect_success);
              entries_ptr->reset(entries.release());
              std::move(signal).Run();
            },
            entries, load_entries_loop.QuitClosure(), expect_success));
    load_entries_loop.Run();
  }

  void LoadKeysAndEntriesInRange(SharedProtoDatabaseClient* client,
                                 const std::string& start,
                                 const std::string& end,
                                 bool expect_success,
                                 std::unique_ptr<KeyValueMap>* entries) {
    base::RunLoop load_entries_in_range_loop;
    client->LoadKeysAndEntriesInRange(
        start, end,
        base::BindOnce(
            [](std::unique_ptr<KeyValueMap>* entries_ptr,
               base::OnceClosure signal, bool expect_success, bool success,
               std::unique_ptr<KeyValueMap> entries) {
              ASSERT_EQ(success, expect_success);
              *entries_ptr = std::move(entries);
              std::move(signal).Run();
            },
            entries, load_entries_in_range_loop.QuitClosure(), expect_success));
    load_entries_in_range_loop.Run();
  }

  void LoadKeys(SharedProtoDatabaseClient* client,
                bool expect_success,
                std::unique_ptr<KeyVector>* keys) {
    base::RunLoop load_keys_loop;
    client->LoadKeys(base::BindOnce(
        [](std::unique_ptr<KeyVector>* keys_ptr, base::OnceClosure signal,
           bool expect_success, bool success, std::unique_ptr<KeyVector> keys) {
          ASSERT_EQ(success, expect_success);
          keys_ptr->reset(keys.release());
          std::move(signal).Run();
        },
        keys, load_keys_loop.QuitClosure(), expect_success));
    load_keys_loop.Run();
  }

  std::unique_ptr<std::string> GetEntry(SharedProtoDatabaseClient* client,
                                        const std::string& key,
                                        bool expect_success) {
    std::unique_ptr<std::string> entry;
    base::RunLoop get_key_loop;
    client->GetEntry(
        key, base::BindOnce(
                 [](std::unique_ptr<std::string>* entry_ptr,
                    base::OnceClosure signal, bool expect_success, bool success,
                    std::unique_ptr<std::string> entry) {
                   ASSERT_EQ(success, expect_success);
                   entry_ptr->reset(entry.release());
                   std::move(signal).Run();
                 },
                 &entry, get_key_loop.QuitClosure(), expect_success));
    get_key_loop.Run();
    return entry;
  }

  void Destroy(SharedProtoDatabaseClient* client, bool expect_success) {
    base::RunLoop destroy_loop;
    client->Destroy(base::BindOnce(
        [](bool expect_success, base::OnceClosure signal, bool success) {
          ASSERT_EQ(success, expect_success);
          std::move(signal).Run();
        },
        expect_success, destroy_loop.QuitClosure()));
    destroy_loop.Run();
  }

  // Sets the obsolete client list to given list, runs clean up tasks and waits
  // for them to complete.
  void DestroyObsoleteClientsAndWait(const ProtoDbType* client_list) {
    SharedProtoDatabaseClient::SetObsoleteClientListForTesting(client_list);
    base::RunLoop wait_loop;
    Callbacks::UpdateCallback wait_callback = base::BindOnce(
        [](base::OnceClosure closure, bool success) {
          EXPECT_TRUE(success);
          std::move(closure).Run();
        },
        wait_loop.QuitClosure());

    SharedProtoDatabaseClient::DestroyObsoleteSharedProtoDatabaseClients(
        std::make_unique<ProtoLevelDBWrapper>(
            db_->database_task_runner_for_testing(), GetLevelDB()),
        std::move(wait_callback));
    wait_loop.Run();
    SharedProtoDatabaseClient::SetObsoleteClientListForTesting(nullptr);
  }

  void UpdateMetadataAsync(
      SharedProtoDatabaseClient* client,
      SharedDBMetadataProto::MigrationStatus migration_status) {
    base::RunLoop wait_loop;
    Callbacks::UpdateCallback wait_callback = base::BindOnce(
        [](base::OnceClosure closure, bool success) {
          EXPECT_TRUE(success);
          std::move(closure).Run();
        },
        wait_loop.QuitClosure());
    client->set_migration_status(migration_status);
    SharedProtoDatabaseClient::UpdateClientMetadataAsync(
        client->parent_db_, client->prefix_, migration_status,
        std::move(wait_callback));
    wait_loop.Run();
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<SharedProtoDatabase> db_;
};

TEST_F(SharedProtoDatabaseClientTest, InitSuccess) {
  auto status = Enums::InitStatus::kError;
  auto client = GetClientAndWait(ProtoDbType::TEST_DATABASE0,
                                 true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);

  client->Init("client", base::BindOnce([](Enums::InitStatus status) {
                 ASSERT_EQ(status, Enums::InitStatus::kOK);
               }));
}

TEST_F(SharedProtoDatabaseClientTest, InitFail) {
  auto status = Enums::InitStatus::kError;
  auto client = GetClientAndWait(ProtoDbType::TEST_DATABASE0,
                                 false /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kInvalidOperation);

  client->Init("client", base::BindOnce([](Enums::InitStatus status) {
                 ASSERT_EQ(status, Enums::InitStatus::kError);
               }));
}

// Ensure that our LevelDB contains the properly prefixed entries and also
// removes prefixed entries correctly.
TEST_F(SharedProtoDatabaseClientTest, UpdateEntriesAppropriatePrefix) {
  auto status = Enums::InitStatus::kError;
  auto client = GetClientAndWait(ProtoDbType::TEST_DATABASE0,
                                 true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);

  KeyVector key_list = {"entry1", "entry2", "entry3"};
  UpdateEntries(client.get(), key_list, leveldb_proto::KeyVector(), true);

  // Make sure those entries are in the LevelDB with the appropriate prefix.
  KeyVector keys;
  LevelDB* db = GetLevelDB();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), 3U);
  ASSERT_TRUE(ContainsKeys(keys, key_list, ProtoDbType::TEST_DATABASE0));

  auto remove_keys = {key_list[0]};
  // Now try to delete one entry.
  UpdateEntries(client.get(), leveldb_proto::KeyVector(), remove_keys, true);

  keys.clear();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), 2U);
  ASSERT_TRUE(ContainsKeys(keys, {key_list[1], key_list[2]},
                           ProtoDbType::TEST_DATABASE0));
}

TEST_F(SharedProtoDatabaseClientTest,
       UpdateEntries_DeletesCorrectClientEntries) {
  auto status = Enums::InitStatus::kError;
  auto client_a = GetClientAndWait(ProtoDbType::TEST_DATABASE0,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);
  auto client_b = GetClientAndWait(ProtoDbType::TEST_DATABASE2,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);

  KeyVector key_list = {"entry1", "entry2", "entry3"};
  UpdateEntries(client_a.get(), key_list, leveldb_proto::KeyVector(), true);
  UpdateEntries(client_b.get(), key_list, leveldb_proto::KeyVector(), true);

  KeyVector keys;
  LevelDB* db = GetLevelDB();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), 6U);
  ASSERT_TRUE(ContainsKeys(keys, key_list, ProtoDbType::TEST_DATABASE0));
  ASSERT_TRUE(ContainsKeys(keys, key_list, ProtoDbType::TEST_DATABASE2));

  // Now delete from client_b and ensure only client A's values exist.
  UpdateEntries(client_b.get(), leveldb_proto::KeyVector(), key_list, true);
  keys.clear();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), 3U);
  ASSERT_TRUE(ContainsKeys(keys, key_list, ProtoDbType::TEST_DATABASE0));
}

TEST_F(SharedProtoDatabaseClientTest,
       UpdateEntriesWithRemoveFilter_DeletesCorrectEntries) {
  auto status = Enums::InitStatus::kError;
  auto client = GetClientAndWait(ProtoDbType::TEST_DATABASE0,
                                 true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);

  KeyVector key_list = {"entry1", "entry2", "testentry3"};
  UpdateEntries(client.get(), key_list, leveldb_proto::KeyVector(), true);

  KeyVector keys;
  LevelDB* db = GetLevelDB();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), 3U);

  UpdateEntriesWithRemoveFilter(client.get(), leveldb_proto::KeyVector(),
                                base::BindRepeating([](const std::string& key) {
                                  return base::StartsWith(
                                      key, "test",
                                      base::CompareCase::INSENSITIVE_ASCII);
                                }),
                                true);

  keys.clear();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), 2U);
  // Make sure "testentry3" was removed.
  ASSERT_TRUE(
      ContainsKeys(keys, {"entry1", "entry2"}, ProtoDbType::TEST_DATABASE0));
}

TEST_F(SharedProtoDatabaseClientTest, LoadEntriesWithFilter) {
  auto status = Enums::InitStatus::kError;
  auto client_a = GetClientAndWait(ProtoDbType::TEST_DATABASE0,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);
  auto client_b = GetClientAndWait(ProtoDbType::TEST_DATABASE2,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);

  KeyVector key_list_a = {"entry123", "entry2123", "testentry3"};
  UpdateEntries(client_a.get(), key_list_a, leveldb_proto::KeyVector(), true);
  KeyVector key_list_b = {"testentry124", "entry2124", "testentry4"};
  UpdateEntries(client_b.get(), key_list_b, leveldb_proto::KeyVector(), true);

  std::unique_ptr<ValueVector> entries;
  LoadEntriesWithFilter(client_a.get(), leveldb_proto::KeyFilter(),
                        leveldb::ReadOptions(), std::string(), true, &entries);
  ASSERT_EQ(entries->size(), 3U);
  ASSERT_TRUE(
      ContainsEntries(key_list_a, *entries, ProtoDbType::TEST_DATABASE0));

  LoadEntriesWithFilter(client_b.get(), leveldb_proto::KeyFilter(),
                        leveldb::ReadOptions(), std::string(), true, &entries);
  ASSERT_EQ(entries->size(), 3U);
  ASSERT_TRUE(
      ContainsEntries(key_list_b, *entries, ProtoDbType::TEST_DATABASE2));

  // Now test the actual filtering functionality.
  LoadEntriesWithFilter(
      client_b.get(), base::BindRepeating([](const std::string& key) {
        // Strips the entries that start with "test" from |key_list_b|.
        return !base::StartsWith(key, "test",
                                 base::CompareCase::INSENSITIVE_ASCII);
      }),
      leveldb::ReadOptions(), std::string(), true, &entries);
  ASSERT_EQ(entries->size(), 1U);
  ASSERT_TRUE(
      ContainsEntries({"entry2124"}, *entries, ProtoDbType::TEST_DATABASE2));
}

TEST_F(SharedProtoDatabaseClientTest, LoadKeysAndEntriesInRange) {
  auto status = Enums::InitStatus::kError;
  auto client_a = GetClientAndWait(ProtoDbType::TEST_DATABASE0,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);
  auto client_b = GetClientAndWait(ProtoDbType::TEST_DATABASE2,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);

  KeyVector key_list_a = {"entry0",           "entry1", "entry2", "entry3",
                          "entry3notinrange", "entry4", "entry5"};
  UpdateEntries(client_a.get(), key_list_a, leveldb_proto::KeyVector(), true);
  KeyVector key_list_b = {"entry2", "entry3", "entry4"};
  UpdateEntries(client_b.get(), key_list_b, leveldb_proto::KeyVector(), true);

  std::unique_ptr<KeyValueMap> keys_and_entries_a;
  LoadKeysAndEntriesInRange(client_a.get(), "entry1", "entry3", true,
                            &keys_and_entries_a);

  std::unique_ptr<KeyValueMap> keys_and_entries_b;
  LoadKeysAndEntriesInRange(client_b.get(), "entry0", "entry1", true,
                            &keys_and_entries_b);

  ValueVector entries;

  for (auto& pair : *keys_and_entries_a) {
    entries.push_back(pair.second);
  }

  ASSERT_EQ(keys_and_entries_a->size(), 3U);
  ASSERT_EQ(keys_and_entries_b->size(), 0U);
  ASSERT_TRUE(ContainsEntries({"entry1", "entry2", "entry3"}, entries,
                              ProtoDbType::TEST_DATABASE0));
}

TEST_F(SharedProtoDatabaseClientTest, LoadKeys) {
  auto status = Enums::InitStatus::kError;
  auto client_a = GetClientAndWait(ProtoDbType::TEST_DATABASE0,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);
  auto client_b = GetClientAndWait(ProtoDbType::TEST_DATABASE2,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);

  KeyVector key_list_a = {"entry123", "entry2123", "testentry3", "testing"};
  UpdateEntries(client_a.get(), key_list_a, leveldb_proto::KeyVector(), true);
  KeyVector key_list_b = {"testentry124", "entry2124", "testentry4"};
  UpdateEntries(client_b.get(), key_list_b, leveldb_proto::KeyVector(), true);

  std::unique_ptr<KeyVector> keys;
  LoadKeys(client_a.get(), true, &keys);
  ASSERT_EQ(keys->size(), 4U);
  ASSERT_TRUE(ContainsKeys(*keys, key_list_a, ProtoDbType::LAST));
  LoadKeys(client_b.get(), true, &keys);
  ASSERT_EQ(keys->size(), 3U);
  ASSERT_TRUE(ContainsKeys(*keys, key_list_b, ProtoDbType::LAST));
}

TEST_F(SharedProtoDatabaseClientTest, GetEntry) {
  auto status = Enums::InitStatus::kError;
  auto client_a = GetClientAndWait(ProtoDbType::TEST_DATABASE0,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);
  auto client_b = GetClientAndWait(ProtoDbType::TEST_DATABASE2,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);

  KeyVector key_list = {"a", "b", "c"};
  // Add the same entries to both because we want to make sure we only get the
  // entries from the proper client.
  UpdateEntries(client_a.get(), key_list, leveldb_proto::KeyVector(), true);
  UpdateEntries(client_b.get(), key_list, leveldb_proto::KeyVector(), true);

  auto a_prefix =
      SharedProtoDatabaseClient::PrefixForDatabase(ProtoDbType::TEST_DATABASE0);
  auto b_prefix =
      SharedProtoDatabaseClient::PrefixForDatabase(ProtoDbType::TEST_DATABASE2);

  for (auto& key : key_list) {
    auto entry = GetEntry(client_a.get(), key, true);
    ASSERT_EQ(*entry, a_prefix + key);
    entry = GetEntry(client_b.get(), key, true);
    ASSERT_EQ(*entry, b_prefix + key);
  }
}

TEST_F(SharedProtoDatabaseClientTest, TestCleanupObsoleteClients) {
  auto status = Enums::InitStatus::kError;
  auto client_a = GetClientAndWait(ProtoDbType::TEST_DATABASE0,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);
  auto client_b = GetClientAndWait(ProtoDbType::TEST_DATABASE1,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);
  auto client_c = GetClientAndWait(ProtoDbType::TEST_DATABASE2,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);

  KeyVector test_keys = {"a", "b", "c"};
  UpdateEntries(client_a.get(), test_keys, leveldb_proto::KeyVector(), true);
  UpdateEntries(client_b.get(), test_keys, leveldb_proto::KeyVector(), true);
  UpdateEntries(client_c.get(), test_keys, leveldb_proto::KeyVector(), true);

  // Check that the original list does not clear any data from test DBs.
  DestroyObsoleteClientsAndWait(nullptr /* client_list */);

  KeyVector keys;
  LevelDB* db = GetLevelDB();
  db->LoadKeys(&keys);
  EXPECT_EQ(keys.size(), test_keys.size() * 3);

  // Mark some DBs obsolete.
  const ProtoDbType kObsoleteList1[] = {ProtoDbType::TEST_DATABASE0,
                                        ProtoDbType::TEST_DATABASE1,
                                        ProtoDbType::LAST};
  DestroyObsoleteClientsAndWait(kObsoleteList1);

  keys.clear();
  db->LoadKeys(&keys);

  EXPECT_EQ(keys.size(), test_keys.size());
  EXPECT_FALSE(ContainsKeys(keys, test_keys, ProtoDbType::TEST_DATABASE0));
  EXPECT_FALSE(ContainsKeys(keys, test_keys, ProtoDbType::TEST_DATABASE1));
  EXPECT_TRUE(ContainsKeys(keys, test_keys, ProtoDbType::TEST_DATABASE2));

  // Make all the DBs obsolete.
  const ProtoDbType kObsoleteList2[] = {
      ProtoDbType::TEST_DATABASE0, ProtoDbType::TEST_DATABASE1,
      ProtoDbType::TEST_DATABASE2, ProtoDbType::LAST};
  DestroyObsoleteClientsAndWait(kObsoleteList2);

  // Nothing should remain.
  keys.clear();
  db->LoadKeys(&keys);
  EXPECT_EQ(keys.size(), 0U);
}

TEST_F(SharedProtoDatabaseClientTest, TestDestroy) {
  auto status = Enums::InitStatus::kError;
  auto client_a = GetClientAndWait(ProtoDbType::TEST_DATABASE0,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);
  auto client_b = GetClientAndWait(ProtoDbType::TEST_DATABASE2,
                                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);

  KeyVector key_list = {"a", "b", "c"};
  // Add the same entries to both because we want to make sure we only destroy
  // the entries from the proper client.
  UpdateEntries(client_a.get(), key_list, leveldb_proto::KeyVector(), true);
  UpdateEntries(client_b.get(), key_list, leveldb_proto::KeyVector(), true);

  // Delete only client A.
  KeyVector keys;
  LevelDB* db = GetLevelDB();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), key_list.size() * 2);
  ASSERT_TRUE(ContainsKeys(keys, key_list, ProtoDbType::TEST_DATABASE0));
  ASSERT_TRUE(ContainsKeys(keys, key_list, ProtoDbType::TEST_DATABASE2));
  Destroy(client_a.get(), true);

  // Make sure only client B remains and delete client B.
  keys.clear();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), key_list.size());
  ASSERT_TRUE(ContainsKeys(keys, key_list, ProtoDbType::TEST_DATABASE2));
  Destroy(client_b.get(), true);

  // Nothing should remain.
  keys.clear();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), 0U);
}

TEST_F(SharedProtoDatabaseClientTest, UpdateClientMetadataAsync) {
  auto status = Enums::InitStatus::kError;
  auto client_a = GetClientAndWait(ProtoDbType::TEST_DATABASE0,
                                   true /* create_if_missing */, &status);
  EXPECT_EQ(status, Enums::InitStatus::kOK);
  EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
            client_a->migration_status());

  auto client_b = GetClientAndWait(ProtoDbType::TEST_DATABASE1,
                                   true /* create_if_missing */, &status);
  EXPECT_EQ(status, Enums::InitStatus::kOK);
  EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
            client_b->migration_status());

  auto client_c = GetClientAndWait(ProtoDbType::TEST_DATABASE2,
                                   true /* create_if_missing */, &status);
  EXPECT_EQ(status, Enums::InitStatus::kOK);
  EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
            client_c->migration_status());

  UpdateMetadataAsync(client_a.get(),
                      SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL);
  UpdateMetadataAsync(
      client_b.get(),
      SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED);

  client_a.reset();
  client_b.reset();
  client_c.reset();

  auto client_d = GetClientAndWait(
      ProtoDbType::TEST_DATABASE0, true /* create_if_missing */, &status,
      SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL);
  EXPECT_EQ(status, Enums::InitStatus::kOK);

  auto client_e = GetClientAndWait(
      ProtoDbType::TEST_DATABASE1, true /* create_if_missing */, &status,
      SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED);
  EXPECT_EQ(status, Enums::InitStatus::kOK);

  auto client_f = GetClientAndWait(ProtoDbType::TEST_DATABASE2,
                                   true /* create_if_missing */, &status);
  EXPECT_EQ(status, Enums::InitStatus::kOK);

  UpdateMetadataAsync(client_d.get(),
                      SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED);
  UpdateMetadataAsync(client_e.get(),
                      SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED);
}

}  // namespace leveldb_proto
