// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/sb_database.h"

#include <optional>
#include <unordered_map>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_simple_task_runner.h"
#include "components/safe_browsing/core/browser/db/v4_store.h"
#include "components/safe_browsing/core/browser/db/v4_store.pb.h"
#include "components/safe_browsing/core/browser/db/v5_store.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/proto/v5_store.pb.h"
#include "crypto/hash.h"
#include "testing/platform_test.h"

namespace safe_browsing {

// TODO(crbug.com/362791941): Handle references to v4.
class FakeV4Store : public V4Store {
 public:
  FakeV4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
              const base::FilePath& store_path,
              PrefixSize v5_prefix_size,
              const bool hash_prefix_matches)
      : V4Store(task_runner,
                store_path,
                v5_prefix_size,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false),
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
// |GetStoresMatchingFullHash()| method in |SBDatabase|.
class FakeV4StoreFactory : public V4StoreFactory {
 public:
  explicit FakeV4StoreFactory(bool hash_prefix_matches)
      : hash_prefix_should_match_(hash_prefix_matches) {}

  V4StorePtr CreateV4Store(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& store_path,
      PrefixSize v5_prefix_size,
      bool is_eligible_for_migration,
      bool is_extensions_blocklist) override {
    return V4StorePtr(new FakeV4Store(task_runner, store_path, v5_prefix_size,
                                      hash_prefix_should_match_),
                      SBStoreDeleter(task_runner));
  }

 private:
  const bool hash_prefix_should_match_;
};

class FakeV5Store : public V5Store {
 public:
  FakeV5Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
              const base::FilePath& store_path,
              PrefixSize prefix_size,
              const base::FilePath& v4_store_path,
              bool hash_prefix_matches)
      : V5Store(task_runner,
                store_path,
                prefix_size,
                v4_store_path,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false),
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

class FakeV5StoreFactory : public V5StoreFactory {
 public:
  explicit FakeV5StoreFactory(bool hash_prefix_matches)
      : hash_prefix_should_match_(hash_prefix_matches) {}

  V5StorePtr CreateV5Store(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& store_path,
      PrefixSize prefix_size,
      const base::FilePath& v4_store_path,
      bool is_eligible_for_v4_to_v5_disk_migration,
      bool is_extensions_blocklist) override {
    return V5StorePtr(new FakeV5Store(task_runner, store_path, prefix_size,
                                      v4_store_path, hash_prefix_should_match_),
                      SBStoreDeleter(task_runner));
  }

 private:
  const bool hash_prefix_should_match_;
};

class SBDatabaseTest : public PlatformTest {
 public:
  SBDatabaseTest()
      : sb_database_(std::unique_ptr<SBDatabase, base::OnTaskRunnerDeleter>(
            nullptr,
            base::OnTaskRunnerDeleter(nullptr))) {}

  void SetUp() override {
    PlatformTest::SetUp();

    // Setup a database in a temporary directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    database_dirname_ = temp_dir_.GetPath().AppendASCII("SBDatabaseTest");

    SetupInfoMapAndExpectedState();
  }

  void TearDown() override {
    SBDatabase::RegisterStoreFactoryForTest(nullptr);
    sb_database_.reset();
    PlatformTest::TearDown();
  }

  void RegisterFactory(bool hash_prefix_matches = true) {
    SBDatabase::RegisterStoreFactoryForTest(
        std::make_unique<FakeV4StoreFactory>(hash_prefix_matches));
  }

  static void RegisterStoreFactoryForTest(
      std::unique_ptr<SBStoreFactory> factory) {
    SBDatabase::RegisterStoreFactoryForTest(std::move(factory));
  }

  const StoreMap* GetStoreMap() { return sb_database_->store_map_.get(); }

  virtual void SetupInfoMapAndExpectedState() = 0;

  void NewDatabaseReadyWithExpectedStorePathsAndIds(
      base::OnceClosure callback,
      std::unique_ptr<SBDatabase, base::OnTaskRunnerDeleter> sb_database) {
    ASSERT_TRUE(sb_database);
    ASSERT_TRUE(sb_database->store_map_);

    ASSERT_EQ(expected_store_paths_.size(), sb_database->store_map_->size());
    ASSERT_EQ(expected_identifiers_.size(), sb_database->store_map_->size());
    for (size_t i = 0; i < expected_identifiers_.size(); i++) {
      const auto& expected_identifier = expected_identifiers_[i];
      const auto& store = sb_database->store_map_->at(expected_identifier);
      ASSERT_TRUE(store);
      const auto& expected_store_path = expected_store_paths_[i];
      EXPECT_EQ(expected_store_path, store->store_path());
    }

    sb_database_ = std::move(sb_database);
    std::move(callback).Run();
  }

  scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() {
    return base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::MayBlock()}, base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  }

  void WaitForSBDatabaseReady(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner,
      std::vector<scoped_refptr<base::TestSimpleTaskRunner>>
          simple_task_runners_to_wait_for) {
    base::RunLoop created_and_called_back_waiter;
    SBDatabase::Create(
        db_task_runner, database_dirname_, list_infos_,
        base::BindOnce(
            &SBDatabaseTest::NewDatabaseReadyWithExpectedStorePathsAndIds,
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
  void VerifyExpectedStoresState(bool expect_new_stores) {
    const StoreMap* new_store_map = sb_database_->store_map_.get();
    std::unique_ptr<StoreStateMap> new_store_state_map =
        sb_database_->GetStoreStateMap();
    EXPECT_EQ(expected_store_state_map_.size(), new_store_map->size());
    EXPECT_EQ(expected_store_state_map_.size(), new_store_state_map->size());
    for (const auto& expected_iter : expected_store_state_map_) {
      const ListIdentifier& identifier = expected_iter.first;
      const std::string& state = expected_iter.second;
      ASSERT_EQ(1u, new_store_map->count(identifier));
      ASSERT_EQ(1u, new_store_state_map->count(identifier));

      // Verify the expected state in the store map and the state map.
      EXPECT_EQ(state, new_store_map->at(identifier)->GetStoreState());
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

  std::unique_ptr<SBDatabase, base::OnTaskRunnerDeleter> sb_database_;
  base::FilePath database_dirname_;
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  ListInfos list_infos_;
  std::vector<ListIdentifier> expected_identifiers_;
  std::vector<base::FilePath> expected_store_paths_;
  StoreStateMap expected_store_state_map_;
  std::unordered_map<ListIdentifier, raw_ptr<SBStore, CtnExperimental>>
      old_stores_map_;
};

// Parameterized test fixture that runs both for v4 and v5.
class SBDatabaseTest_V4V5 : public SBDatabaseTest,
                            public ::testing::WithParamInterface<bool> {
 public:
  SBDatabaseTest_V4V5()
      : SBDatabaseTest_V4V5(/*enable_delete_unused_stores=*/true) {}

  explicit SBDatabaseTest_V4V5(bool enable_delete_unused_stores) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (IsV5Enabled()) {
      enabled_features.push_back(kLocalListsUseSBv5);
    } else {
      disabled_features.push_back(kLocalListsUseSBv5);
    }
    if (enable_delete_unused_stores) {
      enabled_features.push_back(kSafeBrowsingDeleteUnusedStores);
    } else {
      disabled_features.push_back(kSafeBrowsingDeleteUnusedStores);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool IsV5Enabled() const { return GetParam(); }

  void RegisterFactory(bool hash_prefix_matches = true) {
    if (IsV5Enabled()) {
      RegisterStoreFactoryForTest(
          std::make_unique<FakeV5StoreFactory>(hash_prefix_matches));
    } else {
      RegisterStoreFactoryForTest(
          std::make_unique<FakeV4StoreFactory>(hash_prefix_matches));
    }
  }

  // Sets up the list info map and expected store states.
  void SetupInfoMapAndExpectedState() override {
    if (IsV5Enabled()) {
      ListIdentifier malware_id_v5(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
      ListIdentifier phishing_id_v5(SBThreatType::SB_THREAT_TYPE_URL_PHISHING);

      list_infos_.emplace_back(true, "win_url_malware", malware_id_v5,
                               SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
      expected_identifiers_.push_back(malware_id_v5);
      expected_store_paths_.push_back(
          database_dirname_.AppendASCII("win_url_malware_v5.store"));

      list_infos_.emplace_back(true, "linux_url_malware", phishing_id_v5,
                               SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
      expected_identifiers_.push_back(phishing_id_v5);
      expected_store_paths_.push_back(
          database_dirname_.AppendASCII("linux_url_malware_v5.store"));
    } else {
      ListIdentifier win_malware_id(WINDOWS_PLATFORM, URL, MALWARE_THREAT);
      list_infos_.emplace_back(true, "win_url_malware", win_malware_id,
                               SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
      expected_identifiers_.push_back(win_malware_id);
      expected_store_paths_.push_back(
          database_dirname_.AppendASCII("win_url_malware.store"));

      ListIdentifier linux_malware_id(LINUX_PLATFORM, URL, MALWARE_THREAT);
      list_infos_.emplace_back(true, "linux_url_malware", linux_malware_id,
                               SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
      expected_identifiers_.push_back(linux_malware_id);
      expected_store_paths_.push_back(
          database_dirname_.AppendASCII("linux_url_malware.store"));
    }
  }

  std::unique_ptr<SBUpdateResponseMap> CreateFakeUpdateResponseMap(
      StoreStateMap store_state_map,
      bool use_valid_response_type) {
    auto update_map = std::make_unique<SBUpdateResponseMap>();
    for (const auto& store_state_iter : store_state_map) {
      ListIdentifier identifier = store_state_iter.first;
      auto sb_response = std::make_unique<SBUpdateResponse>();
      if (IsV5Enabled()) {
        auto v5_response = std::make_unique<V5::HashList>();
        v5_response->set_version(store_state_iter.second);
        if (use_valid_response_type) {
          std::array<uint8_t, 32> empty_checksum =
              crypto::hash::Sha256(base::span<const uint8_t>());
          v5_response->set_sha256_checksum(
              std::string(empty_checksum.begin(), empty_checksum.end()));
          sb_response->v5_response = std::move(v5_response);
        } else {
          auto* additions = v5_response->mutable_additions_four_bytes();
          additions->set_rice_parameter(3);
          additions->set_entries_count(1);
          additions->set_encoded_data("");
          sb_response->v5_response = std::move(v5_response);
        }
      } else {
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
        sb_response->v4_response = std::move(lur);
      }
      update_map->insert({identifier, std::move(sb_response)});
    }
    return update_map;
  }

  // Helper to test startup cleanup behavior under various feature and file
  // configurations.
  void RunStartupInactiveStoreFilesCleanupTest(
      bool create_unused_files,
      bool expect_unused_files_deleted,
      bool expect_time_recorded,
      std::optional<bool> expected_cleanup_needed_sample) {
    base::HistogramTester histogram_tester;
    RegisterFactory();

    ASSERT_FALSE(expected_store_paths_.empty());
    ASSERT_TRUE(base::CreateDirectory(database_dirname_));

    // Create active store files on disk.
    for (const auto& expected_path : expected_store_paths_) {
      ASSERT_TRUE(base::WriteFile(expected_path, "active_store"));
    }

    base::FilePath deprecated_store =
        database_dirname_.AppendASCII("CertCsdDownloadAllowlist.store");
    base::FilePath deprecated_store_hash =
        database_dirname_.AppendASCII("CertCsdDownloadAllowlist.store.0");
    base::FilePath ip_malware_store =
        database_dirname_.AppendASCII("IpMalware.store");
    base::FilePath stale_hash =
        expected_store_paths_[0].AddExtensionASCII("old_ext");
    base::FilePath non_store_file =
        database_dirname_.AppendASCII("unrelated_file.txt");

    if (create_unused_files) {
      ASSERT_TRUE(base::WriteFile(deprecated_store, "deprecated"));
      ASSERT_TRUE(base::WriteFile(deprecated_store_hash, "deprecated_hash"));
      ASSERT_TRUE(base::WriteFile(ip_malware_store, "ip_malware"));
      ASSERT_TRUE(base::WriteFile(stale_hash, "stale_hash"));
      ASSERT_TRUE(base::WriteFile(non_store_file, "unrelated"));
    }

    WaitForSBDatabaseReady(CreateTaskRunner(),
                           /*simple_task_runners_to_wait_for=*/{});

    // Active store files should always exist.
    for (const auto& expected_path : expected_store_paths_) {
      EXPECT_TRUE(base::PathExists(expected_path));
    }

    if (create_unused_files) {
      // Unrelated non-store file should never be touched by cleanup.
      EXPECT_TRUE(base::PathExists(non_store_file));

      if (expect_unused_files_deleted) {
        EXPECT_FALSE(base::PathExists(deprecated_store));
        EXPECT_FALSE(base::PathExists(deprecated_store_hash));
        EXPECT_FALSE(base::PathExists(ip_malware_store));
        EXPECT_FALSE(base::PathExists(stale_hash));
      } else {
        EXPECT_TRUE(base::PathExists(deprecated_store));
        EXPECT_TRUE(base::PathExists(deprecated_store_hash));
        EXPECT_TRUE(base::PathExists(ip_malware_store));
        EXPECT_TRUE(base::PathExists(stale_hash));
      }
    }

    histogram_tester.ExpectTotalCount("SafeBrowsing.UnusedStoresCleanup.Time",
                                      expect_time_recorded ? 1 : 0);

    if (expected_cleanup_needed_sample.has_value()) {
      histogram_tester.ExpectUniqueSample(
          "SafeBrowsing.UnusedStoresCleanup.Needed",
          /*sample=*/expected_cleanup_needed_sample.value(),
          /*expected_bucket_count=*/1);
    } else {
      histogram_tester.ExpectTotalCount(
          "SafeBrowsing.UnusedStoresCleanup.Needed", 0);
    }

    if (expect_unused_files_deleted) {
      histogram_tester.ExpectUniqueSample(
          "SafeBrowsing.UnusedStoresCleanup.FileCount.Cleaned", /*sample=*/4,
          /*expected_bucket_count=*/1);
      histogram_tester.ExpectUniqueSample(
          "SafeBrowsing.UnusedStoresCleanup.FileCount.DeleteFailed",
          /*sample=*/0, /*expected_bucket_count=*/1);
      histogram_tester.ExpectUniqueSample(
          "SafeBrowsing.UnusedStoresCleanup.FileCount.ActiveRemaining",
          /*sample=*/expected_store_paths_.size(),
          /*expected_bucket_count=*/1);
    } else {
      histogram_tester.ExpectTotalCount(
          "SafeBrowsing.UnusedStoresCleanup.FileCount.Cleaned", 0);
      histogram_tester.ExpectTotalCount(
          "SafeBrowsing.UnusedStoresCleanup.FileCount.DeleteFailed", 0);
      histogram_tester.ExpectTotalCount(
          "SafeBrowsing.UnusedStoresCleanup.FileCount.ActiveRemaining", 0);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Fixture for testing behavior when kSafeBrowsingDeleteUnusedStores is
// disabled.
class SBDatabaseTest_DeleteUnusedStoresDisabled : public SBDatabaseTest_V4V5 {
 public:
  SBDatabaseTest_DeleteUnusedStoresDisabled()
      : SBDatabaseTest_V4V5(/*enable_delete_unused_stores=*/false) {}
};

// Test to set up the database with fake stores.
TEST_P(SBDatabaseTest_V4V5, TestSetupDatabaseWithFakeStores) {
  RegisterFactory();
  WaitForSBDatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // Test succeeds if it does not time out.
}

// Test to check database updates as expected.
TEST_P(SBDatabaseTest_V4V5, TestApplyUpdateWithNewStates) {
  RegisterFactory();

  WaitForSBDatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(sb_database_);
  const StoreMap* db_stores = GetStoreMap();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    SBStore* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] =
        store->GetStoreState() + "_fake";
    old_stores_map_[store_iter.first] = store;
  }

  base::RunLoop callback_db_updated_run_loop;
  sb_database_->ApplyUpdate(
      CreateFakeUpdateResponseMap(expected_store_state_map_,
                                  /*use_valid_response_type=*/true),
      callback_db_updated_run_loop.QuitClosure());

  // Wait for the ApplyUpdate callback to get called.
  callback_db_updated_run_loop.Run();

  VerifyExpectedStoresState(true);
}

// Test to ensure no state updates leads to no store updates.
TEST_P(SBDatabaseTest_V4V5, TestApplyUpdateWithNoNewState) {
  RegisterFactory();

  WaitForSBDatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(sb_database_);
  const StoreMap* db_stores = GetStoreMap();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    SBStore* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] = store->GetStoreState();
    old_stores_map_[store_iter.first] = store;
  }

  base::RunLoop callback_db_updated_run_loop;
  sb_database_->ApplyUpdate(
      CreateFakeUpdateResponseMap(expected_store_state_map_,
                                  /*use_valid_response_type=*/true),
      callback_db_updated_run_loop.QuitClosure());

  callback_db_updated_run_loop.Run();

  VerifyExpectedStoresState(false);
}

// Test to ensure no updates leads to no store updates.
TEST_P(SBDatabaseTest_V4V5, TestApplyUpdateWithEmptyUpdate) {
  RegisterFactory();

  WaitForSBDatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(sb_database_);
  const StoreMap* db_stores = GetStoreMap();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    SBStore* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] = store->GetStoreState();
    old_stores_map_[store_iter.first] = store;
  }

  base::RunLoop callback_db_updated_run_loop;
  sb_database_->ApplyUpdate(
      CreateFakeUpdateResponseMap({}, /*use_valid_response_type=*/true),
      callback_db_updated_run_loop.QuitClosure());

  callback_db_updated_run_loop.Run();

  VerifyExpectedStoresState(false);
}

// Test to ensure invalid update leads to no store changes.
TEST_P(SBDatabaseTest_V4V5, TestApplyUpdateWithInvalidUpdate) {
  RegisterFactory();

  WaitForSBDatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // The database has now been created. Time to try to update it.
  EXPECT_TRUE(sb_database_);
  const StoreMap* db_stores = GetStoreMap();
  EXPECT_EQ(expected_store_paths_.size(), db_stores->size());
  for (const auto& store_iter : *db_stores) {
    SBStore* store = store_iter.second.get();
    expected_store_state_map_[store_iter.first] = store->GetStoreState();
    old_stores_map_[store_iter.first] = store;
  }

  base::RunLoop callback_db_updated_run_loop;
  sb_database_->ApplyUpdate(
      CreateFakeUpdateResponseMap(expected_store_state_map_,
                                  /*use_valid_response_type=*/false),
      callback_db_updated_run_loop.QuitClosure());
  callback_db_updated_run_loop.Run();

  VerifyExpectedStoresState(false);
}

// Test to ensure the case that all stores match a given full hash.
TEST_P(SBDatabaseTest_V4V5, TestAllStoresMatchFullHash) {
  bool hash_prefix_matches = true;
  RegisterFactory(hash_prefix_matches);

  WaitForSBDatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  StoresToCheck stores_to_check(
      {expected_identifiers_[0], expected_identifiers_[1]});
  base::test::TestFuture<DbLookupResult> results;
  sb_database_->GetStoresMatchingFullHash({"anything"}, stores_to_check,
                                          results.GetCallback());
  FullHashToStoreAndHashPrefixesMap map = results.Get().results;
  StoreAndHashPrefixes store_and_hash_prefixes = map["anything"];
  EXPECT_EQ(2u, store_and_hash_prefixes.size());
  StoresToCheck stores_found;
  for (const auto& it : store_and_hash_prefixes) {
    stores_found.insert(it.list_id);
  }
  EXPECT_EQ(stores_to_check, stores_found);
}

// Test to ensure the case that no stores match a given full hash.
TEST_P(SBDatabaseTest_V4V5, TestNoStoreMatchesFullHash) {
  bool hash_prefix_matches = false;
  RegisterFactory(hash_prefix_matches);

  WaitForSBDatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  base::test::TestFuture<DbLookupResult> results;
  sb_database_->GetStoresMatchingFullHash(
      {"anything"},
      StoresToCheck({expected_identifiers_[0], expected_identifiers_[1]}),
      results.GetCallback());
  FullHashToStoreAndHashPrefixesMap map = results.Get().results;
  StoreAndHashPrefixes store_and_hash_prefixes = map["anything"];
  EXPECT_TRUE(store_and_hash_prefixes.empty());
}

// Test to ensure the case that some stores match a given full hash.
TEST_P(SBDatabaseTest_V4V5, TestSomeStoresMatchFullHash) {
  // Setup stores to not match the full hash.
  bool hash_prefix_matches = false;
  RegisterFactory(hash_prefix_matches);

  WaitForSBDatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // Set the store corresponding to expected_identifiers_[0] to match the full
  // hash.
  const ListIdentifier& matched_id = expected_identifiers_[0];
  if (IsV5Enabled()) {
    FakeV5Store* store =
        static_cast<FakeV5Store*>(GetStoreMap()->at(matched_id).get());
    store->set_hash_prefix_matches(true);
  } else {
    FakeV4Store* store =
        static_cast<FakeV4Store*>(GetStoreMap()->at(matched_id).get());
    store->set_hash_prefix_matches(true);
  }

  base::test::TestFuture<DbLookupResult> results;
  sb_database_->GetStoresMatchingFullHash(
      {"anything"},
      StoresToCheck({expected_identifiers_[0], expected_identifiers_[1]}),
      results.GetCallback());
  FullHashToStoreAndHashPrefixesMap map = results.Get().results;
  StoreAndHashPrefixes store_and_hash_prefixes = map["anything"];
  EXPECT_EQ(1u, store_and_hash_prefixes.size());
  EXPECT_EQ(store_and_hash_prefixes.begin()->list_id, matched_id);
  EXPECT_FALSE(store_and_hash_prefixes.begin()->hash_prefix.empty());
}

// Test to ensure the case that only some stores are reported to match a given
// full hash because of StoresToCheck.
TEST_P(SBDatabaseTest_V4V5, TestSomeStoresMatchFullHashBecauseOfStoresToMatch) {
  // Setup all stores to match the full hash.
  bool hash_prefix_matches = true;
  RegisterFactory(hash_prefix_matches);

  WaitForSBDatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // Don't add expected_identifiers_[0] to the StoresToCheck.
  base::test::TestFuture<DbLookupResult> results;
  sb_database_->GetStoresMatchingFullHash(
      {"anything"}, StoresToCheck({expected_identifiers_[1]}),
      results.GetCallback());
  FullHashToStoreAndHashPrefixesMap map = results.Get().results;
  StoreAndHashPrefixes store_and_hash_prefixes = map["anything"];
  EXPECT_EQ(1u, store_and_hash_prefixes.size());
  EXPECT_EQ(store_and_hash_prefixes.begin()->list_id, expected_identifiers_[1]);
  EXPECT_FALSE(store_and_hash_prefixes.begin()->hash_prefix.empty());
}

TEST_P(SBDatabaseTest_V4V5, VerifyChecksumCalledAsync) {
  bool hash_prefix_matches = true;
  RegisterFactory(hash_prefix_matches);

  WaitForSBDatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  base::test::TestFuture<const std::vector<ListIdentifier>&>
      verify_checksum_future;
  sb_database_->VerifyChecksum(verify_checksum_future.GetCallback());
  // `verify_checksum_future` should not be ready because callback is called
  // asynchronously.
  EXPECT_FALSE(verify_checksum_future.IsReady());
  EXPECT_TRUE(verify_checksum_future.Wait());
}

TEST_P(SBDatabaseTest_V4V5, DeleteUnusedStoreFilesOnStartup) {
  RunStartupInactiveStoreFilesCleanupTest(
      /*create_unused_files=*/true,
      /*expect_unused_files_deleted=*/true,
      /*expect_time_recorded=*/true,
      /*expected_cleanup_needed_sample=*/true);
}

TEST_P(SBDatabaseTest_V4V5, DeleteUnusedStoreFilesOnStartup_NoUnusedFiles) {
  RunStartupInactiveStoreFilesCleanupTest(
      /*create_unused_files=*/false,
      /*expect_unused_files_deleted=*/false,
      /*expect_time_recorded=*/true,
      /*expected_cleanup_needed_sample=*/false);
}

TEST_P(SBDatabaseTest_DeleteUnusedStoresDisabled,
       UnusedStoreFilesNotDeletedWhenFeatureDisabled) {
  RunStartupInactiveStoreFilesCleanupTest(
      /*create_unused_files=*/true,
      /*expect_unused_files_deleted=*/false,
      /*expect_time_recorded=*/false,
      /*expected_cleanup_needed_sample=*/std::nullopt);
}

TEST_P(SBDatabaseTest_V4V5, VerifyChecksumCancelled) {
  bool hash_prefix_matches = true;
  RegisterFactory(hash_prefix_matches);

  scoped_refptr<base::TestSimpleTaskRunner> db_task_runner =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();
  WaitForSBDatabaseReady(db_task_runner,
                         /*simple_task_runners_to_wait_for=*/{db_task_runner});

  base::test::TestFuture<const std::vector<ListIdentifier>&>
      verify_checksum_future;
  sb_database_->VerifyChecksum(verify_checksum_future.GetCallback());
  EXPECT_FALSE(verify_checksum_future.IsReady());

  // Post task to destroy SBDatabase on db thread.
  sb_database_.reset();

  // Simulate SBDatabase::~SBDatabase being called on the db thread prior to
  // SBDatabase::OnChecksumVerified() being called on UI thread.
  db_task_runner->RunPendingTasks();
  base::RunLoop().RunUntilIdle();

  // Callback should not be called since database is destroyed.
  EXPECT_FALSE(verify_checksum_future.IsReady());
}

// Test that we can properly check for unsupported stores
TEST_P(SBDatabaseTest_V4V5, TestStoresAvailable) {
  bool hash_prefix_matches = false;
  RegisterFactory(hash_prefix_matches);

  WaitForSBDatabaseReady(CreateTaskRunner(),
                         /*simple_task_runners_to_wait_for=*/{});

  // Doesn't exist in out list
  ListIdentifier bogus_id =
      IsV5Enabled()
          ? ListIdentifier(
                SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING)
          : ListIdentifier(LINUX_PLATFORM, CHROME_EXTENSION, CSD_ALLOWLIST);

  EXPECT_TRUE(sb_database_->AreAllStoresAvailable(
      StoresToCheck({expected_identifiers_[0], expected_identifiers_[1]})));
  EXPECT_TRUE(sb_database_->AreAnyStoresAvailable(
      StoresToCheck({expected_identifiers_[0], expected_identifiers_[1]})));

  EXPECT_TRUE(sb_database_->AreAllStoresAvailable(
      StoresToCheck({expected_identifiers_[1]})));
  EXPECT_TRUE(sb_database_->AreAnyStoresAvailable(
      StoresToCheck({expected_identifiers_[1]})));

  EXPECT_FALSE(sb_database_->AreAllStoresAvailable(
      StoresToCheck({expected_identifiers_[1], bogus_id})));
  EXPECT_TRUE(sb_database_->AreAnyStoresAvailable(
      StoresToCheck({expected_identifiers_[1], bogus_id})));

  EXPECT_FALSE(sb_database_->AreAllStoresAvailable(StoresToCheck({bogus_id})));
}

namespace {

// Test class for tracking lifetime of base::RepeatingClosure passed to
// SBDatabase::ApplyUpdate().
class TestApplyUpdateCallback {
 public:
  TestApplyUpdateCallback() = default;
  ~TestApplyUpdateCallback() = default;

  base::RepeatingClosure CreateRepeatingClosure() {
    was_callback_destroyed_ = true;
    return base::BindRepeating(&TestApplyUpdateCallback::Run,
                               weak_ptr_factory_.GetWeakPtr(),
                               base::Owned(new base::AutoReset<bool>(
                                   &was_callback_destroyed_, false)));
  }

  bool was_called() const { return was_called_; }

  bool was_callback_destroyed() const { return was_callback_destroyed_; }

 private:
  void Run(base::AutoReset<bool>* param) { was_called_ = true; }

  bool was_called_ = false;
  bool was_callback_destroyed_ = false;
  base::WeakPtrFactory<TestApplyUpdateCallback> weak_ptr_factory_{this};
};

}  // anonymous namespace

// Test to ensure that the callback to the database is dropped when the database
// gets destroyed. See http://crbug.com/683147#c5 for more details.
TEST_P(SBDatabaseTest_V4V5, UsingWeakPtrDropsCallback) {
  scoped_refptr<base::TestSimpleTaskRunner> db_task_runner =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();

  RegisterFactory();

  // Step 1: Create the database.
  WaitForSBDatabaseReady(db_task_runner,
                         /*simple_task_runners_to_wait_for=*/{db_task_runner});

  // Step 2: Try to update the database. This posts ApplyUpdate() on
  // the `db_task_runner`.
  StoreStateMap store_state_map;
  store_state_map[expected_identifiers_[1]] = "new_state";

  // The callback passed to ApplyUpdate() is called from SBStore::ApplyUpdate().
  // We expect the callback not to be executed. Use TestApplyUpdateCallback to
  // verify this.
  TestApplyUpdateCallback test_callback;
  sb_database_->ApplyUpdate(
      CreateFakeUpdateResponseMap(store_state_map,
                                  /*use_valid_response_type=*/true),
      test_callback.CreateRepeatingClosure());

  // Step 3: Post task to destroy SBDatabase on db thread.
  sb_database_.reset();
  EXPECT_FALSE(test_callback.was_callback_destroyed());

  // Step 4: Simulate SBDatabase::~SBDatabase() being called on db thread prior
  // to SBDatabase::UpdatedStoreReady() being called on UI thread.
  // SBDatabase::UpdatedStoreReady() is posted to the UI thread from
  // SBStore::ApplyUpdate().
  db_task_runner->RunPendingTasks();
  EXPECT_TRUE(test_callback.was_callback_destroyed());
  EXPECT_FALSE(test_callback.was_called());
}

TEST_P(SBDatabaseTest_V4V5, TestFactorySelection) {
  base::HistogramTester histogram_tester;

  // Clear and setup list_infos_ with base name (no extension).
  list_infos_.clear();
  expected_identifiers_.clear();
  expected_store_paths_.clear();

  if (IsV5Enabled()) {
    // Use different threat types for V5 to avoid duplicate keys in StoreMap.
    ListIdentifier malware_id_v5(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
    ListIdentifier phishing_id_v5(SBThreatType::SB_THREAT_TYPE_URL_PHISHING);

    list_infos_.emplace_back(true, "win_url_malware", malware_id_v5,
                             SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
    expected_identifiers_.push_back(malware_id_v5);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("win_url_malware_v5.store"));

    list_infos_.emplace_back(true, "linux_url_malware", phishing_id_v5,
                             SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    expected_identifiers_.push_back(phishing_id_v5);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("linux_url_malware_v5.store"));
  } else {
    ListIdentifier win_malware_id(WINDOWS_PLATFORM, URL, MALWARE_THREAT);
    ListIdentifier linux_malware_id(LINUX_PLATFORM, URL, MALWARE_THREAT);

    list_infos_.emplace_back(true, "win_url_malware", win_malware_id,
                             SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
    expected_identifiers_.push_back(win_malware_id);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("win_url_malware.store"));

    list_infos_.emplace_back(true, "linux_url_malware", linux_malware_id,
                             SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
    expected_identifiers_.push_back(linux_malware_id);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("linux_url_malware.store"));
  }

  // Do not register fake factory, let it use the default.
  WaitForSBDatabaseReady(CreateTaskRunner(), {});

  ASSERT_TRUE(sb_database_);

  if (IsV5Enabled()) {
    // V5 store read should be logged, V4 should not.
    histogram_tester.ExpectBucketCount("SafeBrowsing.V5StoreRead.Result",
                                       V5StoreReadResult::kFileOpenFailure,
                                       list_infos_.size());
    histogram_tester.ExpectTotalCount("SafeBrowsing.V4StoreRead.Result", 0);

    // V5 store ready on startup should be logged as false, V4 should not be
    // logged.
    histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Store.ReadyOnStartup",
                                        false, list_infos_.size());
    histogram_tester.ExpectTotalCount("SafeBrowsing.V4Store.ReadyOnStartup", 0);
  } else {
    // V4 store read should be logged, V5 should not.
    histogram_tester.ExpectBucketCount("SafeBrowsing.V4StoreRead.Result",
                                       FILE_UNREADABLE_FAILURE,
                                       list_infos_.size());
    histogram_tester.ExpectTotalCount("SafeBrowsing.V5StoreRead.Result", 0);

    // V4 store ready on startup should be logged as false, V5 should not be
    // logged.
    histogram_tester.ExpectUniqueSample("SafeBrowsing.V4Store.ReadyOnStartup",
                                        false, list_infos_.size());
    histogram_tester.ExpectTotalCount("SafeBrowsing.V5Store.ReadyOnStartup", 0);
  }
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBStore.ReadyOnStartup",
                                      false, list_infos_.size());
}

TEST_P(SBDatabaseTest_V4V5, TestRecordFileSizeHistograms) {
  base::HistogramTester histogram_tester;

  // Setup list_infos_ with a few stores.
  list_infos_.clear();
  expected_identifiers_.clear();
  expected_store_paths_.clear();

  if (IsV5Enabled()) {
    ListIdentifier malware_id(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
    ListIdentifier soceng_id(SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    list_infos_.emplace_back(true, "UrlMalware", malware_id,
                             SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
    expected_identifiers_.push_back(malware_id);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("UrlMalware_v5.store"));

    list_infos_.emplace_back(true, "UrlSoceng", soceng_id,
                             SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    expected_identifiers_.push_back(soceng_id);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("UrlSoceng_v5.store"));
  } else {
    ListIdentifier malware_id(WINDOWS_PLATFORM, URL, MALWARE_THREAT);
    ListIdentifier soceng_id(WINDOWS_PLATFORM, URL, SOCIAL_ENGINEERING);
    list_infos_.emplace_back(true, "UrlMalware", malware_id,
                             SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
    expected_identifiers_.push_back(malware_id);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("UrlMalware.store"));

    list_infos_.emplace_back(true, "UrlSoceng", soceng_id,
                             SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    expected_identifiers_.push_back(soceng_id);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("UrlSoceng.store"));
  }

  // Do not register fake factory, let it use the default.
  WaitForSBDatabaseReady(CreateTaskRunner(), {});
  // Record and verify histograms.
  sb_database_->RecordFileSizeHistograms();
  if (IsV5Enabled()) {
    histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Database.Size", 0, 1);
    histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Database.SizeLinear", 0,
                                        1);
    // Confirm V5Store receives appropriate metric prefix.
    histogram_tester.ExpectUniqueSample(
        "SafeBrowsing.V5Database.Size.UrlMalware_v5", 0, 1);
    histogram_tester.ExpectUniqueSample(
        "SafeBrowsing.V5Database.Size.UrlSoceng_v5", 0, 1);
  } else {
    histogram_tester.ExpectUniqueSample("SafeBrowsing.V4Database.Size", 0, 1);
    histogram_tester.ExpectUniqueSample("SafeBrowsing.V4Database.SizeLinear", 0,
                                        1);
    // Confirm V4Store receives appropriate metric prefix.
    histogram_tester.ExpectUniqueSample(
        "SafeBrowsing.V4Database.Size.UrlMalware", 0, 1);
    histogram_tester.ExpectUniqueSample(
        "SafeBrowsing.V4Database.Size.UrlSoceng", 0, 1);
  }
}

TEST_P(SBDatabaseTest_V4V5, TestRecordDatabaseUpdateLatency) {
  base::HistogramTester histogram_tester;

  // Setup list_infos_ with a store.
  list_infos_.clear();
  expected_identifiers_.clear();
  expected_store_paths_.clear();

  ListIdentifier malware_id =
      IsV5Enabled() ? ListIdentifier(SBThreatType::SB_THREAT_TYPE_URL_MALWARE)
                    : ListIdentifier(WINDOWS_PLATFORM, URL, MALWARE_THREAT);
  if (IsV5Enabled()) {
    list_infos_.emplace_back(true, "UrlMalware", malware_id,
                             SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
    expected_identifiers_.push_back(malware_id);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("UrlMalware_v5.store"));
  } else {
    list_infos_.emplace_back(true, "UrlMalware", malware_id,
                             SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
    expected_identifiers_.push_back(malware_id);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("UrlMalware.store"));
  }

  // Do not register fake factory, let it use the default.
  WaitForSBDatabaseReady(CreateTaskRunner(), {});
  ASSERT_TRUE(sb_database_);

  std::string latency_metric = IsV5Enabled()
                                   ? "SafeBrowsing.V5Database.UpdateLatency"
                                   : "SafeBrowsing.V4Database.UpdateLatency";

  // First update to set last_update_ and version to "version_1".
  {
    auto update_map = std::make_unique<SBUpdateResponseMap>();
    auto sb_response = std::make_unique<SBUpdateResponse>();
    if (IsV5Enabled()) {
      auto v5_response = std::make_unique<V5::HashList>();
      v5_response->set_version("version_1");
      sb_response->v5_response = std::move(v5_response);
    } else {
      auto lur = std::make_unique<ListUpdateResponse>();
      lur->set_platform_type(malware_id.platform_type());
      lur->set_threat_entry_type(malware_id.threat_entry_type());
      lur->set_threat_type(malware_id.threat_type());
      lur->set_new_client_state("version_1");
      lur->set_response_type(ListUpdateResponse::FULL_UPDATE);
      sb_response->v4_response = std::move(lur);
    }
    update_map->insert({malware_id, std::move(sb_response)});

    base::RunLoop run_loop;
    sb_database_->ApplyUpdate(std::move(update_map), run_loop.QuitClosure());
    run_loop.Run();
  }

  // Histogram should not be logged yet.
  histogram_tester.ExpectTotalCount(latency_metric, 0);

  // Second update to "version_2" should log the histogram. Logged from
  // `UpdatedStoreReady`.
  {
    auto update_map = std::make_unique<SBUpdateResponseMap>();
    auto sb_response = std::make_unique<SBUpdateResponse>();
    if (IsV5Enabled()) {
      auto v5_response = std::make_unique<V5::HashList>();
      v5_response->set_version("version_2");
      sb_response->v5_response = std::move(v5_response);
    } else {
      auto lur = std::make_unique<ListUpdateResponse>();
      lur->set_platform_type(malware_id.platform_type());
      lur->set_threat_entry_type(malware_id.threat_entry_type());
      lur->set_threat_type(malware_id.threat_type());
      lur->set_new_client_state("version_2");
      lur->set_response_type(ListUpdateResponse::FULL_UPDATE);
      sb_response->v4_response = std::move(lur);
    }
    update_map->insert({malware_id, std::move(sb_response)});

    base::RunLoop run_loop;
    sb_database_->ApplyUpdate(std::move(update_map), run_loop.QuitClosure());
    run_loop.Run();
  }

  // Histogram should be logged now.
  histogram_tester.ExpectTotalCount(latency_metric, 1);

  // Third update to "version_2" (no change) should log the histogram again.
  // Logged from `ApplyUpdate`.
  {
    auto update_map = std::make_unique<SBUpdateResponseMap>();
    auto sb_response = std::make_unique<SBUpdateResponse>();
    if (IsV5Enabled()) {
      auto v5_response = std::make_unique<V5::HashList>();
      v5_response->set_version("version_2");
      sb_response->v5_response = std::move(v5_response);
    } else {
      auto lur = std::make_unique<ListUpdateResponse>();
      lur->set_platform_type(malware_id.platform_type());
      lur->set_threat_entry_type(malware_id.threat_entry_type());
      lur->set_threat_type(malware_id.threat_type());
      lur->set_new_client_state("version_2");
      lur->set_response_type(ListUpdateResponse::FULL_UPDATE);
      sb_response->v4_response = std::move(lur);
    }
    update_map->insert({malware_id, std::move(sb_response)});

    base::RunLoop run_loop;
    sb_database_->ApplyUpdate(std::move(update_map), run_loop.QuitClosure());
    run_loop.Run();
  }

  // Histogram should be logged again (total 2).
  histogram_tester.ExpectTotalCount(latency_metric, 2);
}

TEST(SBDatabaseListInfoTest, TestV4ModeProperties) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kLocalListsUseSBv5);

  ListIdentifier v4_id(WINDOWS_PLATFORM, URL, MALWARE_THREAT);
  ListInfo list_info(/*fetch_updates=*/true, "UrlMalware", v4_id,
                     SBThreatType::SB_THREAT_TYPE_URL_MALWARE);

  EXPECT_EQ("UrlMalware.store", list_info.filename());
  EXPECT_EQ("UrlMalware.store", list_info.v4_filename());
  EXPECT_EQ(4u, list_info.v5_prefix_size().value_or(0));
}

TEST(SBDatabaseListInfoTest, TestV5ModeProperties) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kLocalListsUseSBv5);

  ListIdentifier v5_id(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  ListInfo list_info(/*fetch_updates=*/true, "UrlMalware", v5_id,
                     SBThreatType::SB_THREAT_TYPE_URL_MALWARE);

  EXPECT_EQ("UrlMalware_v5.store", list_info.filename());
  EXPECT_EQ("UrlMalware.store", list_info.v4_filename());
  EXPECT_EQ(4u, list_info.v5_prefix_size().value_or(0));
}

TEST(SBDatabaseListInfoTest, TestNonSyncProperties) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kLocalListsUseSBv5);

  ListIdentifier id(SBThreatType::SB_THREAT_TYPE_API_ABUSE);
  ListInfo list_info(/*fetch_updates=*/false, "", id,
                     SBThreatType::SB_THREAT_TYPE_API_ABUSE);

  EXPECT_EQ("", list_info.filename());
  EXPECT_EQ("", list_info.v4_filename());
  EXPECT_FALSE(list_info.v5_prefix_size().has_value());
}

INSTANTIATE_TEST_SUITE_P(All, SBDatabaseTest_V4V5, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         SBDatabaseTest_DeleteUnusedStoresDisabled,
                         ::testing::Bool());

}  // namespace safe_browsing
