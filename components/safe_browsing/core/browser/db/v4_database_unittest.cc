// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v4_database.h"

#include <unordered_map>
#include <utility>

#include "base/debug/leak_annotations.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_simple_task_runner.h"
#include "components/safe_browsing/core/browser/db/v4_store.h"
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

  HashPrefixStr GetMatchingHashPrefix(const FullHashStr& full_hash) override {
    return hash_prefix_should_match_ ? full_hash : HashPrefixStr();
  }

  bool HasValidData() override { return true; }

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

  V4StorePtr CreateV4Store(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& store_path) override {
    return V4StorePtr(
        new FakeV4Store(task_runner, store_path, hash_prefix_should_match_),
        V4StoreDeleter(task_runner));
  }

 private:
  const bool hash_prefix_should_match_;
};

class V4DatabaseTest : public PlatformTest {
 public:
  V4DatabaseTest()
      : v4_database_(std::unique_ptr<V4Database, base::OnTaskRunnerDeleter>(
            nullptr,
            base::OnTaskRunnerDeleter(nullptr))),
        linux_malware_id_(LINUX_PLATFORM, URL, MALWARE_THREAT),
        win_malware_id_(WINDOWS_PLATFORM, URL, MALWARE_THREAT) {}

  void SetUp() override {
    PlatformTest::SetUp();

    // Setup a database in a temporary directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    database_dirname_ = temp_dir_.GetPath().AppendASCII("V4DatabaseTest");

    SetupInfoMapAndExpectedState();
  }

  void TearDown() override {
    V4Database::RegisterStoreFactoryForTest(nullptr);
    v4_database_.reset();
    PlatformTest::TearDown();
  }

  void RegisterFactory(bool hash_prefix_matches = true) {
    V4Database::RegisterStoreFactoryForTest(
        std::make_unique<FakeV4StoreFactory>(hash_prefix_matches));
  }

  void SetupInfoMapAndExpectedState() {
    list_infos_.emplace_back(true, "win_url_malware", win_malware_id_,
                             SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
    expected_identifiers_.push_back(win_malware_id_);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("win_url_malware.store"));

    list_infos_.emplace_back(true, "linux_url_malware", linux_malware_id_,
                             SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
    expected_identifiers_.push_back(linux_malware_id_);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("linux_url_malware.store"));
  }

  void NewDatabaseReadyWithExpectedStorePathsAndIds(
      base::OnceClosure callback,
      std::unique_ptr<V4Database, base::OnTaskRunnerDeleter> v4_database) {
    ASSERT_TRUE(v4_database);
    ASSERT_TRUE(v4_database->store_map_);

    ASSERT_EQ(expected_store_paths_.size(), v4_database->store_map_->size());
    ASSERT_EQ(expected_identifiers_.size(), v4_database->store_map_->size());
    for (size_t i = 0; i < expected_identifiers_.size(); i++) {
      const auto& expected_identifier = expected_identifiers_[i];
      const auto& store = v4_database->store_map_->at(expected_identifier);
      ASSERT_TRUE(store);
      const auto& expected_store_path = expected_store_paths_[i];
      EXPECT_EQ(expected_store_path, store->store_path());
    }

    v4_database_ = std::move(v4_database);
    std::move(callback).Run();
  }

  scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() {
    return base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::MayBlock()}, base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  }

  void WaitForV4DatabaseReady(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner,
      std::vector<scoped_refptr<base::TestSimpleTaskRunner>>
          simple_task_runners_to_wait_for) {
    base::RunLoop created_and_called_back_waiter;
    V4Database::Create(
        db_task_runner, database_dirname_, list_infos_,
        base::BindOnce(
            &V4DatabaseTest::NewDatabaseReadyWithExpectedStorePathsAndIds,
            base::Unretained(this),
            created_and_called_back_waiter.QuitClosure()));
    // `created_and_called_back_waiter` should not be ready because it should be
    // called asynchronously.
    EXPECT_FALSE(created_and_called_back_waiter.AnyQuitCalled());
    // TestSimpleTaskRunner won't run any tasks (regardless of how long the test
    // waits) unless TestSimpleTaskRunner::RunPendingTasks() is called.
    for (scoped_refptr<base::TestSimpleTaskRunner> simple_task_runner :
         simple_task_runners_to_wait_for) {
      simple_task_runner->RunPendingTasks();
    }
    created_and_called_back_waiter.Run();
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

  std::unique_ptr<V4Database, base::OnTaskRunnerDeleter> v4_database_;
  base::FilePath database_dirname_;
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  ListInfos list_infos_;
  std::vector<ListIdentifier> expected_identifiers_;
  std::vector<base::FilePath> expected_store_paths_;
  StoreStateMap expected_store_state_map_;
  std::unordered_map<ListIdentifier, raw_ptr<V4Store, CtnExperimental>>
      old_stores_map_;
  const ListIdentifier linux_malware_id_, win_malware_id_;
};

// Test to set up the database with fake stores.
TEST_F(V4DatabaseTest, TestSetupDatabaseWithFakeStores) {
  RegisterFactory();
  WaitForV4DatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // Test succeeds if it does not time out.
}

// Test to check database updates as expected.
TEST_F(V4DatabaseTest, TestApplyUpdateWithNewStates) {
  RegisterFactory();

  WaitForV4DatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(v4_database_);
  const StoreMap* db_stores = v4_database_->store_map_.get();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    V4Store* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] = store->state() + "_fake";
    old_stores_map_[store_iter.first] = store;
  }

  base::RunLoop callback_db_updated_run_loop;
  v4_database_->ApplyUpdate(
      CreateFakeServerResponse(expected_store_state_map_, true),
      callback_db_updated_run_loop.QuitClosure());

  // Wait for the ApplyUpdate callback to get called.
  callback_db_updated_run_loop.Run();

  VerifyExpectedStoresState(true);
}

// Test to ensure no state updates leads to no store updates.
TEST_F(V4DatabaseTest, TestApplyUpdateWithNoNewState) {
  RegisterFactory();

  WaitForV4DatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(v4_database_);
  const StoreMap* db_stores = v4_database_->store_map_.get();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    V4Store* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] = store->state();
    old_stores_map_[store_iter.first] = store;
  }

  base::RunLoop callback_db_updated_run_loop;
  v4_database_->ApplyUpdate(
      CreateFakeServerResponse(expected_store_state_map_, true),
      callback_db_updated_run_loop.QuitClosure());

  callback_db_updated_run_loop.Run();

  VerifyExpectedStoresState(false);
}

// Test to ensure no updates leads to no store updates.
TEST_F(V4DatabaseTest, TestApplyUpdateWithEmptyUpdate) {
  RegisterFactory();

  WaitForV4DatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(v4_database_);
  const StoreMap* db_stores = v4_database_->store_map_.get();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    V4Store* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] = store->state();
    old_stores_map_[store_iter.first] = store;
  }

  base::RunLoop callback_db_updated_run_loop;
  auto parsed_server_response = std::make_unique<ParsedServerResponse>();
  v4_database_->ApplyUpdate(std::move(parsed_server_response),
                            callback_db_updated_run_loop.QuitClosure());

  callback_db_updated_run_loop.Run();

  VerifyExpectedStoresState(false);
}

// Test to ensure invalid update leads to no store changes.
TEST_F(V4DatabaseTest, TestApplyUpdateWithInvalidUpdate) {
  RegisterFactory();

  WaitForV4DatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(v4_database_);
  const StoreMap* db_stores = v4_database_->store_map_.get();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    V4Store* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] = store->state();
    old_stores_map_[store_iter.first] = store;
  }

  base::RunLoop callback_db_updated_run_loop;
  v4_database_->ApplyUpdate(
      CreateFakeServerResponse(expected_store_state_map_, false),
      callback_db_updated_run_loop.QuitClosure());
  callback_db_updated_run_loop.Run();

  VerifyExpectedStoresState(false);
}

// Test to ensure the case that all stores match a given full hash.
TEST_F(V4DatabaseTest, TestAllStoresMatchFullHash) {
  bool hash_prefix_matches = true;
  RegisterFactory(hash_prefix_matches);

  WaitForV4DatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  StoresToCheck stores_to_check({linux_malware_id_, win_malware_id_});
  base::test::TestFuture<FullHashToStoreAndHashPrefixesMap> results;
  v4_database_->GetStoresMatchingFullHash({"anything"}, stores_to_check,
                                          results.GetCallback());
  FullHashToStoreAndHashPrefixesMap map = results.Get();
  StoreAndHashPrefixes store_and_hash_prefixes = map["anything"];
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

  WaitForV4DatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  base::test::TestFuture<FullHashToStoreAndHashPrefixesMap> results;
  v4_database_->GetStoresMatchingFullHash(
      {"anything"}, StoresToCheck({linux_malware_id_, win_malware_id_}),
      results.GetCallback());
  FullHashToStoreAndHashPrefixesMap map = results.Get();
  StoreAndHashPrefixes store_and_hash_prefixes = map["anything"];
  EXPECT_TRUE(store_and_hash_prefixes.empty());
}

// Test to ensure the case that some stores match a given full hash.
TEST_F(V4DatabaseTest, TestSomeStoresMatchFullHash) {
  // Setup stores to not match the full hash.
  bool hash_prefix_matches = false;
  RegisterFactory(hash_prefix_matches);

  WaitForV4DatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // Set the store corresponding to linux_malware_id_ to match the full hash.
  FakeV4Store* store = static_cast<FakeV4Store*>(
      v4_database_->store_map_->at(win_malware_id_).get());
  store->set_hash_prefix_matches(true);

  base::test::TestFuture<FullHashToStoreAndHashPrefixesMap> results;
  v4_database_->GetStoresMatchingFullHash(
      {"anything"}, StoresToCheck({linux_malware_id_, win_malware_id_}),
      results.GetCallback());
  FullHashToStoreAndHashPrefixesMap map = results.Get();
  StoreAndHashPrefixes store_and_hash_prefixes = map["anything"];
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

  WaitForV4DatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // Don't add win_malware_id_ to the StoresToCheck.
  base::test::TestFuture<FullHashToStoreAndHashPrefixesMap> results;
  v4_database_->GetStoresMatchingFullHash(
      {"anything"}, StoresToCheck({linux_malware_id_}), results.GetCallback());
  FullHashToStoreAndHashPrefixesMap map = results.Get();
  StoreAndHashPrefixes store_and_hash_prefixes = map["anything"];
  EXPECT_EQ(1u, store_and_hash_prefixes.size());
  EXPECT_EQ(store_and_hash_prefixes.begin()->list_id, linux_malware_id_);
  EXPECT_FALSE(store_and_hash_prefixes.begin()->hash_prefix.empty());
}

TEST_F(V4DatabaseTest, VerifyChecksumCalledAsync) {
  bool hash_prefix_matches = true;
  RegisterFactory(hash_prefix_matches);

  WaitForV4DatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  base::test::TestFuture<const std::vector<ListIdentifier>&>
      verify_checksum_future;
  v4_database_->VerifyChecksum(verify_checksum_future.GetCallback());
  // `verify_checksum_future` should not be ready because callback is called
  // asynchronously.
  EXPECT_FALSE(verify_checksum_future.IsReady());
  EXPECT_TRUE(verify_checksum_future.Wait());
}

TEST_F(V4DatabaseTest, VerifyChecksumCancelled) {
  bool hash_prefix_matches = true;
  RegisterFactory(hash_prefix_matches);

  scoped_refptr<base::TestSimpleTaskRunner> db_task_runner =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();
  WaitForV4DatabaseReady(db_task_runner,
                         /*simple_task_runners_to_wait_for=*/{db_task_runner});

  base::test::TestFuture<const std::vector<ListIdentifier>&>
      verify_checksum_future;
  v4_database_->VerifyChecksum(verify_checksum_future.GetCallback());
  EXPECT_FALSE(verify_checksum_future.IsReady());

  // Post task to destroy V4Database on db thread.
  v4_database_.reset();

  // Simulate V4Database::~V4Database being called on the db thread prior to
  // V4Database::OnChecksumVerified() being called on UI thread.
  db_task_runner->RunPendingTasks();
  base::RunLoop().RunUntilIdle();

  // Callback should not be called since database is destroyed.
  EXPECT_FALSE(verify_checksum_future.IsReady());
}

// Test that we can properly check for unsupported stores
TEST_F(V4DatabaseTest, TestStoresAvailable) {
  bool hash_prefix_matches = false;
  RegisterFactory(hash_prefix_matches);

  WaitForV4DatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // Doesn't exist in out list
  const ListIdentifier bogus_id(LINUX_PLATFORM, CHROME_EXTENSION,
                                CSD_ALLOWLIST);

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
  scoped_refptr<base::TestSimpleTaskRunner> db_task_runner =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();

  RegisterFactory();

  // Step 1: Create the database.
  WaitForV4DatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{db_task_runner});

  // Step 2: Try to update the database. This posts V4Store::ApplyUpdate() on
  // the `db_task_runner`.
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
  v4_database_->ApplyUpdate(std::move(parsed_server_response),
                            base::NullCallback());

  // Step 3: Post task to destroy V4Database on db thread.
  v4_database_.reset();

  // Step 4: Simulate V4Database::~V4Database() being called on db thread prior
  // to V4Database::UpdatedStoreReady() being called on UI thread.
  // V4Database::UpdatedStoreReady() is posted to the UI thread from
  // V4Store::ApplyUpdate().
  db_task_runner->RunPendingTasks();
  base::RunLoop().RunUntilIdle();
}

}  // namespace safe_browsing
