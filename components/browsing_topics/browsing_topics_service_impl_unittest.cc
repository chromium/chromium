// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_service_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/browsing_topics/test_util.h"
#include "components/browsing_topics/util.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/content/browser/test_page_content_annotator.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browsing_topics_site_data_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browsing_topics_test_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_view_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"

namespace browsing_topics {

namespace {

constexpr base::TimeDelta kCalculatorDelay = base::Seconds(10);

constexpr browsing_topics::HmacKey kTestKey = {1};

constexpr base::Time kTime1 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
constexpr base::Time kTime2 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(2));

constexpr size_t kTaxonomySize = 349;
constexpr int kTaxonomyVersion = 1;
constexpr int64_t kModelVersion = 5000000000LL;

EpochTopics CreateTestEpochTopics(
    const std::vector<std::pair<Topic, std::set<HashedDomain>>>& topics,
    base::Time calculation_time,
    size_t padded_top_topics_start_index = 5) {
  DCHECK_EQ(topics.size(), 5u);

  std::vector<TopicAndDomains> top_topics_and_observing_domains;
  for (size_t i = 0; i < 5; ++i) {
    top_topics_and_observing_domains.emplace_back(topics[i].first,
                                                  topics[i].second);
  }

  return EpochTopics(std::move(top_topics_and_observing_domains),
                     padded_top_topics_start_index, kTaxonomySize,
                     kTaxonomyVersion, kModelVersion, calculation_time);
}

}  // namespace

// A tester class that allows mocking the topics calculators (i.e. the result
// and the finish delay).
class TesterBrowsingTopicsService : public BrowsingTopicsServiceImpl {
 public:
  TesterBrowsingTopicsService(
      const base::FilePath& profile_path,
      privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
      history::HistoryService* history_service,
      content::BrowsingTopicsSiteDataManager* site_data_manager,
      optimization_guide::PageContentAnnotationsService* annotations_service,
      base::queue<EpochTopics> mock_calculator_results,
      base::TimeDelta calculator_finish_delay)
      : BrowsingTopicsServiceImpl(profile_path,
                                  privacy_sandbox_settings,
                                  history_service,
                                  site_data_manager,
                                  annotations_service),
        mock_calculator_results_(std::move(mock_calculator_results)),
        calculator_finish_delay_(calculator_finish_delay) {}

  ~TesterBrowsingTopicsService() override = default;

  TesterBrowsingTopicsService(const TesterBrowsingTopicsService&) = delete;
  TesterBrowsingTopicsService& operator=(const TesterBrowsingTopicsService&) =
      delete;
  TesterBrowsingTopicsService(TesterBrowsingTopicsService&&) = delete;
  TesterBrowsingTopicsService& operator=(TesterBrowsingTopicsService&&) =
      delete;

  std::unique_ptr<BrowsingTopicsCalculator> CreateCalculator(
      privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
      history::HistoryService* history_service,
      content::BrowsingTopicsSiteDataManager* site_data_manager,
      optimization_guide::PageContentAnnotationsService* annotations_service,
      const base::circular_deque<EpochTopics>& epochs,
      BrowsingTopicsCalculator::CalculateCompletedCallback callback) override {
    DCHECK(!mock_calculator_results_.empty());

    ++started_calculations_count_;

    EpochTopics next_epoch = std::move(mock_calculator_results_.front());
    mock_calculator_results_.pop();

    return std::make_unique<TesterBrowsingTopicsCalculator>(
        privacy_sandbox_settings, history_service, site_data_manager,
        annotations_service, std::move(callback), std::move(next_epoch),
        calculator_finish_delay_);
  }

  const BrowsingTopicsState& browsing_topics_state() override {
    return BrowsingTopicsServiceImpl::browsing_topics_state();
  }

  void OnTopicsDataAccessibleSinceUpdated() override {
    BrowsingTopicsServiceImpl::OnTopicsDataAccessibleSinceUpdated();
  }

  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override {
    BrowsingTopicsServiceImpl::OnURLsDeleted(history_service, deletion_info);
  }

  // The number of calculations that have started, including those that have
  // finished, those that are ongoing, and those that have been canceled.
  size_t started_calculations_count() const {
    return started_calculations_count_;
  }

 private:
  base::queue<EpochTopics> mock_calculator_results_;
  base::TimeDelta calculator_finish_delay_;

  size_t started_calculations_count_ = 0u;
};

class BrowsingTopicsServiceImplTest
    : public content::RenderViewHostTestHarness {
 public:
  BrowsingTopicsServiceImplTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kBrowsingTopics},
        /*disabled_features=*/{});

    OverrideHmacKeyForTesting(kTestKey);

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    content_settings::CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    privacy_sandbox::RegisterProfilePrefs(prefs_.registry());

    host_content_settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        &prefs_, /*is_off_the_record=*/false, /*store_last_modified=*/false,
        /*restore_session=*/false);
    cookie_settings_ = base::MakeRefCounted<content_settings::CookieSettings>(
        host_content_settings_map_.get(), &prefs_, false, "chrome-extension");

    auto privacy_sandbox_delegate = std::make_unique<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>();
    privacy_sandbox_delegate->SetUpDefaultResponse(/*restricted=*/false);
    privacy_sandbox_settings_ =
        std::make_unique<privacy_sandbox::PrivacySandboxSettings>(
            std::move(privacy_sandbox_delegate),
            host_content_settings_map_.get(), cookie_settings_, &prefs_,
            /*incognito_profile=*/false);

    history_service_ = std::make_unique<history::HistoryService>();
    history_service_->Init(
        history::TestHistoryDatabaseParamsForPath(temp_dir_.GetPath()));

    optimization_guide_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    page_content_annotations_service_ =
        std::make_unique<optimization_guide::PageContentAnnotationsService>(
            "en-US", optimization_guide_model_provider_.get(),
            history_service_.get(), nullptr, base::FilePath(), nullptr,
            nullptr);

    page_content_annotations_service_->OverridePageContentAnnotatorForTesting(
        &test_page_content_annotator_);

    task_environment()->RunUntilIdle();
  }

  ~BrowsingTopicsServiceImplTest() override = default;

  void TearDown() override {
    DCHECK(history_service_);

    browsing_topics_service_.reset();

    base::RunLoop run_loop;
    history_service_->SetOnBackendDestroyTask(run_loop.QuitClosure());
    history_service_->Shutdown();
    run_loop.Run();

    page_content_annotations_service_.reset();
    optimization_guide_model_provider_.reset();
    task_environment()->RunUntilIdle();

    host_content_settings_map_->ShutdownOnUIThread();

    content::RenderViewHostTestHarness::TearDown();
  }

  void NavigateToPage(const GURL& url) {
    auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    simulator->SetTransition(ui::PageTransition::PAGE_TRANSITION_TYPED);
    simulator->Commit();
  }

  content::BrowsingTopicsSiteDataManager* topics_site_data_manager() {
    return web_contents()
        ->GetPrimaryMainFrame()
        ->GetProcess()
        ->GetStoragePartition()
        ->GetBrowsingTopicsSiteDataManager();
  }

  base::FilePath BrowsingTopicsStateFilePath() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("BrowsingTopicsState"));
  }

  void CreateBrowsingTopicsStateFile(
      const std::vector<EpochTopics>& epochs,
      base::Time next_scheduled_calculation_time) {
    base::Value::List epochs_list;
    for (const EpochTopics& epoch : epochs) {
      epochs_list.Append(epoch.ToDictValue());
    }

    base::Value::Dict dict;
    dict.Set("epochs", std::move(epochs_list));
    dict.Set("next_scheduled_calculation_time",
             base::TimeToValue(next_scheduled_calculation_time));
    dict.Set("hex_encoded_hmac_key", base::HexEncode(kTestKey));
    dict.Set("config_version", 1);

    JSONFileValueSerializer(BrowsingTopicsStateFilePath()).Serialize(dict);
  }

  void InitializeBrowsingTopicsService(
      base::queue<EpochTopics> mock_calculator_results) {
    browsing_topics_service_ = std::make_unique<TesterBrowsingTopicsService>(
        temp_dir_.GetPath(), privacy_sandbox_settings_.get(),
        history_service_.get(), topics_site_data_manager(),
        page_content_annotations_service_.get(),
        std::move(mock_calculator_results), kCalculatorDelay);
  }

  const BrowsingTopicsState& browsing_topics_state() {
    DCHECK(browsing_topics_service_);
    return browsing_topics_service_->browsing_topics_state();
  }

  HashedDomain GetHashedDomain(const std::string& domain) {
    return HashContextDomainForStorage(kTestKey, domain);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  base::ScopedTempDir temp_dir_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  std::unique_ptr<privacy_sandbox::PrivacySandboxSettings>
      privacy_sandbox_settings_;

  std::unique_ptr<history::HistoryService> history_service_;

  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  std::unique_ptr<optimization_guide::PageContentAnnotationsService>
      page_content_annotations_service_;

  optimization_guide::TestPageContentAnnotator test_page_content_annotator_;

  std::unique_ptr<TesterBrowsingTopicsService> browsing_topics_service_;
};

TEST_F(BrowsingTopicsServiceImplTest, EmptyInitialState_CalculationScheduling) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     kTime1));
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(6), {}},
                                                      {Topic(7), {}},
                                                      {Topic(8), {}},
                                                      {Topic(9), {}},
                                                      {Topic(10), {}}},
                                                     kTime2));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 0u);

  // Finish file loading.
  task_environment()->RunUntilIdle();

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);

  EXPECT_TRUE(browsing_topics_state().epochs().empty());

  // Finish the calculation.
  task_environment()->FastForwardBy(kCalculatorDelay);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 1u);
  EXPECT_EQ(browsing_topics_state().epochs()[0].calculation_time(), kTime1);
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            start_time + kCalculatorDelay + base::Days(7));

  // Advance the time to right before the next scheduled calculation. The next
  // calculation should not happen.
  task_environment()->FastForwardBy(base::Days(7) - base::Seconds(1));

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);

  // Advance the time to the scheduled calculation time. A calculation should
  // happen.
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 2u);

  // Finish the calculation.
  task_environment()->FastForwardBy(kCalculatorDelay);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_EQ(browsing_topics_state().epochs()[1].calculation_time(), kTime2);
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            start_time + 2 * kCalculatorDelay + 2 * base::Days(7));
}

TEST_F(BrowsingTopicsServiceImplTest,
       StartFromPreexistingState_CalculateAtScheduledTime) {
  base::Time start_time = base::Time::Now();

  std::vector<EpochTopics> preexisting_epochs;
  preexisting_epochs.push_back(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     kTime1));

  CreateBrowsingTopicsStateFile(
      std::move(preexisting_epochs),
      /*next_scheduled_calculation_time=*/start_time + base::Days(1));

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(6), {}},
                                                      {Topic(7), {}},
                                                      {Topic(8), {}},
                                                      {Topic(9), {}},
                                                      {Topic(10), {}}},
                                                     kTime2));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading.
  task_environment()->RunUntilIdle();

  EXPECT_EQ(browsing_topics_state().epochs().size(), 1u);
  EXPECT_EQ(browsing_topics_state().epochs()[0].calculation_time(), kTime1);
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            start_time + base::Days(1));

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 0u);

  // Advance the time to the scheduled calculation time. A calculation should
  // happen.
  task_environment()->FastForwardBy(base::Days(1));

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);
  // Finish the calculation.
  task_environment()->FastForwardBy(kCalculatorDelay);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_EQ(browsing_topics_state().epochs()[1].calculation_time(), kTime2);
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            start_time + base::Days(1) + kCalculatorDelay + base::Days(7));
}

TEST_F(
    BrowsingTopicsServiceImplTest,
    StartFromPreexistingState_ScheduledTimeReachedBeforeStartup_CalculateImmediately) {
  base::Time start_time = base::Time::Now();

  std::vector<EpochTopics> preexisting_epochs;
  preexisting_epochs.push_back(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     kTime1));

  CreateBrowsingTopicsStateFile(
      std::move(preexisting_epochs),
      /*next_scheduled_calculation_time=*/start_time - base::Seconds(1));

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(6), {}},
                                                      {Topic(7), {}},
                                                      {Topic(8), {}},
                                                      {Topic(9), {}},
                                                      {Topic(10), {}}},
                                                     kTime2));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading.
  task_environment()->RunUntilIdle();

  EXPECT_EQ(browsing_topics_state().epochs().size(), 1u);
  EXPECT_EQ(browsing_topics_state().epochs()[0].calculation_time(), kTime1);
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            start_time - base::Seconds(1));

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);
}

TEST_F(
    BrowsingTopicsServiceImplTest,
    StartFromPreexistingState_TopicsAccessibleSinceUpdated_ResetStateAndStorage_CalculateAtScheduledTime) {
  base::Time start_time = base::Time::Now();

  std::vector<EpochTopics> preexisting_epochs;
  preexisting_epochs.push_back(
      CreateTestEpochTopics({{Topic(1), {}},
                             {Topic(2), {}},
                             {Topic(3), {}},
                             {Topic(4), {}},
                             {Topic(5), {}}},
                            start_time - base::Days(1)));

  // Add some arbitrary data to site data storage. The intent is just to test
  // data deletion.
  topics_site_data_manager()->OnBrowsingTopicsApiUsed(
      HashMainFrameHostForStorage("a.com"), {HashedDomain(1)},
      base::Time::Now());

  task_environment()->FastForwardBy(base::Seconds(1));
  privacy_sandbox_settings_->OnCookiesCleared();

  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      1u);

  CreateBrowsingTopicsStateFile(
      std::move(preexisting_epochs),
      /*next_scheduled_calculation_time=*/start_time + base::Days(1));

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time - base::Days(1)));
  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading.
  task_environment()->RunUntilIdle();

  EXPECT_EQ(browsing_topics_state().epochs().size(), 0u);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 0u);
  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      0u);
}

TEST_F(
    BrowsingTopicsServiceImplTest,
    StartFromPreexistingState_UnexpectedNextCalculationDelay_ResetState_CalculateImmediately) {
  base::Time start_time = base::Time::Now();

  std::vector<EpochTopics> preexisting_epochs;
  preexisting_epochs.push_back(
      CreateTestEpochTopics({{Topic(1), {}},
                             {Topic(2), {}},
                             {Topic(3), {}},
                             {Topic(4), {}},
                             {Topic(5), {}}},
                            start_time - base::Days(1)));

  privacy_sandbox_settings_->OnCookiesCleared();

  CreateBrowsingTopicsStateFile(
      std::move(preexisting_epochs),
      /*next_scheduled_calculation_time=*/start_time + base::Days(15));

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(6), {}},
                                                      {Topic(7), {}},
                                                      {Topic(8), {}},
                                                      {Topic(9), {}},
                                                      {Topic(10), {}}},
                                                     kTime2));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading.
  task_environment()->RunUntilIdle();

  EXPECT_EQ(browsing_topics_state().epochs().size(), 0u);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);
}

TEST_F(BrowsingTopicsServiceImplTest,
       StartFromPreexistingState_DefaultHandlingBeforeLoadFinish) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::Time start_time = base::Time::Now();

  std::vector<EpochTopics> preexisting_epochs;
  preexisting_epochs.push_back(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));

  CreateBrowsingTopicsStateFile(
      std::move(preexisting_epochs),
      /*next_scheduled_calculation_time=*/start_time + base::Days(1));

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(6), {}},
                                                      {Topic(7), {}},
                                                      {Topic(8), {}},
                                                      {Topic(9), {}},
                                                      {Topic(10), {}}},
                                                     kTime2));
  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  NavigateToPage(GURL("https://www.foo.com"));

  EXPECT_TRUE(browsing_topics_service_
                  ->GetBrowsingTopicsForJsApi(
                      /*context_origin=*/url::Origin::Create(
                          GURL("https://www.bar.com")),
                      web_contents()->GetPrimaryMainFrame())
                  .empty());
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEmptyReasonName,
      0 /* kStateNotReady */);

  EXPECT_TRUE(browsing_topics_service_
                  ->GetTopicsForSiteForDisplay(
                      url::Origin::Create(GURL("https://www.bar.com")))
                  .empty());
  EXPECT_TRUE(browsing_topics_service_->GetTopTopicsForDisplay().empty());

  base::test::TestFuture<mojom::WebUIGetBrowsingTopicsStateResultPtr> future1;
  browsing_topics_service_->GetBrowsingTopicsStateForWebUi(
      /*calculate_now=*/false, future1.GetCallback());
  EXPECT_TRUE(future1.IsReady());
  EXPECT_EQ(future1.Take()->get_override_status_message(),
            "State loading hasn't finished. Please retry shortly.");

  // Finish file loading.
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(browsing_topics_service_
                   ->GetBrowsingTopicsForJsApi(
                       /*context_origin=*/url::Origin::Create(
                           GURL("https://www.bar.com")),
                       web_contents()->GetPrimaryMainFrame())
                   .empty());
  EXPECT_FALSE(browsing_topics_service_
                   ->GetTopicsForSiteForDisplay(
                       url::Origin::Create(GURL("https://www.bar.com")))
                   .empty());
  EXPECT_FALSE(browsing_topics_service_->GetTopTopicsForDisplay().empty());

  base::test::TestFuture<mojom::WebUIGetBrowsingTopicsStateResultPtr> future2;
  browsing_topics_service_->GetBrowsingTopicsStateForWebUi(
      /*calculate_now=*/false, future2.GetCallback());
  EXPECT_TRUE(future2.IsReady());
  EXPECT_FALSE(future2.Take()->is_override_status_message());
}

TEST_F(
    BrowsingTopicsServiceImplTest,
    OnTopicsDataAccessibleSinceUpdated_ResetState_ClearTopicsSiteDataStorage) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     start_time));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time + kCalculatorDelay + base::Days(7)));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading and two calculations.
  task_environment()->FastForwardBy(2 * kCalculatorDelay + base::Days(7));

  // Add some arbitrary data to site data storage. The intent is just to test
  // data deletion.
  topics_site_data_manager()->OnBrowsingTopicsApiUsed(
      HashMainFrameHostForStorage("a.com"), {HashedDomain(1)},
      base::Time::Now());

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      1u);

  task_environment()->FastForwardBy(base::Seconds(1));
  privacy_sandbox_settings_->OnCookiesCleared();

  EXPECT_EQ(browsing_topics_state().epochs().size(), 0u);
  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      0u);
}

TEST_F(BrowsingTopicsServiceImplTest,
       OnURLsDeleted_TimeRangeOverlapWithOneEpoch) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     start_time));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time + kCalculatorDelay + base::Days(7)));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading and two calculations.
  task_environment()->FastForwardBy(2 * kCalculatorDelay + base::Days(7));

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_FALSE(browsing_topics_state().epochs()[0].empty());
  EXPECT_FALSE(browsing_topics_state().epochs()[1].empty());

  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(start_time + base::Days(5),
                                 start_time + base::Days(6)),
      /*is_from_expiration=*/false, /*deleted_rows=*/{}, /*favicon_urls=*/{},
      /*restrict_urls=*/absl::nullopt);

  browsing_topics_service_->OnURLsDeleted(history_service_.get(),
                                          deletion_info);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_FALSE(browsing_topics_state().epochs()[0].empty());
  EXPECT_TRUE(browsing_topics_state().epochs()[1].empty());
}

TEST_F(BrowsingTopicsServiceImplTest,
       OnURLsDeleted_TimeRangeOverlapWithAllEpochs) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     start_time));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time + kCalculatorDelay + base::Days(7)));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading and two calculations.
  task_environment()->FastForwardBy(2 * kCalculatorDelay + base::Days(7));

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_FALSE(browsing_topics_state().epochs()[0].empty());
  EXPECT_FALSE(browsing_topics_state().epochs()[1].empty());

  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(start_time, start_time + base::Days(2)),
      /*is_from_expiration=*/false, /*deleted_rows=*/{}, /*favicon_urls=*/{},
      /*restrict_urls=*/absl::nullopt);

  browsing_topics_service_->OnURLsDeleted(history_service_.get(),
                                          deletion_info);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_TRUE(browsing_topics_state().epochs()[0].empty());
  EXPECT_TRUE(browsing_topics_state().epochs()[1].empty());
}

TEST_F(BrowsingTopicsServiceImplTest, Recalculate) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     kTime1));
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(6), {}},
                                                      {Topic(7), {}},
                                                      {Topic(8), {}},
                                                      {Topic(9), {}},
                                                      {Topic(10), {}}},
                                                     kTime2));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->FastForwardBy(kCalculatorDelay - base::Seconds(1));

  EXPECT_EQ(browsing_topics_state().epochs().size(), 0u);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);

  // History deletion during a calculation should trigger the re-calculation.
  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(start_time, start_time + base::Days(2)),
      /*is_from_expiration=*/false, /*deleted_rows=*/{}, /*favicon_urls=*/{},
      /*restrict_urls=*/absl::nullopt);
  browsing_topics_service_->OnURLsDeleted(history_service_.get(),
                                          deletion_info);

  // The calculation shouldn't finish at the originally expected time, as it was
  // dropped and a new calculation has started.
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_EQ(browsing_topics_state().epochs().size(), 0u);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 2u);

  // Finish the re-started calculation.
  task_environment()->FastForwardBy(kCalculatorDelay - base::Seconds(1));
  EXPECT_EQ(browsing_topics_state().epochs().size(), 1u);

  // Expect that the result comes from the re-started calculator.
  EXPECT_EQ(browsing_topics_state().epochs()[0].calculation_time(), kTime2);
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            base::Time::Now() + base::Days(7));
}

TEST_F(BrowsingTopicsServiceImplTest,
       GetBrowsingTopicsForJsApi_PrivacySandboxSettingsDisabled) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->RunUntilIdle();

  privacy_sandbox_settings_->SetPrivacySandboxEnabled(false);

  NavigateToPage(GURL("https://www.foo.com"));

  EXPECT_TRUE(browsing_topics_service_
                  ->GetBrowsingTopicsForJsApi(
                      /*context_origin=*/url::Origin::Create(
                          GURL("https://www.bar.com")),
                      web_contents()->GetPrimaryMainFrame())
                  .empty());

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEmptyReasonName,
      1 /* kAccessDisallowedBySettings */);
}

TEST_F(BrowsingTopicsServiceImplTest, GetBrowsingTopicsForJsApi_OneEpoch) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->FastForwardBy(kCalculatorDelay);

  NavigateToPage(GURL("https://www.foo.com"));

  // Current time is before the epoch switch time.
  std::vector<blink::mojom::EpochTopicPtr> result =
      browsing_topics_service_->GetBrowsingTopicsForJsApi(
          /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
          web_contents()->GetPrimaryMainFrame());

  EXPECT_TRUE(result.empty());

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEmptyReasonName,
      2 /* kNoCandicateTopics */);

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(base::Days(7) - base::Seconds(1));

  result = browsing_topics_service_->GetBrowsingTopicsForJsApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0]->topic, 2);
  EXPECT_EQ(result[0]->config_version, "chrome.1");
  EXPECT_EQ(result[0]->taxonomy_version, "1");
  EXPECT_EQ(result[0]->model_version, "5000000000");
  EXPECT_EQ(result[0]->version, "chrome.1:1:5000000000");
}

TEST_F(BrowsingTopicsServiceImplTest,
       GetBrowsingTopicsForJsApi_OneEpoch_Filtered) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->FastForwardBy(kCalculatorDelay);

  NavigateToPage(GURL("https://www.foo.com"));

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(base::Days(7) - base::Seconds(1));

  std::vector<blink::mojom::EpochTopicPtr> result =
      browsing_topics_service_->GetBrowsingTopicsForJsApi(
          /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
          web_contents()->GetPrimaryMainFrame());

  EXPECT_TRUE(result.empty());

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEmptyReasonName,
      3 /* kCandicateTopicsFiltered */);
}

TEST_F(BrowsingTopicsServiceImplTest,
       GetBrowsingTopicsForJsApi_TopicNotAllowedByPrivacySandboxSettings) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->FastForwardBy(kCalculatorDelay);

  NavigateToPage(GURL("https://www.foo.com"));

  // Current time is before the epoch switch time.
  std::vector<blink::mojom::EpochTopicPtr> result =
      browsing_topics_service_->GetBrowsingTopicsForJsApi(
          /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
          web_contents()->GetPrimaryMainFrame());

  EXPECT_TRUE(result.empty());

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(base::Days(7) - base::Seconds(1));

  privacy_sandbox_settings_->SetTopicAllowed(
      privacy_sandbox::CanonicalTopic(Topic(2), /*taxonomy_version=*/1), false);

  result = browsing_topics_service_->GetBrowsingTopicsForJsApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame());

  EXPECT_TRUE(result.empty());
}

TEST_F(BrowsingTopicsServiceImplTest, GetBrowsingTopicsForJsApi_FourEpochs) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(11), {GetHashedDomain("bar.com")}},
                             {Topic(12), {GetHashedDomain("bar.com")}},
                             {Topic(13), {GetHashedDomain("bar.com")}},
                             {Topic(14), {GetHashedDomain("bar.com")}},
                             {Topic(15), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(16), {GetHashedDomain("bar.com")}},
                             {Topic(17), {GetHashedDomain("bar.com")}},
                             {Topic(18), {GetHashedDomain("bar.com")}},
                             {Topic(19), {GetHashedDomain("bar.com")}},
                             {Topic(20), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(4 * kCalculatorDelay + 3 * base::Days(7));

  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  NavigateToPage(GURL("https://www.foo.com"));

  // Current time is before the epoch switch time.
  std::vector<blink::mojom::EpochTopicPtr> result =
      browsing_topics_service_->GetBrowsingTopicsForJsApi(
          /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
          web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(result.size(), 3u);
  std::set<int> result_set;
  result_set.insert(result[0]->topic);
  result_set.insert(result[1]->topic);
  result_set.insert(result[2]->topic);
  EXPECT_EQ(result_set, std::set<int>({2, 7, 12}));

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(base::Days(7) - base::Seconds(1));

  result = browsing_topics_service_->GetBrowsingTopicsForJsApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(result.size(), 3u);
  result_set.clear();
  result_set.insert(result[0]->topic);
  result_set.insert(result[1]->topic);
  result_set.insert(result[2]->topic);
  EXPECT_EQ(result_set, std::set<int>({7, 12, 17}));
}

TEST_F(BrowsingTopicsServiceImplTest,
       GetBrowsingTopicsForJsApi_DuplicateTopicsRemoved) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(4 * kCalculatorDelay + 3 * base::Days(7));

  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  NavigateToPage(GURL("https://www.foo.com"));

  // Current time is before the epoch switch time.
  std::vector<blink::mojom::EpochTopicPtr> result =
      browsing_topics_service_->GetBrowsingTopicsForJsApi(
          /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
          web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(result.size(), 2u);
  std::set<int> result_set;
  result_set.insert(result[0]->topic);
  result_set.insert(result[1]->topic);
  EXPECT_EQ(result_set, std::set<int>({2, 7}));

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(base::Days(7) - base::Seconds(1));

  result = browsing_topics_service_->GetBrowsingTopicsForJsApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(result.size(), 2u);
  result_set.clear();
  result_set.insert(result[0]->topic);
  result_set.insert(result[1]->topic);
  EXPECT_EQ(result_set, std::set<int>({2, 7}));
}

// TODO(yaoxia): Re-enable. This test currently fails solely due to it's
// generating a lot of GMOCK WARNING output.
TEST_F(BrowsingTopicsServiceImplTest,
       DISABLED_GetBrowsingTopicsForJsApi_TopicsReturnedInRandomOrder) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(4 * kCalculatorDelay + 3 * base::Days(7));

  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  NavigateToPage(GURL("https://www.foo.com"));

  // Current time is before the epoch switch time.

  // Expect that in 100 rounds, both Topic(2) and Topic(7) should be seen being
  // at the front.
  bool seen_topic_2_at_front = false;
  bool seen_topic_7_at_front = false;

  for (int i = 0; i < 100; ++i) {
    std::vector<blink::mojom::EpochTopicPtr> result =
        browsing_topics_service_->GetBrowsingTopicsForJsApi(
            /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
            web_contents()->GetPrimaryMainFrame());

    EXPECT_EQ(result.size(), 2u);
    std::set<int> result_set;
    result_set.insert(result[0]->topic);
    result_set.insert(result[1]->topic);
    EXPECT_EQ(result_set, std::set<int>({2, 7}));

    seen_topic_2_at_front = seen_topic_2_at_front || (result[0]->topic == 2);
    seen_topic_7_at_front = seen_topic_2_at_front || (result[0]->topic == 7);
  }

  EXPECT_TRUE(seen_topic_2_at_front);
  EXPECT_TRUE(seen_topic_7_at_front);
}

TEST_F(BrowsingTopicsServiceImplTest,
       GetBrowsingTopicsForJsApi_TrackedUsageContext) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->RunUntilIdle();

  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      0u);

  NavigateToPage(GURL("https://www.foo.com"));
  browsing_topics_service_->GetBrowsingTopicsForJsApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame());

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("www.foo.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain,
            GetHashedDomain("bar.com"));
}

TEST_F(BrowsingTopicsServiceImplTest, ApiResultUkm_ZeroAndOneTopic) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(kCalculatorDelay);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 1u);

  NavigateToPage(GURL("https://www.foo.com"));

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Current time is before the epoch switch time. Expect one ukm event without
  // any metrics.
  browsing_topics_service_->GetBrowsingTopicsForJsApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame());

  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEntryName);
  EXPECT_EQ(1u, entries.size());

  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEmptyReasonName,
      2 /* kNoCandicateTopics */);
  EXPECT_FALSE(ukm_recorder.GetEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic0Name));

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(base::Days(7) - base::Seconds(1));

  browsing_topics_service_->GetBrowsingTopicsForJsApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame());

  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEntryName);
  EXPECT_EQ(2u, entries.size());

  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic0Name,
      2);
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic0IsTrueTopTopicName,
      true);
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic0ModelVersionName,
      5000000000ULL);
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic0TaxonomyVersionName,
      1);

  EXPECT_FALSE(ukm_recorder.GetEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic1Name));
}

TEST_F(BrowsingTopicsServiceImplTest,
       ApiResultUkm_ReportTrueStatusIfTheTopicHasMixedTrueAndRandomStatus) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/0));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1, /*padded_top_topics_start_index=*/0));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1, /*padded_top_topics_start_index=*/0));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(4 * kCalculatorDelay + 3 * base::Days(7));

  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  NavigateToPage(GURL("https://www.foo.com"));

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(base::Days(7) - base::Seconds(1));

  browsing_topics_service_->GetBrowsingTopicsForJsApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame());

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEntryName);
  EXPECT_EQ(1u, entries.size());

  const int64_t* topic0_metric = ukm_recorder.GetEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic0Name);
  const int64_t* topic0_is_true_metric = ukm_recorder.GetEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic0IsTrueTopTopicName);
  const int64_t* topic1_metric = ukm_recorder.GetEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic1Name);
  const int64_t* topic1_is_true_metric = ukm_recorder.GetEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic1IsTrueTopTopicName);

  EXPECT_TRUE(topic0_metric);
  EXPECT_TRUE(topic0_is_true_metric);
  EXPECT_TRUE(topic1_metric);
  EXPECT_TRUE(topic1_is_true_metric);

  EXPECT_TRUE((*topic0_metric == 2 && *topic0_is_true_metric == false &&
               *topic1_metric == 7 && *topic1_is_true_metric == true) ||
              (*topic0_metric == 7 && *topic0_is_true_metric == true &&
               *topic1_metric == 2 && *topic1_is_true_metric == false));

  EXPECT_FALSE(ukm_recorder.GetEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic2Name));
}

TEST_F(BrowsingTopicsServiceImplTest, GetTopicsForSiteForDisplay) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/0));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(4 * kCalculatorDelay + 3 * base::Days(7));

  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  NavigateToPage(GURL("https://www.foo.com"));

  // Current time is before the epoch switch time.
  std::vector<privacy_sandbox::CanonicalTopic> result =
      browsing_topics_service_->GetTopicsForSiteForDisplay(
          web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  EXPECT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0].topic_id(), Topic(2));
  EXPECT_EQ(result[1].topic_id(), Topic(7));
  EXPECT_EQ(result[0].taxonomy_version(), 1);
  EXPECT_EQ(result[1].taxonomy_version(), 1);
}

TEST_F(BrowsingTopicsServiceImplTest, GetTopTopicsForDisplay) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/1));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/2));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(4 * kCalculatorDelay + 3 * base::Days(7));

  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  NavigateToPage(GURL("https://www.foo.com"));

  // Current time is before the epoch switch time.
  std::vector<privacy_sandbox::CanonicalTopic> result =
      browsing_topics_service_->GetTopTopicsForDisplay();

  EXPECT_EQ(result.size(), 13u);
  EXPECT_EQ(result[0].topic_id(), Topic(1));
  EXPECT_EQ(result[1].topic_id(), Topic(6));
  EXPECT_EQ(result[2].topic_id(), Topic(7));
  EXPECT_EQ(result[3].topic_id(), Topic(8));
  EXPECT_EQ(result[4].topic_id(), Topic(9));
  EXPECT_EQ(result[5].topic_id(), Topic(10));
  EXPECT_EQ(result[6].topic_id(), Topic(1));
  EXPECT_EQ(result[7].topic_id(), Topic(2));
  EXPECT_EQ(result[8].topic_id(), Topic(6));
  EXPECT_EQ(result[9].topic_id(), Topic(7));
  EXPECT_EQ(result[10].topic_id(), Topic(8));
  EXPECT_EQ(result[11].topic_id(), Topic(9));
  EXPECT_EQ(result[12].topic_id(), Topic(10));
}

TEST_F(BrowsingTopicsServiceImplTest,
       GetBrowsingTopicsStateForWebUi_CalculationInProgress) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     start_time));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->RunUntilIdle();

  base::test::TestFuture<mojom::WebUIGetBrowsingTopicsStateResultPtr> future1;
  base::test::TestFuture<mojom::WebUIGetBrowsingTopicsStateResultPtr> future2;
  browsing_topics_service_->GetBrowsingTopicsStateForWebUi(
      /*calculate_now=*/false, future1.GetCallback());
  browsing_topics_service_->GetBrowsingTopicsStateForWebUi(
      /*calculate_now=*/true, future2.GetCallback());

  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  task_environment()->FastForwardBy(kCalculatorDelay);

  // The callbacks are invoked after the calculation has finished.
  EXPECT_TRUE(future1.IsReady());
  EXPECT_TRUE(future2.IsReady());

  mojom::WebUIGetBrowsingTopicsStateResultPtr result1 = future1.Take();
  mojom::WebUIGetBrowsingTopicsStateResultPtr result2 = future2.Take();
  EXPECT_EQ(result1, result2);

  mojom::WebUIBrowsingTopicsStatePtr& webui_state1 =
      result1->get_browsing_topics_state();

  EXPECT_EQ(webui_state1->epochs.size(), 1u);
  EXPECT_EQ(webui_state1->next_scheduled_calculation_time,
            start_time + kCalculatorDelay + base::Days(7));
}

TEST_F(BrowsingTopicsServiceImplTest,
       GetBrowsingTopicsStateForWebUi_CalculationNow) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     start_time));

  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {}},
                             {Topic(2), {}},
                             {Topic(3), {}},
                             {Topic(4), {}},
                             {Topic(5), {}}},
                            start_time + kCalculatorDelay + base::Days(1)));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->FastForwardBy(kCalculatorDelay);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 1u);
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            start_time + kCalculatorDelay + base::Days(7));

  // Advance by some time smaller than the periodic update interval.
  task_environment()->FastForwardBy(base::Days(1));

  base::test::TestFuture<mojom::WebUIGetBrowsingTopicsStateResultPtr> future;
  browsing_topics_service_->GetBrowsingTopicsStateForWebUi(
      /*calculate_now=*/true, future.GetCallback());

  EXPECT_FALSE(future.IsReady());
  task_environment()->FastForwardBy(kCalculatorDelay);
  EXPECT_TRUE(future.IsReady());

  mojom::WebUIGetBrowsingTopicsStateResultPtr result = future.Take();
  mojom::WebUIBrowsingTopicsStatePtr& webui_state =
      result->get_browsing_topics_state();

  EXPECT_EQ(webui_state->epochs.size(), 2u);

  // The `next_scheduled_calculation_time` is reset to 7 days after.
  EXPECT_EQ(webui_state->next_scheduled_calculation_time,
            start_time + 2 * kCalculatorDelay + base::Days(1) + base::Days(7));
}

TEST_F(BrowsingTopicsServiceImplTest, GetBrowsingTopicsStateForWebUi) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {HashedDomain(123), HashedDomain(456)}},
                             {Topic(2), {}},
                             {Topic(0), {}},  // blocked
                             {Topic(4), {}},
                             {Topic(5), {}}},
                            start_time));

  // Failed calculation.
  mock_calculator_results.push(EpochTopics(start_time + base::Days(7)));

  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time + base::Days(14),
                            /*padded_top_topics_start_index=*/2));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading and three calculations.
  task_environment()->FastForwardBy(3 * kCalculatorDelay + 2 * base::Days(7));

  base::test::TestFuture<mojom::WebUIGetBrowsingTopicsStateResultPtr> future;
  browsing_topics_service_->GetBrowsingTopicsStateForWebUi(
      /*calculate_now=*/false, future.GetCallback());
  EXPECT_TRUE(future.IsReady());

  mojom::WebUIGetBrowsingTopicsStateResultPtr result = future.Take();
  mojom::WebUIBrowsingTopicsStatePtr& webui_state =
      result->get_browsing_topics_state();

  EXPECT_EQ(webui_state->epochs.size(), 3u);
  EXPECT_EQ(webui_state->next_scheduled_calculation_time,
            start_time + 3 * kCalculatorDelay + 3 * base::Days(7));

  const mojom::WebUIEpochPtr& epoch0 = webui_state->epochs[0];
  const mojom::WebUIEpochPtr& epoch1 = webui_state->epochs[1];
  const mojom::WebUIEpochPtr& epoch2 = webui_state->epochs[2];

  EXPECT_EQ(epoch0->calculation_time, start_time + base::Days(14));
  EXPECT_EQ(epoch0->model_version, "5000000000");
  EXPECT_EQ(epoch0->taxonomy_version, "1");
  EXPECT_EQ(epoch0->topics.size(), 5u);
  EXPECT_EQ(epoch0->topics[0]->topic_id, 6);
  EXPECT_EQ(epoch0->topics[0]->topic_name, u"Entertainment industry");
  EXPECT_TRUE(epoch0->topics[0]->is_real_topic);
  EXPECT_TRUE(epoch0->topics[0]->observed_by_domains.empty());
  EXPECT_EQ(epoch0->topics[1]->topic_id, 7);
  EXPECT_EQ(epoch0->topics[1]->topic_name, u"Humor");
  EXPECT_TRUE(epoch0->topics[1]->is_real_topic);
  EXPECT_TRUE(epoch0->topics[1]->observed_by_domains.empty());
  EXPECT_EQ(epoch0->topics[2]->topic_id, 8);
  EXPECT_EQ(epoch0->topics[2]->topic_name, u"Live comedy");
  EXPECT_FALSE(epoch0->topics[2]->is_real_topic);
  EXPECT_TRUE(epoch0->topics[2]->observed_by_domains.empty());
  EXPECT_EQ(epoch0->topics[3]->topic_id, 9);
  EXPECT_EQ(epoch0->topics[3]->topic_name, u"Live sporting events");
  EXPECT_FALSE(epoch0->topics[3]->is_real_topic);
  EXPECT_TRUE(epoch0->topics[3]->observed_by_domains.empty());
  EXPECT_EQ(epoch0->topics[4]->topic_id, 10);
  EXPECT_EQ(epoch0->topics[4]->topic_name, u"Magic");
  EXPECT_FALSE(epoch0->topics[4]->is_real_topic);
  EXPECT_TRUE(epoch0->topics[4]->observed_by_domains.empty());

  EXPECT_EQ(epoch1->calculation_time, start_time + base::Days(7));
  EXPECT_EQ(epoch1->model_version, "0");
  EXPECT_EQ(epoch1->taxonomy_version, "0");
  EXPECT_EQ(epoch1->topics.size(), 0u);

  EXPECT_EQ(epoch2->calculation_time, start_time);
  EXPECT_EQ(epoch2->model_version, "5000000000");
  EXPECT_EQ(epoch2->taxonomy_version, "1");
  EXPECT_EQ(epoch2->topics.size(), 5u);
  EXPECT_EQ(epoch2->topics[0]->topic_id, 1);
  EXPECT_EQ(epoch2->topics[0]->topic_name, u"Arts & entertainment");
  EXPECT_TRUE(epoch2->topics[0]->is_real_topic);
  EXPECT_EQ(epoch2->topics[0]->observed_by_domains.size(), 2u);
  EXPECT_EQ(epoch2->topics[0]->observed_by_domains[0], "123");
  EXPECT_EQ(epoch2->topics[0]->observed_by_domains[1], "456");
  EXPECT_EQ(epoch2->topics[1]->topic_id, 2);
  EXPECT_EQ(epoch2->topics[1]->topic_name, u"Acting & theater");
  EXPECT_TRUE(epoch2->topics[1]->is_real_topic);
  EXPECT_TRUE(epoch2->topics[1]->observed_by_domains.empty());
  EXPECT_EQ(epoch2->topics[2]->topic_id, 0);
  EXPECT_EQ(epoch2->topics[2]->topic_name, u"Unknown");
  EXPECT_TRUE(epoch2->topics[2]->is_real_topic);
  EXPECT_TRUE(epoch2->topics[2]->observed_by_domains.empty());
  EXPECT_EQ(epoch2->topics[3]->topic_id, 4);
  EXPECT_EQ(epoch2->topics[3]->topic_name, u"Concerts & music festivals");
  EXPECT_TRUE(epoch2->topics[3]->is_real_topic);
  EXPECT_TRUE(epoch2->topics[3]->observed_by_domains.empty());
  EXPECT_EQ(epoch2->topics[4]->topic_id, 5);
  EXPECT_EQ(epoch2->topics[4]->topic_name, u"Dance");
  EXPECT_TRUE(epoch2->topics[4]->is_real_topic);
  EXPECT_TRUE(epoch2->topics[4]->observed_by_domains.empty());
}

TEST_F(BrowsingTopicsServiceImplTest, ClearTopic) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     start_time));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time + kCalculatorDelay + base::Days(7)));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading and two calculations.
  task_environment()->FastForwardBy(2 * kCalculatorDelay + base::Days(7));

  browsing_topics_service_->ClearTopic(
      privacy_sandbox::CanonicalTopic(Topic(3), /*taxonomy_version=*/1));

  std::vector<privacy_sandbox::CanonicalTopic> result =
      browsing_topics_service_->GetTopTopicsForDisplay();

  EXPECT_EQ(result.size(), 9u);
  EXPECT_EQ(result[0].topic_id(), Topic(1));
  EXPECT_EQ(result[1].topic_id(), Topic(2));
  EXPECT_EQ(result[2].topic_id(), Topic(4));
  EXPECT_EQ(result[3].topic_id(), Topic(5));
  EXPECT_EQ(result[4].topic_id(), Topic(6));
  EXPECT_EQ(result[5].topic_id(), Topic(7));
  EXPECT_EQ(result[6].topic_id(), Topic(8));
  EXPECT_EQ(result[7].topic_id(), Topic(9));
  EXPECT_EQ(result[8].topic_id(), Topic(10));
}

TEST_F(BrowsingTopicsServiceImplTest, ClearTopicBeforeLoadFinish) {
  base::Time start_time = base::Time::Now();

  std::vector<EpochTopics> preexisting_epochs;
  preexisting_epochs.push_back(
      CreateTestEpochTopics({{Topic(1), {}},
                             {Topic(2), {}},
                             {Topic(3), {}},
                             {Topic(4), {}},
                             {Topic(5), {}}},
                            start_time - base::Days(1)));

  CreateBrowsingTopicsStateFile(
      std::move(preexisting_epochs),
      /*next_scheduled_calculation_time=*/start_time + base::Days(1));

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(6), {}},
                                                      {Topic(7), {}},
                                                      {Topic(8), {}},
                                                      {Topic(9), {}},
                                                      {Topic(10), {}}},
                                                     kTime2));
  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  privacy_sandbox_settings_->SetTopicAllowed(
      privacy_sandbox::CanonicalTopic(Topic(3), /*taxonomy_version=*/1), false);

  // Finish file loading.
  task_environment()->RunUntilIdle();

  // If a topic in the settings is cleared before load finish, all pre-existing
  // topics data will be cleared in the `BrowsingTopicsState` after load finish.
  std::vector<privacy_sandbox::CanonicalTopic> result =
      browsing_topics_service_->GetTopTopicsForDisplay();
  EXPECT_EQ(result.size(), 0u);
}

TEST_F(BrowsingTopicsServiceImplTest, ClearAllTopicsData) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     start_time));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time + kCalculatorDelay + base::Days(7)));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading and two calculations.
  task_environment()->FastForwardBy(2 * kCalculatorDelay + base::Days(7));

  // Add some arbitrary data to site data storage. The intent is just to test
  // data deletion.
  topics_site_data_manager()->OnBrowsingTopicsApiUsed(
      HashMainFrameHostForStorage("a.com"), {HashedDomain(1), HashedDomain(2)},
      base::Time::Now());

  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      2u);

  task_environment()->FastForwardBy(base::Seconds(1));

  browsing_topics_service_->ClearAllTopicsData();

  task_environment()->RunUntilIdle();

  std::vector<privacy_sandbox::CanonicalTopic> result =
      browsing_topics_service_->GetTopTopicsForDisplay();

  EXPECT_TRUE(browsing_topics_service_->GetTopTopicsForDisplay().empty());
  EXPECT_TRUE(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).empty());
}

TEST_F(BrowsingTopicsServiceImplTest, ClearTopicsDataForOrigin) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("b.com")}},
                             {Topic(2), {GetHashedDomain("b.com")}},
                             {Topic(3), {GetHashedDomain("b.com")}},
                             {Topic(4), {GetHashedDomain("b.com")}},
                             {Topic(5), {GetHashedDomain("b.com")}}},
                            kTime1));
  mock_calculator_results.push(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("b.com")}},
                             {Topic(7), {GetHashedDomain("b.com")}},
                             {Topic(8), {GetHashedDomain("b.com")}},
                             {Topic(9), {GetHashedDomain("b.com")}},
                             {Topic(10), {GetHashedDomain("b.com")}}},
                            kTime1 + kCalculatorDelay + base::Days(7)));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  NavigateToPage(GURL("https://a.com"));

  // Finish file loading and two calculations.
  task_environment()->FastForwardBy(2 * kCalculatorDelay + base::Days(7));

  std::vector<blink::mojom::EpochTopicPtr> api_call_result =
      browsing_topics_service_->GetBrowsingTopicsForJsApi(
          /*context_origin=*/url::Origin::Create(GURL("https://b.com")),
          web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(api_call_result.size(), 1u);
  EXPECT_EQ(api_call_result[0]->topic, 3);

  // Add some arbitrary data to site data storage. The intent is just to test
  // data deletion.
  topics_site_data_manager()->OnBrowsingTopicsApiUsed(
      HashMainFrameHostForStorage("d.com"),
      {GetHashedDomain("b.com"), GetHashedDomain("c.com")}, base::Time::Now());

  // The data is from both the manual `OnBrowsingTopicsApiUsed` call and the
  // usage tracking due to the previous `GetBrowsingTopicsForJsApi`.
  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      3u);

  task_environment()->FastForwardBy(base::Seconds(1));

  browsing_topics_service_->ClearTopicsDataForOrigin(
      url::Origin::Create(GURL("https://b.com")));

  task_environment()->RunUntilIdle();

  // Note that this won't trigger another usage storing to the database, because
  // the same context domain was seen in the page before.
  api_call_result = browsing_topics_service_->GetBrowsingTopicsForJsApi(
      /*context_origin=*/url::Origin::Create(GURL("https://b.com")),
      web_contents()->GetPrimaryMainFrame());

  // Since the domain "b.com" is removed. The candidate topic won't be returned.
  EXPECT_TRUE(api_call_result.empty());

  std::vector<browsing_topics::ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());

  // The context domain "b.com" should have been cleared.
  EXPECT_EQ(api_usage_contexts.size(), 1u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("d.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain,
            GetHashedDomain("c.com"));
}

}  // namespace browsing_topics
