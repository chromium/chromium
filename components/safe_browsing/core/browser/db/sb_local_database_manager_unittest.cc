// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/sb_local_database_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_view_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_simple_task_runner.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/sb_database.h"
#include "components/safe_browsing/core/browser/db/sb_store.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "crypto/hash.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/platform_test.h"

// TODO(crbug.com/362791941): Handle v4 references
// TODO(crbug.com/362791941): Convert |comments| to `comments`
namespace safe_browsing {

using enum ExtendedReportingLevel;
using HighConfidenceAllowlistCheckLoggingDetails =
    SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails;
using CheckUrlForHighConfidenceAllowlistFuture = base::test::
    TestFuture<bool, std::optional<HighConfidenceAllowlistCheckLoggingDetails>>;

namespace {
typedef std::vector<FullHashInfo> FullHashInfos;

// Utility function for populating hashes.
FullHashStr HashForUrl(const GURL& url) {
  std::vector<FullHashStr> full_hashes;
  SBProtocolManagerUtil::UrlToFullHashes(url, &full_hashes);
  // ASSERT_GE(full_hashes.size(), 1u);
  return full_hashes[0];
}

const int kDefaultStoreFileSizeInBytes = 320000;

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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), full_hash_infos_));
  }

 private:
  FullHashInfos full_hash_infos_;
};

class FakeGetHashProtocolManagerFactory
    : public V4GetHashProtocolManagerFactory {
 public:
  explicit FakeGetHashProtocolManagerFactory(
      const FullHashInfos& full_hash_infos)
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
  explicit ScopedFakeGetHashProtocolManagerFactory(
      const FullHashInfos& full_hash_infos) {
    V4GetHashProtocolManager::RegisterFactory(
        std::make_unique<FakeGetHashProtocolManagerFactory>(full_hash_infos));
  }
  ~ScopedFakeGetHashProtocolManagerFactory() {
    V4GetHashProtocolManager::RegisterFactory(nullptr);
  }
};

// Use this if you want V5 GetFullHashes() to always return prescribed results.
class FakeV5GetHashProtocolManager : public V5GetHashProtocolManager {
 public:
  FakeV5GetHashProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& config,
      V5SearchHashesCache* cache,
      SBThreatType threat_type,
      ThreatMetadata metadata)
      : V5GetHashProtocolManager(url_loader_factory, config, cache),
        threat_type_(threat_type),
        metadata_(metadata) {}

  FakeV5GetHashProtocolManager(const FakeV5GetHashProtocolManager&) = delete;
  FakeV5GetHashProtocolManager& operator=(const FakeV5GetHashProtocolManager&) =
      delete;

  ~FakeV5GetHashProtocolManager() override = default;

  void GetFullHashes(const std::map<FullHashStr, std::vector<SBThreatType>>&
                         full_hash_to_threat_types,
                     FullHashCallback callback) override {
    last_full_hash_to_threat_types_ = full_hash_to_threat_types;
    if (hold_callback_) {
      held_callback_ = std::move(callback);
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), threat_type_, metadata_));
  }

  const std::map<FullHashStr, std::vector<SBThreatType>>&
  last_full_hash_to_threat_types() const {
    return last_full_hash_to_threat_types_;
  }

  void set_hold_callback(bool hold) { hold_callback_ = hold; }

  void RunHeldCallback() {
    if (!held_callback_.is_null()) {
      std::move(held_callback_).Run(threat_type_, metadata_);
    }
  }

 private:
  SBThreatType threat_type_;
  ThreatMetadata metadata_;
  std::map<FullHashStr, std::vector<SBThreatType>>
      last_full_hash_to_threat_types_;
  bool hold_callback_ = false;
  FullHashCallback held_callback_;
};

}  // namespace

// Use this if you want to use a real V4GetHashProtocolManager, but substitute
// the server response via the |test_url_loader_factory|.
// This must be defined outside the anonymous namespace so that it can be
// included as a friend class for V4GetHashProtocolManager.
class GetHashProtocolManagerFactoryWithTestUrlLoader
    : public V4GetHashProtocolManagerFactory {
 public:
  GetHashProtocolManagerFactoryWithTestUrlLoader(
      network::TestURLLoaderFactory* test_url_loader_factory) {
    test_shared_loader_factory_ = test_url_loader_factory->GetSafeWeakWrapper();
  }

  std::unique_ptr<V4GetHashProtocolManager> CreateProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const StoresToCheck& stores_to_check,
      const V4ProtocolConfig& config) override {
    return base::WrapUnique(new V4GetHashProtocolManager(
        test_shared_loader_factory_, stores_to_check, config));
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

// Use ScopedGetHashProtocolManagerFactoryWithTestUrlLoader in scope, then
// reset. You should make sure the DatabaseManager is created _after_ this.
class ScopedGetHashProtocolManagerFactoryWithTestUrlLoader {
 public:
  ScopedGetHashProtocolManagerFactoryWithTestUrlLoader(
      network::TestURLLoaderFactory* test_url_loader_factory) {
    V4GetHashProtocolManager::RegisterFactory(
        std::make_unique<GetHashProtocolManagerFactoryWithTestUrlLoader>(
            test_url_loader_factory));
  }
  ~ScopedGetHashProtocolManagerFactoryWithTestUrlLoader() {
    V4GetHashProtocolManager::RegisterFactory(nullptr);
  }
};

class FakeSBStore : public SBStore {
 public:
  explicit FakeSBStore(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : SBStore(task_runner, base::FilePath()) {}

  FakeSBStore(const FakeSBStore&) = delete;
  FakeSBStore& operator=(const FakeSBStore&) = delete;

  ~FakeSBStore() override = default;

  int64_t RecordAndReturnFileSize(const std::string& base_metric) override {
    return 0;
  }
  void Reset() override {}
  bool VerifyChecksum() override { return true; }
  void ApplyUpdate(std::unique_ptr<SBUpdateResponse> response,
                   const scoped_refptr<base::SequencedTaskRunner>& runner,
                   UpdatedStoreReadyCallback callback) override {}
  HashPrefixStr GetMatchingHashPrefix(const FullHashStr& full_hash) override {
    return "";
  }
  std::string GetMetricPrefix() const override { return "Fake"; }
  const std::string& GetStoreState() const override { return state_; }

 private:
  std::string state_ = "state";
};

class FakeSBDatabase : public SBDatabase {
 public:
  static void Create(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      std::unique_ptr<StoreMap> store_map,
      const StoreAndHashPrefixes& store_and_hash_prefixes,
      NewDatabaseReadyCallback new_db_callback,
      bool stores_available,
      int64_t store_file_size) {
    // Mimics SBDatabase::Create
    const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner =
        base::SequencedTaskRunner::GetCurrentDefault();
    db_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeSBDatabase::CreateOnTaskRunner, db_task_runner,
                       std::move(store_map), store_and_hash_prefixes,
                       callback_task_runner, std::move(new_db_callback),
                       stores_available, store_file_size));
  }

  // SBDatabase implementation
  void GetStoresMatchingFullHash(
      const std::vector<FullHashStr>& full_hashes,
      const StoresToCheck& stores_to_check,
      base::OnceCallback<void(DbLookupResult)> callback) override {
    DbLookupResult lookup_result;
    lookup_result.db_thread_post_time = base::TimeTicks::Now();
    lookup_result.db_thread_start_time = base::TimeTicks::Now();

    for (const auto& full_hash : full_hashes) {
      for (const StoreAndHashPrefix& stored_sahp : store_and_hash_prefixes_) {
        if (stores_to_check.count(stored_sahp.list_id) == 0) {
          continue;
        }
        const PrefixSize& prefix_size = stored_sahp.hash_prefix.size();
        if (!full_hash.compare(0, prefix_size, stored_sahp.hash_prefix)) {
          lookup_result.results[full_hash].push_back(stored_sahp);
        }
      }
    }
    lookup_result.db_thread_end_time = base::TimeTicks::Now();
    // Simulate async behavior of real implementation.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(lookup_result)));
  }

  // SBDatabase implementation
  int64_t GetStoreSizeInBytes(const ListIdentifier& store) const override {
    return store_file_size_;
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
      const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner,
      NewDatabaseReadyCallback new_db_callback,
      bool stores_available,
      int64_t store_file_size) {
    // Mimics the semantics of SBDatabase::CreateOnTaskRunner
    std::unique_ptr<FakeSBDatabase, base::OnTaskRunnerDeleter> fake_sb_database(
        new FakeSBDatabase(db_task_runner, std::move(store_map),
                           store_and_hash_prefixes, stores_available,
                           store_file_size),
        base::OnTaskRunnerDeleter(db_task_runner));
    callback_task_runner->PostTask(FROM_HERE,
                                   base::BindOnce(std::move(new_db_callback),
                                                  std::move(fake_sb_database)));
  }

  FakeSBDatabase(const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
                 std::unique_ptr<StoreMap> store_map,
                 const StoreAndHashPrefixes& store_and_hash_prefixes,
                 bool stores_available,
                 int64_t store_file_size)
      : SBDatabase(db_task_runner, std::move(store_map)),
        store_and_hash_prefixes_(store_and_hash_prefixes),
        stores_available_(stores_available),
        store_file_size_(store_file_size) {}

  const StoreAndHashPrefixes store_and_hash_prefixes_;
  const bool stores_available_;
  int64_t store_file_size_;
};

// TODO(nparker): This might be simpler with a mock and EXPECT calls.
// That would also catch unexpected calls.
class TestClient : public SafeBrowsingDatabaseManager::Client {
 public:
  TestClient(SBThreatType sb_threat_type,
             const GURL& url,
             SBLocalDatabaseManager* manager_to_cancel = nullptr)
      : SafeBrowsingDatabaseManager::Client(GetPassKeyForTesting()),
        expected_sb_threat_type_(sb_threat_type),
        expected_urls_(1, url),
        manager_to_cancel_(manager_to_cancel) {}

  TestClient(SBThreatType sb_threat_type, const std::vector<GURL>& url_chain)
      : SafeBrowsingDatabaseManager::Client(GetPassKeyForTesting()),
        expected_sb_threat_type_(sb_threat_type),
        expected_urls_(url_chain) {}

  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type) override {
    ASSERT_EQ(expected_urls_[0], url);
    ASSERT_EQ(expected_sb_threat_type_, threat_type);
    on_check_browse_url_result_call_count_++;
    if (manager_to_cancel_) {
      manager_to_cancel_->CancelCheck(this);
    }
  }

  void OnCheckSubresourceFilterUrlResult(
      const GURL& url,
      SBThreatType threat_type,
      const SubresourceFilterMatch& subresource_filter_match) override {
    ASSERT_EQ(expected_urls_[0], url);
    ASSERT_EQ(expected_sb_threat_type_, threat_type);
    on_check_subresource_filter_url_result_called_ = true;
    if (manager_to_cancel_) {
      manager_to_cancel_->CancelCheck(this);
    }
  }

  void OnCheckDownloadUrlResult(const std::vector<GURL>& url_chain,
                                SBThreatType threat_type) override {
    ASSERT_EQ(expected_urls_, url_chain);
    ASSERT_EQ(expected_sb_threat_type_, threat_type);
    on_check_download_urls_result_called_ = true;
  }

  std::vector<GURL>* mutable_expected_urls() { return &expected_urls_; }

  bool on_check_browse_url_result_called() const {
    return on_check_browse_url_result_call_count_ > 0;
  }
  int on_check_browse_url_result_call_count() const {
    return on_check_browse_url_result_call_count_;
  }
  bool on_check_subresource_filter_url_result_called() {
    return on_check_subresource_filter_url_result_called_;
  }
  bool on_check_download_urls_result_called() {
    return on_check_download_urls_result_called_;
  }

  base::WeakPtr<V5GetHashProtocolManager> GetV5GetHashProtocolManager()
      override {
    return v5_get_hash_protocol_manager_;
  }

  void SetV5GetHashProtocolManager(
      base::WeakPtr<V5GetHashProtocolManager> manager) {
    v5_get_hash_protocol_manager_ = manager;
  }

 private:
  const SBThreatType expected_sb_threat_type_;
  std::vector<GURL> expected_urls_;
  int on_check_browse_url_result_call_count_ = 0;
  bool on_check_subresource_filter_url_result_called_ = false;
  bool on_check_download_urls_result_called_ = false;
  raw_ptr<SBLocalDatabaseManager> manager_to_cancel_;
  base::WeakPtr<V5GetHashProtocolManager> v5_get_hash_protocol_manager_;
};

class TestAllowlistClient : public SafeBrowsingDatabaseManager::Client {
 public:
  // |match_expected| specifies whether a full hash match is expected.
  // |expected_sb_threat_type| identifies which callback method to expect to get
  // called.
  explicit TestAllowlistClient(bool match_expected,
                               SBThreatType expected_sb_threat_type)
      : SafeBrowsingDatabaseManager::Client(GetPassKeyForTesting()),
        expected_sb_threat_type_(expected_sb_threat_type),
        match_expected_(match_expected) {}

  void OnCheckAllowlistUrlResult(bool is_allowlisted) override {
    EXPECT_EQ(match_expected_, is_allowlisted);
    EXPECT_EQ(SBThreatType::SB_THREAT_TYPE_CSD_ALLOWLIST,
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
  TestExtensionClient(const std::set<FullHashStr>& expected_bad_crxs)
      : SafeBrowsingDatabaseManager::Client(GetPassKeyForTesting()),
        expected_bad_crxs_(expected_bad_crxs),
        on_check_extensions_result_called_(false) {}

  void OnCheckExtensionsResult(const std::set<FullHashStr>& bad_crxs) override {
    EXPECT_EQ(expected_bad_crxs_, bad_crxs);
    on_check_extensions_result_called_ = true;
  }

  bool on_check_extensions_result_called() {
    return on_check_extensions_result_called_;
  }

 private:
  const std::set<FullHashStr> expected_bad_crxs_;
  bool on_check_extensions_result_called_;
};

class FakeSBLocalDatabaseManager : public SBLocalDatabaseManager {
 public:
  FakeSBLocalDatabaseManager(
      const base::FilePath& base_path,
      ExtendedReportingLevelCallback extended_reporting_level_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : SBLocalDatabaseManager(base_path,
                               extended_reporting_level_callback,
                               base::SequencedTaskRunner::GetCurrentDefault(),
                               base::SequencedTaskRunner::GetCurrentDefault(),
                               task_runner),
        perform_full_hash_check_called_(false) {}

  // SBLocalDatabaseManager impl:
  void PerformFullHashCheck(std::unique_ptr<PendingCheck> check) override {
    perform_full_hash_check_called_ = true;
    RemovePendingCheck(pending_checks_.find(check.get()));
  }

  static bool PerformFullHashCheckCalled(
      scoped_refptr<safe_browsing::SBLocalDatabaseManager>& sb_ldbm) {
    FakeSBLocalDatabaseManager* fake =
        static_cast<FakeSBLocalDatabaseManager*>(sb_ldbm.get());
    return fake->perform_full_hash_check_called_;
  }

  void OnFullHashResponseV4(
      std::unique_ptr<PendingCheck> check,
      const std::vector<FullHashInfo>& full_hash_infos) override {
    RemovePendingCheck(pending_checks_.find(check.get()));
  }

 private:
  ~FakeSBLocalDatabaseManager() override = default;

  bool perform_full_hash_check_called_;
};

class SBLocalDatabaseManagerTest : public PlatformTest {
 public:
  using enum SBThreatType;

  SBLocalDatabaseManagerTest() : task_runner_(new base::TestSimpleTaskRunner) {}

  void SetUp() override {
    PlatformTest::SetUp();

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());
    DVLOG(1) << "base_dir_: " << base_dir_.GetPath().value();

    extended_reporting_level_ = SBER_LEVEL_OFF;
    erl_callback_ = base::BindRepeating(
        &SBLocalDatabaseManagerTest::GetExtendedReportingLevel,
        base::Unretained(this));

    sb_local_database_manager_ =
        base::WrapRefCounted(new SBLocalDatabaseManager(
            base_dir_.GetPath(), erl_callback_,
            base::SequencedTaskRunner::GetCurrentDefault(),
            base::SequencedTaskRunner::GetCurrentDefault(), task_runner_));

    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (std::string(test_info->name()) != "QueuedCheckWithFullHash") {
      StartLocalDatabaseManager();
    }
  }

  void TearDown() override {
    ShutdownLocalDatabaseManager();

    PlatformTest::TearDown();
  }

  void ForceDisableLocalDatabaseManager() {
    sb_local_database_manager_->enabled_ = false;
  }

  void ForceEnableLocalDatabaseManager() {
    sb_local_database_manager_->enabled_ = true;
  }

  const SBLocalDatabaseManager::QueuedChecks& GetQueuedChecks() {
    return sb_local_database_manager_->queued_checks_;
  }

  const SBLocalDatabaseManager::PendingChecks& GetPendingChecks() {
    return sb_local_database_manager_->pending_checks_;
  }

  ExtendedReportingLevel GetExtendedReportingLevel() {
    return extended_reporting_level_;
  }

  void PopulateArtificialDatabase() {
    sb_local_database_manager_->PopulateArtificialDatabase();
  }

  void V5UpdateRequestCompleted(
      std::optional<std::map<ListIdentifier, V5::HashList>>
          parsed_server_response) {
    sb_local_database_manager_->V5UpdateRequestCompleted(
        std::move(parsed_server_response));
  }

  void DatabaseUpdated() { sb_local_database_manager_->DatabaseUpdated(); }

  const ListInfos& GetListInfos() const {
    return sb_local_database_manager_->list_infos_;
  }

  void ReplaceSBDatabase(const StoreAndHashPrefixes& store_and_hash_prefixes,
                         bool stores_available = false,
                         int64_t store_file_size = kDefaultStoreFileSizeInBytes,
                         bool wait_for_tasks_for_new_db = true) {
    // Disable the SBLocalDatabaseManager first so that if the callback to
    // verify checksum has been scheduled, then it doesn't do anything when it
    // is called back.
    ForceDisableLocalDatabaseManager();
    // Wait to make sure that the callback gets executed if it has already been
    // scheduled.
    WaitForTasksOnTaskRunner();
    // Re-enable the SBLocalDatabaseManager otherwise the checks won't work and
    // the fake database won't be set either.
    ForceEnableLocalDatabaseManager();

    // Populate fake stores for active lists so GetStoreStateMap() returns
    // non-empty store states, which V5UpdateProtocolManager expects when
    // scheduling updates on database readiness.
    auto store_map = std::make_unique<StoreMap>();
    for (const auto& info : GetListInfos()) {
      if (info.fetch_updates()) {
        store_map->emplace(info.list_id(),
                           SBStorePtr(new FakeSBStore(task_runner_),
                                      SBStoreDeleter(task_runner_)));
      }
    }

    NewDatabaseReadyCallback db_ready_callback = base::BindOnce(
        &SBLocalDatabaseManager::DatabaseReadyForChecks,
        base::Unretained(sb_local_database_manager_.get()), base::Time::Now());
    FakeSBDatabase::Create(
        task_runner_, std::move(store_map), store_and_hash_prefixes,
        std::move(db_ready_callback), stores_available, store_file_size);
    if (wait_for_tasks_for_new_db) {
      WaitForTasksOnTaskRunner();
    }
  }

  void ResetLocalDatabaseManager() {
    StopLocalDatabaseManager();
    sb_local_database_manager_ =
        base::WrapRefCounted(new SBLocalDatabaseManager(
            base_dir_.GetPath(), erl_callback_,
            base::SequencedTaskRunner::GetCurrentDefault(),
            base::SequencedTaskRunner::GetCurrentDefault(), task_runner_));
    StartLocalDatabaseManager();
  }

  void ResetSBDatabase() { sb_local_database_manager_->sb_database_.reset(); }

  void StartLocalDatabaseManager() {
    sb_local_database_manager_->StartOnUIThread(test_shared_loader_factory_,
                                                GetTestV4ProtocolConfig());
  }

  void StopLocalDatabaseManager() {
    if (sb_local_database_manager_) {
      sb_local_database_manager_->StopOnUIThread(/*shutdown=*/false);
    }

    // Force destruction of the database.
    WaitForTasksOnTaskRunner();
  }

  void ShutdownLocalDatabaseManager() {
    if (sb_local_database_manager_) {
      sb_local_database_manager_->StopOnUIThread(/*shutdown=*/true);
    }

    // Force destruction of the database.
    WaitForTasksOnTaskRunner();
  }

  void WaitForTasksOnTaskRunner() {
    // Wait for tasks on the task runner so we're sure that the
    // SBLocalDatabaseManager has read the data from disk.
    task_runner_->RunPendingTasks();
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  // For those tests that need the fake manager
  void SetupFakeManager() {
    // StopLocalDatabaseManager before resetting it because that's what
    // ~SBLocalDatabaseManager expects.
    StopLocalDatabaseManager();
    sb_local_database_manager_ =
        base::WrapRefCounted(new FakeSBLocalDatabaseManager(
            base_dir_.GetPath(), erl_callback_, task_runner_));
    StartLocalDatabaseManager();
    WaitForTasksOnTaskRunner();
  }

  void ValidateHighConfidenceAllowlistHistograms(
      std::optional<HighConfidenceAllowlistCheckLoggingDetails> logging_details,
      bool expected_all_stores_available_sample,
      bool expected_allowlist_too_small_sample) {
    ASSERT_TRUE(logging_details.has_value());
    EXPECT_EQ(logging_details.value().were_all_stores_available,
              expected_all_stores_available_sample);
    EXPECT_EQ(logging_details.value().was_allowlist_size_too_small,
              expected_allowlist_too_small_sample);
  }

  const SBThreatTypeSet usual_threat_types_ = CreateSBThreatTypeSet(
      {SB_THREAT_TYPE_URL_PHISHING, SB_THREAT_TYPE_URL_MALWARE,
       SB_THREAT_TYPE_URL_UNWANTED});

  void SetUpV5Client(TestClient& client,
                     SBThreatType threat_type,
                     ThreatMetadata metadata) {
    if (!v5_fake_manager_) {
      v5_cache_ =
          std::make_unique<V5SearchHashesCache>(/*history_service=*/nullptr);
      v5_fake_manager_ = std::make_unique<FakeV5GetHashProtocolManager>(
          test_shared_loader_factory_, GetTestV4ProtocolConfig(),
          v5_cache_.get(), threat_type, metadata);
    }
    client.SetV5GetHashProtocolManager(v5_fake_manager_->GetWeakPtr());
  }

  FakeV5GetHashProtocolManager* v5_fake_manager() {
    return v5_fake_manager_.get();
  }

  void ResetV5FakeManager() { v5_fake_manager_.reset(); }

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  base::ScopedTempDir base_dir_;
  ExtendedReportingLevel extended_reporting_level_;
  ExtendedReportingLevelCallback erl_callback_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  scoped_refptr<SBLocalDatabaseManager> sb_local_database_manager_;
  std::unique_ptr<V5SearchHashesCache> v5_cache_;
  std::unique_ptr<FakeV5GetHashProtocolManager> v5_fake_manager_;
};

class SBLocalDatabaseManagerTest_V4V5
    : public SBLocalDatabaseManagerTest,
      public ::testing::WithParamInterface<bool> {
 public:
  SBLocalDatabaseManagerTest_V4V5() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(kLocalListsUseSBv5);
    } else {
      feature_list_.InitAndDisableFeature(kLocalListsUseSBv5);
    }
  }

  bool IsV5() const { return GetParam(); }

  void SetUpV5ClientIfNeeded(TestClient& client,
                             SBThreatType threat_type,
                             ThreatMetadata metadata) {
    if (IsV5()) {
      SetUpV5Client(client, threat_type, metadata);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(SBLocalDatabaseManagerTest_V4V5, TestGetThreatSource) {
  WaitForTasksOnTaskRunner();
  ThreatSource expected_threat_source =
      GetParam() ? ThreatSource::LOCAL_PVER5_LOCAL_BLOCKLIST
                 : ThreatSource::LOCAL_PVER4;
  EXPECT_EQ(expected_threat_source,
            sb_local_database_manager_->GetBrowseUrlThreatSource(
                CheckBrowseUrlType::kHashDatabase));
  EXPECT_EQ(expected_threat_source,
            sb_local_database_manager_->GetNonBrowseUrlThreatSource());
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, TestCanCheckUrl) {
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(
      sb_local_database_manager_->CanCheckUrl(GURL("http://example.com/a/")));
  EXPECT_TRUE(
      sb_local_database_manager_->CanCheckUrl(GURL("https://example.com/a/")));
  EXPECT_TRUE(
      sb_local_database_manager_->CanCheckUrl(GURL("ftp://example.com/a/")));
  EXPECT_FALSE(
      sb_local_database_manager_->CanCheckUrl(GURL("adp://example.com/a/")));
}

TEST_P(SBLocalDatabaseManagerTest_V4V5,
       TestCheckBrowseUrlWithEmptyStoresReturnsNoMatch) {
  WaitForTasksOnTaskRunner();
  const GURL url("http://example.com/a/");
  TestClient client(SB_THREAT_TYPE_SAFE, url);
  SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());
  bool result = sb_local_database_manager_->CheckBrowseUrl(
      url, usual_threat_types_, &client, CheckBrowseUrlType::kHashDatabase);

  EXPECT_FALSE(result);
  EXPECT_FALSE(client.on_check_browse_url_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_browse_url_result_called());
}

TEST_P(SBLocalDatabaseManagerTest_V4V5,
       TestCheckBrowseUrlWithFakeDbReturnsMatch) {
  base::HistogramTester histograms;
  // Setup to receive full-hash misses. We won't make URL requests.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  std::string url_bad_no_scheme("example.com/bad/");
  FullHashStr bad_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_bad_no_scheme))));
  const HashPrefixStr bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes);

  const GURL url_bad("https://" + url_bad_no_scheme);
  TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
  SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase));

  // Wait for PerformFullHashCheck to complete.
  WaitForTasksOnTaskRunner();

  EXPECT_TRUE(client.on_check_browse_url_result_called());

  if (IsV5()) {
    CHECK(v5_fake_manager());
    const auto& last_map = v5_fake_manager()->last_full_hash_to_threat_types();
    auto it = last_map.find(bad_full_hash);
    ASSERT_NE(it, last_map.end());
    EXPECT_EQ(it->second,
              std::vector<SBThreatType>{SB_THREAT_TYPE_URL_MALWARE});
  }

  if (GetParam()) {
    histograms.ExpectTotalCount("SafeBrowsing.V5CheckUrl.TimeTaken.LocalLookup",
                                1);
    histograms.ExpectTotalCount(
        "SafeBrowsing.V5CheckUrl.TimeTaken.LocalLookup.UiCallbackQueueDelay",
        1);
    histograms.ExpectTotalCount(
        "SafeBrowsing.V5CheckUrl.TimeTaken.GetFullHashQueueDelay", 1);
    histograms.ExpectTotalCount(
        "SafeBrowsing.V5CheckUrl.TimeTaken.GetFullHashDuration", 1);
    histograms.ExpectTotalCount(
        "SafeBrowsing.V5CheckUrl.TimeTaken.ResponseProcessingDuration", 1);
  } else {
    histograms.ExpectTotalCount("SafeBrowsing.V4CheckUrl.TimeTaken.LocalLookup",
                                1);
    histograms.ExpectTotalCount(
        "SafeBrowsing.V4CheckUrl.TimeTaken.LocalLookup.UiCallbackQueueDelay",
        1);
    histograms.ExpectTotalCount(
        "SafeBrowsing.V4CheckUrl.TimeTaken.GetFullHashQueueDelay", 1);
    histograms.ExpectTotalCount(
        "SafeBrowsing.V4CheckUrl.TimeTaken.GetFullHashDuration", 1);
    histograms.ExpectTotalCount(
        "SafeBrowsing.V4CheckUrl.TimeTaken.ResponseProcessingDuration", 1);
  }
  histograms.ExpectTotalCount("SafeBrowsing.SBCheckUrl.TimeTaken.LocalLookup",
                              1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.SBCheckUrl.TimeTaken.LocalLookup.UiCallbackQueueDelay", 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.SBCheckUrl.TimeTaken.GetFullHashQueueDelay", 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.SBCheckUrl.TimeTaken.GetFullHashDuration", 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.SBCheckUrl.TimeTaken.ResponseProcessingDuration", 1);
}

class SBLocalDatabaseManagerTest_V5 : public SBLocalDatabaseManagerTest {
 public:
  SBLocalDatabaseManagerTest_V5() {
    feature_list_.InitAndEnableFeature(kLocalListsUseSBv5);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SBLocalDatabaseManagerTest_V5,
       TestCheckBrowseUrl_V5_NullManagerReturnsSafe) {
  std::string url_bad_no_scheme("example.com/bad/");
  FullHashStr bad_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_bad_no_scheme))));
  const GURL url_bad("https://" + url_bad_no_scheme);

  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  const HashPrefixStr bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes);

  TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase));

  WaitForTasksOnTaskRunner();

  EXPECT_TRUE(client.on_check_browse_url_result_called());
}

// Test that cancelling a check while GetFullHashes is in flight prevents the
// callback from being called.
TEST_F(SBLocalDatabaseManagerTest_V5, CancelWhileGetFullHashesInFlight) {
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  std::string url_bad_no_scheme("example.com/bad/");
  FullHashStr bad_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_bad_no_scheme))));
  const HashPrefixStr bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes);

  const GURL url_bad("https://" + url_bad_no_scheme);
  TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
  SetUpV5Client(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                /*metadata=*/ThreatMetadata());
  v5_fake_manager()->set_hold_callback(true);

  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase));
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(client.on_check_browse_url_result_called());

  sb_local_database_manager_->CancelCheck(&client);
  v5_fake_manager()->RunHeldCallback();
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(client.on_check_browse_url_result_called());
}

// Test that stopping safe browsing while GetFullHashes is in flight invokes
// the client callback with a safe response.
TEST_F(SBLocalDatabaseManagerTest_V5, StopWhileGetFullHashesInFlight) {
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  std::string url_bad_no_scheme("example.com/bad/");
  FullHashStr bad_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_bad_no_scheme))));
  const HashPrefixStr bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes);

  const GURL url_bad("https://" + url_bad_no_scheme);
  TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
  SetUpV5Client(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                /*metadata=*/ThreatMetadata());
  v5_fake_manager()->set_hold_callback(true);

  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase));
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(client.on_check_browse_url_result_called());

  StopLocalDatabaseManager();
  EXPECT_TRUE(client.on_check_browse_url_result_called());
  EXPECT_EQ(1, client.on_check_browse_url_result_call_count());

  v5_fake_manager()->RunHeldCallback();
  WaitForTasksOnTaskRunner();
  EXPECT_EQ(1, client.on_check_browse_url_result_call_count());
}

// Test that shutting down safe browsing while GetFullHashes is in flight
// drops the check without invoking the callback.
TEST_F(SBLocalDatabaseManagerTest_V5, ShutdownWhileGetFullHashesInFlight) {
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  std::string url_bad_no_scheme("example.com/bad/");
  FullHashStr bad_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_bad_no_scheme))));
  const HashPrefixStr bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes);

  const GURL url_bad("https://" + url_bad_no_scheme);
  TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
  SetUpV5Client(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                /*metadata=*/ThreatMetadata());
  v5_fake_manager()->set_hold_callback(true);

  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase));
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(client.on_check_browse_url_result_called());

  ShutdownLocalDatabaseManager();
  EXPECT_FALSE(client.on_check_browse_url_result_called());

  v5_fake_manager()->RunHeldCallback();
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(client.on_check_browse_url_result_called());

  sb_local_database_manager_->CancelCheck(&client);
}

// Test that destroying V5GetHashProtocolManager while GetFullHashes is in
// flight responds safe to the client.
TEST_F(SBLocalDatabaseManagerTest_V5,
       DestroyProtocolManagerWhileGetFullHashesInFlight) {
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  std::string url_bad_no_scheme("example.com/bad/");
  FullHashStr bad_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_bad_no_scheme))));
  const HashPrefixStr bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes);

  const GURL url_bad("https://" + url_bad_no_scheme);
  TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
  SetUpV5Client(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                /*metadata=*/ThreatMetadata());
  v5_fake_manager()->set_hold_callback(true);

  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase));
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(client.on_check_browse_url_result_called());

  ResetV5FakeManager();
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_browse_url_result_called());
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, TestCheckCsdAllowlistWithPrefixMatch) {
  std::unique_ptr<ScopedFakeGetHashProtocolManagerFactory> pin;
  if (!IsV5()) {
    // Setup to receive full-hash misses. We won't make URL requests.
    pin = std::make_unique<ScopedFakeGetHashProtocolManagerFactory>(
        FullHashInfos({}));
    ResetLocalDatabaseManager();
    WaitForTasksOnTaskRunner();
  }

  std::string url_safe_no_scheme("example.com/safe/");
  FullHashStr safe_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_safe_no_scheme))));
  const HashPrefixStr safe_hash_prefix(safe_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlCsdAllowlistId(),
                                       safe_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true);

  TestAllowlistClient client(
      /* match_expected= */ false,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_CSD_ALLOWLIST);
  const GURL url_check("https://" + url_safe_no_scheme);
  EXPECT_EQ(AsyncMatch::ASYNC, sb_local_database_manager_->CheckCsdAllowlistUrl(
                                   url_check, &client));

  EXPECT_FALSE(client.callback_called());

  // Wait for PerformFullHashCheck to complete.
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.callback_called());
}

// This is like CsdAllowlistWithPrefixMatch, but we also verify the
// full-hash-match results in an appropriate callback value.
TEST_F(SBLocalDatabaseManagerTest,
       TestCheckCsdAllowlistWithPrefixTheFullMatch) {
  // v5's get full hash manager does not support CSD allowlist.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kLocalListsUseSBv5);

  std::string url_safe_no_scheme("example.com/safe/");
  FullHashStr safe_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_safe_no_scheme))));

  // Setup to receive full-hash hit. We won't make URL requests.
  FullHashInfos infos(
      {{safe_full_hash, GetUrlCsdAllowlistId(), base::Time::Now()}});
  ScopedFakeGetHashProtocolManagerFactory pin(infos);
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  const HashPrefixStr safe_hash_prefix(safe_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlCsdAllowlistId(),
                                       safe_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true);

  TestAllowlistClient client(
      /* match_expected= */ true,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_CSD_ALLOWLIST);
  const GURL url_check("https://" + url_safe_no_scheme);
  EXPECT_EQ(AsyncMatch::ASYNC, sb_local_database_manager_->CheckCsdAllowlistUrl(
                                   url_check, &client));

  EXPECT_FALSE(client.callback_called());

  // Wait for PerformFullHashCheck to complete.
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.callback_called());
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, TestCheckCsdAllowlistWithFullMatch) {
  std::unique_ptr<ScopedFakeGetHashProtocolManagerFactory> pin;
  if (!IsV5()) {
    // Setup to receive full-hash misses. We won't make URL requests.
    pin = std::make_unique<ScopedFakeGetHashProtocolManagerFactory>(
        FullHashInfos({}));
    ResetLocalDatabaseManager();
    WaitForTasksOnTaskRunner();
  }

  std::string url_safe_no_scheme("example.com/safe/");
  FullHashStr safe_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_safe_no_scheme))));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlCsdAllowlistId(), safe_full_hash);
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true);

  TestAllowlistClient client(
      /* match_expected= */ true,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_CSD_ALLOWLIST);
  const GURL url_check("https://" + url_safe_no_scheme);
  auto result =
      sb_local_database_manager_->CheckCsdAllowlistUrl(url_check, &client);

  EXPECT_EQ(AsyncMatch::ASYNC, result);
  EXPECT_FALSE(client.callback_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.callback_called());
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, TestCheckCsdAllowlistWithNoMatch) {
  std::unique_ptr<ScopedFakeGetHashProtocolManagerFactory> pin;
  if (!IsV5()) {
    // Setup to receive full-hash misses. We won't make URL requests.
    pin = std::make_unique<ScopedFakeGetHashProtocolManagerFactory>(
        FullHashInfos({}));
    ResetLocalDatabaseManager();
    WaitForTasksOnTaskRunner();
  }

  // Add a full hash that won't match the URL we check.
  std::string url_safe_no_scheme("example.com/safe/");
  FullHashStr safe_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_safe_no_scheme))));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), safe_full_hash);
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true);

  TestAllowlistClient client(
      /* match_expected= */ false,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_CSD_ALLOWLIST);
  const GURL url_check("https://other.com/");
  auto result =
      sb_local_database_manager_->CheckCsdAllowlistUrl(url_check, &client);

  EXPECT_EQ(AsyncMatch::ASYNC, result);
  EXPECT_FALSE(client.callback_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.callback_called());
}

// When allowlist is unavailable, all URLS should be allowed.
TEST_P(SBLocalDatabaseManagerTest_V4V5, TestCheckCsdAllowlistUnavailable) {
  std::unique_ptr<ScopedFakeGetHashProtocolManagerFactory> pin;
  if (!IsV5()) {
    // Setup to receive full-hash misses. We won't make URL requests.
    pin = std::make_unique<ScopedFakeGetHashProtocolManagerFactory>(
        FullHashInfos({}));
    ResetLocalDatabaseManager();
    WaitForTasksOnTaskRunner();
  }

  StoreAndHashPrefixes store_and_hash_prefixes;
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ false);

  TestAllowlistClient client(
      /* match_expected= */ false,
      /* expected_sb_threat_type= */ SB_THREAT_TYPE_CSD_ALLOWLIST);
  const GURL url_check("https://other.com/");
  EXPECT_EQ(AsyncMatch::MATCH, sb_local_database_manager_->CheckCsdAllowlistUrl(
                                   url_check, &client));

  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(client.callback_called());
}

TEST_P(SBLocalDatabaseManagerTest_V4V5,
       TestCheckBrowseUrlReturnsNoMatchWhenDisabled) {
  WaitForTasksOnTaskRunner();

  // The same URL returns |false| in the previous test because
  // sb_local_database_manager_ is enabled.
  ForceDisableLocalDatabaseManager();

  const GURL url("http://example.com/a/");
  TestClient client(SB_THREAT_TYPE_SAFE, url);
  SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());
  EXPECT_TRUE(sb_local_database_manager_->CheckBrowseUrl(
      url, usual_threat_types_, &client, CheckBrowseUrlType::kHashDatabase));
}

// Hash prefix matches on the high confidence allowlist, but not a full hash
// match, so it says there is no match and does not perform a full hash check.
// This can only happen with an invalid db setup.
TEST_P(SBLocalDatabaseManagerTest_V4V5,
       TestCheckUrlForHCAllowlistWithPrefixMatchButNoLocalFullHashMatch) {
  SetupFakeManager();
  std::string url_safe_no_scheme("example.com/safe/");
  FullHashStr safe_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_safe_no_scheme))));

  // Setup to match hash prefix in the local database.
  const HashPrefixStr safe_hash_prefix(safe_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlHighConfidenceAllowlistId(),
                                       safe_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true);

  // Confirm there is no match and the full hash check is not performed.
  const GURL url_check("https://" + url_safe_no_scheme);
  CheckUrlForHighConfidenceAllowlistFuture future;
  sb_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
      url_check, future.GetCallback());
  EXPECT_FALSE(future.Get<0>());
  ValidateHighConfidenceAllowlistHistograms(
      future.Get<1>(),
      /*expected_all_stores_available_sample=*/true,
      /*expected_allowlist_too_small_sample=*/false);

  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(FakeSBLocalDatabaseManager::PerformFullHashCheckCalled(
      sb_local_database_manager_));
}

// Full hash match on the high confidence allowlist. Returns true
// synchronously and the full hash check is not performed.
TEST_P(SBLocalDatabaseManagerTest_V4V5,
       TestCheckUrlForHCAllowlistWithLocalFullHashMatch) {
  SetupFakeManager();
  std::string url_safe_no_scheme("example.com/safe/");
  FullHashStr safe_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_safe_no_scheme))));

  // Setup to match full hash in the local database.
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlHighConfidenceAllowlistId(),
                                       safe_full_hash);
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true);

  // Confirm there is a match and the full hash check is not performed.
  const GURL url_check("https://" + url_safe_no_scheme);
  CheckUrlForHighConfidenceAllowlistFuture future;
  sb_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
      url_check, future.GetCallback());
  EXPECT_TRUE(future.Get<0>());
  ValidateHighConfidenceAllowlistHistograms(
      future.Get<1>(),
      /*expected_all_stores_available_sample=*/true,
      /*expected_allowlist_too_small_sample=*/false);
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(FakeSBLocalDatabaseManager::PerformFullHashCheckCalled(
      sb_local_database_manager_));
}

// Hash prefix has no match on the high confidence allowlist. Returns false
// synchronously and the full hash check is not performed.
TEST_P(SBLocalDatabaseManagerTest_V4V5, TestCheckUrlForHCAllowlistWithNoMatch) {
  SetupFakeManager();
  std::string url_safe_no_scheme("example.com/safe/");
  FullHashStr safe_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_safe_no_scheme))));

  // Add a full hash that won't match the URL we check.
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlHighConfidenceAllowlistId(),
                                       safe_full_hash);
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true);

  // Confirm there is no match and the full hash check is not performed.
  const GURL url_check("https://example.com/other/");
  CheckUrlForHighConfidenceAllowlistFuture future;
  sb_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
      url_check, future.GetCallback());
  EXPECT_FALSE(future.Get<0>());
  ValidateHighConfidenceAllowlistHistograms(
      future.Get<1>(),
      /*expected_all_stores_available_sample=*/true,
      /*expected_allowlist_too_small_sample=*/false);
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(FakeSBLocalDatabaseManager::PerformFullHashCheckCalled(
      sb_local_database_manager_));
}

// When allowlist is unavailable, all URLs should be considered as matches.
TEST_P(SBLocalDatabaseManagerTest_V4V5, TestCheckUrlForHCAllowlistUnavailable) {
  SetupFakeManager();

  // Setup local database as unavailable.
  StoreAndHashPrefixes store_and_hash_prefixes;
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ false);

  // Confirm there is a match and the full hash check is not performed.
  const GURL url_check("https://example.com/safe");
  CheckUrlForHighConfidenceAllowlistFuture future;
  sb_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
      url_check, future.GetCallback());
  EXPECT_TRUE(future.Get<0>());
  ValidateHighConfidenceAllowlistHistograms(
      future.Get<1>(),
      /*expected_all_stores_available_sample=*/false,
      /*expected_allowlist_too_small_sample=*/false);
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(FakeSBLocalDatabaseManager::PerformFullHashCheckCalled(
      sb_local_database_manager_));
}

TEST_P(SBLocalDatabaseManagerTest_V4V5,
       TestCheckUrlForHCAllowlistAfterStopping) {
  SetupFakeManager();
  std::string url_safe_no_scheme("example.com/safe/");
  FullHashStr safe_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_safe_no_scheme))));

  // Setup to match full hash in the local database.
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlHighConfidenceAllowlistId(),
                                       safe_full_hash);
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true);

  const GURL url_check("https://" + url_safe_no_scheme);
  CheckUrlForHighConfidenceAllowlistFuture future;
  sb_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
      url_check, future.GetCallback());
  EXPECT_EQ(1ul, GetPendingChecks().size());
  StopLocalDatabaseManager();
  EXPECT_TRUE(GetPendingChecks().empty());

  EXPECT_TRUE(future.Get<0>());
  EXPECT_TRUE(future.Get<1>());
}

// When allowlist is available but the size is too small, all URLs should be
// considered as matches.
TEST_P(SBLocalDatabaseManagerTest_V4V5, TestCheckUrlForHCAllowlistSmallSize) {
  SetupFakeManager();

  // Setup the size of the allowlist to be smaller than the threshold. (10
  // entries)
  StoreAndHashPrefixes store_and_hash_prefixes;
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true,
                    /* store_file_size= */ 32 * 10);

  // Confirm there is a match and the full hash check is not performed.
  const GURL url_check("https://example.com/safe");
  CheckUrlForHighConfidenceAllowlistFuture future;
  sb_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
      url_check, future.GetCallback());
  EXPECT_TRUE(future.Get<0>());
  ValidateHighConfidenceAllowlistHistograms(
      future.Get<1>(),
      /*expected_all_stores_available_sample=*/true,
      /*expected_allowlist_too_small_sample=*/true);
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(FakeSBLocalDatabaseManager::PerformFullHashCheckCalled(
      sb_local_database_manager_));
}

// Tests the command line flag that skips the high-confidence allowlist check.
TEST_P(SBLocalDatabaseManagerTest_V4V5,
       TestCheckUrlForHCAllowlistSkippedViaCommandLineSwitch) {
  SetupFakeManager();
  std::string url_safe_no_scheme("example.com/safe/");
  FullHashStr safe_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_safe_no_scheme))));

  // Setup to match full hash in the local database.
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlHighConfidenceAllowlistId(),
                                       safe_full_hash);
  ReplaceSBDatabase(store_and_hash_prefixes, /*stores_available=*/true);
  const GURL url_check("https://" + url_safe_no_scheme);

  // First, confirm the high-confidence allowlist is checked by default.
  CheckUrlForHighConfidenceAllowlistFuture future1;
  sb_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
      url_check, future1.GetCallback());
  EXPECT_TRUE(future1.Get<0>());
  EXPECT_TRUE(future1.Get<1>());

  // Now, check that the high-confidence allowlist is skipped if the command
  // line switch is present.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      "safe-browsing-skip-high-confidence-allowlist");
  CheckUrlForHighConfidenceAllowlistFuture future2;
  sb_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
      url_check, future2.GetCallback());
  EXPECT_FALSE(future2.Get<0>());
  EXPECT_FALSE(future2.Get<1>());
}

TEST_F(SBLocalDatabaseManagerTest, TestGetSeverestThreatTypeAndMetadata) {
  WaitForTasksOnTaskRunner();

  FullHashStr fh_malware("Malware");
  FullHashInfo fhi_malware(fh_malware, GetUrlMalwareId(), base::Time::Now());

  FullHashStr fh_api("api");
  FullHashInfo fhi_api(fh_api, GetChromeUrlApiId(), base::Time::Now());

  FullHashStr fh_example("example");
  std::vector<FullHashInfo> fhis({fhi_malware, fhi_api});
  std::vector<FullHashStr> full_hashes({fh_malware, fh_example, fh_api});

  std::vector<SBThreatType> full_hash_threat_types(full_hashes.size(),
                                                   SB_THREAT_TYPE_SAFE);
  SBThreatType result_threat_type;
  ThreatMetadata metadata;

  const std::vector<SBThreatType> expected_full_hash_threat_types(
      {SB_THREAT_TYPE_URL_MALWARE, SB_THREAT_TYPE_SAFE,
       SB_THREAT_TYPE_API_ABUSE});

  sb_local_database_manager_->GetSeverestThreatTypeAndMetadata(
      fhis, full_hashes, &full_hash_threat_types, &result_threat_type,
      &metadata);
  EXPECT_EQ(expected_full_hash_threat_types, full_hash_threat_types);

  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, result_threat_type);

  // Reversing the list has no effect.
  std::reverse(std::begin(fhis), std::end(fhis));
  full_hash_threat_types.assign(full_hashes.size(), SB_THREAT_TYPE_SAFE);

  sb_local_database_manager_->GetSeverestThreatTypeAndMetadata(
      fhis, full_hashes, &full_hash_threat_types, &result_threat_type,
      &metadata);
  EXPECT_EQ(expected_full_hash_threat_types, full_hash_threat_types);
  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, result_threat_type);

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V4LocalDatabaseManager.ThreatInfoSize",
      /* sample */ 2, /* expected_count */ 2);
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, TestChecksAreQueued) {
  base::HistogramTester histograms;
  const GURL url("https://www.example.com/");
  TestClient client(SB_THREAT_TYPE_SAFE, url);
  SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());
  EXPECT_TRUE(GetQueuedChecks().empty());
  sb_local_database_manager_->CheckBrowseUrl(url, usual_threat_types_, &client,
                                             CheckBrowseUrlType::kHashDatabase);
  // The database is unavailable so the check should get queued.
  EXPECT_EQ(1ul, GetQueuedChecks().size());

  // Wait for the DB to load and dequeue the check.
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(GetQueuedChecks().empty());

  // Wait for the DB thread search and UI thread reply callback to execute.
  WaitForTasksOnTaskRunner();

  if (GetParam()) {
    histograms.ExpectTotalCount(
        "SafeBrowsing.V5CheckUrl.TimeTaken.DatabaseNotReadyQueueDelay", 1);
    histograms.ExpectTotalCount("SafeBrowsing.V5CheckUrl.TimeTaken.LocalLookup",
                                1);
    histograms.ExpectTotalCount(
        "SafeBrowsing.V5CheckUrl.TimeTaken.LocalLookup.DbThreadQueueDelay", 1);
    histograms.ExpectTotalCount(
        "SafeBrowsing.V5CheckUrl.TimeTaken.LocalLookup.StoreLookupDuration", 1);
    histograms.ExpectTotalCount(
        "SafeBrowsing.V5CheckUrl.TimeTaken.LocalLookup.UiCallbackQueueDelay",
        1);
  } else {
    histograms.ExpectTotalCount(
        "SafeBrowsing.V4CheckUrl.TimeTaken.DatabaseNotReadyQueueDelay", 1);
    histograms.ExpectTotalCount("SafeBrowsing.V4CheckUrl.TimeTaken.LocalLookup",
                                1);
    histograms.ExpectTotalCount(
        "SafeBrowsing.V4CheckUrl.TimeTaken.LocalLookup.DbThreadQueueDelay", 1);
    histograms.ExpectTotalCount(
        "SafeBrowsing.V4CheckUrl.TimeTaken.LocalLookup.StoreLookupDuration", 1);
    histograms.ExpectTotalCount(
        "SafeBrowsing.V4CheckUrl.TimeTaken.LocalLookup.UiCallbackQueueDelay",
        1);
  }
  histograms.ExpectTotalCount(
      "SafeBrowsing.SBCheckUrl.TimeTaken.DatabaseNotReadyQueueDelay", 1);
  histograms.ExpectTotalCount("SafeBrowsing.SBCheckUrl.TimeTaken.LocalLookup",
                              1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.SBCheckUrl.TimeTaken.LocalLookup.DbThreadQueueDelay", 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.SBCheckUrl.TimeTaken.LocalLookup.StoreLookupDuration", 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.SBCheckUrl.TimeTaken.LocalLookup.UiCallbackQueueDelay", 1);

  ResetSBDatabase();
  sb_local_database_manager_->CheckBrowseUrl(url, usual_threat_types_, &client,
                                             CheckBrowseUrlType::kHashDatabase);
  // The database is unavailable so the check should get queued.
  EXPECT_EQ(1ul, GetQueuedChecks().size());

  StopLocalDatabaseManager();
  EXPECT_TRUE(GetQueuedChecks().empty());
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, CancelWhileQueued) {
  const GURL url("https://www.example.com/");
  TestClient client(SB_THREAT_TYPE_SAFE, url);
  SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());

  // The following function waits for the DB to load. Otherwise we may have a
  // pending attempt to create a V4Store at teardown, which will leak.
  WaitForTasksOnTaskRunner();

  ResetSBDatabase();

  sb_local_database_manager_->CheckBrowseUrl(url, usual_threat_types_, &client,
                                             CheckBrowseUrlType::kHashDatabase);
  // The database is unavailable so the check should get queued.
  EXPECT_EQ(1ul, GetQueuedChecks().size());

  sb_local_database_manager_->CancelCheck(&client);

  EXPECT_TRUE(GetQueuedChecks().empty());
}

// Verify that a window where checks cannot be cancelled is closed.
TEST_P(SBLocalDatabaseManagerTest_V4V5, CancelPending) {
  // Setup to receive full-hash misses.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));

  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // Put a match in the db that will cause a protocol-manager request.
  std::string url_bad_no_scheme("example.com/bad/");
  FullHashStr bad_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_bad_no_scheme))));
  const HashPrefixStr bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes);

  const GURL url_bad("https://" + url_bad_no_scheme);
  // Test that a request flows through to the callback.
  {
    TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
    SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                          /*metadata=*/ThreatMetadata());
    EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
        url_bad, usual_threat_types_, &client,
        CheckBrowseUrlType::kHashDatabase));
    EXPECT_FALSE(client.on_check_browse_url_result_called());
    WaitForTasksOnTaskRunner();
    EXPECT_TRUE(client.on_check_browse_url_result_called());
  }

  // Test that cancel prevents the callback from being called.
  {
    TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
    SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                          /*metadata=*/ThreatMetadata());
    EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
        url_bad, usual_threat_types_, &client,
        CheckBrowseUrlType::kHashDatabase));
    sb_local_database_manager_->CancelCheck(&client);
    EXPECT_FALSE(client.on_check_browse_url_result_called());
    WaitForTasksOnTaskRunner();
    EXPECT_FALSE(client.on_check_browse_url_result_called());
  }

  // Test that the client gets a safe response for a pending check when safe
  // browsing is stopped.
  {
    TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
    SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                          /*metadata=*/ThreatMetadata());
    EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
        url_bad, usual_threat_types_, &client,
        CheckBrowseUrlType::kHashDatabase));
    EXPECT_EQ(1ul, GetPendingChecks().size());
    EXPECT_FALSE(client.on_check_browse_url_result_called());
    EXPECT_TRUE((*GetPendingChecks().begin())->is_in_pending_checks);
    StopLocalDatabaseManager();
    EXPECT_TRUE(GetPendingChecks().empty());
    EXPECT_TRUE(client.on_check_browse_url_result_called());
  }

  // Clean up from the database being shut down.
  StartLocalDatabaseManager();
  ReplaceSBDatabase(store_and_hash_prefixes);

  // Test that the client does not get a response for a pending check when safe
  // browsing is shutdown.
  {
    TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
    SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                          /*metadata=*/ThreatMetadata());
    EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
        url_bad, usual_threat_types_, &client,
        CheckBrowseUrlType::kHashDatabase));
    EXPECT_EQ(1ul, GetPendingChecks().size());
    EXPECT_FALSE(client.on_check_browse_url_result_called());
    EXPECT_TRUE((*GetPendingChecks().begin())->is_in_pending_checks);
    ShutdownLocalDatabaseManager();
    EXPECT_TRUE(GetPendingChecks().empty());
    EXPECT_FALSE(client.on_check_browse_url_result_called());
    sb_local_database_manager_->CancelCheck(&client);
  }
}

// Verifies that calling CancelCheck while PerformFullHashCheck is queued on
// the UI thread does not result in a crash when PerformFullHashCheck executes.
TEST_P(SBLocalDatabaseManagerTest_V4V5, CancelPendingFullHashCheck) {
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  std::string url_bad_no_scheme("example.com/bad/");
  FullHashStr bad_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_bad_no_scheme))));
  const HashPrefixStr bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes, /*stores_available=*/true);

  const GURL url_bad("https://" + url_bad_no_scheme);
  TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
  SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());

  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase));

  // Post CancelCheck so it runs when PerformFullHashCheck is scheduled
  // but has not yet run.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<SBLocalDatabaseManager> mgr,
             SafeBrowsingDatabaseManager::Client* client,
             base::RepeatingClosure quit_closure) {
            mgr->CancelCheck(client);
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, quit_closure);
          },
          sb_local_database_manager_, &client, run_loop.QuitClosure()));

  run_loop.Run();
}

// When the database load flushes the queued requests, make sure that
// CancelCheck() is not fatal in the client callback.
TEST_P(SBLocalDatabaseManagerTest_V4V5, CancelQueued) {
  const GURL url("http://example.com/a/");

  TestClient client1(SB_THREAT_TYPE_SAFE, url,
                     sb_local_database_manager_.get());
  SetUpV5ClientIfNeeded(client1, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());
  TestClient client2(SB_THREAT_TYPE_SAFE, url);
  SetUpV5ClientIfNeeded(client2, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url, usual_threat_types_, &client1, CheckBrowseUrlType::kHashDatabase));
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url, usual_threat_types_, &client2, CheckBrowseUrlType::kHashDatabase));
  EXPECT_EQ(2ul, GetQueuedChecks().size());
  EXPECT_FALSE(client1.on_check_browse_url_result_called());
  EXPECT_FALSE(client2.on_check_browse_url_result_called());
  WaitForTasksOnTaskRunner();
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client1.on_check_browse_url_result_called());
  EXPECT_TRUE(client2.on_check_browse_url_result_called());
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, ShutdownCancelsQueued) {
  const GURL url("http://example.com/a/");
  TestClient client1(SB_THREAT_TYPE_SAFE, url,
                     sb_local_database_manager_.get());
  SetUpV5ClientIfNeeded(client1, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());
  TestClient client2(SB_THREAT_TYPE_SAFE, url);
  SetUpV5ClientIfNeeded(client2, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url, usual_threat_types_, &client1, CheckBrowseUrlType::kHashDatabase));
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url, usual_threat_types_, &client2, CheckBrowseUrlType::kHashDatabase));
  EXPECT_EQ(2ul, GetQueuedChecks().size());
  ShutdownLocalDatabaseManager();
  EXPECT_TRUE(GetQueuedChecks().empty());
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(client1.on_check_browse_url_result_called());
  EXPECT_FALSE(client2.on_check_browse_url_result_called());
  // Client should still be able to cancel checks post-shutdown.
  sb_local_database_manager_->CancelCheck(&client1);
  sb_local_database_manager_->CancelCheck(&client2);
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, QueuedCheckWithFullHash) {
  std::string url_bad_no_scheme("example.com/bad/");
  const GURL url_bad("https://" + url_bad_no_scheme);

  FullHashStr bad_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_bad_no_scheme))));
  const HashPrefixStr bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);

  FullHashInfo fhi(bad_full_hash, GetUrlMalwareId(), base::Time());
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({fhi}));

  ReplaceSBDatabase(store_and_hash_prefixes, false,
                    kDefaultStoreFileSizeInBytes, false);
  StartLocalDatabaseManager();

  // The fake database returns a matched hash prefix.
  TestClient client(SB_THREAT_TYPE_URL_MALWARE, url_bad);
  SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_URL_MALWARE,
                        /*metadata=*/ThreatMetadata());
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase));

  EXPECT_EQ(1ul, GetQueuedChecks().size());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(GetQueuedChecks().empty());

  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_browse_url_result_called());
}

// This test is somewhat similar to TestCheckBrowseUrlWithFakeDbReturnsMatch but
// it uses a fake SBLocalDatabaseManager to assert that PerformFullHashCheck is
// called async.
TEST_P(SBLocalDatabaseManagerTest_V4V5, PerformFullHashCheckCalledAsync) {
  SetupFakeManager();

  std::string url_bad_no_scheme("example.com/bad/");
  FullHashStr bad_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_bad_no_scheme))));
  const HashPrefixStr bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes);

  const GURL url_bad("https://" + url_bad_no_scheme);
  TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
  SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());
  // The fake database returns a matched hash prefix.
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase));

  EXPECT_FALSE(FakeSBLocalDatabaseManager::PerformFullHashCheckCalled(
      sb_local_database_manager_));

  // Wait for PerformFullHashCheck to complete.
  WaitForTasksOnTaskRunner();

  EXPECT_TRUE(FakeSBLocalDatabaseManager::PerformFullHashCheckCalled(
      sb_local_database_manager_));
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, UsingWeakPtrDropsCallback) {
  SetupFakeManager();

  std::string url_bad_no_scheme("example.com/bad/");
  FullHashStr bad_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_bad_no_scheme))));
  const HashPrefixStr bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(), bad_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes);

  const GURL url_bad("https://" + url_bad_no_scheme);
  TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
  SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase));
  sb_local_database_manager_->StopOnUIThread(true);

  // Release the SBLocalDatabaseManager object right away before the callback
  // gets called. When the callback gets called, without using a weak-ptr
  // factory, this leads to a use after free. However, using the weak-ptr means
  // that the callback is simply dropped.
  sb_local_database_manager_ = nullptr;

  // Wait for the tasks scheduled by StopOnUIThread to complete.
  WaitForTasksOnTaskRunner();
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, TestMatchDownloadAllowlistUrl) {
  SetupFakeManager();
  GURL good_url("http://safe.com");
  GURL other_url("http://iffy.com");

  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlCsdDownloadAllowlistId(),
                                       HashForUrl(good_url));

  ReplaceSBDatabase(store_and_hash_prefixes, false /* not available */);
  // Verify it defaults to false when DB is not available.
  base::test::TestFuture<bool> future1;
  sb_local_database_manager_->MatchDownloadAllowlistUrl(good_url,
                                                        future1.GetCallback());
  EXPECT_FALSE(future1.Get());

  ReplaceSBDatabase(store_and_hash_prefixes, true /* available */);
  // Not allowlisted.
  base::test::TestFuture<bool> future2;
  sb_local_database_manager_->MatchDownloadAllowlistUrl(other_url,
                                                        future2.GetCallback());
  EXPECT_FALSE(future2.Get());
  // Allowlisted.
  base::test::TestFuture<bool> future3;
  sb_local_database_manager_->MatchDownloadAllowlistUrl(good_url,
                                                        future3.GetCallback());
  EXPECT_TRUE(future3.Get());

  EXPECT_FALSE(FakeSBLocalDatabaseManager::PerformFullHashCheckCalled(
      sb_local_database_manager_));
}

// This verifies the fix for race in http://crbug.com/660293
TEST_P(SBLocalDatabaseManagerTest_V4V5,
       TestCheckBrowseUrlWithSameClientAndCancel) {
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));
  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(),
                                       HashPrefixStr("sن\340\t\006_"));
  ReplaceSBDatabase(store_and_hash_prefixes);

  GURL first_url("http://example.com/a");
  GURL second_url("http://example.com/");
  TestClient client(SB_THREAT_TYPE_SAFE, first_url);
  SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());
  // The fake database returns a matched hash prefix.
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      first_url, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase));

  // That check gets queued. Now, let's cancel the check. After this, we should
  // not receive a call for |OnCheckBrowseUrlResult| with |first_url|.
  sb_local_database_manager_->CancelCheck(&client);

  // Now, re-use that client but for |second_url|.
  client.mutable_expected_urls()->assign(1, second_url);
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      second_url, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase));

  // Wait for PerformFullHashCheck to complete.
  WaitForTasksOnTaskRunner();
  // |on_check_browse_url_result_called_| is true only if OnCheckBrowseUrlResult
  // gets called with the |url| equal to |expected_url|, which is |second_url|
  // in
  // this test.
  EXPECT_TRUE(client.on_check_browse_url_result_called());
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, TestSubresourceFilterCallback) {
  // Setup to receive full-hash misses.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));

  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  std::string url_bad_no_scheme("example.com/bad/");
  FullHashStr bad_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_bad_no_scheme))));
  const HashPrefixStr bad_hash_prefix(bad_full_hash.substr(0, 5));

  // Put a match in the db that will cause a protocol-manager request.
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlSubresourceFilterId(),
                                       bad_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes, /*stores_available=*/true);

  const GURL url_bad("https://" + url_bad_no_scheme);
  // Test that a request flows through to the callback.
  {
    TestClient client(SB_THREAT_TYPE_SAFE, url_bad);
    SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                          /*metadata=*/ThreatMetadata());
    EXPECT_FALSE(sb_local_database_manager_->CheckUrlForSubresourceFilter(
        url_bad, &client));
    EXPECT_FALSE(client.on_check_subresource_filter_url_result_called());
    WaitForTasksOnTaskRunner();
    EXPECT_TRUE(client.on_check_subresource_filter_url_result_called());

    if (IsV5()) {
      CHECK(v5_fake_manager());
      const auto& last_map =
          v5_fake_manager()->last_full_hash_to_threat_types();
      auto it = last_map.find(bad_full_hash);
      ASSERT_NE(it, last_map.end());
      EXPECT_EQ(it->second,
                std::vector<SBThreatType>{SB_THREAT_TYPE_SUBRESOURCE_FILTER});
    }
  }
}

TEST_F(SBLocalDatabaseManagerTest,
       TestCheckExtensionIDsNothingBlocklisted_WithNetworkCheck) {
  // Explicitly disable the features allowing network bypass.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kExtensionBlocklistSkipNetworkQuery,
                             kLocalListsUseSBv5});

  // Setup to receive full-hash misses.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));

  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // extension_id_1 is in the local DB but the full hash won't match.
  const FullHashStr extension_id_1("aapbdbdomjkkjkaonfhkkikfgjllcleb"),
      extension_id_2("aapbdbdomjkkjkaonfhkkikfgjllclec");

  // Put a match in the db that will cause a protocol-manager request.
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetChromeExtMalwareId(), extension_id_1);
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true);

  const std::set<FullHashStr> expected_bad_crxs({});
  const std::set<FullHashStr> extension_ids({extension_id_2, extension_id_1});
  TestExtensionClient client(expected_bad_crxs);
  EXPECT_FALSE(
      sb_local_database_manager_->CheckExtensionIDs(extension_ids, &client));
  EXPECT_FALSE(client.on_check_extensions_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_extensions_result_called());
}

struct ExtensionSkipNetworkQueryTestCase {
  bool enable_skip_network_query;
  bool enable_v5;
};

class SBLocalDatabaseManagerTest_ExtensionSkipNetworkQuery
    : public SBLocalDatabaseManagerTest,
      public ::testing::WithParamInterface<ExtensionSkipNetworkQueryTestCase> {
 public:
  SBLocalDatabaseManagerTest_ExtensionSkipNetworkQuery() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam().enable_skip_network_query) {
      enabled_features.push_back(kExtensionBlocklistSkipNetworkQuery);
    } else {
      disabled_features.push_back(kExtensionBlocklistSkipNetworkQuery);
    }
    if (GetParam().enable_v5) {
      enabled_features.push_back(kLocalListsUseSBv5);
    } else {
      disabled_features.push_back(kLocalListsUseSBv5);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(SBLocalDatabaseManagerTest_ExtensionSkipNetworkQuery,
       TestCheckExtensionIDsNothingBlocklisted_WithoutNetworkCheck) {
  // Reset the database manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // Both extensions are good and not in the local DB.
  const FullHashStr extension_id_1("aapbdbdomjkkjkaonfhkkikfgjllcleb"),
      extension_id_2("aapbdbdomjkkjkaonfhkkikfgjllclec");

  // Replace database with empty store (nothing blocklisted).
  StoreAndHashPrefixes store_and_hash_prefixes;
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true);

  const std::set<FullHashStr> expected_bad_crxs({});
  const std::set<FullHashStr> extension_ids({extension_id_2, extension_id_1});
  TestExtensionClient client(expected_bad_crxs);
  EXPECT_FALSE(
      sb_local_database_manager_->CheckExtensionIDs(extension_ids, &client));
  EXPECT_FALSE(client.on_check_extensions_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_extensions_result_called());
}

TEST_F(SBLocalDatabaseManagerTest,
       TestCheckExtensionIDsOneIsBlocklisted_WithNetworkCheck) {
  // Explicitly disable the features allowing network bypass.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kExtensionBlocklistSkipNetworkQuery,
                             kLocalListsUseSBv5});

  // bad_extension_id is in the local DB and the full hash will match.
  const FullHashStr bad_extension_id("aapbdbdomjkkjkaonfhkkikfgjllcleb"),
      good_extension_id("aapbdbdomjkkjkaonfhkkikfgjllclec");
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
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true);

  const std::set<FullHashStr> expected_bad_crxs({bad_extension_id});
  const std::set<FullHashStr> extension_ids(
      {good_extension_id, bad_extension_id});
  TestExtensionClient client(expected_bad_crxs);
  EXPECT_FALSE(
      sb_local_database_manager_->CheckExtensionIDs(extension_ids, &client));
  EXPECT_FALSE(client.on_check_extensions_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_extensions_result_called());
}

TEST_P(SBLocalDatabaseManagerTest_ExtensionSkipNetworkQuery,
       TestCheckExtensionIDsOneIsBlocklisted_WithoutNetworkCheck) {
  // Reset the database manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // bad_extension_id is in the local DB.
  const FullHashStr bad_extension_id("aapbdbdomjkkjkaonfhkkikfgjllcleb"),
      good_extension_id("aapbdbdomjkkjkaonfhkkikfgjllclec");

  // Put a match in the db. In V5, local DB stores 16-byte hashes.
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(
      GetChromeExtMalwareId(),
      GetParam().enable_v5 ? SBStore::ExtensionIdToHash(bad_extension_id)
                           : bad_extension_id);
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true);

  const std::set<FullHashStr> expected_bad_crxs({bad_extension_id});
  const std::set<FullHashStr> extension_ids(
      {good_extension_id, bad_extension_id});
  TestExtensionClient client(expected_bad_crxs);
  EXPECT_FALSE(
      sb_local_database_manager_->CheckExtensionIDs(extension_ids, &client));
  EXPECT_FALSE(client.on_check_extensions_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_extensions_result_called());
}

// This is similar to |TestCheckExtensionIDsOneIsBlocklisted|, but it uses a
// real |V4GetHashProtocolManager| instead of |FakeGetHashProtocolManager|. This
// tests that the values passed into the protocol manager are usable.
TEST_F(
    SBLocalDatabaseManagerTest,
    TestCheckExtensionIDsOneIsBlocklisted_RealProtocolManager_WithNetworkCheck) {
  // Explicitly disable the features allowing network bypass.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kExtensionBlocklistSkipNetworkQuery,
                             kLocalListsUseSBv5});

  // bad_extension_id is in the local DB and the full hash will match.
  const FullHashStr bad_extension_id("aapbdbdomjkkjkaonfhkkikfgjllcleb"),
      good_extension_id("aapbdbdomjkkjkaonfhkkikfgjllclec");

  auto test_url_loader_factory =
      std::make_unique<network::TestURLLoaderFactory>();
  ASSERT_EQ(test_url_loader_factory->NumPending(), 0);
  ScopedGetHashProtocolManagerFactoryWithTestUrlLoader pin(
      test_url_loader_factory.get());

  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // Put a match in the db that will cause a protocol-manager request.
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetChromeExtMalwareId(),
                                       bad_extension_id);
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true);

  const std::set<FullHashStr> expected_bad_crxs({bad_extension_id});
  const std::set<FullHashStr> extension_ids(
      {good_extension_id, bad_extension_id});
  TestExtensionClient client(expected_bad_crxs);
  EXPECT_FALSE(
      sb_local_database_manager_->CheckExtensionIDs(extension_ids, &client));
  EXPECT_FALSE(client.on_check_extensions_result_called());

  // Setup to receive full-hash hit.
  ASSERT_EQ(test_url_loader_factory->NumPending(), 1);
  std::vector<TestV4HashResponseInfo> response_infos;
  response_infos.emplace_back(bad_extension_id, GetChromeExtMalwareId());
  std::string response = GetV4HashResponse(response_infos);
  test_url_loader_factory->AddResponse(
      test_url_loader_factory->GetPendingRequest(0)->request.url.spec(),
      response);

  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_extensions_result_called());
}

TEST_P(
    SBLocalDatabaseManagerTest_ExtensionSkipNetworkQuery,
    TestCheckExtensionIDsOneIsBlocklisted_RealProtocolManager_WithoutNetworkCheck) {
  auto test_url_loader_factory =
      std::make_unique<network::TestURLLoaderFactory>();
  ASSERT_EQ(test_url_loader_factory->NumPending(), 0);
  ScopedGetHashProtocolManagerFactoryWithTestUrlLoader pin(
      test_url_loader_factory.get());

  // Reset the database manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // bad_extension_id is in the local DB.
  const FullHashStr bad_extension_id("aapbdbdomjkkjkaonfhkkikfgjllcleb"),
      good_extension_id("aapbdbdomjkkjkaonfhkkikfgjllclec");

  // Put a match in the db. In V5, local DB stores 16-byte hashes.
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(
      GetChromeExtMalwareId(),
      GetParam().enable_v5 ? SBStore::ExtensionIdToHash(bad_extension_id)
                           : bad_extension_id);
  ReplaceSBDatabase(store_and_hash_prefixes, /* stores_available= */ true);

  const std::set<FullHashStr> expected_bad_crxs({bad_extension_id});
  const std::set<FullHashStr> extension_ids(
      {good_extension_id, bad_extension_id});
  TestExtensionClient client(expected_bad_crxs);
  EXPECT_FALSE(
      sb_local_database_manager_->CheckExtensionIDs(extension_ids, &client));
  EXPECT_FALSE(client.on_check_extensions_result_called());

  // Verify that NO network request is pending because we bypassed it.
  ASSERT_EQ(test_url_loader_factory->NumPending(), 0);

  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_extensions_result_called());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SBLocalDatabaseManagerTest_ExtensionSkipNetworkQuery,
    ::testing::Values(
        ExtensionSkipNetworkQueryTestCase{/*enable_skip_network_query=*/true,
                                          /*enable_v5=*/false},
        ExtensionSkipNetworkQueryTestCase{/*enable_skip_network_query=*/false,
                                          /*enable_v5=*/true},
        ExtensionSkipNetworkQueryTestCase{/*enable_skip_network_query=*/true,
                                          /*enable_v5=*/true}),
    [](const ::testing::TestParamInfo<ExtensionSkipNetworkQueryTestCase>&
           info) {
      std::string name;
      name += info.param.enable_skip_network_query
                  ? "SkipNetworkQueryFeatureOn"
                  : "SkipNetworkQueryFeatureOff";
      name += "_";
      name += info.param.enable_v5 ? "V5FeatureOn" : "V5FeatureOff";
      return name;
    });

TEST_P(SBLocalDatabaseManagerTest_V4V5,
       TestCheckDownloadUrlNothingBlocklisted) {
  // Setup to receive full-hash misses.
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({}));

  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // Put a match in the db that will cause a protocol-manager request.
  std::string url_bad_no_scheme("example.com/bad/");
  FullHashStr bad_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_bad_no_scheme))));
  const HashPrefixStr bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalBinId(), bad_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes, /*stores_available=*/true);

  const GURL url_bad("https://" + url_bad_no_scheme),
      url_good("https://example.com/good/");
  const std::vector<GURL> url_chain({url_good, url_bad});

  TestClient client(SB_THREAT_TYPE_SAFE, url_chain);
  SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());
  EXPECT_FALSE(
      sb_local_database_manager_->CheckDownloadUrl(url_chain, &client));
  EXPECT_FALSE(client.on_check_download_urls_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_download_urls_result_called());
}

TEST_P(SBLocalDatabaseManagerTest_V4V5,
       TestCheckDownloadUrlWithOneBlocklisted) {
  // Setup to receive full-hash hit.
  std::string url_bad_no_scheme("example.com/bad/");
  FullHashStr bad_full_hash(std::string(
      base::as_string_view(crypto::hash::Sha256(url_bad_no_scheme))));
  FullHashInfo fhi(bad_full_hash, GetUrlMalBinId(), base::Time());
  ScopedFakeGetHashProtocolManagerFactory pin(FullHashInfos({fhi}));

  // Reset the database manager so it picks up the replacement protocol manager.
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  const GURL url_bad("https://" + url_bad_no_scheme),
      url_good("https://example.com/good/");
  const std::vector<GURL> url_chain({url_good, url_bad});

  // Put a match in the db that will cause a protocol-manager request.
  const HashPrefixStr bad_hash_prefix(bad_full_hash.substr(0, 5));
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalBinId(), bad_hash_prefix);
  ReplaceSBDatabase(store_and_hash_prefixes, /*stores_available=*/true);

  TestClient client(SB_THREAT_TYPE_URL_BINARY_MALWARE, url_chain);
  SetUpV5ClientIfNeeded(client,
                        /*threat_type=*/SB_THREAT_TYPE_URL_BINARY_MALWARE,
                        /*metadata=*/ThreatMetadata());
  EXPECT_FALSE(
      sb_local_database_manager_->CheckDownloadUrl(url_chain, &client));
  EXPECT_FALSE(client.on_check_download_urls_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_download_urls_result_called());

  if (IsV5()) {
    CHECK(v5_fake_manager());
    const auto& last_map = v5_fake_manager()->last_full_hash_to_threat_types();
    auto it = last_map.find(bad_full_hash);
    ASSERT_NE(it, last_map.end());
    EXPECT_EQ(it->second,
              std::vector<SBThreatType>{SB_THREAT_TYPE_URL_BINARY_MALWARE});
  }
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, NotificationOnUpdate) {
  WaitForTasksOnTaskRunner();
  base::RunLoop run_loop;
  auto callback_subscription =
      sb_local_database_manager_->RegisterDatabaseUpdatedCallback(
          run_loop.QuitClosure());
  DatabaseUpdated();
  run_loop.Run();
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, FlagOneUrlAsPhishing) {
  SetupFakeManager();
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "mark_as_phishing", "https://example.com/1/");
  PopulateArtificialDatabase();

  const GURL url_bad("https://example.com/1/");
  TestClient client_bad(SB_THREAT_TYPE_URL_PHISHING, url_bad);
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, &client_bad,
      CheckBrowseUrlType::kHashDatabase));
  // PerformFullHashCheck will not be called if there is a match within the
  // artificial database
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(FakeSBLocalDatabaseManager::PerformFullHashCheckCalled(
      sb_local_database_manager_));

  const GURL url_good("https://other.example.com");
  TestClient client(SB_THREAT_TYPE_SAFE, url_good);
  bool result = sb_local_database_manager_->CheckBrowseUrl(
      url_good, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase);

  EXPECT_FALSE(result);
  EXPECT_FALSE(client.on_check_browse_url_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_browse_url_result_called());

  WaitForTasksOnTaskRunner();
  StopLocalDatabaseManager();
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, FlagOneUrlAsMalware) {
  SetupFakeManager();
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "mark_as_malware", "https://example.com/1/");
  PopulateArtificialDatabase();

  const GURL url_bad("https://example.com/1/");
  TestClient client_bad(SB_THREAT_TYPE_URL_MALWARE, url_bad);
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, &client_bad,
      CheckBrowseUrlType::kHashDatabase));
  // PerformFullHashCheck will not be called if there is a match within the
  // artificial database
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(FakeSBLocalDatabaseManager::PerformFullHashCheckCalled(
      sb_local_database_manager_));

  const GURL url_good("https://other.example.com");
  TestClient client(SB_THREAT_TYPE_SAFE, url_good);
  bool result = sb_local_database_manager_->CheckBrowseUrl(
      url_good, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase);

  EXPECT_FALSE(result);
  EXPECT_FALSE(client.on_check_browse_url_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_browse_url_result_called());

  WaitForTasksOnTaskRunner();
  StopLocalDatabaseManager();
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, FlagOneUrlAsUWS) {
  SetupFakeManager();
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "mark_as_uws", "https://example.com/1/");
  PopulateArtificialDatabase();

  const GURL url_bad("https://example.com/1/");
  TestClient client_bad(SB_THREAT_TYPE_URL_UNWANTED, url_bad);
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_bad, usual_threat_types_, &client_bad,
      CheckBrowseUrlType::kHashDatabase));
  // PerformFullHashCheck will not be called if there is a match within the
  // artificial database
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(FakeSBLocalDatabaseManager::PerformFullHashCheckCalled(
      sb_local_database_manager_));

  const GURL url_good("https://other.example.com");
  TestClient client(SB_THREAT_TYPE_SAFE, url_good);
  bool result = sb_local_database_manager_->CheckBrowseUrl(
      url_good, usual_threat_types_, &client,
      CheckBrowseUrlType::kHashDatabase);

  EXPECT_FALSE(result);
  EXPECT_FALSE(client.on_check_browse_url_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client.on_check_browse_url_result_called());

  WaitForTasksOnTaskRunner();
  StopLocalDatabaseManager();
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, FlagMultipleUrls) {
  SetupFakeManager();
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "mark_as_phishing", "https://example.com/1/");
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "mark_as_malware", "https://2.example.com");
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "mark_as_uws", "https://example.test.com");
  PopulateArtificialDatabase();

  const GURL url_phishing("https://example.com/1/");
  const GURL url_malware("https://2.example.com");
  const GURL url_uws("https://example.test.com");

  TestClient client_phishing(SB_THREAT_TYPE_URL_PHISHING, url_phishing);
  TestClient client_malware(SB_THREAT_TYPE_URL_MALWARE, url_malware);
  TestClient client_uws(SB_THREAT_TYPE_URL_UNWANTED, url_uws);

  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_phishing, usual_threat_types_, &client_phishing,
      CheckBrowseUrlType::kHashDatabase));
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_malware, usual_threat_types_, &client_malware,
      CheckBrowseUrlType::kHashDatabase));
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url_uws, usual_threat_types_, &client_uws,
      CheckBrowseUrlType::kHashDatabase));
  // PerformFullHashCheck will not be called if there is a match within the
  // artificial database
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(FakeSBLocalDatabaseManager::PerformFullHashCheckCalled(
      sb_local_database_manager_));

  const GURL url_good("https://other.example.com");
  TestClient client_good(SB_THREAT_TYPE_SAFE, url_good);
  bool result = sb_local_database_manager_->CheckBrowseUrl(
      url_good, usual_threat_types_, &client_good,
      CheckBrowseUrlType::kHashDatabase);

  EXPECT_FALSE(result);
  EXPECT_FALSE(client_good.on_check_browse_url_result_called());
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client_good.on_check_browse_url_result_called());

  StopLocalDatabaseManager();
}

namespace {

class MultipleArtificialMatchesTestClient
    : public SafeBrowsingDatabaseManager::Client {
 public:
  explicit MultipleArtificialMatchesTestClient(const GURL& url)
      : SafeBrowsingDatabaseManager::Client(GetPassKeyForTesting()),
        expected_url_(url) {}

  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type) override {
    EXPECT_EQ(expected_url_, url);
    EXPECT_TRUE(threat_type == SBThreatType::SB_THREAT_TYPE_URL_MALWARE ||
                threat_type == SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    called_ = true;
  }

  bool called() const { return called_; }

 private:
  GURL expected_url_;
  bool called_ = false;
};

}  // namespace

TEST_F(SBLocalDatabaseManagerTest_V5, FlagOneUrlWithMultipleFlags) {
  SetupFakeManager();
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "mark_as_phishing", "https://example.com/1/");
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "mark_as_malware", "https://example.com/1/");
  PopulateArtificialDatabase();

  const GURL url("https://example.com/1/");
  MultipleArtificialMatchesTestClient client(url);
  EXPECT_FALSE(sb_local_database_manager_->CheckBrowseUrl(
      url, usual_threat_types_, &client, CheckBrowseUrlType::kHashDatabase));
  EXPECT_FALSE(client.called());

  WaitForTasksOnTaskRunner();

  EXPECT_TRUE(client.called());
  EXPECT_FALSE(FakeSBLocalDatabaseManager::PerformFullHashCheckCalled(
      sb_local_database_manager_));

  StopLocalDatabaseManager();
}

TEST_F(SBLocalDatabaseManagerTest_V5,
       FlagOneUrlAsPasswordProtectionAllowlisted) {
  SetupFakeManager();
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kMarkAsPasswordProtectionAllowlisted, "https://example.com/1/");

  StoreAndHashPrefixes store_and_hash_prefixes;
  ReplaceSBDatabase(store_and_hash_prefixes, /*stores_available=*/true);
  PopulateArtificialDatabase();

  const GURL url_allowlisted("https://example.com/1/");
  TestAllowlistClient client_allowlisted(
      /*match_expected=*/true, SB_THREAT_TYPE_CSD_ALLOWLIST);

  EXPECT_EQ(AsyncMatch::ASYNC, sb_local_database_manager_->CheckCsdAllowlistUrl(
                                   url_allowlisted, &client_allowlisted));
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client_allowlisted.callback_called());

  const GURL url_not_allowlisted("https://example.com/not_allowlisted/");
  TestAllowlistClient client_not_allowlisted(
      /*match_expected=*/false, SB_THREAT_TYPE_CSD_ALLOWLIST);

  EXPECT_EQ(AsyncMatch::ASYNC,
            sb_local_database_manager_->CheckCsdAllowlistUrl(
                url_not_allowlisted, &client_not_allowlisted));
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(client_not_allowlisted.callback_called());

  StopLocalDatabaseManager();
}

TEST_F(SBLocalDatabaseManagerTest_V5, FlagOneUrlAsHighConfidenceAllowlisted) {
  SetupFakeManager();
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kMarkAsHighConfidenceAllowlisted, "https://example.com/hc/");

  StoreAndHashPrefixes store_and_hash_prefixes;
  ReplaceSBDatabase(store_and_hash_prefixes, /*stores_available=*/true);
  PopulateArtificialDatabase();

  const GURL url_allowlisted("https://example.com/hc/");
  CheckUrlForHighConfidenceAllowlistFuture future_allowlisted;
  sb_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
      url_allowlisted, future_allowlisted.GetCallback());
  WaitForTasksOnTaskRunner();
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(future_allowlisted.Get<0>());

  const GURL url_not_allowlisted("https://example.com/not_hc/");
  CheckUrlForHighConfidenceAllowlistFuture future_not_allowlisted;
  sb_local_database_manager_->CheckUrlForHighConfidenceAllowlist(
      url_not_allowlisted, future_not_allowlisted.GetCallback());
  WaitForTasksOnTaskRunner();
  WaitForTasksOnTaskRunner();
  EXPECT_FALSE(future_not_allowlisted.Get<0>());

  StopLocalDatabaseManager();
}

// Verify that the correct set of lists is synced on each platform: iOS,
// Chrome-branded desktop, and non-Chrome-branded desktop.
TEST_P(SBLocalDatabaseManagerTest_V4V5, SyncedLists) {
  WaitForTasksOnTaskRunner();

#if BUILDFLAG(IS_IOS)
  std::vector<ListIdentifier> expected_lists{
      GetUrlSocEngId(),       GetUrlMalwareId(),
      GetUrlUwsId(),          GetUrlBillingId(),
      GetUrlCsdAllowlistId(), GetUrlHighConfidenceAllowlistId()};
#elif BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::vector<ListIdentifier> expected_lists{GetUrlSocEngId(),
                                             GetUrlMalwareId(),
                                             GetUrlUwsId(),
                                             GetUrlMalBinId(),
                                             GetChromeExtMalwareId(),
                                             GetUrlBillingId(),
                                             GetUrlCsdDownloadAllowlistId(),
                                             GetUrlCsdAllowlistId(),
                                             GetUrlSubresourceFilterId(),
                                             GetUrlSuspiciousSiteId(),
                                             GetUrlHighConfidenceAllowlistId()};
#else
  std::vector<ListIdentifier> expected_lists{GetUrlSocEngId(),
                                             GetUrlMalwareId(),
                                             GetUrlUwsId(),
                                             GetUrlMalBinId(),
                                             GetChromeExtMalwareId(),
                                             GetUrlBillingId(),
                                             GetUrlCsdDownloadAllowlistId()};
#endif

  std::vector<ListIdentifier> synced_lists;
  for (const auto& info : GetListInfos()) {
    if (info.fetch_updates()) {
      synced_lists.push_back(info.list_id());
    }
  }
  EXPECT_EQ(expected_lists, synced_lists);
}

TEST_F(SBLocalDatabaseManagerTest, TestQueuedChecksMatchArtificialPrefixes) {
  const GURL url("https://www.example.com/");
  TestClient client(SB_THREAT_TYPE_URL_MALWARE, url);
  EXPECT_TRUE(GetQueuedChecks().empty());
  sb_local_database_manager_->CheckBrowseUrl(url, usual_threat_types_, &client,
                                             CheckBrowseUrlType::kHashDatabase);
  // The database is unavailable so the check should get queued.
  EXPECT_EQ(1ul, GetQueuedChecks().size());

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "mark_as_malware", "https://example.com/");
  WaitForTasksOnTaskRunner();

  EXPECT_FALSE(client.on_check_browse_url_result_called());
  WaitForTasksOnTaskRunner();

  EXPECT_TRUE(client.on_check_browse_url_result_called());
  EXPECT_TRUE(GetQueuedChecks().empty());
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, DatabaseInitializationHistograms) {
  WaitForTasksOnTaskRunner();
  base::HistogramTester histograms;
  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  if (GetParam()) {
    histograms.ExpectTotalCount("SafeBrowsing.V4DatabaseInitializationTime", 0);
    histograms.ExpectTotalCount("SafeBrowsing.V5DatabaseInitializationTime", 1);
  } else {
    histograms.ExpectTotalCount("SafeBrowsing.V4DatabaseInitializationTime", 1);
    histograms.ExpectTotalCount("SafeBrowsing.V5DatabaseInitializationTime", 0);
  }
  histograms.ExpectTotalCount("SafeBrowsing.SBDatabaseInitializationTime", 1);
}

TEST_P(SBLocalDatabaseManagerTest_V4V5, TimeSinceLastUpdateResponseHistograms) {
  base::HistogramTester histograms;
  ASSERT_TRUE(sb_local_database_manager_->update_protocol_manager_);
  sb_local_database_manager_->update_protocol_manager_->last_response_time_ =
      base::Time::Now() - base::Seconds(10);

  const GURL url("http://example.com/a/");
  TestClient client(SB_THREAT_TYPE_SAFE, url);
  SetUpV5ClientIfNeeded(client, /*threat_type=*/SB_THREAT_TYPE_SAFE,
                        /*metadata=*/ThreatMetadata());
  sb_local_database_manager_->CheckBrowseUrl(url, usual_threat_types_, &client,
                                             CheckBrowseUrlType::kHashDatabase);
  WaitForTasksOnTaskRunner();

  histograms.ExpectTotalCount(
      "SafeBrowsing.V5LocalDatabaseManager.TimeSinceLastUpdateResponse",
      GetParam() ? 1 : 0);
  histograms.ExpectTotalCount(
      "SafeBrowsing.V4LocalDatabaseManager.TimeSinceLastUpdateResponse",
      GetParam() ? 0 : 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.SBLocalDatabaseManager.TimeSinceLastUpdateResponse", 1);
}

TEST_F(SBLocalDatabaseManagerTest, V5UpdateRequestCompleted) {
  WaitForTasksOnTaskRunner();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kLocalListsUseSBv5);

  ResetLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  ASSERT_TRUE(sb_local_database_manager_->IsDatabaseReady());

  std::unique_ptr<StoreStateMap> state_map =
      sb_local_database_manager_->GetStoreStateMap();
  ListIdentifier malware_list_id = GetUrlMalwareId();
  ASSERT_EQ(1u, state_map->count(malware_list_id));

  std::map<ListIdentifier, V5::HashList> parsed_server_response;
  V5::HashList hash_list;
  hash_list.set_name(GetV5ListName(malware_list_id));
  hash_list.set_version("new_version_state");
  hash_list.set_sha256_checksum(
      std::string(base::as_string_view(crypto::hash::Sha256(""))));

  parsed_server_response[malware_list_id] = hash_list;

  V5UpdateRequestCompleted(std::move(parsed_server_response));

  WaitForTasksOnTaskRunner();

  std::unique_ptr<StoreStateMap> new_state_map =
      sb_local_database_manager_->GetStoreStateMap();
  EXPECT_EQ("new_version_state", new_state_map->at(malware_list_id));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SBLocalDatabaseManagerTest_V4V5,
                         ::testing::Bool());

}  // namespace safe_browsing
