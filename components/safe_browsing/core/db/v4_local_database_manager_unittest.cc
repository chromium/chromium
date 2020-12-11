// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/db/v4_local_database_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_tokenizer.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/safe_browsing/core/common/test_task_environment.h"
#include "components/safe_browsing/core/db/v4_database.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/db/v4_test_util.h"
#include "components/safe_browsing/core/features.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/platform_test.h"

namespace safe_browsing {

namespace {

typedef std::vector<FullHashInfo> FullHashInfos;

// Utility function for populating hashes.
FullHash HashForUrl(const GURL& url) {
  std::vector<FullHash> full_hashes;
  V4ProtocolManagerUtil::UrlToFullHashes(url, &full_hashes);
  // ASSERT_GE(full_hashes.size(), 1u);
  return full_hashes[0];
}

// Use this if you want GetFullHashes() to always return prescribed results.
class FakeGetHashProtocolManager : public V4GetHashProtocolManager {
 public:
  FakeGetHashProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const StoresToCheck& stores_to_check,
      const V4ProtocolConfig& config,
      const FullHashInfos& full_hash_infos)
      : V4GetHashProtocolManager(url_loader_factory, stores_to_check, config),
        full_hash_infos_(full_hash_infos) {}

  void GetFullHashes(const FullHashToStoreAndHashPrefixesMap,
                     const std::vector<std::string>&,
                     FullHashCallback callback) override {
    // Async, since the real manager might use a fetcher.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), full_hash_infos_));
  }

 private:
  FullHashInfos full_hash_infos_;
};

class FakeGetHashProtocolManagerFactory
    : public V4GetHashProtocolManagerFactory {
 public:
  FakeGetHashProtocolManagerFactory(const FullHashInfos& full_hash_infos)
      : full_hash_infos_(full_hash_infos) {}

  std::unique_ptr<V4GetHashProtocolManager> CreateProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const StoresToCheck& stores_to_check,
      const V4ProtocolConfig& config) override {
    return std::make_unique<FakeGetHashProtocolManager>(
        url_loader_factory, stores_to_check, config, full_hash_infos_);
  }

 private:
  FullHashInfos full_hash_infos_;
};

// Use FakeGetHashProtocolManagerFactory in scope, then reset.
// You should make sure the DatabaseManager is created _after_ this.
class ScopedFakeGetHashProtocolManagerFactory {
 public:
  ScopedFakeGetHashProtocolManagerFactory(
      const FullHashInfos& full_hash_infos) {
    V4GetHashProtocolManager::RegisterFactory(
        std::make_unique<FakeGetHashProtocolManagerFactory>(full_hash_infos));
  }
  ~ScopedFakeGetHashProtocolManagerFactory() {
    V4GetHashProtocolManager::RegisterFactory(nullptr);
  }
};

}  // namespace

class FakeV4Database : public V4Database {
 public:
  static void Create(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      std::unique_ptr<StoreMap> store_map,
      const StoreAndHashPrefixes& store_and_hash_prefixes,
      NewDatabaseReadyCallback new_db_callback,
      bool stores_available) {
    // Mimics V4Database::Create
    const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner =
        base::ThreadTaskRunnerHandle::Get();
    db_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeV4Database::CreateOnTaskRunner, db_task_runner,
                       std::move(store_map), store_and_hash_prefixes,
                       callback_task_runner, std::move(new_db_callback),
                       stores_available));
  }

  // V4Database implementation
  void GetStoresMatchingFullHash(
      const FullHash& full_hash,
      const StoresToCheck& stores_to_check,
      StoreAndHashPrefixes* store_and_hash_prefixes) override {
    store_and_hash_prefixes->clear();
    for (const StoreAndHashPrefix& stored_sahp : store_and_hash_prefixes_) {
      if (stores_to_check.count(stored_sahp.list_id) == 0)
        continue;
      const PrefixSize& prefix_size = stored_sahp.hash_prefix.size();
      if (!full_hash.compare(0, prefix_size, stored_sahp.hash_prefix)) {
        store_and_hash_prefixes->push_back(stored_sahp);
      }
    }
  }

  bool AreAllStoresAvailable(
      const StoresToCheck& stores_to_check) const override {
    return stores_available_;
  }

  bool AreAnyStoresAvailable(
      const StoresToCheck& stores_to_check) const override {
    return stores_available_;
  }

 private:
  static void CreateOnTaskRunner(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      std::unique_ptr<StoreMap> store_map,
      const StoreAndHashPrefixes& store_and_hash_prefixes,
      const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner,
      NewDatabaseReadyCallback new_db_callback,
      bool stores_available) {
    // Mimics the semantics of V4Database::CreateOnTaskRunner
    std::unique_ptr<FakeV4Database> fake_v4_database(
        new FakeV4Database(db_task_runner, std::move(store_map),
                           store_and_hash_prefixes, stores_available));
    callback_task_runner->PostTask(FROM_HERE,
                                   base::BindOnce(std::move(new_db_callback),
                                                  std::move(fake_v4_database)));
  }

  FakeV4Database(const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
                 std::unique_ptr<StoreMap> store_map,
                 const StoreAndHashPrefixes& store_and_hash_prefixes,
                 bool stores_available)
      : V4Database(db_task_runner, std::move(store_map)),
        store_and_hash_prefixes_(store_and_hash_prefixes),
        stores_available_(stores_available) {}

  const StoreAndHashPrefixes store_and_hash_prefixes_;
  const bool stores_available_;
};

// TODO(nparker): This might be simpler with a mock and EXPECT calls.
// That would also catch unexpected calls.
class TestClient : public SafeBrowsingDatabaseManager::Client {
 public:
  TestClient(SBThreatType sb_threat_type,
             const GURL& url,
             V4LocalDatabaseManager* manager_to_cancel = nullptr)
      : expected_sb_threat_type_(sb_threat_type),
        expected_urls_(1, url),
        manager_to_cancel_(manager_to_cancel) {}

  TestClient(SBThreatType sb_threat_type, const std::vector<GURL>& url_chain)
      : expected_sb_threat_type_(sb_threat_type), expected_urls_(url_chain) {}

  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type,
                              const ThreatMetadata& metadata) override {
    ASSERT_EQ(expected_urls_[0], url);
    ASSERT_EQ(expected_sb_threat_type_, threat_type);
    on_check_browse_url_result_called_ = true;
    if (manager_to_cancel_) {
      manager_to_cancel_->CancelCheck(this);
    }
  }

  void OnCheckResourceUrlResult(const GURL& url,
                                SBThreatType threat_type,
                                const std::string& threat_hash) override {
    ASSERT_EQ(expected_urls_[0], url);
    ASSERT_EQ(expected_sb_threat_type_, threat_type);
    ASSERT_EQ(threat_type == SB_THREAT_TYPE_SAFE, threat_hash.empty());
    on_check_resource_url_result_called_ = true;
  }

  void OnCheckDownloadUrlResult(const std::vector<GURL>& url_chain,
                                SBThreatType threat_type) override {
    ASSERT_EQ(expected_urls_, url_chain);
    ASSERT_EQ(expected_sb_threat_type_, threat_type);
    on_check_download_urls_result_called_ = true;
  }

  std::vector<GURL>* mutable_expected_urls() { return &expected_urls_; }

  bool on_check_browse_url_result_called() {
    return on_check_browse_url_result_called_;
  }
  bool on_check_download_urls_result_called() {
    return on_check_download_urls_result_called_;
  }
  bool on_check_resource_url_result_called() {
    return on_check_resource_url_result_called_;
  }

 private:
  const SBThreatType expected_sb_threat_type_;
  std::vector<GURL> expected_urls_;
  bool on_check_browse_url_result_called_ = false;
  bool on_check_download_urls_result_called_ = false;
  bool on_check_resource_url_result_called_ = false;
  V4LocalDatabaseManager* manager_to_cancel_;
};

class TestAllowlistClient : public SafeBrowsingDatabaseManager::Client {
 public:
  // |match_expected| specifies whether a full hash match is expected.
  // |expected_sb_threat_type| identifies which callback method to expect to get
  // called.
  explicit TestAllowlistClient(bool match_expected,
                               SBThreatType expected_sb_threat_type)
      : expected_sb_threat_type_(expected_sb_threat_type),
        match_expected_(match_expected) {}

  void OnCheckWhitelistUrlResult(bool is_allowlisted) override {
    EXPECT_EQ(match_expected_, is_allowlisted);
    EXPECT_EQ(SB_THREAT_TYPE_CSD_WHITELIST, expected_sb_threat_type_);
    callback_called_ = true;
  }

  void OnCheckUrlForHighConfidenceAllowlist(bool is_allowlisted) override {
    EXPECT_EQ(match_expected_, is_allowlisted);
    EXPECT_EQ(SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST,
              expected_sb_threat_type_);
    callback_called_ = true;
  }

  bool callback_called() { return callback_called_; }

 private:
  const SBThreatType expected_sb_threat_type_;
  const bool match_expected_;
  bool callback_called_ = false;
};

class TestExtensionClient : public SafeBrowsingDatabaseManager::Client {
 public:
  TestExtensionClient(const std::set<FullHash>& expected_bad_crxs)
      : expected_bad_crxs_(expected_bad_crxs),
        on_check_extensions_result_called_(false) {}

  void OnCheckExtensionsResult(const std::set<FullHash>& bad_crxs) override {
    EXPECT_EQ(expected_bad_crxs_, bad_crxs);
    on_check_extensions_result_called_ = true;
  }

  bool on_check_extensions_result_called() {
    return on_check_extensions_result_called_;
  }

 private:
  const std::set<FullHash> expected_bad_crxs_;
  bool on_check_extensions_result_called_;
};

class FakeV4LocalDatabaseManager : public V4LocalDatabaseManager {
 public:
  FakeV4LocalDatabaseManager(
      const base::FilePath& base_path,
      ExtendedReportingLevelCallback extended_reporting_level_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : V4LocalDatabaseManager(base_path,
                               extended_reporting_level_callback,
                               task_runner),
        perform_full_hash_check_called_(false) {}

  // V4LocalDatabaseManager impl:
  void PerformFullHashCheck(std::unique_ptr<PendingCheck> check) override {
    perform_full_hash_check_called_ = true;
  }

  static bool PerformFullHashCheckCalled(
      scoped_refptr<safe_browsing::V4LocalDatabaseManager>& v4_ldbm) {
    FakeV4LocalDatabaseManager* fake =
        static_cast<FakeV4LocalDatabaseManager*>(v4_ldbm.get());
    return fake->perform_full_hash_check_called_;
  }

 private:
  ~FakeV4LocalDatabaseManager() override {}

  bool perform_full_hash_check_called_;
};

class V4LocalDatabaseManagerTest : public PlatformTest {
 public:
  V4LocalDatabaseManagerTest() : task_runner_(new base::TestSimpleTaskRunner) {}

  void SetUp() override {
    PlatformTest::SetUp();

    task_environment_ = CreateTestTaskEnvironment();

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());
    DVLOG(1) << "base_dir_: " << base_dir_.GetPath().value();

    extended_reporting_level_ = SBER_LEVEL_OFF;
    erl_callback_ = base::BindRepeating(
        &V4LocalDatabaseManagerTest::GetExtendedReportingLevel,
        base::Unretained(this));

    v4_local_database_manager_ =
        base::WrapRefCounted(new V4LocalDatabaseManager(
            base_dir_.GetPath(), erl_callback_, task_runner_));

    StartLocalDatabaseManager();
  }

  void TearDown() override {
    StopLocalDatabaseManager();

    PlatformTest::TearDown();
  }

  void ForceDisableLocalDatabaseManager() {
    v4_local_database_manager_->enabled_ = false;
  }

  void ForceEnableLocalDatabaseManager() {
    v4_local_database_manager_->enabled_ = true;
  }

  const V4LocalDatabaseManager::QueuedChecks& GetQueuedChecks() {
    return v4_local_database_manager_->queued_checks_;
  }

  ExtendedReportingLevel GetExtendedReportingLevel() {
    return extended_reporting_level_;
  }

  void PopulateArtificialDatabase() {
    v4_local_database_manager_->PopulateArtificialDatabase();
  }

  void ReplaceV4Database(const StoreAndHashPrefixes& store_and_hash_prefixes,
                         bool stores_available = false) {
    // Disable the V4LocalDatabaseManager first so that if the callback to
    // verify checksum has been scheduled, then it doesn't do anything when it
    // is called back.
    ForceDisableLocalDatabaseManager();
    // Wait to make sure that the callback gets executed if it has already been
    // scheduled.
    WaitForTasksOnTaskRunner();
    // Re-enable the V4LocalDatabaseManager otherwise the checks won't work and
    // the fake database won't be set either.
    ForceEnableLocalDatabaseManager();

    NewDatabaseReadyCallback db_ready_callback =
        base::BindOnce(&V4LocalDatabaseManager::DatabaseReadyForChecks,
                       base::Unretained(v4_local_database_manager_.get()));
    FakeV4Database::Create(task_runner_, std::make_unique<StoreMap>(),
                           store_and_hash_prefixes,
                           std::move(db_ready_callback), stores_available);
    WaitForTasksOnTaskRunner();
  }

  void ResetLocalDatabaseManager() {
    StopLocalDatabaseManager();
    v4_local_database_manager_ =
        base::WrapRefCounted(new V4LocalDatabaseManager(
            base_dir_.GetPath(), erl_callback_, task_runner_));
    StartLocalDatabaseManager();
  }

  void ResetV4Database() {
    V4Database::Destroy(std::move(v4_local_database_manager_->v4_database_));
  }

  void StartLocalDatabaseManager() {
    v4_local_database_manager_->StartOnIOThread(test_shared_loader_factory_,
                                                GetTestV4ProtocolConfig());
  }

  void StopLocalDatabaseManager() {
    if (v4_local_database_manager_) {
      v4_local_database_manager_->StopOnIOThread(true);
    }

    // Force destruction of the database.
    WaitForTasksOnTaskRunner();
  }

  void WaitForTasksOnTaskRunner() {
    // Wait for tasks on the task runner so we're sure that the
    // V4LocalDatabaseManager has read the data from disk.
    task_runner_->RunPendingTasks();
    base::RunLoop().RunUntilIdle();
  }

  // For those tests that need the fake manager
  void SetupFakeManager() {
    // StopLocalDatabaseManager before resetting it because that's what
    // ~V4LocalDatabaseManager expects.
    StopLocalDatabaseManager();
    v4_local_database_manager_ =
        base::WrapRefCounted(new FakeV4LocalDatabaseManager(
            base_dir_.GetPath(), erl_callback_, task_runner_));
    StartLocalDatabaseManager();
    WaitForTasksOnTaskRunner();
  }

  const SBThreatTypeSet usual_threat_types_ = CreateSBThreatTypeSet(
      {SB_THREAT_TYPE_URL_PHISHING, SB_THREAT_TYPE_URL_MALWARE,
       SB_THREAT_TYPE_URL_UNWANTED});

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  base::ScopedTempDir base_dir_;
  ExtendedReportingLevel extended_reporting_level_;
  ExtendedReportingLevelCallback erl_callback_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  scoped_refptr<V4LocalDatabaseManager> v4_local_database_manager_;
};

TEST_F(V4LocalDatabaseManagerTest, TestGetThreatSource) {
  WaitForTasksOnTaskRunner();
  EXPECT_EQ(ThreatSource::LOCAL_PVER4,
            v4_local_database_manager_->GetThreatSource());
}

TEST_F(V4LocalDatabaseManagerTest, TestIsSupported) {
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(v4_local_database_manager_->IsSupported());
}

TEST_F(V4LocalDatabaseManagerTest, TestCanCheckUrl) {
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(
      v4_local_database_manager_->CanCheckUrl(GURL("http://example.com/a/")));
  EXPECT_TRUE(
      v4_local_database_manager_->CanCheckUrl(GURL("https://example.com/a/")));
  EXPECT_TRUE(
      v4_local_database_manager_->CanCheckUrl(GURL("ftp://example.com/a/")));
  EXPECT_FALSE(
      v4_local_database_manager_->CanCheckUrl(GURL("adp://example.com/a/")));
}

TEST_F(V4LocalDatabaseManagerTest,
       TestCheckBrowseUrlWithEmptyStoresReturnsNoMatch) {
  WaitForTasksOnTaskRunner();
  // Both the stores are empty right now so CheckBrowseUrl should return true.
  EXPECT_TRUE(v4_local_database_manager_->CheckBrowseUrl(
      GURL("http://example.com/a/"), usual_threat_types_, nullptr));
}

TEST_F(V4LocalDatabaseManagerTest, TestCheckBrowseUrlWithFakeDbReturnsMatch) {
  WaitForTasksOnTaskRunner();

  std::string url_bad_no_scheme("example.com/bad/");
  FullHash bad_full_hash(crypto::SHA256HashString(url_bad_no_scheme));
  const HashPrefix bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);
  ReplaceV4Database(store_and_hash_prefixes);

  const GURL url_bad("https://" + url_bad_no_scheme);
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, nullptr));

  // Wait for PerformFullHashCheck to complete.
  WaitForTasksOnTaskRunner();
}

TEST_F(V4LocalDatabaseManagerTest, TestCheckCsdWhitelistWithPrefixMatch) {
  // Setup to receive full-hash misses. We won't make URL requests.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  std::string url_safe_no_scheme("example.com/safe/");
  FullHash safe_full_hash(crypto::SHA256HashString(url_safe_no_scheme));
  const HashPrefix safe_hash_prefix(safe_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlCsdWhitelistId(),
                                       safe_hash_prefix);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  TestAllowlistClient client(
      /* match_expected= */ false,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_CSD_WHITELIST);
  const GURL url_check("https://" + url_safe_no_scheme);
  EXPECT_EQ(AsyncMatch::ASYNC, v4_local_database_manager_->CheckCsdWhitelistUrl(
                                   url_check, &client));

  EXPECT_FALSE(client.callback_called());

  // Wait for PerformFullHashCheck to complete.
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.callback_called());
}

// This is like CsdWhitelistWithPrefixMatch, but we also verify the
// full-hash-match results in an appropriate callback value.
TEST_F(V4LocalDatabaseManagerTest,
       TestCheckCsdWhitelistWithPrefixTheFullMatch) {
  std::string url_safe_no_scheme("example.com/safe/");
  FullHash safe_full_hash(crypto::SHA256HashString(url_safe_no_scheme));

  // Setup to receive full-hash hit. We won't make URL requests.
  FullHashInfos infos(
      {{safe_full_hash, GetUrlCsdWhitelistId(), base::Time::Now()}});
  ScopedFakeGetHashProtocolManagerFactory pin(infos);
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  const HashPrefix safe_hash_prefix(safe_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlCsdWhitelistId(),
                                       safe_hash_prefix);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  TestAllowlistClient client(
      /* match_expected= */ true,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_CSD_WHITELIST);
  const GURL url_check("https://" + url_safe_no_scheme);
  EXPECT_EQ(AsyncMatch::ASYNC, v4_local_database_manager_->CheckCsdWhitelistUrl(
                                   url_check, &client));

  EXPECT_FALSE(client.callback_called());

  // Wait for PerformFullHashCheck to complete.
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.callback_called());
}

TEST_F(V4LocalDatabaseManagerTest, TestCheckCsdWhitelistWithFullMatch) {
  // Setup to receive full-hash misses. We won't make URL requests.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  std::string url_safe_no_scheme("example.com/safe/");
  FullHash safe_full_hash(crypto::SHA256HashString(url_safe_no_scheme));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlCsdWhitelistId(), safe_full_hash);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  TestAllowlistClient client(
      /* match_expected= */ false,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_CSD_WHITELIST);
  const GURL url_check("https://" + url_safe_no_scheme);
  EXPECT_EQ(AsyncMatch::MATCH, v4_local_database_manager_->CheckCsdWhitelistUrl(
                                   url_check, &client));

  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(client.callback_called());
}

TEST_F(V4LocalDatabaseManagerTest, TestCheckCsdWhitelistWithNoMatch) {
  // Setup to receive full-hash misses. We won't make URL requests.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // Add a full hash that won't match the URL we check.
  std::string url_safe_no_scheme("example.com/safe/");
  FullHash safe_full_hash(crypto::SHA256HashString(url_safe_no_scheme));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), safe_full_hash);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  TestAllowlistClient client(
      /* match_expected= */ true,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_CSD_WHITELIST);
  const GURL url_check("https://other.com/");
  EXPECT_EQ(
      AsyncMatch::NO_MATCH,
      v4_local_database_manager_->CheckCsdWhitelistUrl(url_check, &client));

  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(client.callback_called());
}

// When allowlist is unavailable, all URLS should be allowed.
TEST_F(V4LocalDatabaseManagerTest, TestCheckCsdWhitelistUnavailable) {
  // Setup to receive full-hash misses. We won't make URL requests.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  StoreAndHashPrefixes store_and_hash_prefixes;
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ false);

  TestAllowlistClient client(
      /* match_expected= */ false,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_CSD_WHITELIST);
  const GURL url_check("https://other.com/");
  EXPECT_EQ(AsyncMatch::MATCH, v4_local_database_manager_->CheckCsdWhitelistUrl(
                                   url_check, &client));

  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(client.callback_called());
}

TEST_F(V4LocalDatabaseManagerTest,
       TestCheckBrowseUrlReturnsNoMatchWhenDisabled) {
  WaitForTasksOnTaskRunner();

  // The same URL returns |false| in the previous test because
  // v4_local_database_manager_ is enabled.
  ForceDisableLocalDatabaseManager();

  EXPECT_TRUE(v4_local_database_manager_->CheckBrowseUrl(
      GURL("http://example.com/a/"), usual_threat_types_, nullptr));
}

// Hash prefix matches on the high confidence allowlist, but full hash match
// fails.
TEST_F(V4LocalDatabaseManagerTest,
       TestCheckUrlForHCAllowlistWithPrefixMatchButNoFullHashMatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({safe_browsing::kRealTimeUrlLookupEnabled}, {});

  std::string url_safe_no_scheme("example.com/safe/");
  FullHash safe_full_hash(crypto::SHA256HashString(url_safe_no_scheme));

  // Setup to receive full-hash misses. We won't make URL requests.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // Setup to match hash prefix in the local database.
  const HashPrefix safe_hash_prefix(safe_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlHighConfidenceAllowlistId(),
                                       safe_hash_prefix);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  // Setup the allowlist client to verify the callback.
  TestAllowlistClient client(
      /* match_expected= */ false,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST);

  // Lookup the high confidence allowlist.
  const GURL url_check("https://" + url_safe_no_scheme);
  EXPECT_EQ(AsyncMatch::ASYNC,
            v4_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
                url_check, &client));

  EXPECT_FALSE(client.callback_called());

  // Wait for PerformFullHashCheck to complete.
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.callback_called());
}

// Hash prefix matches on the high confidence allowlist, and subsequently the
// full hash also matches.
TEST_F(V4LocalDatabaseManagerTest,
       TestCheckUrlForHCAllowlistWithPrefixMatchAndFullHashMatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({safe_browsing::kRealTimeUrlLookupEnabled}, {});

  std::string url_safe_no_scheme("example.com/safe/");
  FullHash safe_full_hash(crypto::SHA256HashString(url_safe_no_scheme));

  // Setup to receive full-hash hit. We won't make URL requests.
  FullHashInfos infos(
      {{safe_full_hash, GetUrlHighConfidenceAllowlistId(), base::Time::Now()}});
  ScopedFakeGetHashProtocolManagerFactory pin(infos);
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // Setup to match hash prefix in the local database.
  const HashPrefix safe_hash_prefix(safe_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlHighConfidenceAllowlistId(),
                                       safe_hash_prefix);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  // Setup the allowlist client to verify the callback.
  TestAllowlistClient client(
      /* match_expected= */ true,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST);

  // Lookup the high confidence allowlist.
  const GURL url_check("https://" + url_safe_no_scheme);
  EXPECT_EQ(AsyncMatch::ASYNC,
            v4_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
                url_check, &client));

  EXPECT_FALSE(client.callback_called());

  // Wait for PerformFullHashCheck to complete.
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.callback_called());
}

// Full hash match on the high confidence allowlist. Returns |MATCH|
// synchronously and callback isn't called.
TEST_F(V4LocalDatabaseManagerTest,
       TestCheckUrlForHCAllowlistWithLocalFullHashMatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({safe_browsing::kRealTimeUrlLookupEnabled}, {});

  std::string url_safe_no_scheme("example.com/safe/");
  FullHash safe_full_hash(crypto::SHA256HashString(url_safe_no_scheme));

  // Setup to receive full-hash misses. We won't make URL requests.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // Setup to match full hash in the local database.
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlHighConfidenceAllowlistId(),
                                       safe_full_hash);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  // Setup the allowlist client to verify the callback isn't called.
  TestAllowlistClient client(
      /* match_expected= */ false,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST);
  const GURL url_check("https://" + url_safe_no_scheme);
  EXPECT_EQ(AsyncMatch::MATCH,
            v4_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
                url_check, &client));

  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(client.callback_called());
}

// Hash prefix has no match on the high confidence allowlist. Returns |NO_MATCH|
// synchronously and callback isn't called.
TEST_F(V4LocalDatabaseManagerTest, TestCheckUrlForHCAllowlistWithNoMatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({safe_browsing::kRealTimeUrlLookupEnabled}, {});

  std::string url_safe_no_scheme("example.com/safe/");
  FullHash safe_full_hash(crypto::SHA256HashString(url_safe_no_scheme));

  // Setup to receive full-hash misses. We won't make URL requests.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // Add a full hash that won't match the URL we check.
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), safe_full_hash);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  // Setup the allowlist client to verify the callback isn't called.
  TestAllowlistClient client(
      /* match_expected= */ false,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST);
  const GURL url_check("https://example.com/other/");
  EXPECT_EQ(AsyncMatch::NO_MATCH,
            v4_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
                url_check, &client));

  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(client.callback_called());
}

// When allowlist is unavailable, all URLS should be considered MATCH.
TEST_F(V4LocalDatabaseManagerTest, TestCheckUrlForHCAllowlistUnavailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({safe_browsing::kRealTimeUrlLookupEnabled}, {});

  // Setup to receive full-hash misses. We won't make URL requests.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // Setup local database as unavailable.
  StoreAndHashPrefixes store_and_hash_prefixes;
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ false);

  // Setup the allowlist client to verify the callback isn't called.
  TestAllowlistClient client(
      /* match_expected= */ false,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST);

  const GURL url_check("https://example.com/safe");
  EXPECT_EQ(AsyncMatch::MATCH,
            v4_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
                url_check, &client));

  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(client.callback_called());
}

TEST_F(V4LocalDatabaseManagerTest, TestGetSeverestThreatTypeAndMetadata) {
  base::HistogramTester histograms;
  WaitForTasksOnTaskRunner();

  FullHash fh_malware("Malware");
  FullHashInfo fhi_malware(fh_malware, GetUrlMalwareId(), base::Time::Now());
  fhi_malware.metadata.population_id = "malware_popid";

  FullHash fh_api("api");
  FullHashInfo fhi_api(fh_api, GetChromeUrlApiId(), base::Time::Now());
  fhi_api.metadata.population_id = "api_popid";

  FullHash fh_example("example");
  std::vector<FullHashInfo> fhis({fhi_malware, fhi_api});
  std::vector<FullHash> full_hashes({fh_malware, fh_example, fh_api});

  std::vector<SBThreatType> full_hash_threat_types(full_hashes.size(),
                                                   SB_THREAT_TYPE_SAFE);
  SBThreatType result_threat_type;
  ThreatMetadata metadata;
  FullHash matching_full_hash;

  const std::vector<SBThreatType> expected_full_hash_threat_types(
      {SB_THREAT_TYPE_URL_MALWARE, SB_THREAT_TYPE_SAFE,
       SB_THREAT_TYPE_API_ABUSE});

  v4_local_database_manager_->GetSeverestThreatTypeAndMetadata(
      fhis, full_hashes, &full_hash_threat_types, &result_threat_type,
      &metadata, &matching_full_hash);
  EXPECT_EQ(expected_full_hash_threat_types, full_hash_threat_types);

  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, result_threat_type);
  EXPECT_EQ("malware_popid", metadata.population_id);
  EXPECT_EQ(fh_malware, matching_full_hash);

  // Reversing the list has no effect.
  std::reverse(std::begin(fhis), std::end(fhis));
  full_hash_threat_types.assign(full_hashes.size(), SB_THREAT_TYPE_SAFE);

  v4_local_database_manager_->GetSeverestThreatTypeAndMetadata(
      fhis, full_hashes, &full_hash_threat_types, &result_threat_type,
      &metadata, &matching_full_hash);
  EXPECT_EQ(expected_full_hash_threat_types, full_hash_threat_types);
  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, result_threat_type);
  EXPECT_EQ("malware_popid", metadata.population_id);
  EXPECT_EQ(fh_malware, matching_full_hash);

  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4LocalDatabaseManager.ThreatInfoSize",
      /* sample */ 2, /* expected_count */ 2);
}

TEST_F(V4LocalDatabaseManagerTest, TestChecksAreQueued) {
  const GURL url("https://www.example.com/");
  TestClient client(SB_THREAT_TYPE_SAFE, url);
  EXPECT_TRUE(GetQueuedChecks().empty());
  v4_local_database_manager_->CheckBrowseUrl(url, usual_threat_types_, &client);
  // The database is unavailable so the check should get queued.
  EXPECT_EQ(1ul, GetQueuedChecks().size());

  // The following function waits for the DB to load.
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(GetQueuedChecks().empty());

  ResetV4Database();
  v4_local_database_manager_->CheckBrowseUrl(url, usual_threat_types_, &client);
  // The database is unavailable so the check should get queued.
  EXPECT_EQ(1ul, GetQueuedChecks().size());

  StopLocalDatabaseManager();
  EXPECT_TRUE(GetQueuedChecks().empty());
}

// Verify that a window where checks cannot be cancelled is closed.
TEST_F(V4LocalDatabaseManagerTest, CancelPending) {
  // Setup to receive full-hash misses.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));

  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // Put a match in the db that will cause a protocol-manager request.
  std::string url_bad_no_scheme("example.com/bad/");
  FullHash bad_full_hash(crypto::SHA256HashString(url_bad_no_scheme));
  const HashPrefix bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);
  ReplaceV4Database(store_and_hash_prefixes);

  const GURL url_bad("https://" + url_bad_no_scheme);
  // Test that a request flows through to the callback.
  {
    TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
    EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
        url_bad, usual_threat_types_, &client));
    EXPECT_FALSE(client.on_check_browse_url_result_called());
    WaitForTasksOnTaskRunner();
    EXPECT_TRUE(client.on_check_browse_url_result_called());
  }

  // Test that cancel prevents the callback from being called.
  {
    TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
    EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
        url_bad, usual_threat_types_, &client));
    v4_local_database_manager_->CancelCheck(&client);
    EXPECT_FALSE(client.on_check_browse_url_result_called());
    WaitForTasksOnTaskRunner();
    EXPECT_FALSE(client.on_check_browse_url_result_called());
  }
}

// When the database load flushes the queued requests, make sure that
// CancelCheck() is not fatal in the client callback.
TEST_F(V4LocalDatabaseManagerTest, CancelQueued) {
  const GURL url("http://example.com/a/");

  TestClient client1(SB_THREAT_TYPE_SAFE, url,
                     v4_local_database_manager_.get());
  TestClient client2(SB_THREAT_TYPE_SAFE, url);
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      url, usual_threat_types_, &client1));
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      url, usual_threat_types_, &client2));
  EXPECT_EQ(2ul, GetQueuedChecks().size());
  EXPECT_FALSE(client1.on_check_browse_url_result_called());
  EXPECT_FALSE(client2.on_check_browse_url_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client1.on_check_browse_url_result_called());
  EXPECT_TRUE(client2.on_check_browse_url_result_called());
}

// This test is somewhat similar to TestCheckBrowseUrlWithFakeDbReturnsMatch but
// it uses a fake V4LocalDatabaseManager to assert that PerformFullHashCheck is
// called async.
TEST_F(V4LocalDatabaseManagerTest, PerformFullHashCheckCalledAsync) {
  SetupFakeManager();

  std::string url_bad_no_scheme("example.com/bad/");
  FullHash bad_full_hash(crypto::SHA256HashString(url_bad_no_scheme));
  const HashPrefix bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);
  ReplaceV4Database(store_and_hash_prefixes);

  const GURL url_bad("https://" + url_bad_no_scheme);
  // The fake database returns a matched hash prefix.
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, nullptr));

  EXPECT_FALSE(FakeV4LocalDatabaseManager::PerformFullHashCheckCalled(
      v4_local_database_manager_));

  // Wait for PerformFullHashCheck to complete.
  WaitForTasksOnTaskRunner();

  EXPECT_TRUE(FakeV4LocalDatabaseManager::PerformFullHashCheckCalled(
      v4_local_database_manager_));
}

TEST_F(V4LocalDatabaseManagerTest, UsingWeakPtrDropsCallback) {
  SetupFakeManager();

  std::string url_bad_no_scheme("example.com/bad/");
  FullHash bad_full_hash(crypto::SHA256HashString(url_bad_no_scheme));
  const HashPrefix bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);
  ReplaceV4Database(store_and_hash_prefixes);

  const GURL url_bad("https://" + url_bad_no_scheme);
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, nullptr));
  v4_local_database_manager_->StopOnIOThread(true);

  // Release the V4LocalDatabaseManager object right away before the callback
  // gets called. When the callback gets called, without using a weak-ptr
  // factory, this leads to a use after free. However, using the weak-ptr means
  // that the callback is simply dropped.
  v4_local_database_manager_ = nullptr;

  // Wait for the tasks scheduled by StopOnIOThread to complete.
  WaitForTasksOnTaskRunner();
}

TEST_F(V4LocalDatabaseManagerTest, TestMatchDownloadWhitelistString) {
  SetupFakeManager();
  const std::string good_cert = "Good Cert";
  const std::string other_cert = "Other Cert";
  FullHash good_hash(crypto::SHA256HashString(good_cert));

  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetCertCsdDownloadWhitelistId(),
                                       good_hash);

  ReplaceV4Database(store_and_hash_prefixes, false /* not available */);
  // Verify it defaults to false when DB is not available.
  EXPECT_FALSE(
      v4_local_database_manager_->MatchDownloadWhitelistString(good_cert));

  ReplaceV4Database(store_and_hash_prefixes, true /* available */);
  // Not whitelisted.
  EXPECT_FALSE(
      v4_local_database_manager_->MatchDownloadWhitelistString(other_cert));
  // Whitelisted.
  EXPECT_TRUE(
      v4_local_database_manager_->MatchDownloadWhitelistString(good_cert));

  EXPECT_FALSE(FakeV4LocalDatabaseManager::PerformFullHashCheckCalled(
      v4_local_database_manager_));
}

TEST_F(V4LocalDatabaseManagerTest, TestMatchDownloadWhitelistUrl) {
  SetupFakeManager();
  GURL good_url("http://safe.com");
  GURL other_url("http://iffy.com");

  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlCsdDownloadWhitelistId(),
                                       HashForUrl(good_url));

  ReplaceV4Database(store_and_hash_prefixes, false /* not available */);
  // Verify it defaults to false when DB is not available.
  EXPECT_FALSE(v4_local_database_manager_->MatchDownloadWhitelistUrl(good_url));

  ReplaceV4Database(store_and_hash_prefixes, true /* available */);
  // Not whitelisted.
  EXPECT_FALSE(
      v4_local_database_manager_->MatchDownloadWhitelistUrl(other_url));
  // Whitelisted.
  EXPECT_TRUE(v4_local_database_manager_->MatchDownloadWhitelistUrl(good_url));

  EXPECT_FALSE(FakeV4LocalDatabaseManager::PerformFullHashCheckCalled(
      v4_local_database_manager_));
}

TEST_F(V4LocalDatabaseManagerTest, TestMatchMalwareIP) {
  SetupFakeManager();

  // >>> hashlib.sha1(socket.inet_pton(socket.AF_INET6,
  // '::ffff:192.168.1.2')).digest() + chr(128)
  // '\xb3\xe0z\xafAv#h\x9a\xcf<\xf3ee\x94\xda\xf6y\xb1\xad\x80'
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetIpMalwareId(),
                                       FullHash("\xB3\xE0z\xAF"
                                                "Av#h\x9A\xCF<\xF3"
                                                "ee\x94\xDA\xF6y\xB1\xAD\x80"));
  ReplaceV4Database(store_and_hash_prefixes);

  EXPECT_FALSE(v4_local_database_manager_->MatchMalwareIP(""));
  // Not blacklisted.
  EXPECT_FALSE(v4_local_database_manager_->MatchMalwareIP("192.168.1.1"));
  // Blacklisted.
  EXPECT_TRUE(v4_local_database_manager_->MatchMalwareIP("192.168.1.2"));

  EXPECT_FALSE(FakeV4LocalDatabaseManager::PerformFullHashCheckCalled(
      v4_local_database_manager_));
}

// This verifies the fix for race in http://crbug.com/660293
TEST_F(V4LocalDatabaseManagerTest, TestCheckBrowseUrlWithSameClientAndCancel) {
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));
  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(),
                                       HashPrefix("sن\340\t\006_"));
  ReplaceV4Database(store_and_hash_prefixes);

  GURL first_url("http://example.com/a");
  GURL second_url("http://example.com/");
  TestClient client(SB_THREAT_TYPE_SAFE, first_url);
  // The fake database returns a matched hash prefix.
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      first_url, usual_threat_types_, &client));

  // That check gets queued. Now, let's cancel the check. After this, we should
  // not receive a call for |OnCheckBrowseUrlResult| with |first_url|.
  v4_local_database_manager_->CancelCheck(&client);

  // Now, re-use that client but for |second_url|.
  client.mutable_expected_urls()->assign(1, second_url);
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      second_url, usual_threat_types_, &client));

  // Wait for PerformFullHashCheck to complete.
  WaitForTasksOnTaskRunner();
  // |on_check_browse_url_result_called_| is true only if OnCheckBrowseUrlResult
  // gets called with the |url| equal to |expected_url|, which is |second_url|
  // in
  // this test.
  EXPECT_TRUE(client.on_check_browse_url_result_called());
}

TEST_F(V4LocalDatabaseManagerTest, TestCheckResourceUrl) {
  // Setup to receive full-hash misses.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));

  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  std::string url_bad_no_scheme("example.com/bad/");
  FullHash bad_full_hash(crypto::SHA256HashString(url_bad_no_scheme));
  const HashPrefix bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetChromeUrlClientIncidentId(),
                                       bad_hash_prefix);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  const GURL url_bad("https://" + url_bad_no_scheme);
  TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
  EXPECT_FALSE(v4_local_database_manager_->CheckResourceUrl(url_bad, &client));
  EXPECT_FALSE(client.on_check_resource_url_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_resource_url_result_called());
}

TEST_F(V4LocalDatabaseManagerTest, TestSubresourceFilterCallback) {
  // Setup to receive full-hash misses.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));

  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  std::string url_bad_no_scheme("example.com/bad/");
  FullHash bad_full_hash(crypto::SHA256HashString(url_bad_no_scheme));
  const HashPrefix bad_hash_prefix(bad_full_hash.substr(0, 5));

  // Put a match in the db that will cause a protocol-manager request.
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlSubresourceFilterId(),
                                       bad_hash_prefix);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  const GURL url_bad("https://" + url_bad_no_scheme);
  // Test that a request flows through to the callback.
  {
    TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
    EXPECT_FALSE(v4_local_database_manager_->CheckUrlForSubresourceFilter(
        url_bad, &client));
    EXPECT_FALSE(client.on_check_browse_url_result_called());
    WaitForTasksOnTaskRunner();
    EXPECT_TRUE(client.on_check_browse_url_result_called());
  }
}

TEST_F(V4LocalDatabaseManagerTest, TestCheckResourceUrlReturnsBad) {
  // Setup to receive full-hash hit.
  std::string url_bad_no_scheme("example.com/bad/");
  FullHash bad_full_hash(crypto::SHA256HashString(url_bad_no_scheme));
  FullHashInfo fhi(bad_full_hash, GetChromeUrlClientIncidentId(), base::Time());
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({fhi}));

  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // Put a match in the db that will cause a protocol-manager request.
  const HashPrefix bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetChromeUrlClientIncidentId(),
                                       bad_hash_prefix);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  const GURL url_bad("https://" + url_bad_no_scheme);
  TestClient client(SB_THREAT_TYPE_BLACKLISTED_RESOURCE, url_bad);
  EXPECT_FALSE(v4_local_database_manager_->CheckResourceUrl(url_bad, &client));
  EXPECT_FALSE(client.on_check_resource_url_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_resource_url_result_called());
}

TEST_F(V4LocalDatabaseManagerTest, TestCheckExtensionIDsNothingBlacklisted) {
  // Setup to receive full-hash misses.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));

  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // bad_extension_id is in the local DB but the full hash won't match.
  const FullHash bad_extension_id("aaaabbbbccccdddd"),
      good_extension_id("ddddccccbbbbaaaa");

  // Put a match in the db that will cause a protocol-manager request.
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetChromeExtMalwareId(),
                                       bad_extension_id);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  const std::set<FullHash> expected_bad_crxs({});
  const std::set<FullHash> extension_ids({good_extension_id, bad_extension_id});
  TestExtensionClient client(expected_bad_crxs);
  EXPECT_FALSE(
      v4_local_database_manager_->CheckExtensionIDs(extension_ids, &client));
  EXPECT_FALSE(client.on_check_extensions_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_extensions_result_called());
}

TEST_F(V4LocalDatabaseManagerTest, TestCheckExtensionIDsOneIsBlacklisted) {
  // bad_extension_id is in the local DB and the full hash will match.
  const FullHash bad_extension_id("aaaabbbbccccdddd"),
      good_extension_id("ddddccccbbbbaaaa");
  FullHashInfo fhi(bad_extension_id, GetChromeExtMalwareId(), base::Time());

  // Setup to receive full-hash hit.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({fhi}));

  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // Put a match in the db that will cause a protocol-manager request.
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetChromeExtMalwareId(),
                                       bad_extension_id);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  const std::set<FullHash> expected_bad_crxs({bad_extension_id});
  const std::set<FullHash> extension_ids({good_extension_id, bad_extension_id});
  TestExtensionClient client(expected_bad_crxs);
  EXPECT_FALSE(
      v4_local_database_manager_->CheckExtensionIDs(extension_ids, &client));
  EXPECT_FALSE(client.on_check_extensions_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_extensions_result_called());
}

TEST_F(V4LocalDatabaseManagerTest, TestCheckDownloadUrlNothingBlacklisted) {
  // Setup to receive full-hash misses.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));

  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // Put a match in the db that will cause a protocol-manager request.
  std::string url_bad_no_scheme("example.com/bad/");
  FullHash bad_full_hash(crypto::SHA256HashString(url_bad_no_scheme));
  const HashPrefix bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalBinId(), bad_hash_prefix);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  const GURL url_bad("https://" + url_bad_no_scheme),
      url_good("https://example.com/good/");
  const std::vector<GURL> url_chain({url_good, url_bad});

  TestClient client(SB_THREAT_TYPE_SAFE, url_chain);
  EXPECT_FALSE(
      v4_local_database_manager_->CheckDownloadUrl(url_chain, &client));
  EXPECT_FALSE(client.on_check_download_urls_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_download_urls_result_called());
}

TEST_F(V4LocalDatabaseManagerTest, TestCheckDownloadUrlWithOneBlacklisted) {
  // Setup to receive full-hash hit.
  std::string url_bad_no_scheme("example.com/bad/");
  FullHash bad_full_hash(crypto::SHA256HashString(url_bad_no_scheme));
  FullHashInfo fhi(bad_full_hash, GetUrlMalBinId(), base::Time());
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({fhi}));

  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  const GURL url_bad("https://" + url_bad_no_scheme),
      url_good("https://example.com/good/");
  const std::vector<GURL> url_chain({url_good, url_bad});

  // Put a match in the db that will cause a protocol-manager request.
  const HashPrefix bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalBinId(), bad_hash_prefix);
  ReplaceV4Database(store_and_hash_prefixes, /* stores_available= */ true);

  TestClient client(SB_THREAT_TYPE_URL_BINARY_MALWARE, url_chain);
  EXPECT_FALSE(
      v4_local_database_manager_->CheckDownloadUrl(url_chain, &client));
  EXPECT_FALSE(client.on_check_download_urls_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_download_urls_result_called());
}

TEST_F(V4LocalDatabaseManagerTest, NotificationOnUpdate) {
  base::RunLoop run_loop;
  auto callback_subscription =
      v4_local_database_manager_->RegisterDatabaseUpdatedCallback(
          run_loop.QuitClosure());

  // Creates and associates a V4Database instance.
  StoreAndHashPrefixes store_and_hash_prefixes;
  ReplaceV4Database(store_and_hash_prefixes);

  v4_local_database_manager_->DatabaseUpdated();

  run_loop.Run();
}

TEST_F(V4LocalDatabaseManagerTest, FlagOneUrlAsPhishing) {
  SetupFakeManager();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "mark_as_phishing", "https://example.com/1/");
  PopulateArtificialDatabase();

  const GURL url_bad("https://example.com/1/");
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, nullptr));
  // PerformFullHashCheck will not be called if there is a match within the
  // artificial database
  EXPECT_FALSE(FakeV4LocalDatabaseManager::PerformFullHashCheckCalled(
      v4_local_database_manager_));

  const GURL url_good("https://other.example.com");
  EXPECT_TRUE(v4_local_database_manager_->CheckBrowseUrl(
      url_good, usual_threat_types_, nullptr));

  StopLocalDatabaseManager();
}

TEST_F(V4LocalDatabaseManagerTest, FlagOneUrlAsMalware) {
  SetupFakeManager();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "mark_as_malware", "https://example.com/1/");
  PopulateArtificialDatabase();

  const GURL url_bad("https://example.com/1/");
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, nullptr));
  // PerformFullHashCheck will not be called if there is a match within the
  // artificial database
  EXPECT_FALSE(FakeV4LocalDatabaseManager::PerformFullHashCheckCalled(
      v4_local_database_manager_));

  const GURL url_good("https://other.example.com");
  EXPECT_TRUE(v4_local_database_manager_->CheckBrowseUrl(
      url_good, usual_threat_types_, nullptr));

  StopLocalDatabaseManager();
}

TEST_F(V4LocalDatabaseManagerTest, FlagOneUrlAsUWS) {
  SetupFakeManager();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "mark_as_uws", "https://example.com/1/");
  PopulateArtificialDatabase();

  const GURL url_bad("https://example.com/1/");
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, nullptr));
  // PerformFullHashCheck will not be called if there is a match within the
  // artificial database
  EXPECT_FALSE(FakeV4LocalDatabaseManager::PerformFullHashCheckCalled(
      v4_local_database_manager_));

  const GURL url_good("https://other.example.com");
  EXPECT_TRUE(v4_local_database_manager_->CheckBrowseUrl(
      url_good, usual_threat_types_, nullptr));

  StopLocalDatabaseManager();
}

TEST_F(V4LocalDatabaseManagerTest, FlagMultipleUrls) {
  SetupFakeManager();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "mark_as_phishing", "https://example.com/1/");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "mark_as_malware", "https://2.example.com");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "mark_as_uws", "https://example.test.com");
  PopulateArtificialDatabase();

  const GURL url_phishing("https://example.com/1/");
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      url_phishing, usual_threat_types_, nullptr));
  const GURL url_malware("https://2.example.com");
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      url_malware, usual_threat_types_, nullptr));
  const GURL url_uws("https://example.test.com");
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      url_uws, usual_threat_types_, nullptr));
  // PerformFullHashCheck will not be called if there is a match within the
  // artificial database
  EXPECT_FALSE(FakeV4LocalDatabaseManager::PerformFullHashCheckCalled(
      v4_local_database_manager_));

  const GURL url_good("https://other.example.com");
  EXPECT_TRUE(v4_local_database_manager_->CheckBrowseUrl(
      url_good, usual_threat_types_, nullptr));

  StopLocalDatabaseManager();
}

}  // namespace safe_browsing
