// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/debug/leak_annotations.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "components/safe_browsing/db/v4_database.h"
#include "components/safe_browsing/db/v4_store.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class FakeV4Store : public V4Store {
 public:
  FakeV4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
              const base::FilePath& store_path,
              const bool hash_prefix_matches)
      : V4Store(
            task_runner,
            base::FilePath(store_path.value() + FILE_PATH_LITERAL(".store"))),
        hash_prefix_should_match_(hash_prefix_matches) {}

  HashPrefix GetMatchingHashPrefix(const FullHash& full_hash) override {
    return hash_prefix_should_match_ ? full_hash : HashPrefix();
  }

  bool HasValidData() const override { return true; }

  void set_hash_prefix_matches(bool hash_prefix_matches) {
    hash_prefix_should_match_ = hash_prefix_matches;
  }

 private:
  bool hash_prefix_should_match_;
};

// This factory creates a "fake" store. It allows the caller to specify whether
// the store has a hash prefix matching a full hash. This is used to test the
// |GetStoresMatchingFullHash()| method in |V4Database|.
class FakeV4StoreFactory : public V4StoreFactory {
 public:
  explicit FakeV4StoreFactory(bool hash_prefix_matches)
      : hash_prefix_should_match_(hash_prefix_matches) {}

  std::unique_ptr<V4Store> CreateV4Store(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& store_path) override {
    return std::make_unique<FakeV4Store>(task_runner, store_path,
                                         hash_prefix_should_match_);
  }

 private:
  const bool hash_prefix_should_match_;
};

class V4DatabaseTest : public PlatformTest {
 public:
  V4DatabaseTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        linux_malware_id_(LINUX_PLATFORM, URL, MALWARE_THREAT),
        win_malware_id_(WINDOWS_PLATFORM, URL, MALWARE_THREAT) {}

  void SetUp() override {
    PlatformTest::SetUp();

    // Setup a database in a temporary directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    database_dirname_ = temp_dir_.GetPath().AppendASCII("V4DatabaseTest");

    created_but_not_called_back_ = false;
    created_and_called_back_ = false;
    verify_checksum_called_back_ = false;

    callback_db_updated_ =
        base::Bind(&V4DatabaseTest::DatabaseUpdated, base::Unretained(this));

    callback_db_ready_ = base::Bind(
        &V4DatabaseTest::NewDatabaseReadyWithExpectedStorePathsAndIds,
        base::Unretained(this));

    SetupInfoMapAndExpectedState();
  }

  void TearDown() override {
    V4Database::RegisterStoreFactoryForTest(nullptr);
    PlatformTest::TearDown();
  }

  void RegisterFactory(bool hash_prefix_matches = true) {
    V4Database::RegisterStoreFactoryForTest(
        std::make_unique<FakeV4StoreFactory>(hash_prefix_matches));
  }

  void SetupInfoMapAndExpectedState() {
    list_infos_.emplace_back(true, "win_url_malware", win_malware_id_,
                             SB_THREAT_TYPE_URL_MALWARE);
    expected_identifiers_.push_back(win_malware_id_);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("win_url_malware.store"));

    list_infos_.emplace_back(true, "linux_url_malware", linux_malware_id_,
                             SB_THREAT_TYPE_URL_MALWARE);
    expected_identifiers_.push_back(linux_malware_id_);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("linux_url_malware.store"));
  }

  void DatabaseUpdated() {}

  void NewDatabaseReadyWithExpectedStorePathsAndIds(
      std::unique_ptr<V4Database> v4_database) {
    ASSERT_TRUE(v4_database);
    ASSERT_TRUE(v4_database->store_map_);

    // The following check ensures that the callback was called asynchronously.
    EXPECT_TRUE(created_but_not_called_back_);

    ASSERT_EQ(expected_store_paths_.size(), v4_database->store_map_->size());
    ASSERT_EQ(expected_identifiers_.size(), v4_database->store_map_->size());
    for (size_t i = 0; i < expected_identifiers_.size(); i++) {
      const auto& expected_identifier = expected_identifiers_[i];
      const auto& store = (*v4_database->store_map_)[expected_identifier];
      ASSERT_TRUE(store);
      const auto& expected_store_path = expected_store_paths_[i];
      EXPECT_EQ(expected_store_path, store->store_path());
    }

    EXPECT_FALSE(created_and_called_back_);
    created_and_called_back_ = true;

    v4_database_ = std::move(v4_database);
  }

  std::unique_ptr<ParsedServerResponse> CreateFakeServerResponse(
      StoreStateMap store_state_map,
      bool use_valid_response_type) {
    auto parsed_server_response = std::make_unique<ParsedServerResponse>();
    for (const auto& store_state_iter : store_state_map) {
      ListIdentifier identifier = store_state_iter.first;
      auto lur = std::make_unique<ListUpdateResponse>();
      lur->set_platform_type(identifier.platform_type());
      lur->set_threat_entry_type(identifier.threat_entry_type());
      lur->set_threat_type(identifier.threat_type());
      lur->set_new_client_state(store_state_iter.second);
      if (use_valid_response_type) {
        lur->set_response_type(ListUpdateResponse::FULL_UPDATE);
      } else {
        lur->set_response_type(ListUpdateResponse::RESPONSE_TYPE_UNSPECIFIED);
      }
      parsed_server_response->push_back(std::move(lur));
    }
    return parsed_server_response;
  }

  void VerifyExpectedStoresState(bool expect_new_stores) {
    const StoreMap* new_store_map = v4_database_->store_map_.get();
    std::unique_ptr<StoreStateMap> new_store_state_map =
        v4_database_->GetStoreStateMap();
    EXPECT_EQ(expected_store_state_map_.size(), new_store_map->size());
    EXPECT_EQ(expected_store_state_map_.size(), new_store_state_map->size());
    for (const auto& expected_iter : expected_store_state_map_) {
      const ListIdentifier& identifier = expected_iter.first;
      const std::string& state = expected_iter.second;
      ASSERT_EQ(1u, new_store_map->count(identifier));
      ASSERT_EQ(1u, new_store_state_map->count(identifier));

      // Verify the expected state in the store map and the state map.
      EXPECT_EQ(state, new_store_map->at(identifier)->state());
      EXPECT_EQ(state, new_store_state_map->at(identifier));

      if (expect_new_stores) {
        // Verify that a new store was created.
        EXPECT_NE(old_stores_map_.at(identifier),
                  new_store_map->at(identifier).get());
      } else {
        // Verify that NO new store was created.
        EXPECT_EQ(old_stores_map_.at(identifier),
                  new_store_map->at(identifier).get());
      }
    }
  }

  void VerifyChecksumCallback(const std::vector<ListIdentifier>& stores) {
    EXPECT_FALSE(verify_checksum_called_back_);
    verify_checksum_called_back_ = true;
  }

  void WaitForTasksOnTaskRunner() {
    task_runner_->RunPendingTasks();
    base::RunLoop().RunUntilIdle();
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<V4Database> v4_database_;
  base::FilePath database_dirname_;
  base::ScopedTempDir temp_dir_;
  content::BrowserTaskEnvironment task_environment_;
  bool created_but_not_called_back_;
  bool created_and_called_back_;
  bool verify_checksum_called_back_;
  ListInfos list_infos_;
  std::vector<ListIdentifier> expected_identifiers_;
  std::vector<base::FilePath> expected_store_paths_;
  DatabaseUpdatedCallback callback_db_updated_;
  NewDatabaseReadyCallback callback_db_ready_;
  StoreStateMap expected_store_state_map_;
  std::unordered_map<ListIdentifier, V4Store*> old_stores_map_;
  const ListIdentifier linux_malware_id_, win_malware_id_;
};

// Test to set up the database with fake stores.
TEST_F(V4DatabaseTest, TestSetupDatabaseWithFakeStores) {
  RegisterFactory();

  V4Database::Create(task_runner_, database_dirname_, list_infos_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  WaitForTasksOnTaskRunner();
  EXPECT_EQ(true, created_and_called_back_);
}

// Test to check database updates as expected.
TEST_F(V4DatabaseTest, TestApplyUpdateWithNewStates) {
  RegisterFactory();

  V4Database::Create(task_runner_, database_dirname_, list_infos_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  WaitForTasksOnTaskRunner();

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(v4_database_);
  const StoreMap* db_stores = v4_database_->store_map_.get();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    V4Store* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] = store->state() + "_fake";
    old_stores_map_[store_iter.first] = store;
  }

  v4_database_->ApplyUpdate(
      CreateFakeServerResponse(expected_store_state_map_, true),
      callback_db_updated_);

  // Wait for the ApplyUpdate callback to get called.
  WaitForTasksOnTaskRunner();

  VerifyExpectedStoresState(true);

  // Wait for the old stores to get destroyed on task runner.
  WaitForTasksOnTaskRunner();
}

// Test to ensure no state updates leads to no store updates.
TEST_F(V4DatabaseTest, TestApplyUpdateWithNoNewState) {
  RegisterFactory();

  V4Database::Create(task_runner_, database_dirname_, list_infos_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  WaitForTasksOnTaskRunner();

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(v4_database_);
  const StoreMap* db_stores = v4_database_->store_map_.get();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    V4Store* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] = store->state();
    old_stores_map_[store_iter.first] = store;
  }

  v4_database_->ApplyUpdate(
      CreateFakeServerResponse(expected_store_state_map_, true),
      callback_db_updated_);

  WaitForTasksOnTaskRunner();

  VerifyExpectedStoresState(false);
}

// Test to ensure no updates leads to no store updates.
TEST_F(V4DatabaseTest, TestApplyUpdateWithEmptyUpdate) {
  RegisterFactory();

  V4Database::Create(task_runner_, database_dirname_, list_infos_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  WaitForTasksOnTaskRunner();

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(v4_database_);
  const StoreMap* db_stores = v4_database_->store_map_.get();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    V4Store* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] = store->state();
    old_stores_map_[store_iter.first] = store;
  }

  auto parsed_server_response = std::make_unique<ParsedServerResponse>();
  v4_database_->ApplyUpdate(std::move(parsed_server_response),
                            callback_db_updated_);

  WaitForTasksOnTaskRunner();

  VerifyExpectedStoresState(false);
}

// Test to ensure invalid update leads to no store changes.
TEST_F(V4DatabaseTest, TestApplyUpdateWithInvalidUpdate) {
  RegisterFactory();

  V4Database::Create(task_runner_, database_dirname_, list_infos_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  WaitForTasksOnTaskRunner();

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(v4_database_);
  const StoreMap* db_stores = v4_database_->store_map_.get();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    V4Store* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] = store->state();
    old_stores_map_[store_iter.first] = store;
  }

  v4_database_->ApplyUpdate(
      CreateFakeServerResponse(expected_store_state_map_, false),
      callback_db_updated_);
  WaitForTasksOnTaskRunner();

  VerifyExpectedStoresState(false);
}

// Test to ensure the case that all stores match a given full hash.
TEST_F(V4DatabaseTest, TestAllStoresMatchFullHash) {
  bool hash_prefix_matches = true;
  RegisterFactory(hash_prefix_matches);

  V4Database::Create(task_runner_, database_dirname_, list_infos_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  WaitForTasksOnTaskRunner();
  EXPECT_EQ(true, created_and_called_back_);

  StoresToCheck stores_to_check({linux_malware_id_, win_malware_id_});
  StoreAndHashPrefixes store_and_hash_prefixes;
  v4_database_->GetStoresMatchingFullHash("anything", stores_to_check,
                                          &store_and_hash_prefixes);
  EXPECT_EQ(2u, store_and_hash_prefixes.size());
  StoresToCheck stores_found;
  for (const auto& it : store_and_hash_prefixes) {
    stores_found.insert(it.list_id);
  }
  EXPECT_EQ(stores_to_check, stores_found);
}

// Test to ensure the case that no stores match a given full hash.
TEST_F(V4DatabaseTest, TestNoStoreMatchesFullHash) {
  bool hash_prefix_matches = false;
  RegisterFactory(hash_prefix_matches);

  V4Database::Create(task_runner_, database_dirname_, list_infos_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  WaitForTasksOnTaskRunner();
  EXPECT_EQ(true, created_and_called_back_);

  StoreAndHashPrefixes store_and_hash_prefixes;
  v4_database_->GetStoresMatchingFullHash(
      "anything", StoresToCheck({linux_malware_id_, win_malware_id_}),
      &store_and_hash_prefixes);
  EXPECT_TRUE(store_and_hash_prefixes.empty());
}

// Test to ensure the case that some stores match a given full hash.
TEST_F(V4DatabaseTest, TestSomeStoresMatchFullHash) {
  // Setup stores to not match the full hash.
  bool hash_prefix_matches = false;
  RegisterFactory(hash_prefix_matches);

  V4Database::Create(task_runner_, database_dirname_, list_infos_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  WaitForTasksOnTaskRunner();
  EXPECT_EQ(true, created_and_called_back_);

  // Set the store corresponding to linux_malware_id_ to match the full hash.
  FakeV4Store* store = static_cast<FakeV4Store*>(
      v4_database_->store_map_->at(win_malware_id_).get());
  store->set_hash_prefix_matches(true);

  StoreAndHashPrefixes store_and_hash_prefixes;
  v4_database_->GetStoresMatchingFullHash(
      "anything", StoresToCheck({linux_malware_id_, win_malware_id_}),
      &store_and_hash_prefixes);
  EXPECT_EQ(1u, store_and_hash_prefixes.size());
  EXPECT_EQ(store_and_hash_prefixes.begin()->list_id, win_malware_id_);
  EXPECT_FALSE(store_and_hash_prefixes.begin()->hash_prefix.empty());
}

// Test to ensure the case that only some stores are reported to match a given
// full hash because of StoresToCheck.
TEST_F(V4DatabaseTest, TestSomeStoresMatchFullHashBecauseOfStoresToMatch) {
  // Setup all stores to match the full hash.
  bool hash_prefix_matches = true;
  RegisterFactory(hash_prefix_matches);

  V4Database::Create(task_runner_, database_dirname_, list_infos_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  WaitForTasksOnTaskRunner();
  EXPECT_EQ(true, created_and_called_back_);

  // Don't add win_malware_id_ to the StoresToCheck.
  StoreAndHashPrefixes store_and_hash_prefixes;
  v4_database_->GetStoresMatchingFullHash(
      "anything", StoresToCheck({linux_malware_id_}), &store_and_hash_prefixes);
  EXPECT_EQ(1u, store_and_hash_prefixes.size());
  EXPECT_EQ(store_and_hash_prefixes.begin()->list_id, linux_malware_id_);
  EXPECT_FALSE(store_and_hash_prefixes.begin()->hash_prefix.empty());
}

TEST_F(V4DatabaseTest, VerifyChecksumCalledAsync) {
  bool hash_prefix_matches = true;
  RegisterFactory(hash_prefix_matches);

  V4Database::Create(task_runner_, database_dirname_, list_infos_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  WaitForTasksOnTaskRunner();
  EXPECT_EQ(true, created_and_called_back_);

  // verify_checksum_called_back_ set to false in the constructor.
  EXPECT_FALSE(verify_checksum_called_back_);
  // Now call VerifyChecksum and pass the callback that sets
  // verify_checksum_called_back_ to true.
  v4_database_->VerifyChecksum(base::Bind(
      &V4DatabaseTest::VerifyChecksumCallback, base::Unretained(this)));
  // verify_checksum_called_back_ should still be false since the checksum
  // verification is async.
  EXPECT_FALSE(verify_checksum_called_back_);
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(verify_checksum_called_back_);
}

TEST_F(V4DatabaseTest, VerifyChecksumCancelled) {
  bool hash_prefix_matches = true;
  RegisterFactory(hash_prefix_matches);

  V4Database::Create(task_runner_, database_dirname_, list_infos_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  WaitForTasksOnTaskRunner();
  EXPECT_EQ(true, created_and_called_back_);

  EXPECT_FALSE(verify_checksum_called_back_);
  v4_database_->VerifyChecksum(base::Bind(
      &V4DatabaseTest::VerifyChecksumCallback, base::Unretained(this)));
  EXPECT_FALSE(verify_checksum_called_back_);
  // Destroy database.
  V4Database::Destroy(std::move(v4_database_));
  WaitForTasksOnTaskRunner();
  // Callback should not be called since database is destroyed.
  EXPECT_FALSE(verify_checksum_called_back_);
}

// Test that we can properly check for unsupported stores
TEST_F(V4DatabaseTest, TestStoresAvailable) {
  bool hash_prefix_matches = false;
  RegisterFactory(hash_prefix_matches);

  V4Database::Create(task_runner_, database_dirname_, list_infos_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  WaitForTasksOnTaskRunner();
  EXPECT_EQ(true, created_and_called_back_);

  // Doesn't exist in out list
  const ListIdentifier bogus_id(LINUX_PLATFORM, CHROME_EXTENSION,
                                CSD_WHITELIST);

  EXPECT_TRUE(v4_database_->AreAllStoresAvailable(
      StoresToCheck({linux_malware_id_, win_malware_id_})));
  EXPECT_TRUE(v4_database_->AreAnyStoresAvailable(
      StoresToCheck({linux_malware_id_, win_malware_id_})));

  EXPECT_TRUE(
      v4_database_->AreAllStoresAvailable(StoresToCheck({linux_malware_id_})));
  EXPECT_TRUE(
      v4_database_->AreAnyStoresAvailable(StoresToCheck({linux_malware_id_})));

  EXPECT_FALSE(v4_database_->AreAllStoresAvailable(
      StoresToCheck({linux_malware_id_, bogus_id})));
  EXPECT_TRUE(v4_database_->AreAnyStoresAvailable(
      StoresToCheck({linux_malware_id_, bogus_id})));

  EXPECT_FALSE(v4_database_->AreAllStoresAvailable(StoresToCheck({bogus_id})));
}

// Test to ensure that the callback to the database is dropped when the database
// gets destroyed. See http://crbug.com/683147#c5 for more details.
TEST_F(V4DatabaseTest, UsingWeakPtrDropsCallback) {
  RegisterFactory();

  // Step 1: Create the database.
  V4Database::Create(task_runner_, database_dirname_, list_infos_,
                     callback_db_ready_);
  created_but_not_called_back_ = true;
  WaitForTasksOnTaskRunner();

  // Step 2: Try to update the database. This posts V4Store::ApplyUpdate on the
  // task runner.
  auto parsed_server_response = std::make_unique<ParsedServerResponse>();
  auto lur = std::make_unique<ListUpdateResponse>();
  lur->set_platform_type(linux_malware_id_.platform_type());
  lur->set_threat_entry_type(linux_malware_id_.threat_entry_type());
  lur->set_threat_type(linux_malware_id_.threat_type());
  lur->set_new_client_state("new_state");
  lur->set_response_type(ListUpdateResponse::FULL_UPDATE);
  parsed_server_response->push_back(std::move(lur));

  // We pass |null_callback| as the second argument to |ApplyUpdate| since we
  // expect it to not get called. This callback method is called from
  // V4Database::UpdatedStoreReady but we expect that call to get dropped.
  base::Callback<void()> null_callback;
  v4_database_->ApplyUpdate(std::move(parsed_server_response), null_callback);

  // Step 3: Before V4Store::ApplyUpdate gets executed on the task runner,
  // destroy the database. This posts ~V4Database() on the task runner.
  V4Database::Destroy(std::move(v4_database_));

  // Step 4: Wait for the task runner to go to completion. The test should
  // finish to completion and the |null_callback| should not get called.
  WaitForTasksOnTaskRunner();
}

}  // namespace safe_browsing
