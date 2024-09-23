// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/hints_manager.h"

#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/bloom_filter.h"
#include "components/optimization_guide/core/hint_cache.h"
#include "components/optimization_guide/core/hints_component_util.h"
#include "components/optimization_guide/core/hints_fetcher.h"
#include "components/optimization_guide/core/hints_fetcher_factory.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_navigation_data.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/proto_database_provider_test_base.h"
#include "components/optimization_guide/core/tab_url_provider.h"
#include "components/optimization_guide/core/top_host_provider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

// Allows for default hour to pass + random delay between 30 and 60 seconds.
constexpr int kUpdateFetchHintsTimeSecs = 61 * 60;  // 1 hours and 1 minutes.

const int kDefaultHostBloomFilterNumHashFunctions = 7;
const int kDefaultHostBloomFilterNumBits = 511;

void PopulateBloomFilterWithDefaultHost(BloomFilter* bloom_filter) {
  bloom_filter->Add("host.com");
}

void AddBloomFilterToConfig(proto::OptimizationType optimization_type,
                            const BloomFilter& bloom_filter,
                            int num_hash_functions,
                            int num_bits,
                            bool is_allowlist,
                            proto::Configuration* config) {
  std::string bloom_filter_data(
      reinterpret_cast<const char*>(&bloom_filter.bytes()[0]),
      bloom_filter.bytes().size());
  proto::OptimizationFilter* of_proto =
      is_allowlist ? config->add_optimization_allowlists()
                   : config->add_optimization_blocklists();
  of_proto->set_optimization_type(optimization_type);
  std::unique_ptr<proto::BloomFilter> bloom_filter_proto =
      std::make_unique<proto::BloomFilter>();
  bloom_filter_proto->set_num_hash_functions(num_hash_functions);
  bloom_filter_proto->set_num_bits(num_bits);
  bloom_filter_proto->set_data(bloom_filter_data);
  of_proto->set_allocated_bloom_filter(bloom_filter_proto.release());
}

std::unique_ptr<proto::GetHintsResponse> BuildHintsResponse(
    const std::vector<std::string>& hosts,
    const std::vector<std::string>& urls) {
  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  for (const auto& host : hosts) {
    proto::Hint* hint = get_hints_response->add_hints();
    hint->set_key_representation(proto::HOST);
    hint->set_key(host);
    hint->add_allowlisted_optimizations()->set_optimization_type(
        proto::NOSCRIPT);
    proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern("page pattern");
    proto::Optimization* opt = page_hint->add_allowlisted_optimizations();
    opt->set_optimization_type(proto::DEFER_ALL_SCRIPT);
  }
  for (const auto& url : urls) {
    proto::Hint* hint = get_hints_response->add_hints();
    hint->set_key_representation(proto::FULL_URL);
    hint->set_key(url);
    hint->mutable_max_cache_duration()->set_seconds(60 * 60);
    proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern(url);
    proto::Optimization* opt = page_hint->add_allowlisted_optimizations();
    opt->set_optimization_type(proto::COMPRESS_PUBLIC_IMAGES);
    opt->mutable_any_metadata()->set_type_url("someurl");
  }
  return get_hints_response;
}

void RunHintsFetchedCallbackWithResponse(
    HintsFetchedCallback hints_fetched_callback,
    std::unique_ptr<proto::GetHintsResponse> response) {
  std::move(hints_fetched_callback).Run(std::move(response));
}

// Returns the default params used for the kOptimizationHints feature.
base::FieldTrialParams GetOptimizationHintsDefaultFeatureParams() {
  return {{
      "max_host_keyed_hint_cache_size",
      "1",
  }};
}

std::unique_ptr<base::test::ScopedFeatureList>
SetUpDeferStartupActiveTabsHintsFetch(bool is_enabled) {
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list =
      std::make_unique<base::test::ScopedFeatureList>();
  auto params = GetOptimizationHintsDefaultFeatureParams();

  params["defer_startup_active_tabs_hints_fetch"] =
      is_enabled ? "true" : "false";
  scoped_feature_list->InitAndEnableFeatureWithParameters(
      features::kOptimizationHints, params);
  return scoped_feature_list;
}

}  // namespace

// A mock class implementation of TopHostProvider.
class FakeTopHostProvider : public TopHostProvider {
 public:
  explicit FakeTopHostProvider(const std::vector<std::string>& top_hosts)
      : top_hosts_(top_hosts) {}

  std::vector<std::string> GetTopHosts() override {
    num_top_hosts_called_++;

    return top_hosts_;
  }

  int get_num_top_hosts_called() const { return num_top_hosts_called_; }

 private:
  std::vector<std::string> top_hosts_;
  int num_top_hosts_called_ = 0;
};

// A mock class implementation of TabUrlProvider.
class FakeTabUrlProvider : public TabUrlProvider {
 public:
  const std::vector<GURL> GetUrlsOfActiveTabs(
      const base::TimeDelta& duration_since_last_shown) override {
    num_urls_called_++;
    return urls_;
  }

  void SetUrls(const std::vector<GURL>& urls) { urls_ = urls; }

  int get_num_urls_called() const { return num_urls_called_; }

 private:
  std::vector<GURL> urls_;
  int num_urls_called_ = 0;
};

enum class HintsFetcherEndState {
  kFetchFailed = 0,
  kFetchSuccessWithHostHints = 1,
  kFetchSuccessWithNoHints = 2,
  kFetchSuccessWithURLHints = 3,
};

// A mock class implementation of HintsFetcher. It will iterate through the
// provided fetch states each time it is called. If it reaches the end of the
// loop, it will just continue using the last fetch state.
class TestHintsFetcher : public HintsFetcher {
 public:
  TestHintsFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL optimization_guide_service_url,
      PrefService* pref_service,
      const std::vector<HintsFetcherEndState>& fetch_states,
      OptimizationGuideLogger* optimization_guide_logger)
      : HintsFetcher(url_loader_factory,
                     optimization_guide_service_url,
                     pref_service,
                     &optimization_guide_logger_),
        fetch_states_(fetch_states) {
    DCHECK(!fetch_states_.empty());
  }
  bool is_request_context_metadata_filled = false;
  bool FetchOptimizationGuideServiceHints(
      const std::vector<std::string>& hosts,
      const std::vector<GURL>& urls,
      const base::flat_set<proto::OptimizationType>& optimization_types,
      proto::RequestContext request_context,
      const std::string& locale,
      const std::string& access_token,
      bool skip_cache,
      HintsFetchedCallback hints_fetched_callback,
      std::optional<proto::RequestContextMetadata> request_context_metadata)
      override {
    HintsFetcherEndState fetch_state =
        num_fetches_requested_ < static_cast<int>(fetch_states_.size())
            ? fetch_states_[num_fetches_requested_]
            : fetch_states_.back();
    if (request_context_metadata.has_value()) {
      is_request_context_metadata_filled = true;
    }
    num_fetches_requested_++;
    locale_requested_ = locale;
    request_context_requested_ = request_context;
    switch (fetch_state) {
      case HintsFetcherEndState::kFetchFailed:
        std::move(hints_fetched_callback).Run(std::nullopt);
        return false;
      case HintsFetcherEndState::kFetchSuccessWithHostHints:
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&RunHintsFetchedCallbackWithResponse,
                                      std::move(hints_fetched_callback),
                                      BuildHintsResponse({"host.com"}, {})));
        return true;
      case HintsFetcherEndState::kFetchSuccessWithURLHints:
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &RunHintsFetchedCallbackWithResponse,
                std::move(hints_fetched_callback),
                BuildHintsResponse({"somedomain.org"},
                                   {"https://somedomain.org/news/whatever"})));
        return true;
      case HintsFetcherEndState::kFetchSuccessWithNoHints:
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&RunHintsFetchedCallbackWithResponse,
                                      std::move(hints_fetched_callback),
                                      BuildHintsResponse({}, {})));
        return true;
    }
    return true;
  }

  int num_fetches_requested() const { return num_fetches_requested_; }

  std::string locale_requested() const { return locale_requested_; }

  proto::RequestContext request_context_requested() const {
    return request_context_requested_;
  }

 private:
  OptimizationGuideLogger optimization_guide_logger_;
  std::vector<HintsFetcherEndState> fetch_states_;
  int num_fetches_requested_ = 0;
  std::string locale_requested_;
  proto::RequestContext request_context_requested_ =
      proto::RequestContext::CONTEXT_UNSPECIFIED;
};

// A mock class of HintsFetcherFactory that returns instances of
// TestHintsFetchers with the provided fetch state.
class TestHintsFetcherFactory : public HintsFetcherFactory {
 public:
  TestHintsFetcherFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL optimization_guide_service_url,
      PrefService* pref_service,
      const std::vector<HintsFetcherEndState>& fetch_states)
      : HintsFetcherFactory(url_loader_factory,
                            optimization_guide_service_url,
                            pref_service),
        fetch_states_(fetch_states) {}

  std::unique_ptr<HintsFetcher> BuildInstance(
      OptimizationGuideLogger* optimization_guide_logger) override {
    return std::make_unique<TestHintsFetcher>(
        url_loader_factory_, optimization_guide_service_url_, pref_service_,
        fetch_states_, optimization_guide_logger);
  }

 private:
  std::vector<HintsFetcherEndState> fetch_states_;
};

class HintsManagerTest : public ProtoDatabaseProviderTestBase {
 public:
  HintsManagerTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kOptimizationHints,
             GetOptimizationHintsDefaultFeatureParams()},
            {features::kRemoteOptimizationGuideFetching,
             {{"batch_update_hints_for_top_hosts", "true"}}},
            {features::kOptimizationHintsComponent,
             {{"check_failed_component_version_pref", "true"}}},
        },
        /*disabled_features=*/{});

    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    prefs::RegisterProfilePrefs(pref_service_->registry());
    unified_consent::UnifiedConsentService::RegisterPrefs(
        pref_service_->registry());
  }
  ~HintsManagerTest() override = default;

  HintsManagerTest(const HintsManagerTest&) = delete;
  HintsManagerTest& operator=(const HintsManagerTest&) = delete;

  void SetUp() override {
    ProtoDatabaseProviderTestBase::SetUp();
    CreateHintsManager(/*top_host_provider=*/nullptr);
  }

  void TearDown() override {
    ResetHintsManager();
    pref_service_.reset();
    ProtoDatabaseProviderTestBase::TearDown();
  }

  void CreateHintsManager(
      std::unique_ptr<FakeTopHostProvider> top_host_provider,
      signin::IdentityManager* identity_manager = nullptr) {
    if (hints_manager_)
      ResetHintsManager();

    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    hint_store_ = std::make_unique<OptimizationGuideStore>(
        db_provider_.get(), temp_dir(),
        task_environment_.GetMainThreadTaskRunner(), pref_service_.get());

    tab_url_provider_ = std::make_unique<FakeTabUrlProvider>();

    top_host_provider_ = std::move(top_host_provider);

    hints_manager_ = std::make_unique<HintsManager>(
        /*is_off_the_record=*/false, /*application_locale=*/"en-US",
        pref_service(), hint_store_->AsWeakPtr(), top_host_provider_.get(),
        tab_url_provider_.get(), url_loader_factory_,
        /*push_notification_manager=*/nullptr,
        /*identity_manager=*/identity_manager, &optimization_guide_logger_);
    hints_manager_->SetClockForTesting(task_environment_.GetMockClock());

    // Run until hint cache is initialized and the HintsManager is ready to
    // process hints.
    RunUntilIdle();
  }

  void ResetHintsManager() {
    hints_manager_->Shutdown();
    hints_manager_.reset();
    tab_url_provider_.reset();
    if (top_host_provider_) {
      top_host_provider_.reset();
    }
    hint_store_.reset();
    RunUntilIdle();
  }

  void ProcessInvalidHintsComponentInfo(const std::string& version) {
    HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("notaconfigfile")));

    base::RunLoop run_loop;
    hints_manager_->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager_->OnHintsComponentAvailable(info);
    run_loop.Run();
  }

  void ProcessHintsComponentInfoWithBadConfig(const std::string& version) {
    HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("badconfig.pb")));
    ASSERT_TRUE(base::WriteFile(info.path, "garbage"));

    hints_manager_->OnHintsComponentAvailable(info);
    RunUntilIdle();
  }

  void ProcessHints(const proto::Configuration& config,
                    const std::string& version,
                    bool should_wait = true) {
    HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("somefile.pb")));
    ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(config, info.path));

    base::RunLoop run_loop;
    if (should_wait)
      hints_manager_->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager_->OnHintsComponentAvailable(info);
    if (should_wait)
      run_loop.Run();
  }

  void InitializeWithDefaultConfig(const std::string& version,
                                   bool should_wait = true) {
    proto::Configuration config;
    proto::Hint* hint1 = config.add_hints();
    hint1->set_key("somedomain.org");
    hint1->set_key_representation(proto::HOST);
    hint1->set_version("someversion");
    proto::PageHint* page_hint1 = hint1->add_page_hints();
    page_hint1->set_page_pattern("/news/");
    proto::Optimization* default_opt =
        page_hint1->add_allowlisted_optimizations();
    default_opt->set_optimization_type(proto::NOSCRIPT);
    // Add another hint so somedomain.org hint is not in-memory initially.
    proto::Hint* hint2 = config.add_hints();
    hint2->set_key("somedomain2.org");
    hint2->set_key_representation(proto::HOST);
    hint2->set_version("someversion");
    proto::Optimization* opt = hint2->add_allowlisted_optimizations();
    opt->set_optimization_type(proto::NOSCRIPT);

    ProcessHints(config, version, should_wait);
  }

  std::unique_ptr<HintsFetcherFactory> BuildTestHintsFetcherFactory(
      const std::vector<HintsFetcherEndState>& fetch_states) {
    return std::make_unique<TestHintsFetcherFactory>(
        url_loader_factory_, GURL("https://hintsserver.com"), pref_service(),
        fetch_states);
  }

  void MoveClockForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
    RunUntilIdle();
  }

  // Creates navigation data for a navigation to |url| with registered
  // |optimization_types|.
  std::unique_ptr<OptimizationGuideNavigationData> CreateTestNavigationData(
      const GURL& url,
      const std::vector<proto::OptimizationType>& optimization_types) {
    auto navigation_data = std::make_unique<OptimizationGuideNavigationData>(
        /*navigation_id=*/1, /*navigation_start*/ base::TimeTicks::Now());
    navigation_data->set_navigation_url(url);
    navigation_data->set_registered_optimization_types(optimization_types);
    return navigation_data;
  }

  void CallOnNavigationStartOrRedirect(
      OptimizationGuideNavigationData* navigation_data,
      base::OnceClosure callback) {
    hints_manager()->OnNavigationStartOrRedirect(navigation_data,
                                                 std::move(callback));
  }

  HintsManager* hints_manager() const { return hints_manager_.get(); }

  int32_t num_batch_update_hints_fetches_initiated() const {
    return hints_manager()->num_batch_update_hints_fetches_initiated();
  }

  TestHintsFetcher* active_tabs_batch_update_hints_fetcher() const {
    return static_cast<TestHintsFetcher*>(
        hints_manager()->active_tabs_batch_update_hints_fetcher());
  }

  GURL url_with_hints() const {
    return GURL("https://somedomain.org/news/whatever");
  }

  GURL url_with_url_keyed_hint() const {
    return GURL("https://somedomain.org/news/whatever");
  }

  GURL url_without_hints() const {
    return GURL("https://url_without_hints.org/");
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

  PrefService* pref_service() const { return pref_service_.get(); }

  FakeTabUrlProvider* tab_url_provider() const {
    return tab_url_provider_.get();
  }

  FakeTopHostProvider* top_host_provider() const {
    return top_host_provider_.get();
  }

  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  OptimizationGuideLogger optimization_guide_logger_;
  std::unique_ptr<HintsManager> hints_manager_;

 private:
  void WriteConfigToFile(const proto::Configuration& config,
                         const base::FilePath& filePath) {
    std::string serialized_config;
    ASSERT_TRUE(config.SerializeToString(&serialized_config));
    ASSERT_TRUE(base::WriteFile(filePath, serialized_config));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<OptimizationGuideStore> hint_store_;
  std::unique_ptr<FakeTabUrlProvider> tab_url_provider_;
  std::unique_ptr<FakeTopHostProvider> top_host_provider_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(HintsManagerTest, ProcessHintsWithValidCommandLineOverride) {
  base::HistogramTester histogram_tester;

  proto::Configuration config;
  proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(proto::HOST);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  proto::Optimization* optimization =
      page_hint->add_allowlisted_optimizations();
  optimization->set_optimization_type(proto::NOSCRIPT);
  BloomFilter bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                           kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  AddBloomFilterToConfig(proto::PERFORMANCE_HINTS, bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  encoded_config = base::Base64Encode(encoded_config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kHintsProtoOverride, encoded_config);
  CreateHintsManager(/*top_host_provider=*/nullptr);
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  // The below histogram should not be recorded since hints weren't coming
  // directly from the component.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                      ProcessHintsComponentResult::kSuccess, 1);
  // However, we still expect the local histogram for the hints being updated to
  // be recorded.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.UpdateComponentHints.Result", true, 1);

  // Bloom filters passed via command line are processed on the background
  // thread so make sure everything has finished before checking if it has been
  // loaded.
  RunUntilIdle();

  EXPECT_TRUE(hints_manager()->HasLoadedOptimizationBlocklist(
      proto::LITE_PAGE_REDIRECT));
  EXPECT_FALSE(hints_manager()->HasLoadedOptimizationAllowlist(
      proto::PERFORMANCE_HINTS));
  const base::Value::Dict& previous_opt_types_with_filter =
      pref_service()->GetDict(prefs::kPreviousOptimizationTypesWithFilter);
  EXPECT_EQ(2u, previous_opt_types_with_filter.size());
  EXPECT_TRUE(previous_opt_types_with_filter.contains(
      optimization_guide::proto::OptimizationType_Name(
          proto::LITE_PAGE_REDIRECT)));
  EXPECT_TRUE(previous_opt_types_with_filter.contains(
      optimization_guide::proto::OptimizationType_Name(
          proto::PERFORMANCE_HINTS)));

  // Now register a new type with an allowlist that has not yet been loaded.
  hints_manager()->RegisterOptimizationTypes({proto::PERFORMANCE_HINTS});
  RunUntilIdle();

  EXPECT_TRUE(hints_manager()->HasLoadedOptimizationAllowlist(
      proto::PERFORMANCE_HINTS));
}

TEST_F(HintsManagerTest, ProcessHintsWithInvalidCommandLineOverride) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kHintsProtoOverride, "this-is-not-a-proto");
  CreateHintsManager(/*top_host_provider=*/nullptr);

  // The below histogram should not be recorded since hints weren't coming
  // directly from the component.
  histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult", 0);
  // We also do not expect to update the component hints with bad hints either.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.UpdateComponentHints.Result", 0);
}

TEST_F(HintsManagerTest,
       ProcessHintsWithCommandLineOverrideShouldNotBeOverriddenByNewComponent) {
  proto::Configuration config;
  proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(proto::HOST);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  proto::Optimization* optimization =
      page_hint->add_allowlisted_optimizations();
  optimization->set_optimization_type(proto::NOSCRIPT);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  encoded_config = base::Base64Encode(encoded_config);

  {
    base::HistogramTester histogram_tester;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kHintsProtoOverride, encoded_config);
    CreateHintsManager(/*top_host_provider=*/nullptr);
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.UpdateComponentHints.Result", true, 1);
  }

  // Test that a new component coming in does not update the component hints.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("3.0.0.0");
    // The below histograms should not be recorded since component hints
    // processing is disabled.
    histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult",
                                      0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.UpdateComponentHints.Result", 0);
  }
}

TEST_F(HintsManagerTest, ParseTwoConfigVersions) {
  proto::Configuration config;
  proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(proto::HOST);
  hint1->set_version("someversion");
  proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/news/");
  proto::Optimization* optimization1 =
      page_hint1->add_allowlisted_optimizations();
  optimization1->set_optimization_type(proto::RESOURCE_LOADING);

  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("1.0.0.0");
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }

  // Test the second time parsing the config. This should also update the hints.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0.0");
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }
}

TEST_F(HintsManagerTest, ParseInvalidConfigVersions) {
  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("1.0.0.0");
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }

  {
    base::HistogramTester histogram_tester;
    ProcessHintsComponentInfoWithBadConfig("2.0.0.0");
    // If we have already parsed a version later than this version, we expect
    // for the hints to not be updated.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        ProcessHintsComponentResult::kFailedInvalidConfiguration, 1);
  }
}

TEST_F(HintsManagerTest, ComponentProcessingWhileShutdown) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("10.0.0.0", /*should_wait=*/false);
  hints_manager()->Shutdown();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessingComponentAtShutdown", true, 1);

  EXPECT_TRUE(
      pref_service()->GetString(prefs::kPendingHintsProcessingVersion).empty());
}

TEST_F(HintsManagerTest, ParseOlderConfigVersions) {
  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("10.0.0.0");
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }

  // Test the second time parsing the config. This will be treated by the cache
  // as an older version.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0.0");
    // If we have already parsed a version later than this version, we expect
    // for the hints to not be updated.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        ProcessHintsComponentResult::kSkippedProcessingHints, 1);
  }
}

TEST_F(HintsManagerTest, ParseDuplicateConfigVersions) {
  const std::string version = "3.0.0.0";

  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig(version);
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }

  // Test the second time parsing the config. This will be treated by the cache
  // as a duplicate version.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig(version);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        ProcessHintsComponentResult::kSkippedProcessingHints, 1);
  }
}

TEST_F(HintsManagerTest, ComponentInfoDidNotContainConfig) {
  base::HistogramTester histogram_tester;
  ProcessInvalidHintsComponentInfo("1.0.0.0");
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessHintsResult",
      ProcessHintsComponentResult::kFailedReadingFile, 1);
}

TEST_F(HintsManagerTest, ProcessHintsWithExistingPref) {
  // Write hints processing pref for version 2.0.0.
  pref_service()->SetString(prefs::kPendingHintsProcessingVersion, "2.0.0");
  CreateHintsManager(/*top_host_provider=*/nullptr);

  // Verify config not processed for same version (2.0.0) and pref not cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        ProcessHintsComponentResult::kFailedFinishProcessing, 1);
    EXPECT_FALSE(pref_service()
                     ->GetString(prefs::kPendingHintsProcessingVersion)
                     .empty());
  }

  // Now verify config is processed for different version and pref cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("3.0.0");
    EXPECT_TRUE(pref_service()
                    ->GetString(prefs::kPendingHintsProcessingVersion)
                    .empty());
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }
}

TEST_F(HintsManagerTest,
       ProcessHintsWithExistingPrefDoesNotClearOrCountAsMidProcessing) {
  // Write hints processing pref for version 2.0.0.
  pref_service()->SetString(prefs::kPendingHintsProcessingVersion, "2.0.0");
  CreateHintsManager(/*top_host_provider=*/nullptr);

  // Verify component for same version counts as "failed".
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("2.0.0", /*should_wait=*/false);
  hints_manager()->Shutdown();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessHintsResult",
      ProcessHintsComponentResult::kFailedFinishProcessing, 1);

  // Verify that pref still not cleared at shutdown and was not counted as
  // mid-processing.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessingComponentAtShutdown", false, 1);
  EXPECT_FALSE(
      pref_service()->GetString(prefs::kPendingHintsProcessingVersion).empty());
}

TEST_F(HintsManagerTest, ProcessHintsWithInvalidPref) {
  // Create pref file with invalid version.
  pref_service()->SetString(prefs::kPendingHintsProcessingVersion, "bad-2.0.0");
  CreateHintsManager(/*top_host_provider=*/nullptr);

  // Verify config is processed with pref cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0");
    EXPECT_TRUE(pref_service()
                    ->GetString(prefs::kPendingHintsProcessingVersion)
                    .empty());
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }
}

TEST_F(HintsManagerTest, ProcessHintsUpdatePreviousOptTypesWithFilter) {
  proto::Configuration config_one;
  BloomFilter bloom_filter_one(kDefaultHostBloomFilterNumHashFunctions,
                               kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&bloom_filter_one);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, bloom_filter_one,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config_one);
  AddBloomFilterToConfig(proto::PERFORMANCE_HINTS, bloom_filter_one,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config_one);
  ProcessHints(config_one, "1.0.0.0");

  const base::Value::Dict& dic_one =
      pref_service()->GetDict(prefs::kPreviousOptimizationTypesWithFilter);
  EXPECT_EQ(2u, dic_one.size());
  EXPECT_TRUE(dic_one.contains(optimization_guide::proto::OptimizationType_Name(
      proto::LITE_PAGE_REDIRECT)));
  EXPECT_TRUE(dic_one.contains(optimization_guide::proto::OptimizationType_Name(
      proto::PERFORMANCE_HINTS)));

  proto::Configuration config_two;
  BloomFilter bloom_filter_two(kDefaultHostBloomFilterNumHashFunctions,
                               kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&bloom_filter_two);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, bloom_filter_two,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config_two);
  ProcessHints(config_two, "2.0.0.0");

  const base::Value::Dict& dic_two =
      pref_service()->GetDict(prefs::kPreviousOptimizationTypesWithFilter);
  EXPECT_EQ(1u, dic_two.size());
  EXPECT_TRUE(dic_two.contains(optimization_guide::proto::OptimizationType_Name(
      proto::LITE_PAGE_REDIRECT)));
}

TEST_F(HintsManagerTest,
       OnNavigationStartOrRedirectNoTypesRegisteredShouldNotLoadHint) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  auto navigation_data = CreateTestNavigationData(url_with_hints(), {});

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectTotalCount("OptimizationGuide.LoadedHint.Result", 0);
}

TEST_F(HintsManagerTest, OnNavigationStartOrRedirectWithHint) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  auto navigation_data = CreateTestNavigationData(url_with_hints(), {});

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
}

TEST_F(HintsManagerTest, OnNavigationStartOrRedirectNoHint) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  auto navigation_data =
      CreateTestNavigationData(GURL("https://notinhints.com"), {});

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      false, 1);
}

TEST_F(HintsManagerTest, OnNavigationStartOrRedirectNoHost) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  auto navigation_data = CreateTestNavigationData(GURL("blargh"), {});

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectTotalCount("OptimizationGuide.LoadedHint.Result", 0);
}

TEST_F(HintsManagerTest, OptimizationFiltersAreOnlyLoadedIfTypeIsRegistered) {
  proto::Configuration config;
  BloomFilter bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                           kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  AddBloomFilterToConfig(proto::NOSCRIPT, bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  AddBloomFilterToConfig(proto::DEFER_ALL_SCRIPT, bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);

  {
    base::HistogramTester histogram_tester;

    ProcessHints(config, "1.0.0.0");

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript", 0);
  }

  // Now register the optimization type and see that it is loaded.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});
    run_loop.Run();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        OptimizationFilterStatus::kFoundServerFilterConfig, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        OptimizationFilterStatus::kCreatedServerFilter, 1);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript", 0);
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationBlocklist(
        proto::LITE_PAGE_REDIRECT));
    EXPECT_FALSE(
        hints_manager()->HasLoadedOptimizationBlocklist(proto::NOSCRIPT));
    EXPECT_FALSE(hints_manager()->HasLoadedOptimizationAllowlist(
        proto::DEFER_ALL_SCRIPT));
  }

  // Re-registering the same optimization type does not re-load the filter.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});
    run_loop.Run();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript", 0);
  }

  // Registering a new optimization type without a filter does not trigger a
  // reload of the filter.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes({proto::PERFORMANCE_HINTS});
    run_loop.Run();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript", 0);
  }

  // Registering a new optimization types with filters does trigger a
  // reload of the filters.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {proto::NOSCRIPT, proto::DEFER_ALL_SCRIPT});
    run_loop.Run();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        OptimizationFilterStatus::kFoundServerFilterConfig, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        OptimizationFilterStatus::kCreatedServerFilter, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript",
        OptimizationFilterStatus::kFoundServerFilterConfig, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript",
        OptimizationFilterStatus::kCreatedServerFilter, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript",
        OptimizationFilterStatus::kFoundServerFilterConfig, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript",
        OptimizationFilterStatus::kCreatedServerFilter, 1);
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationBlocklist(
        proto::LITE_PAGE_REDIRECT));
    EXPECT_TRUE(
        hints_manager()->HasLoadedOptimizationBlocklist(proto::NOSCRIPT));
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationAllowlist(
        proto::DEFER_ALL_SCRIPT));
  }
}

TEST_F(HintsManagerTest, OptimizationFiltersOnlyLoadOncePerType) {
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  base::HistogramTester histogram_tester;

  proto::Configuration config;
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  // Make sure it will only load one of an allowlist or a blocklist.
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);
  ProcessHints(config, "1.0.0.0");

  // We found 2 LPR blocklists: parsed one and duped the other.
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      OptimizationFilterStatus::kFoundServerFilterConfig, 3);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      OptimizationFilterStatus::kCreatedServerFilter, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      OptimizationFilterStatus::kFailedServerFilterDuplicateConfig, 2);
}

TEST_F(HintsManagerTest, InvalidOptimizationFilterNotLoaded) {
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  base::HistogramTester histogram_tester;

  int too_many_bits = features::MaxServerBloomFilterByteSize() * 8 + 1;

  proto::Configuration config;
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     too_many_bits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions, too_many_bits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      OptimizationFilterStatus::kFoundServerFilterConfig, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      OptimizationFilterStatus::kFailedServerFilterTooBig, 1);
  EXPECT_FALSE(hints_manager()->HasLoadedOptimizationBlocklist(
      proto::LITE_PAGE_REDIRECT));
}

TEST_F(HintsManagerTest, CanApplyOptimizationUrlWithNoHost) {
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(GURL("urlwithnohost"),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(OptimizationTypeDecision::kInvalidURL, optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasFilterForTypeButNotLoadedYet_ComponentReady) {
  // Simulate a situation where the component is ready, but the filter has not
  // been loaded yet.
  proto::Configuration config;
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");
  // Append the switch for processing hints to force the filter to not get
  // loaded.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kHintsProtoOverride);

  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(GURL("https://whatever.com/123"),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kHadOptimizationFilterButNotLoadedInTime,
            optimization_type_decision);

  // Run until idle to ensure we don't crash because the test object has gone
  // away.
  RunUntilIdle();
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasFilterForTypeButNotLoadedYet_ComponentNotReady) {
  // Simulate a situation where the component not ready, but we know from
  // previous sessions that LITE_PAGE_REDIRECT type has a filter.
  ScopedDictPrefUpdate previous_opt_types_with_filter(
      pref_service(), prefs::kPreviousOptimizationTypesWithFilter);
  previous_opt_types_with_filter->Set(
      optimization_guide::proto::OptimizationType_Name(
          proto::LITE_PAGE_REDIRECT),
      true);

  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(GURL("https://whatever.com/123"),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kHadOptimizationFilterButNotLoadedInTime,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlInAllowlist) {
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  BloomFilter allowlist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&allowlist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, allowlist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);
  ProcessHints(config, "1.0.0.0");

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(GURL("https://m.host.com/123"),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlInBlocklist) {
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(GURL("https://m.host.com/123"),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kNotAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlNotInAllowlistFilter) {
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  BloomFilter allowlist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&allowlist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, allowlist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);
  ProcessHints(config, "1.0.0.0");

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(GURL("https://whatever.com/123"),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kNotAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlNotInBlocklistFilter) {
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(GURL("https://whatever.com/123"),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationOptimizationTypeAllowlistedAtTopLevel) {
  proto::Configuration config;
  proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(proto::HOST);
  hint1->set_version("someversion");
  proto::Optimization* opt1 = hint1->add_allowlisted_optimizations();
  opt1->set_optimization_type(proto::RESOURCE_LOADING);
  ProcessHints(config, "1.0.0.0");

  hints_manager()->RegisterOptimizationTypes({proto::RESOURCE_LOADING});

  auto navigation_data =
      CreateTestNavigationData(url_with_hints(), {proto::RESOURCE_LOADING});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::RESOURCE_LOADING,
                                            &optimization_metadata);
  EXPECT_EQ(OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationHasPageHintButNoMatchingOptType) {
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});

  auto navigation_data =
      CreateTestNavigationData(url_with_hints(), {proto::DEFER_ALL_SCRIPT});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::DEFER_ALL_SCRIPT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationAndPopulatesLoadingPredictorMetadata) {
  hints_manager()->RegisterOptimizationTypes({proto::LOADING_PREDICTOR});
  proto::Configuration config;
  proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(proto::HOST);
  hint->set_version("someversion");
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("/news/");
  proto::Optimization* opt = page_hint->add_allowlisted_optimizations();
  opt->set_optimization_type(proto::LOADING_PREDICTOR);
  opt->mutable_loading_predictor_metadata()->add_subresources()->set_url(
      "https://resource.com/");

  ProcessHints(config, "1.0.0.0");

  auto navigation_data =
      CreateTestNavigationData(url_with_hints(), {proto::LOADING_PREDICTOR});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::LOADING_PREDICTOR,
                                            &optimization_metadata);
  // Make sure loading predictor metadata is populated.
  EXPECT_TRUE(optimization_metadata.loading_predictor_metadata().has_value());
  EXPECT_EQ(OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationAndPopulatesAnyMetadata) {
  hints_manager()->RegisterOptimizationTypes({proto::LOADING_PREDICTOR});
  proto::Configuration config;
  proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(proto::HOST);
  hint->set_version("someversion");
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("/news/");
  proto::Optimization* opt = page_hint->add_allowlisted_optimizations();
  opt->set_optimization_type(proto::LOADING_PREDICTOR);
  proto::LoadingPredictorMetadata lp_metadata;
  lp_metadata.add_subresources()->set_url("https://resource.com/");
  lp_metadata.SerializeToString(opt->mutable_any_metadata()->mutable_value());
  opt->mutable_any_metadata()->set_type_url(
      "type.googleapis.com/com.foo.LoadingPredictorMetadata");

  ProcessHints(config, "1.0.0.0");

  auto navigation_data =
      CreateTestNavigationData(url_with_hints(), {proto::LOADING_PREDICTOR});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::LOADING_PREDICTOR,
                                            &optimization_metadata);
  // Make sure loading predictor metadata is populated.
  EXPECT_TRUE(
      optimization_metadata.ParsedMetadata<proto::LoadingPredictorMetadata>()
          .has_value());
  EXPECT_EQ(OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationNoMatchingPageHint) {
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->RegisterOptimizationTypes({proto::NOSCRIPT});

  auto navigation_data =
      CreateTestNavigationData(GURL("https://somedomain.org/nomatch"), {});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::NOSCRIPT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationNoHintForNavigationMetadataClearedAnyway) {
  InitializeWithDefaultConfig("1.0.0.0");

  auto navigation_data = CreateTestNavigationData(
      GURL("https://nohint.com"), {proto::COMPRESS_PUBLIC_IMAGES});

  hints_manager()->RegisterOptimizationTypes({proto::NOSCRIPT});
  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::NOSCRIPT,
                                            &optimization_metadata);

  EXPECT_EQ(OptimizationTypeDecision::kNoHintAvailable,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationHasHintInCacheButNotLoaded) {
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->RegisterOptimizationTypes({proto::NOSCRIPT});
  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(url_with_hints(), proto::NOSCRIPT,
                                            &optimization_metadata);

  EXPECT_EQ(OptimizationTypeDecision::kHadHintButNotLoadedInTime,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationFilterTakesPrecedence) {
  auto navigation_data = CreateTestNavigationData(
      GURL("https://m.host.com/urlinfilterandhints"), {});

  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  proto::Hint* hint1 = config.add_hints();
  hint1->set_key("host.com");
  hint1->set_key_representation(proto::HOST);
  hint1->set_version("someversion");
  proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("https://m.host.com");
  proto::Optimization* optimization1 =
      page_hint1->add_allowlisted_optimizations();
  optimization1->set_optimization_type(proto::LITE_PAGE_REDIRECT);
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  // Make sure decision points logged correctly.
  EXPECT_EQ(OptimizationTypeDecision::kNotAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationFilterTakesPrecedenceMatchesFilter) {
  auto navigation_data =
      CreateTestNavigationData(GURL("https://notfiltered.com/whatever"),
                               {proto::COMPRESS_PUBLIC_IMAGES});

  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  proto::Hint* hint1 = config.add_hints();
  hint1->set_key("notfiltered.com");
  hint1->set_key_representation(proto::HOST);
  hint1->set_version("someversion");
  proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("https://notfiltered.com");
  proto::Optimization* optimization1 =
      page_hint1->add_allowlisted_optimizations();
  optimization1->set_optimization_type(proto::LITE_PAGE_REDIRECT);
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kAllowedByOptimizationFilter,
            optimization_type_decision);
}

class HintsManagerFetchingDisabledTest : public HintsManagerTest {
 public:
  HintsManagerFetchingDisabledTest() {
    scoped_list_.InitAndDisableFeature(
        features::kRemoteOptimizationGuideFetching);
  }

 private:
  base::test::ScopedFeatureList scoped_list_;
};

TEST_F(HintsManagerFetchingDisabledTest,
       HintsFetchNotAllowedIfFeatureIsNotEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);

  CreateHintsManager(std::make_unique<FakeTopHostProvider>(
      std::vector<std::string>({"example1.com", "example2.com"})));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::Seconds(kUpdateFetchHintsTimeSecs));
  EXPECT_EQ(0, top_host_provider()->get_num_top_hosts_called());
  // Hints fetcher should not even be created.
  EXPECT_FALSE(active_tabs_batch_update_hints_fetcher());
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationAsyncReturnsRightAwayIfNotAllowedToFetch) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {proto::COMPRESS_PUBLIC_IMAGES});
  hints_manager()->CanApplyOptimizationAsync(
      url_without_hints(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kFalse, decision);
      }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kNoHintAvailable, 1);
}

TEST_F(
    HintsManagerTest,
    CanApplyOptimizationAsyncReturnsRightAwayIfNotAllowedToFetchAndNotAllowlistedByAvailableHint) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {proto::COMPRESS_PUBLIC_IMAGES});
  // Wait for hint to be loaded.
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  hints_manager()->CanApplyOptimizationAsync(
      url_with_hints(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kFalse, decision);
      }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kNotAllowedByHint, 1);
}

TEST_F(HintsManagerTest, RemoveFetchedEntriesByHintKeys_Host) {
  int cache_duration_in_secs = 60;
  GURL url("https://host.com/r/cats");

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key(url.spec());
  hint->set_key_representation(proto::FULL_URL);
  hint->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->add_allowlisted_optimizations()->set_optimization_type(
      proto::PERFORMANCE_HINTS);
  page_hint->set_page_pattern("whatever/*");

  hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(url.host());
  page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("anything/*");

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->hint_cache()->UpdateFetchedHints(
      std::move(get_hints_response), base::Time().Now(), {url.host()}, {url},
      run_loop->QuitClosure());
  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));

  run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->RemoveFetchedEntriesByHintKeys(
      run_loop->QuitClosure(), proto::KeyRepresentation::HOST, {url.host()});
  run_loop->Run();

  EXPECT_FALSE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
}

TEST_F(HintsManagerTest, RemoveFetchedEntriesByHintKeys_URL) {
  int cache_duration_in_secs = 60;
  GURL url("https://host.com/r/cats");

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key(url.spec());
  hint->set_key_representation(proto::FULL_URL);
  hint->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->add_allowlisted_optimizations()->set_optimization_type(
      proto::PERFORMANCE_HINTS);
  page_hint->set_page_pattern("whatever/*");

  hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(url.host());
  page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("anything/*");

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->hint_cache()->UpdateFetchedHints(
      std::move(get_hints_response), base::Time().Now(), {url.host()}, {url},
      run_loop->QuitClosure());
  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));

  run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->RemoveFetchedEntriesByHintKeys(
      run_loop->QuitClosure(), proto::KeyRepresentation::FULL_URL,
      {url.spec()});
  run_loop->Run();

  // Both the host and url entries should have been removed to support upgrading
  // hint keys from HOST to FULL_URL.
  EXPECT_FALSE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_FALSE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
}

TEST_F(HintsManagerTest, HintFetcherPrefUpdated_URL) {
  base::Time expiry = base::Time::Now() + base::Hours(1);
  HintsFetcher::AddFetchedHostForTesting(pref_service(), "host-key.com",
                                         expiry);
  HintsFetcher::AddFetchedHostForTesting(pref_service(), "url-key.com", expiry);

  ASSERT_TRUE(
      HintsFetcher::WasHostCoveredByFetch(pref_service(), "host-key.com"));
  ASSERT_TRUE(
      HintsFetcher::WasHostCoveredByFetch(pref_service(), "url-key.com"));

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->RemoveFetchedEntriesByHintKeys(
      run_loop->QuitClosure(), proto::KeyRepresentation::FULL_URL,
      {
          GURL("https://host-key.com/page").spec(),
          GURL("https://url-key.com/page").spec(),
      });
  run_loop->Run();

  EXPECT_FALSE(
      HintsFetcher::WasHostCoveredByFetch(pref_service(), "host-key.com"));
  EXPECT_FALSE(
      HintsFetcher::WasHostCoveredByFetch(pref_service(), "url-key.com"));
}

TEST_F(HintsManagerTest, HintFetcherPrefUpdated_Hosts) {
  base::Time expiry = base::Time::Now() + base::Hours(1);
  HintsFetcher::AddFetchedHostForTesting(pref_service(), "host-key.com",
                                         expiry);
  HintsFetcher::AddFetchedHostForTesting(pref_service(), "url-key.com", expiry);

  ASSERT_TRUE(
      HintsFetcher::WasHostCoveredByFetch(pref_service(), "host-key.com"));
  ASSERT_TRUE(
      HintsFetcher::WasHostCoveredByFetch(pref_service(), "url-key.com"));

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->RemoveFetchedEntriesByHintKeys(
      run_loop->QuitClosure(), proto::KeyRepresentation::HOST,
      {
          "host-key.com",
          "url-key.com",
      });
  run_loop->Run();

  EXPECT_FALSE(
      HintsFetcher::WasHostCoveredByFetch(pref_service(), "host-key.com"));
  EXPECT_FALSE(
      HintsFetcher::WasHostCoveredByFetch(pref_service(), "url-key.com"));
}

class HintsManagerFetchingTest : public HintsManagerTest {
 public:
  HintsManagerFetchingTest() {
    scoped_list_.InitWithFeaturesAndParameters(
        {
            {
                features::kRemoteOptimizationGuideFetching,
                {{"batch_update_hints_for_top_hosts", "true"},
                 {"max_concurrent_page_navigation_fetches", "2"},
                 {"max_concurrent_batch_update_fetches",
                  base::NumberToString(batch_concurrency_limit_)}},
            },
        },
        {features::kRemoteOptimizationGuideFetchingAnonymousDataConsent});
  }

  size_t batch_concurrency_limit() const { return batch_concurrency_limit_; }

 private:
  size_t batch_concurrency_limit_ = 2;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  base::test::ScopedFeatureList scoped_list_;
};

TEST_F(HintsManagerFetchingTest, BatchUpdateFetcherCleanup) {
  EXPECT_GT(batch_concurrency_limit(), 1u);
  for (size_t i = 0; i < batch_concurrency_limit() * 2; ++i) {
    auto request_id_and_fetcher =
        hints_manager_->CreateAndTrackBatchUpdateHintsFetcher();
    // Now run clean up on this id and expect LRU size to be 0.
    hints_manager_->CleanUpBatchUpdateHintsFetcher(
        request_id_and_fetcher.first);
    EXPECT_EQ(0u, hints_manager_->batch_update_hints_fetchers_.size());
  }
  EXPECT_EQ(hints_manager()->num_batch_update_hints_fetches_initiated(),
            int(batch_concurrency_limit() * 2));
}

TEST_F(HintsManagerFetchingTest,
       HintsFetchNotAllowedIfFeatureIsEnabledButUserNotAllowed) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  CreateHintsManager(/*top_host_provider=*/nullptr);
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::Seconds(kUpdateFetchHintsTimeSecs));
  // Hints fetcher should not even be created.
  EXPECT_FALSE(active_tabs_batch_update_hints_fetcher());
}

TEST_F(HintsManagerFetchingTest,
       NoRegisteredOptimizationTypesAndHintsFetchNotAttempted) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  CreateHintsManager(std::make_unique<FakeTopHostProvider>(
      std::vector<std::string>({"example1.com", "example2.com"})));

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch but the fetch is not made.
  MoveClockForwardBy(base::Seconds(kUpdateFetchHintsTimeSecs));
  EXPECT_EQ(0, top_host_provider()->get_num_top_hosts_called());
  // Hints fetcher should not even be created.
  EXPECT_FALSE(active_tabs_batch_update_hints_fetcher());
}

TEST_F(HintsManagerFetchingTest,
       OnlyFilterTypesRegisteredHintsFetchNotAttempted) {
  proto::Configuration config;
  BloomFilter allowlist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&allowlist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, allowlist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  CreateHintsManager(std::make_unique<FakeTopHostProvider>(
      std::vector<std::string>({"example1.com", "example2.com"})));
  ProcessHints(config, "1.0.0.0");

  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));

  // Force timer to expire after random delay and schedule a hints fetch.
  MoveClockForwardBy(base::Seconds(60 * 2));
  EXPECT_EQ(0, top_host_provider()->get_num_top_hosts_called());
  // Hints fetcher should not even be created.
  EXPECT_FALSE(active_tabs_batch_update_hints_fetcher());
}

TEST_F(HintsManagerFetchingTest, HintsFetcherEnabledNoHostsOrUrlsToFetch) {
  auto scoped_feature_list = SetUpDeferStartupActiveTabsHintsFetch(false);
  base::HistogramTester histogram_tester;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  CreateHintsManager(
      std::make_unique<FakeTopHostProvider>(std::vector<std::string>({})));

  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // No hints fetch should happen on startup.
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsManager.ActiveTabUrlsToFetchFor", 0);

  // Force timer to expire after random delay and schedule a hints fetch.
  MoveClockForwardBy(base::Seconds(60 * 2));
  EXPECT_EQ(1, top_host_provider()->get_num_top_hosts_called());
  EXPECT_EQ(1, tab_url_provider()->get_num_urls_called());
  // Hints fetcher should not even be created.
  EXPECT_FALSE(active_tabs_batch_update_hints_fetcher());

  // Move it forward again to make sure timer is scheduled.
  MoveClockForwardBy(base::Seconds(kUpdateFetchHintsTimeSecs));
  EXPECT_EQ(2, top_host_provider()->get_num_top_hosts_called());
  EXPECT_EQ(2, tab_url_provider()->get_num_urls_called());
  // Hints fetcher should not even be created.
  EXPECT_FALSE(active_tabs_batch_update_hints_fetcher());
}

TEST_F(HintsManagerFetchingTest, HintsFetcherEnabledNoHostsButHasUrlsToFetch) {
  auto scoped_feature_list = SetUpDeferStartupActiveTabsHintsFetch(false);
  base::HistogramTester histogram_tester;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  CreateHintsManager(
      std::make_unique<FakeTopHostProvider>(std::vector<std::string>({})));

  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  tab_url_provider()->SetUrls(
      {GURL("https://a.com"), GURL("https://b.com"), GURL("chrome://new-tab")});

  // No hints fetch should happen on startup.
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsManager.ActiveTabUrlsToFetchFor", 0);

  // Force timer to expire after random delay and schedule a hints fetch that
  // succeeds.
  MoveClockForwardBy(base::Seconds(60 * 2));
  EXPECT_EQ(1, top_host_provider()->get_num_top_hosts_called());
  EXPECT_EQ(1, tab_url_provider()->get_num_urls_called());
  EXPECT_EQ(1,
            active_tabs_batch_update_hints_fetcher()->num_fetches_requested());
  EXPECT_EQ("en-US",
            active_tabs_batch_update_hints_fetcher()->locale_requested());
  EXPECT_EQ(
      proto::RequestContext::CONTEXT_BATCH_UPDATE_ACTIVE_TABS,
      active_tabs_batch_update_hints_fetcher()->request_context_requested());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ActiveTabUrlsToFetchFor", 2, 1);

  // Move it forward again to make sure timer is scheduled.
  MoveClockForwardBy(base::Seconds(kUpdateFetchHintsTimeSecs));
  EXPECT_EQ(2, top_host_provider()->get_num_top_hosts_called());
  EXPECT_EQ(2, tab_url_provider()->get_num_urls_called());
  // Urls didn't change and we have all URLs cached in store.
  EXPECT_EQ(1,
            active_tabs_batch_update_hints_fetcher()->num_fetches_requested());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ActiveTabUrlsToFetchFor", 0, 1);
}

// Verifies hints for active tab URLs is not fetched immediately on startup. It
// should be fetched after a random delay for the first time, and then continue
// to be fetched.
TEST_F(HintsManagerFetchingTest, HintsFetcherTimerFetchOnStartup) {
  auto scoped_feature_list = SetUpDeferStartupActiveTabsHintsFetch(false);
  base::HistogramTester histogram_tester;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);

  CreateHintsManager(
      std::make_unique<FakeTopHostProvider>(std::vector<std::string>({})));
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  tab_url_provider()->SetUrls(
      {GURL("https://a.com"), GURL("https://b.com"), GURL("chrome://new-tab")});

  // No hints fetch should happen on startup.
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsManager.ActiveTabUrlsToFetchFor", 0);
  EXPECT_FALSE(active_tabs_batch_update_hints_fetcher());
  EXPECT_EQ(0, tab_url_provider()->get_num_urls_called());

  // Force timer to expire after random delay and schedule a hints fetch that
  // succeeds.
  MoveClockForwardBy(base::Seconds(60 * 2));
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ActiveTabUrlsToFetchFor", 2, 1);
  EXPECT_EQ(1, tab_url_provider()->get_num_urls_called());
  EXPECT_EQ(1,
            active_tabs_batch_update_hints_fetcher()->num_fetches_requested());

  // Move it forward again to make sure timer is scheduled.
  MoveClockForwardBy(base::Seconds(kUpdateFetchHintsTimeSecs));
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ActiveTabUrlsToFetchFor", 0, 1);
  EXPECT_EQ(2, tab_url_provider()->get_num_urls_called());
  EXPECT_EQ(1,
            active_tabs_batch_update_hints_fetcher()->num_fetches_requested());
}

// Verifies the deferred startup mode that fetches hints for active tab URLs on
// deferred startup (but not on immediate startup). It should continue to be
// fetched after a refresh duration.
TEST_F(HintsManagerFetchingTest, HintsFetcherDeferredStartup) {
  auto scoped_feature_list = SetUpDeferStartupActiveTabsHintsFetch(true);
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);

  CreateHintsManager(
      std::make_unique<FakeTopHostProvider>(std::vector<std::string>({})));
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  tab_url_provider()->SetUrls(
      {GURL("https://a.com"), GURL("https://b.com"), GURL("chrome://new-tab")});

  // No hints fetch should happen on startup.
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsManager.ActiveTabUrlsToFetchFor", 0);
  EXPECT_EQ(0, tab_url_provider()->get_num_urls_called());

  // Hints fetch should be triggered on deferred startup.
  hints_manager()->OnDeferredStartup();
  RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ActiveTabUrlsToFetchFor", 2, 1);
  EXPECT_EQ(1, tab_url_provider()->get_num_urls_called());
  EXPECT_EQ(1,
            active_tabs_batch_update_hints_fetcher()->num_fetches_requested());

  // Move it forward again to make sure timer is scheduled.
  MoveClockForwardBy(base::Seconds(kUpdateFetchHintsTimeSecs));
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ActiveTabUrlsToFetchFor", 0, 1);
  EXPECT_EQ(2, tab_url_provider()->get_num_urls_called());
  EXPECT_EQ(1,
            active_tabs_batch_update_hints_fetcher()->num_fetches_requested());
}

TEST_F(HintsManagerFetchingTest,
       HintsFetched_RegisteredOptimizationTypes_AllWithOptFilter) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  BloomFilter allowlist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&allowlist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, allowlist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);
  ProcessHints(config, "1.0.0.0");

  auto navigation_data = CreateTestNavigationData(url_without_hints(),
                                                  {proto::LITE_PAGE_REDIRECT});
  base::HistogramTester histogram_tester;
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus", 0);
}

TEST_F(HintsManagerFetchingTest, HintsFetchedAtNavigationTime) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  auto navigation_data =
      CreateTestNavigationData(url_without_hints(), {proto::DEFER_ALL_SCRIPT});
  base::HistogramTester histogram_tester;
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
      RaceNavigationFetchAttemptStatus::kRaceNavigationFetchHostAndURL, 1);
}

TEST_F(HintsManagerFetchingTest,
       HintsFetchedAtNavigationTime_FetchNotAttempted) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  auto navigation_data =
      CreateTestNavigationData(url_without_hints(), {proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory({HintsFetcherEndState::kFetchFailed}));
  base::HistogramTester histogram_tester;
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
      RaceNavigationFetchAttemptStatus::kRaceNavigationFetchNotAttempted, 1);
}

TEST_F(HintsManagerFetchingTest,
       HintsFetchedAtNavigationTime_HasComponentHintButNotFetched) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  auto navigation_data =
      CreateTestNavigationData(url_with_hints(), {proto::DEFER_ALL_SCRIPT});
  base::HistogramTester histogram_tester;
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
      RaceNavigationFetchAttemptStatus::kRaceNavigationFetchURL, 1);
}

TEST_F(HintsManagerFetchingTest,
       HintsFetchedAtNavigationTime_DoesNotRemoveManualOverride) {
  GURL example_url("http://www.example.com/hasoverride");

  proto::Configuration config;
  proto::Hint* hint = config.add_hints();
  hint->set_key(example_url.spec());
  hint->set_key_representation(proto::FULL_URL);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("*");
  proto::Optimization* opt = page_hint->add_allowlisted_optimizations();
  opt->set_optimization_type(proto::DEFER_ALL_SCRIPT);
  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  encoded_config = base::Base64Encode(encoded_config);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kHintsProtoOverride, encoded_config);

  // Re-create hints manager with override.
  CreateHintsManager(/*top_host_provider=*/nullptr);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});

  auto navigation_data =
      CreateTestNavigationData(example_url, {proto::DEFER_ALL_SCRIPT});
  base::HistogramTester histogram_tester;
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
      RaceNavigationFetchAttemptStatus::kRaceNavigationFetchHost, 1);

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::DEFER_ALL_SCRIPT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_type_decision,
            OptimizationTypeDecision::kAllowedByHint);
}

TEST_F(HintsManagerFetchingTest, URLHintsNotFetchedAtNavigationTime) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  {
    base::HistogramTester histogram_tester;
    auto navigation_data =
        CreateTestNavigationData(url_with_hints(), {proto::DEFER_ALL_SCRIPT});

    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
    RunUntilIdle();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);

    // Make sure navigation data is populated correctly.
    EXPECT_TRUE(navigation_data->hints_fetch_latency().has_value());
    EXPECT_EQ(navigation_data->hints_fetch_attempt_status(),
              RaceNavigationFetchAttemptStatus::kRaceNavigationFetchURL);

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        RaceNavigationFetchAttemptStatus::kRaceNavigationFetchURL, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager."
        "PageNavigationHintsReturnedBeforeDataFlushed",
        true, 1);
    RunUntilIdle();
  }

  {
    base::HistogramTester histogram_tester;
    auto navigation_data =
        CreateTestNavigationData(url_with_hints(), {proto::DEFER_ALL_SCRIPT});
    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
    RunUntilIdle();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        RaceNavigationFetchAttemptStatus::kRaceNavigationFetchNotAttempted, 1);

    // Make sure navigation data is populated correctly.
    EXPECT_FALSE(navigation_data->hints_fetch_latency().has_value());
    EXPECT_EQ(
        navigation_data->hints_fetch_attempt_status(),
        RaceNavigationFetchAttemptStatus::kRaceNavigationFetchNotAttempted);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.HintsManager."
        "PageNavigationHintsReturnedBeforeDataFlushed",
        0);
  }
}

TEST_F(HintsManagerFetchingTest, URLWithNoHintsNotRefetchedAtNavigationTime) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));

  base::HistogramTester histogram_tester;
  {
    auto navigation_data = CreateTestNavigationData(url_without_hints(),
                                                    {proto::DEFER_ALL_SCRIPT});

    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
    RunUntilIdle();
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);

    // Make sure navigation data is populated correctly.
    EXPECT_TRUE(navigation_data->hints_fetch_latency().has_value());
    EXPECT_EQ(navigation_data->hints_fetch_attempt_status(),
              RaceNavigationFetchAttemptStatus::kRaceNavigationFetchHostAndURL);

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        RaceNavigationFetchAttemptStatus::kRaceNavigationFetchHostAndURL, 1);
    RunUntilIdle();
  }

  {
    auto navigation_data = CreateTestNavigationData(url_without_hints(),
                                                    {proto::DEFER_ALL_SCRIPT});
    base::RunLoop run_loop;
    navigation_data = CreateTestNavigationData(url_without_hints(),
                                               {proto::DEFER_ALL_SCRIPT});
    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
    RunUntilIdle();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        RaceNavigationFetchAttemptStatus::kRaceNavigationFetchHost, 1);
    EXPECT_TRUE(navigation_data->hints_fetch_latency().has_value());
    EXPECT_EQ(navigation_data->hints_fetch_attempt_status(),
              RaceNavigationFetchAttemptStatus::kRaceNavigationFetchHost);
  }
}

TEST_F(HintsManagerFetchingTest, CanApplyOptimizationCalledMidFetch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  auto navigation_data =
      CreateTestNavigationData(url_without_hints(), {proto::DEFER_ALL_SCRIPT});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::DEFER_ALL_SCRIPT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_type_decision,
            OptimizationTypeDecision::kHintFetchStartedButNotAvailableInTime);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationCalledPostFetchButNoHintsCameBack) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));

  auto navigation_data =
      CreateTestNavigationData(url_without_hints(), {proto::DEFER_ALL_SCRIPT});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::DEFER_ALL_SCRIPT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_type_decision,
            OptimizationTypeDecision::kNoHintAvailable);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationCalledPostFetchButFetchFailed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory({HintsFetcherEndState::kFetchFailed}));

  auto navigation_data =
      CreateTestNavigationData(url_without_hints(), {proto::DEFER_ALL_SCRIPT});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::DEFER_ALL_SCRIPT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_type_decision,
            OptimizationTypeDecision::kNoHintAvailable);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationWithURLKeyedHintApplicableForOptimizationType) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  auto navigation_data = CreateTestNavigationData(url_with_url_keyed_hint(),
                                                  {proto::DEFER_ALL_SCRIPT});
  // Make sure URL-keyed hint is fetched and processed.
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::COMPRESS_PUBLIC_IMAGES,
                                            &optimization_metadata);

  // Make sure decisions are logged correctly and metadata is populated off
  // a URL-keyed hint.
  EXPECT_EQ(OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
  EXPECT_TRUE(optimization_metadata.any_metadata().has_value());
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationNotAllowedByURLButAllowedByHostKeyedHint) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::NOSCRIPT});

  InitializeWithDefaultConfig("1.0.0.0");

  // Make sure both URL-Keyed and host-keyed hints are processed and cached.
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data =
      CreateTestNavigationData(url_with_url_keyed_hint(), {proto::NOSCRIPT});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::NOSCRIPT,
                                            &optimization_metadata);

  EXPECT_EQ(OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationNotAllowedByURLOrHostKeyedHint) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::RESOURCE_LOADING});

  InitializeWithDefaultConfig("1.0.0.0");

  // Make sure both URL-Keyed and host-keyed hints are processed and cached.
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data = CreateTestNavigationData(url_with_url_keyed_hint(),
                                                  {proto::RESOURCE_LOADING});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::RESOURCE_LOADING,
                                            &optimization_metadata);

  EXPECT_EQ(OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationNoURLKeyedHintOrHostKeyedHint) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));
  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {proto::COMPRESS_PUBLIC_IMAGES});

  // Attempt to fetch a hint but ensure nothing comes back.
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::COMPRESS_PUBLIC_IMAGES,
                                            &optimization_metadata);

  EXPECT_EQ(OptimizationTypeDecision::kNoHintAvailable,
            optimization_type_decision);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationCalledMidFetchForURLKeyedOptimization) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Attempt to fetch a hint but call CanApplyOptimization right away to
  // simulate being mid-fetch.
  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {proto::COMPRESS_PUBLIC_IMAGES});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::COMPRESS_PUBLIC_IMAGES,
                                            &optimization_metadata);

  EXPECT_EQ(OptimizationTypeDecision::kHintFetchStartedButNotAvailableInTime,
            optimization_type_decision);
}

TEST_F(HintsManagerFetchingTest,
       OnNavigationStartOrRedirectWontInitiateFetchIfAlreadyStartedForTheURL) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::RESOURCE_LOADING});

  InitializeWithDefaultConfig("1.0.0.0");

  // Attempt to fetch a hint but initiate the next navigation right away to
  // simulate being mid-fetch.
  auto navigation_data =
      CreateTestNavigationData(url_without_hints(), {proto::RESOURCE_LOADING});
  {
    base::HistogramTester histogram_tester;
    hints_manager()->SetHintsFetcherFactoryForTesting(
        BuildTestHintsFetcherFactory(
            {HintsFetcherEndState::kFetchSuccessWithHostHints}));
    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        RaceNavigationFetchAttemptStatus::kRaceNavigationFetchHostAndURL, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 1, 1);
  }

  {
    base::HistogramTester histogram_tester;
    hints_manager()->SetHintsFetcherFactoryForTesting(
        BuildTestHintsFetcherFactory(
            {HintsFetcherEndState::kFetchSuccessWithHostHints}));
    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        RaceNavigationFetchAttemptStatus::kRaceNavigationFetchAlreadyInProgress,
        1);
    // Should not be recorded since we are not attempting a new fetch.
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 0);

    // Set hints fetch end.so we can figure out if hints fetch start was set.
    navigation_data->set_hints_fetch_end(base::TimeTicks::Now());
    EXPECT_TRUE(navigation_data->hints_fetch_latency().has_value());
    EXPECT_EQ(navigation_data->hints_fetch_attempt_status(),
              RaceNavigationFetchAttemptStatus::
                  kRaceNavigationFetchAlreadyInProgress);
  }
}

TEST_F(HintsManagerFetchingTest,
       PageNavigationHintsFetcherGetsCleanedUpOnceHintsAreStored) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::RESOURCE_LOADING});

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));

  // Attempt to fetch a hint but initiate the next navigation right away to
  // simulate being mid-fetch.
  auto navigation_data =
      CreateTestNavigationData(url_without_hints(), {proto::RESOURCE_LOADING});
  {
    base::HistogramTester histogram_tester;
    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        RaceNavigationFetchAttemptStatus::kRaceNavigationFetchHostAndURL, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 1, 1);

    // Make sure hints are stored (i.e. fetcher is cleaned up).
    RunUntilIdle();
  }

  {
    base::HistogramTester histogram_tester;
    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        RaceNavigationFetchAttemptStatus::kRaceNavigationFetchHost, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 1, 1);
  }
}

TEST_F(HintsManagerFetchingTest,
       PageNavigationHintsFetcherCanFetchMultipleThingsConcurrently) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  auto navigation_data_with_hints = CreateTestNavigationData(
      url_with_url_keyed_hint(), {proto::COMPRESS_PUBLIC_IMAGES});
  auto navigation_data_without_hints = CreateTestNavigationData(
      GURL("https://doesntmatter.com/"), {proto::COMPRESS_PUBLIC_IMAGES});
  auto navigation_data_without_hints2 = CreateTestNavigationData(
      url_without_hints(), {proto::COMPRESS_PUBLIC_IMAGES});

  // Attempt to fetch a hint but initiate the next navigations right away to
  // simulate being mid-fetch.
  base::HistogramTester histogram_tester;
  CallOnNavigationStartOrRedirect(navigation_data_with_hints.get(),
                                  base::DoNothing());
  CallOnNavigationStartOrRedirect(navigation_data_without_hints.get(),
                                  base::DoNothing());
  CallOnNavigationStartOrRedirect(navigation_data_without_hints2.get(),
                                  base::DoNothing());

  // The third one is over the max and should evict another one.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 3);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 1, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 2, 2);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationNewAPIDecisionComesFromInFlightURLHint) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(), {proto::COMPRESS_PUBLIC_IMAGES});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  hints_manager()->CanApplyOptimization(
      url_with_url_keyed_hint(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kTrue, decision);
      }));

  // Wait for the hint to be available and for the callback to execute.
  RunUntilIdle();

  // The new API should have called the async API in the background.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kAllowedByHint, 1);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationNewAPIRequestFailsBeforeFetch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  // The first query should fail since no "navigation" has occurred.
  hints_manager()->CanApplyOptimization(
      url_with_url_keyed_hint(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kUnknown, decision);
      }));

  // Make the hints available after the URL has been queried.
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(), {proto::COMPRESS_PUBLIC_IMAGES});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  // Wait for the hint to become available.
  RunUntilIdle();

  // Now make sure the hint is available with the same API.
  hints_manager()->CanApplyOptimization(
      url_with_url_keyed_hint(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kTrue, decision);
      }));
}

TEST_F(HintsManagerFetchingTest, CanApplyOptimizationNewAPICalledPostFetch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(), {proto::COMPRESS_PUBLIC_IMAGES});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());

  // Wait for the hint to become available.
  RunUntilIdle();

  hints_manager()->CanApplyOptimization(
      url_with_url_keyed_hint(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kTrue, decision);
      }));
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncDecisionComesFromInFlightURLHint) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(), {proto::COMPRESS_PUBLIC_IMAGES});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kTrue, decision);
        EXPECT_TRUE(metadata.any_metadata().has_value());
      }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kAllowedByHint, 1);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncMultipleCallbacksRegisteredForSameTypeAndURL) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(), {proto::COMPRESS_PUBLIC_IMAGES});
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kTrue, decision);
        EXPECT_TRUE(metadata.any_metadata().has_value());
      }));
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kTrue, decision);
        EXPECT_TRUE(metadata.any_metadata().has_value());
      }));
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kAllowedByHint, 2);
}

TEST_F(
    HintsManagerFetchingTest,
    CanApplyOptimizationAsyncDecisionComesFromInFlightURLHintNotAllowlisted) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::RESOURCE_LOADING});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data = CreateTestNavigationData(url_with_url_keyed_hint(),
                                                  {proto::RESOURCE_LOADING});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), proto::RESOURCE_LOADING,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kFalse, decision);
      }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.ResourceLoading",
      OptimizationTypeDecision::kNotAllowedByHint, 1);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncFetchFailsDoesNotStrandCallbacks) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory({HintsFetcherEndState::kFetchFailed}));
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(), {proto::COMPRESS_PUBLIC_IMAGES});
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kFalse, decision);
      }));
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kNotAllowedByHint, 1);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncInfoAlreadyInPriorToCall) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(), {proto::COMPRESS_PUBLIC_IMAGES});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kTrue, decision);
        EXPECT_TRUE(metadata.any_metadata().has_value());
      }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kAllowedByHint, 1);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncInfoAlreadyInPriorToCallAndNotAllowlisted) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::PERFORMANCE_HINTS});

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data = CreateTestNavigationData(url_with_url_keyed_hint(),
                                                  {proto::PERFORMANCE_HINTS});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), proto::PERFORMANCE_HINTS,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kFalse, decision);
      }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.PerformanceHints",
      OptimizationTypeDecision::kNotAllowedByHint, 1);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncHintComesInAndNotAllowlisted) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::PERFORMANCE_HINTS});

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));
  auto navigation_data =
      CreateTestNavigationData(url_without_hints(), {proto::PERFORMANCE_HINTS});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_without_hints(), proto::PERFORMANCE_HINTS,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kFalse, decision);
      }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.PerformanceHints",
      OptimizationTypeDecision::kNoHintAvailable, 1);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncDoesNotStrandCallbacksAtBeginningOfChain) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  GURL url_that_redirected("https://urlthatredirected.com");
  auto navigation_data_redirect = CreateTestNavigationData(
      url_that_redirected, {proto::COMPRESS_PUBLIC_IMAGES});
  hints_manager()->CanApplyOptimizationAsync(
      url_that_redirected, proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kFalse, decision);
      }));
  hints_manager()->OnNavigationFinish(
      {url_that_redirected, GURL("https://otherurl.com/")});
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kNoHintAvailable, 1);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncDoesNotStrandCallbacksIfFetchNotPending) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(), {proto::COMPRESS_PUBLIC_IMAGES});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kFalse, decision);
      }));
  hints_manager()->OnNavigationFinish({url_with_url_keyed_hint()});
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kNotAllowedByHint, 1);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncWithDecisionFromAllowlistReturnsRightAway) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  BloomFilter allowlist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&allowlist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, allowlist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);
  ProcessHints(config, "1.0.0.0");

  auto navigation_data = CreateTestNavigationData(
      GURL("https://notallowed.com/123"), {proto::LITE_PAGE_REDIRECT});
  hints_manager()->CanApplyOptimizationAsync(
      navigation_data->navigation_url(), proto::LITE_PAGE_REDIRECT,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kFalse, decision);
      }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.LitePageRedirect",
      OptimizationTypeDecision::kNotAllowedByOptimizationFilter, 1);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncWithDecisionFromBlocklistReturnsRightAway) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  auto navigation_data = CreateTestNavigationData(
      GURL("https://m.host.com/123"), {proto::LITE_PAGE_REDIRECT});
  hints_manager()->CanApplyOptimizationAsync(
      navigation_data->navigation_url(), proto::LITE_PAGE_REDIRECT,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kFalse, decision);
      }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.LitePageRedirect",
      OptimizationTypeDecision::kNotAllowedByOptimizationFilter, 1);
}

TEST_F(HintsManagerFetchingTest,
       OnNavigationFinishDoesNotPrematurelyInvokeRegisteredCallbacks) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data = CreateTestNavigationData(url_with_url_keyed_hint(),
                                                  {proto::LITE_PAGE_REDIRECT});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kTrue, decision);
        EXPECT_TRUE(metadata.any_metadata().has_value());
      }));
  hints_manager()->OnNavigationFinish({url_with_url_keyed_hint()});
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kAllowedByHint, 1);
}

TEST_F(HintsManagerFetchingTest,
       OnNavigationFinishDoesNotCrashWithoutAnyCallbacksRegistered) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->OnNavigationFinish({url_with_url_keyed_hint()});

  RunUntilIdle();
}

TEST_F(HintsManagerFetchingTest, NewOptTypeRegisteredClearsHintCache) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});

  InitializeWithDefaultConfig("1.0.0.0");

  GURL url("https://host.com/fetched_hint_host");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  auto navigation_data =
      CreateTestNavigationData(url, {proto::DEFER_ALL_SCRIPT});

  // Attempt to fetch a hint but ensure nothing comes back.
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::DEFER_ALL_SCRIPT,
                                            &optimization_metadata);

  EXPECT_EQ(OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);

  // Register a new type that is unlaunched - this should clear the Fetched
  // hints.
  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});

  RunUntilIdle();

  base::RunLoop run_loop;

  base::HistogramTester histogram_tester;

  navigation_data = CreateTestNavigationData(url, {proto::DEFER_ALL_SCRIPT});
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());

  run_loop.Run();

  optimization_type_decision = hints_manager()->CanApplyOptimization(
      navigation_data->navigation_url(), proto::DEFER_ALL_SCRIPT,
      &optimization_metadata);

  // The previously fetched hints for the host should not be available after
  // registering a new optimization type.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
      RaceNavigationFetchAttemptStatus::kRaceNavigationFetchHost, 1);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationOnDemandDecisionMetadataComesFromFetch) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_url_keyed_hint()}, {proto::COMPRESS_PUBLIC_IMAGES},
      proto::RequestContext::CONTEXT_BOOKMARKS,
      base::BindRepeating(
          [](base::RunLoop* run_loop, const GURL& url,
             const base::flat_map<proto::OptimizationType,
                                  OptimizationGuideDecisionWithMetadata>&
                 decisions) {
            ASSERT_EQ(decisions.size(), 1u);
            auto it = decisions.find(proto::COMPRESS_PUBLIC_IMAGES);
            ASSERT_TRUE(it != decisions.end());
            EXPECT_EQ(OptimizationGuideDecision::kTrue, it->second.decision);
            EXPECT_TRUE(
                it->second.metadata.any_metadata().has_value());

            run_loop->Quit();
          },
          run_loop.get()),
      std::nullopt);
  run_loop->Run();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsManager.ConcurrentBatchUpdateFetches", 1, 1);
}

TEST_F(HintsManagerFetchingTest, BatchUpdateCalledMoreThanMaxConcurrent) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  // Call this over the max count.
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_url_keyed_hint()}, {proto::COMPRESS_PUBLIC_IMAGES},
      proto::RequestContext::CONTEXT_BOOKMARKS,
      base::DoNothingAs<void(
          const GURL&,
          const base::flat_map<proto::OptimizationType,
                               OptimizationGuideDecisionWithMetadata>&)>(),
      std::nullopt);
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_url_keyed_hint()}, {proto::COMPRESS_PUBLIC_IMAGES},
      proto::RequestContext::CONTEXT_BOOKMARKS,
      base::DoNothingAs<void(
          const GURL&,
          const base::flat_map<proto::OptimizationType,
                               OptimizationGuideDecisionWithMetadata>&)>(),
      std::nullopt);
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_url_keyed_hint()}, {proto::COMPRESS_PUBLIC_IMAGES},
      proto::RequestContext::CONTEXT_BOOKMARKS,
      base::DoNothingAs<void(
          const GURL&,
          const base::flat_map<proto::OptimizationType,
                               OptimizationGuideDecisionWithMetadata>&)>(),
      std::nullopt);

  // The third one is over the max and should evict another one.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsManager.ConcurrentBatchUpdateFetches", 3);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ConcurrentBatchUpdateFetches", 1, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ConcurrentBatchUpdateFetches", 2, 2);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationOnDemandNoRegistrationAlwaysFetches) {
  base::HistogramTester histogram_tester;

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_url_keyed_hint()},
      {proto::NOSCRIPT, proto::COMPRESS_PUBLIC_IMAGES},
      proto::RequestContext::CONTEXT_BOOKMARKS,
      base::BindRepeating(
          [](base::RunLoop* run_loop, const GURL& url,
             const base::flat_map<proto::OptimizationType,
                                  OptimizationGuideDecisionWithMetadata>&
                 decisions) {
            ASSERT_EQ(decisions.size(), 2u);
            auto it = decisions.find(proto::COMPRESS_PUBLIC_IMAGES);
            ASSERT_TRUE(it != decisions.end());
            EXPECT_EQ(OptimizationGuideDecision::kTrue, it->second.decision);
            EXPECT_TRUE(it->second.metadata.any_metadata().has_value());

            it = decisions.find(proto::NOSCRIPT);
            ASSERT_TRUE(it != decisions.end());
            EXPECT_EQ(OptimizationGuideDecision::kTrue, it->second.decision);

            run_loop->Quit();
          },
          run_loop.get()),
      std::nullopt);
  run_loop->Run();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsManager.ConcurrentBatchUpdateFetches", 1);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationOnDemandNoRegistrationFetchFailure) {
  base::HistogramTester histogram_tester;

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory({HintsFetcherEndState::kFetchFailed}));
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_url_keyed_hint()}, {proto::COMPRESS_PUBLIC_IMAGES},
      proto::RequestContext::CONTEXT_BOOKMARKS,
      base::BindRepeating(
          [](base::RunLoop* run_loop, const GURL& url,
             const base::flat_map<proto::OptimizationType,
                                  OptimizationGuideDecisionWithMetadata>&
                 decisions) {
            ASSERT_EQ(decisions.size(), 1u);
            auto it = decisions.find(proto::COMPRESS_PUBLIC_IMAGES);
            ASSERT_TRUE(it != decisions.end());
            EXPECT_EQ(OptimizationGuideDecision::kFalse, it->second.decision);

            run_loop->Quit();
          },
          run_loop.get()),
      std::nullopt);
  run_loop->Run();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsManager.ConcurrentBatchUpdateFetches", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kNoHintAvailable, 1);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationOnDemandMixedRegistrations) {
  base::HistogramTester histogram_tester;

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->RegisterOptimizationTypes({proto::NOSCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  // Make sure NOSCRIPT is cached and loaded.
  auto navigation_data =
      CreateTestNavigationData(url_with_url_keyed_hint(), {proto::NOSCRIPT});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_hints()}, {proto::NOSCRIPT, proto::COMPRESS_PUBLIC_IMAGES},
      proto::RequestContext::CONTEXT_BOOKMARKS,
      base::BindRepeating(
          [](base::RunLoop* run_loop, const GURL& url,
             const base::flat_map<proto::OptimizationType,
                                  OptimizationGuideDecisionWithMetadata>&
                 decisions) {
            ASSERT_EQ(decisions.size(), 2u);
            auto it = decisions.find(proto::NOSCRIPT);
            ASSERT_TRUE(it != decisions.end());
            // The server has a response for this.
            EXPECT_EQ(OptimizationGuideDecision::kTrue, it->second.decision);

            it = decisions.find(proto::COMPRESS_PUBLIC_IMAGES);
            ASSERT_TRUE(it != decisions.end());
            EXPECT_EQ(OptimizationGuideDecision::kTrue, it->second.decision);
            EXPECT_TRUE(it->second.metadata.any_metadata().has_value());

            run_loop->Quit();
          },
          run_loop.get()),
      std::nullopt);
  run_loop->Run();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsManager.ConcurrentBatchUpdateFetches", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.NoScript",
      OptimizationTypeDecision::kAllowedByHint, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kAllowedByHint, 1);
}

TEST_F(
    HintsManagerFetchingTest,
    CanApplyOptimizationOnDemandDecisionMultipleTypesBothHostAndURLKeyedMixedFetch) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes(
      {proto::NOSCRIPT, proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_url_keyed_hint()},
      {proto::NOSCRIPT, proto::COMPRESS_PUBLIC_IMAGES},
      proto::RequestContext::CONTEXT_BOOKMARKS,
      base::BindRepeating(
          [](base::RunLoop* run_loop, const GURL& url,
             const base::flat_map<proto::OptimizationType,
                                  OptimizationGuideDecisionWithMetadata>&
                 decisions) {
            ASSERT_EQ(decisions.size(), 2u);
            auto it = decisions.find(proto::COMPRESS_PUBLIC_IMAGES);
            ASSERT_TRUE(it != decisions.end());
            EXPECT_EQ(OptimizationGuideDecision::kTrue, it->second.decision);
            EXPECT_TRUE(
                it->second.metadata.any_metadata().has_value());

            it = decisions.find(proto::NOSCRIPT);
            ASSERT_TRUE(it != decisions.end());
            EXPECT_EQ(OptimizationGuideDecision::kTrue, it->second.decision);

            run_loop->Quit();
          },
          run_loop.get()),
      std::nullopt);
  run_loop->Run();
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationOnDemandDecisionFailedFetchDoesNotStrandCallback) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes(
      {proto::NOSCRIPT, proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory({HintsFetcherEndState::kFetchFailed}));
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_url_keyed_hint()},
      {proto::NOSCRIPT, proto::COMPRESS_PUBLIC_IMAGES},
      proto::RequestContext::CONTEXT_BOOKMARKS,
      base::BindRepeating(
          [](base::RunLoop* run_loop, const GURL& url,
             const base::flat_map<proto::OptimizationType,
                                  OptimizationGuideDecisionWithMetadata>&
                 decisions) {
            ASSERT_EQ(decisions.size(), 2u);
            auto it = decisions.find(proto::COMPRESS_PUBLIC_IMAGES);
            ASSERT_TRUE(it != decisions.end());
            EXPECT_EQ(OptimizationGuideDecision::kFalse, it->second.decision);

            it = decisions.find(proto::NOSCRIPT);
            ASSERT_TRUE(it != decisions.end());
            EXPECT_EQ(OptimizationGuideDecision::kFalse, it->second.decision);

            run_loop->Quit();
          },
          run_loop.get()),
      std::nullopt);
  run_loop->Run();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.NoScript",
      OptimizationTypeDecision::kNoHintAvailable, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kNoHintAvailable, 1);
}

// RequestContextMetadata will be sent in fetcher only for appropriate request
// context.
TEST_F(HintsManagerFetchingTest,
       PageInsightsHubContextRequestContextMetadataPihSentGetHintsRequest) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes({proto::TYPE_UNSPECIFIED});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  proto::PageInsightsHubRequestContextMetadata
      page_insights_hub_request_context_metadata =
          proto::PageInsightsHubRequestContextMetadata::default_instance();
  proto::RequestContextMetadata request_context_metadata_var;
  *request_context_metadata_var.mutable_page_insights_hub_metadata() =
      page_insights_hub_request_context_metadata;
  std::optional<proto::RequestContextMetadata> request_context_metadata =
      std::make_optional(request_context_metadata_var);
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_url_keyed_hint()}, {proto::TYPE_UNSPECIFIED},
      proto::RequestContext::CONTEXT_PAGE_INSIGHTS_HUB,
      base::BindRepeating(
          [](base::RunLoop* run_loop, const GURL& url,
             const base::flat_map<proto::OptimizationType,
                                  OptimizationGuideDecisionWithMetadata>&
                 decisions) {
            EXPECT_EQ(decisions.size(), 1u);
            auto it = decisions.find(proto::TYPE_UNSPECIFIED);
            EXPECT_TRUE(it != decisions.end());

            run_loop->Quit();
          },
          run_loop.get()),
      request_context_metadata);
  HintsFetcher* it =
      hints_manager_->batch_update_hints_fetchers_.Peek(0)->second.get();
  TestHintsFetcher* it2 = static_cast<TestHintsFetcher*>(it);
  EXPECT_TRUE(it2->is_request_context_metadata_filled);
  run_loop->Run();
}

// RequestContextMetadata will not be sent in fetcher when the request context
// is not enabled for it.
TEST_F(
    HintsManagerFetchingTest,
    PageInsightsHubContextNotSentRequestContextMetadataPihSentGetHintsRequest) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes({proto::TYPE_UNSPECIFIED});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  proto::PageInsightsHubRequestContextMetadata
      page_insights_hub_request_context_metadata =
          proto::PageInsightsHubRequestContextMetadata::default_instance();
  proto::RequestContextMetadata request_context_metadata_var;
  *request_context_metadata_var.mutable_page_insights_hub_metadata() =
      page_insights_hub_request_context_metadata;
  std::optional<proto::RequestContextMetadata> request_context_metadata =
      std::make_optional(request_context_metadata_var);
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_url_keyed_hint()}, {proto::TYPE_UNSPECIFIED},
      proto::RequestContext::CONTEXT_BOOKMARKS,
      base::BindRepeating(
          [](base::RunLoop* run_loop, const GURL& url,
             const base::flat_map<proto::OptimizationType,
                                  OptimizationGuideDecisionWithMetadata>&
                 decisions) {
            EXPECT_EQ(decisions.size(), 1u);
            auto it = decisions.find(proto::TYPE_UNSPECIFIED);
            EXPECT_TRUE(it != decisions.end());

            run_loop->Quit();
          },
          run_loop.get()),
      request_context_metadata);
  HintsFetcher* it =
      hints_manager_->batch_update_hints_fetchers_.Peek(0)->second.get();
  TestHintsFetcher* it2 = static_cast<TestHintsFetcher*>(it);
  EXPECT_FALSE(it2->is_request_context_metadata_filled);
  run_loop->Run();
}

// Tests the null RequestContextMetadata case.
TEST_F(HintsManagerFetchingTest,
       PageInsightsHubContextRequestContextMetadataPihNotSentGetHintsRequest) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes({proto::TYPE_UNSPECIFIED});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_url_keyed_hint()}, {proto::TYPE_UNSPECIFIED},
      proto::RequestContext::CONTEXT_PAGE_INSIGHTS_HUB,
      base::BindRepeating(
          [](base::RunLoop* run_loop, const GURL& url,
             const base::flat_map<proto::OptimizationType,
                                  OptimizationGuideDecisionWithMetadata>&
                 decisions) {
            EXPECT_EQ(decisions.size(), 1u);
            auto it = decisions.find(proto::TYPE_UNSPECIFIED);
            EXPECT_TRUE(it != decisions.end());

            run_loop->Quit();
          },
          run_loop.get()),
      std::nullopt);
  HintsFetcher* it =
      hints_manager_->batch_update_hints_fetchers_.Peek(0)->second.get();
  TestHintsFetcher* it2 = static_cast<TestHintsFetcher*>(it);
  EXPECT_FALSE(it2->is_request_context_metadata_filled);
  run_loop->Run();
}

class HintsManagerFetchingNoBatchUpdateTest : public HintsManagerTest {
 public:
  HintsManagerFetchingNoBatchUpdateTest() {
    scoped_list_.InitAndEnableFeatureWithParameters(
        features::kRemoteOptimizationGuideFetching,
        {{"batch_update_hints_for_top_hosts", "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_list_;
};

TEST_F(HintsManagerFetchingNoBatchUpdateTest,
       BatchUpdateHintsFetchNotScheduledIfNotAllowed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCheckingUserPermissionsForTesting);
  // Force hints fetch scheduling.
  CreateHintsManager(std::make_unique<FakeTopHostProvider>(
      std::vector<std::string>({"example1.com", "example2.com"})));
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::Seconds(kUpdateFetchHintsTimeSecs));
  // Hints fetcher should not even be created.
  EXPECT_FALSE(active_tabs_batch_update_hints_fetcher());
}

class HintsManagerComponentSkipProcessingTest : public HintsManagerTest {
 public:
  HintsManagerComponentSkipProcessingTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kOptimizationHintsComponent,
        {{"check_failed_component_version_pref", "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HintsManagerComponentSkipProcessingTest, ProcessHintsWithExistingPref) {
  // Write hints processing pref for version 2.0.0.
  pref_service()->SetString(prefs::kPendingHintsProcessingVersion, "2.0.0");
  CreateHintsManager(/*top_host_provider=*/nullptr);

  // Verify config still processed even though pref is existing.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0");
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
    // If it processed correctly, it should clear the pref.
    EXPECT_TRUE(pref_service()
                    ->GetString(prefs::kPendingHintsProcessingVersion)
                    .empty());
  }

  // Now verify config is processed for different version and pref cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("3.0.0");
    EXPECT_TRUE(pref_service()
                    ->GetString(prefs::kPendingHintsProcessingVersion)
                    .empty());
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }
}

class HintsManagerPersonalizedFetchingTest : public HintsManagerFetchingTest {
 public:
  HintsManagerPersonalizedFetchingTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kOptimizationGuidePersonalizedFetching,
        {{"allowed_contexts", "CONTEXT_BOOKMARKS"}});
  }

  HintsManagerPersonalizedFetchingTest(
      const HintsManagerPersonalizedFetchingTest&) = delete;
  HintsManagerPersonalizedFetchingTest& operator=(
      const HintsManagerPersonalizedFetchingTest&) = delete;

  void SetUp() override {
    HintsManagerFetchingTest::SetUp();
    CreateHintsManager(/*top_host_provider=*/nullptr,
                       identity_test_env()->identity_manager());
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 protected:
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/41482478): test is failing on iPhone device.
#if TARGET_OS_IOS && !TARGET_IPHONE_SIMULATOR
#define MAYBE_SuccessfulPersonalizedHintsFetching \
  DISABLED_SuccessfulPersonalizedHintsFetching
#else
#define MAYBE_SuccessfulPersonalizedHintsFetching \
  SuccessfulPersonalizedHintsFetching
#endif
TEST_F(HintsManagerPersonalizedFetchingTest,
       MAYBE_SuccessfulPersonalizedHintsFetching) {
  ASSERT_TRUE(identity_test_env()->identity_manager());
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);

  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_url_keyed_hint()}, {proto::COMPRESS_PUBLIC_IMAGES},
      proto::RequestContext::CONTEXT_BOOKMARKS,
      base::BindRepeating(
          [](base::RunLoop* run_loop, const GURL& url,
             const base::flat_map<proto::OptimizationType,
                                  OptimizationGuideDecisionWithMetadata>&
                 decisions) {
            ASSERT_EQ(decisions.size(), 1u);
            auto it = decisions.find(proto::COMPRESS_PUBLIC_IMAGES);
            ASSERT_TRUE(it != decisions.end());
            EXPECT_EQ(OptimizationGuideDecision::kTrue, it->second.decision);
            EXPECT_TRUE(it->second.metadata.any_metadata().has_value());

            run_loop->Quit();
          },
          run_loop.get()),
      std::nullopt);
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  run_loop->Run();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsManager.ConcurrentBatchUpdateFetches", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.AccessTokenHelper.Result",
      OptimizationGuideAccessTokenResult::kSuccess, 1);
}

TEST_F(HintsManagerPersonalizedFetchingTest, TokenFailure) {
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_url_keyed_hint()}, {proto::COMPRESS_PUBLIC_IMAGES},
      proto::RequestContext::CONTEXT_BOOKMARKS,
      base::BindRepeating(
          [](base::RunLoop* run_loop, const GURL& url,
             const base::flat_map<proto::OptimizationType,
                                  OptimizationGuideDecisionWithMetadata>&
                 decisions) {
            ASSERT_EQ(decisions.size(), 1u);
            auto it = decisions.find(proto::COMPRESS_PUBLIC_IMAGES);
            ASSERT_TRUE(it != decisions.end());
            // Should still request metadata, just without triggering access
            // token requests.
            EXPECT_EQ(OptimizationGuideDecision::kTrue, it->second.decision);

            run_loop->Quit();
          },
          run_loop.get()),
      std::nullopt);
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  run_loop->Run();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsManager.ConcurrentBatchUpdateFetches", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.AccessTokenHelper.Result",
      OptimizationGuideAccessTokenResult::kTransientError, 1);
}

TEST_F(HintsManagerPersonalizedFetchingTest, NoUserSignIn) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->CanApplyOptimizationOnDemand(
      {url_with_url_keyed_hint()}, {proto::COMPRESS_PUBLIC_IMAGES},
      proto::RequestContext::CONTEXT_BOOKMARKS,
      base::BindRepeating(
          [](base::RunLoop* run_loop, const GURL& url,
             const base::flat_map<proto::OptimizationType,
                                  OptimizationGuideDecisionWithMetadata>&
                 decisions) {
            ASSERT_EQ(decisions.size(), 1u);
            auto it = decisions.find(proto::COMPRESS_PUBLIC_IMAGES);
            ASSERT_TRUE(it != decisions.end());
            // Should still request metadata, just without triggering access
            // token requests.
            EXPECT_EQ(OptimizationGuideDecision::kTrue, it->second.decision);

            run_loop->Quit();
          },
          run_loop.get()),
      std::nullopt);
  run_loop->Run();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsManager.ConcurrentBatchUpdateFetches", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.AccessTokenHelper.Result",
      OptimizationGuideAccessTokenResult::kUserNotSignedIn, 1);
}

}  // namespace optimization_guide
