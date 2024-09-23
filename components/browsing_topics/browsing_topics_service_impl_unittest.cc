// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_service_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/browsing_topics/test_util.h"
#include "components/browsing_topics/util.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
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

// Tests can be slow if `TaskEnvironment::FastForwardBy()` is called with a long
// period of time. Thus, use `base::Seconds(1)` as the duration of a day in
// tests.
constexpr base::TimeDelta kOneTestDay = base::Seconds(1);
constexpr base::TimeDelta kEpoch = 7 * kOneTestDay;
constexpr base::TimeDelta kMaxEpochIntroductionDelay = 2 * kOneTestDay;
constexpr base::TimeDelta kDatabaseFetchDelay = base::Milliseconds(1);
constexpr base::TimeDelta kCalculatorDelay = base::Milliseconds(1);
constexpr base::TimeDelta kFirstTimeoutRetryDelay = base::Milliseconds(10);

constexpr browsing_topics::HmacKey kTestKey = {1};

constexpr base::Time kTime1 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
constexpr base::Time kTime2 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(2));
constexpr base::Time kTime3 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(3));

constexpr int kConfigVersion = 1;
constexpr int kTaxonomyVersion = 1;
constexpr int64_t kModelVersion = 5000000000LL;

EpochTopics CreateTestEpochTopics(
    const std::vector<std::pair<Topic, std::set<HashedDomain>>>& topics,
    base::Time calculation_time,
    size_t padded_top_topics_start_index = 5,
    int64_t model_version = kModelVersion,
    int config_version = kConfigVersion) {
  DCHECK_EQ(topics.size(), 5u);

  std::vector<TopicAndDomains> top_topics_and_observing_domains;
  for (size_t i = 0; i < 5; ++i) {
    top_topics_and_observing_domains.emplace_back(topics[i].first,
                                                  topics[i].second);
  }

  return EpochTopics(std::move(top_topics_and_observing_domains),
                     padded_top_topics_start_index, config_version,
                     kTaxonomyVersion, model_version, calculation_time,
                     /*from_manually_triggered_calculation=*/false);
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
      std::unique_ptr<Annotator> annotator,
      base::queue<EpochTopics> mock_calculator_results,
      base::TimeDelta calculator_finish_delay)
      : BrowsingTopicsServiceImpl(
            profile_path,
            privacy_sandbox_settings,
            history_service,
            site_data_manager,
            std::move(annotator),
            base::BindRepeating(
                content_settings::PageSpecificContentSettings::TopicAccessed)),
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
      Annotator* annotator,
      const base::circular_deque<EpochTopics>& epochs,
      bool is_manually_triggered,
      int previous_timeout_count,
      base::Time session_start_time,
      BrowsingTopicsCalculator::CalculateCompletedCallback callback) override {
    DCHECK(!mock_calculator_results_.empty());

    ++started_calculations_count_;

    EpochTopics next_epoch = std::move(mock_calculator_results_.front());
    mock_calculator_results_.pop();

    return std::make_unique<TesterBrowsingTopicsCalculator>(
        privacy_sandbox_settings, history_service, site_data_manager, annotator,
        previous_timeout_count, session_start_time, std::move(callback),
        std::move(next_epoch), calculator_finish_delay_);
  }

  const BrowsingTopicsState& browsing_topics_state() override {
    return BrowsingTopicsServiceImpl::browsing_topics_state();
  }

  void OnTopicsDataAccessibleSinceUpdated() override {
    BrowsingTopicsServiceImpl::OnTopicsDataAccessibleSinceUpdated();
  }

  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override {
    BrowsingTopicsServiceImpl::OnHistoryDeletions(history_service,
                                                  deletion_info);
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
    // Configure a long epoch_retention_duration to prevent epochs from expiring
    // during tests where expiration is irrelevant.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{blink::features::kBrowsingTopics, {}},
         {blink::features::kBrowsingTopicsParameters,
          {{"time_period_per_epoch",
            base::StrCat({base::NumberToString(kEpoch.InSeconds()), "s"})},
           {"first_timeout_retry_delay",
            base::StrCat(
                {base::NumberToString(kFirstTimeoutRetryDelay.InMilliseconds()),
                 "ms"})},
           {"epoch_retention_duration", "3650000d"},
           {"max_epoch_introduction_delay",
            base::StrCat(
                {base::NumberToString(kMaxEpochIntroductionDelay.InSeconds()),
                 "s"})}}}},
        /*disabled_features=*/{});

    OverrideHmacKeyForTesting(kTestKey);

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    content_settings::CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    privacy_sandbox::RegisterProfilePrefs(prefs_.registry());

    host_content_settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        &prefs_, /*is_off_the_record=*/false, /*store_last_modified=*/false,
        /*restore_session=*/false, /*should_record_metrics=*/false);
    tracking_protection_settings_ =
        std::make_unique<privacy_sandbox::TrackingProtectionSettings>(
            &prefs_, host_content_settings_map_.get(),
            /*is_incognito=*/false);
    cookie_settings_ = base::MakeRefCounted<content_settings::CookieSettings>(
        host_content_settings_map_.get(), &prefs_,
        tracking_protection_settings_.get(), false,
        content_settings::CookieSettings::NoFedCmSharingPermissionsCallback(),
        /*tpcd_metadata_manager=*/nullptr, "chrome-extension");

    auto privacy_sandbox_delegate = std::make_unique<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>();
    privacy_sandbox_delegate->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/false);
    privacy_sandbox_delegate->SetUpIsIncognitoProfileResponse(
        /*incognito=*/false);
    privacy_sandbox_settings_ =
        std::make_unique<privacy_sandbox::PrivacySandboxSettingsImpl>(
            std::move(privacy_sandbox_delegate),
            host_content_settings_map_.get(), cookie_settings_,
            tracking_protection_settings_.get(), &prefs_);
    privacy_sandbox_settings_->SetAllPrivacySandboxAllowedForTesting();

    history_service_ = std::make_unique<history::HistoryService>();
    history_service_->Init(
        history::TestHistoryDatabaseParamsForPath(temp_dir_.GetPath()));

    task_environment()->RunUntilIdle();
  }

  ~BrowsingTopicsServiceImplTest() override = default;

  void SetUp() override {
    scoped_attestations_ =
        std::make_unique<privacy_sandbox::ScopedPrivacySandboxAttestations>(
            privacy_sandbox::PrivacySandboxAttestations::CreateForTesting());
    // By default turn on the setting that makes all APIs considered attested as
    // test cases are testing behaviors not related to attestations.
    privacy_sandbox::PrivacySandboxAttestations::GetInstance()
        ->SetAllPrivacySandboxAttestedForTesting(true);

    content::RenderViewHostTestHarness::SetUp();

    content_settings::PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<
            content_settings::TestPageSpecificContentSettingsDelegate>(
            &prefs_, host_content_settings_map_.get()));
  }

  void TearDown() override {
    DCHECK(history_service_);

    browsing_topics_service_.reset();

    base::RunLoop run_loop;
    history_service_->SetOnBackendDestroyTask(run_loop.QuitClosure());
    history_service_->Shutdown();
    run_loop.Run();

    cookie_settings_->ShutdownOnUIThread();
    host_content_settings_map_->ShutdownOnUIThread();
    tracking_protection_settings_->Shutdown();

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
        std::make_unique<TestAnnotator>(), std::move(mock_calculator_results),
        kCalculatorDelay);
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
  std::unique_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
  std::unique_ptr<privacy_sandbox::PrivacySandboxSettings>
      privacy_sandbox_settings_;

  std::unique_ptr<history::HistoryService> history_service_;

  std::unique_ptr<TesterBrowsingTopicsService> browsing_topics_service_;

  base::HistogramTester histogram_tester_;

  std::unique_ptr<privacy_sandbox::ScopedPrivacySandboxAttestations>
      scoped_attestations_;
};

TEST_F(BrowsingTopicsServiceImplTest, EmptyInitialState_CalculationScheduling) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(1), {}},
                                                         {Topic(2), {}},
                                                         {Topic(3), {}},
                                                         {Topic(4), {}},
                                                         {Topic(5), {}}},
                                                        kTime1));
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(6), {}},
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
            start_time + kCalculatorDelay + kEpoch);

  // Advance the time to right before the next scheduled calculation. The next
  // calculation should not happen.
  task_environment()->FastForwardBy(kEpoch - base::Microseconds(1));

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);

  histogram_tester_.ExpectTimeBucketCount(
      "BrowsingTopics.EpochTopicsCalculation.TimeBetweenCalculations",
      kTime2 - kTime1, 0);

  // Advance the time to the scheduled calculation time. A calculation should
  // happen.
  task_environment()->FastForwardBy(base::Microseconds(1));

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 2u);

  // Finish the calculation.
  task_environment()->FastForwardBy(kCalculatorDelay);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_EQ(browsing_topics_state().epochs()[1].calculation_time(), kTime2);
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            start_time + 2 * kCalculatorDelay + 2 * kEpoch);

  histogram_tester_.ExpectTimeBucketCount(
      "BrowsingTopics.EpochTopicsCalculation.TimeBetweenCalculations",
      kTime2 - kTime1, 1);
}

TEST_F(BrowsingTopicsServiceImplTest, WallTimeScheduling) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(1), {}},
                                                         {Topic(2), {}},
                                                         {Topic(3), {}},
                                                         {Topic(4), {}},
                                                         {Topic(5), {}}},
                                                        kTime1));
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(6), {}},
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

  // Advance the time to the scheduled calculation time and simulate system
  // sleep for this period. A calculation should happen.
  task_environment()->SuspendedFastForwardBy(kEpoch);

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 2u);
}

TEST_F(BrowsingTopicsServiceImplTest,
       StartFromPreexistingState_ScheduleEpochsExpiration) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{blink::features::kBrowsingTopics, {}},
       {blink::features::kBrowsingTopicsParameters,
        {{"epoch_retention_duration",
          base::StrCat(
              {base::NumberToString(28 * kOneTestDay.InSeconds()), "s"})}}}},
      /*disabled_features=*/{});

  base::Time start_time = base::Time::Now();

  std::vector<EpochTopics> preexisting_epochs;
  preexisting_epochs.push_back(
      CreateTestEpochTopics({{Topic(1), {}},
                             {Topic(2), {}},
                             {Topic(3), {}},
                             {Topic(4), {}},
                             {Topic(5), {}}},
                            start_time - 29 * kOneTestDay));

  preexisting_epochs.push_back(
      CreateTestEpochTopics({{Topic(1), {}},
                             {Topic(2), {}},
                             {Topic(3), {}},
                             {Topic(4), {}},
                             {Topic(5), {}}},
                            start_time - 27 * kOneTestDay));

  CreateBrowsingTopicsStateFile(
      std::move(preexisting_epochs),
      /*next_scheduled_calculation_time=*/start_time + 2 * kOneTestDay);

  InitializeBrowsingTopicsService(/*mock_calculator_results=*/{});

  // Finish file loading.
  task_environment()->RunUntilIdle();

  // Verify that the first epoch (29 days old) has expired, leaving only one
  // epoch.
  EXPECT_EQ(browsing_topics_state().epochs().size(), 1u);
  EXPECT_EQ(browsing_topics_state().epochs()[0].calculation_time(),
            start_time - 27 * kOneTestDay);

  // Fast-forward time by one day and verify the second epoch also expires.
  task_environment()->FastForwardBy(kOneTestDay);
  EXPECT_EQ(browsing_topics_state().epochs().size(), 0u);
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
      /*next_scheduled_calculation_time=*/start_time + kOneTestDay);

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(6), {}},
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
            start_time + kOneTestDay);

  histogram_tester_.ExpectTimeBucketCount(
      "BrowsingTopics.EpochTopicsCalculation.TimeBetweenCalculations",
      kTime2 - kTime1, 0);

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 0u);

  // Advance the time to the scheduled calculation time. A calculation should
  // happen.
  task_environment()->FastForwardBy(kOneTestDay);

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);
  // Finish the calculation.
  task_environment()->FastForwardBy(kCalculatorDelay);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_EQ(browsing_topics_state().epochs()[1].calculation_time(), kTime2);
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            start_time + kOneTestDay + kCalculatorDelay + kEpoch);

  histogram_tester_.ExpectTimeBucketCount(
      "BrowsingTopics.EpochTopicsCalculation.TimeBetweenCalculations",
      kTime2 - kTime1, 1);
}

TEST_F(BrowsingTopicsServiceImplTest,
       StartFromPreexistingState_CalculateAtScheduledTime_FailedCalculation) {
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
      /*next_scheduled_calculation_time=*/start_time + kOneTestDay);

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(EpochTopics(
      kTime2, CalculatorResultStatus::kFailureAnnotationExecutionError));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(browsing_topics_state().epochs().size(), 1u);
  EXPECT_EQ(browsing_topics_state().epochs()[0].calculation_time(), kTime1);
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            start_time + kOneTestDay);

  histogram_tester_.ExpectTimeBucketCount(
      "BrowsingTopics.EpochTopicsCalculation.TimeBetweenCalculations",
      kTime2 - kTime1, 0);

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 0u);

  // Advance the time to the scheduled calculation time. A calculation should
  // happen.
  task_environment()->FastForwardBy(kOneTestDay);

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);
  // Finish the calculation.
  task_environment()->FastForwardBy(kCalculatorDelay);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_EQ(browsing_topics_state().epochs()[1].calculation_time(), kTime2);
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            start_time + kOneTestDay + kCalculatorDelay + kEpoch);

  histogram_tester_.ExpectTimeBucketCount(
      "BrowsingTopics.EpochTopicsCalculation.TimeBetweenCalculations",
      kTime2 - kTime1, 1);
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
      /*next_scheduled_calculation_time=*/start_time - base::Microseconds(1));

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(6), {}},
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
            start_time - base::Microseconds(1));

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);

  histogram_tester_.ExpectTimeBucketCount(
      "BrowsingTopics.EpochTopicsCalculation.TimeBetweenCalculations",
      kTime2 - kTime1, 0);
}

TEST_F(
    BrowsingTopicsServiceImplTest,
    StartFromPreexistingState_TopicsAccessibleSinceUpdated_ResetStateAndStorage_CalculateAtScheduledTime) {
  base::Time start_time = base::Time::Now();

  std::vector<EpochTopics> preexisting_epochs;
  preexisting_epochs.push_back(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     start_time - kOneTestDay));

  // Add some arbitrary data to site data storage. The intent is just to test
  // data deletion.
  topics_site_data_manager()->OnBrowsingTopicsApiUsed(
      HashMainFrameHostForStorage("a.com"), HashedDomain(1), "a.com",
      base::Time::Now());

  task_environment()->FastForwardBy(base::Microseconds(1));
  privacy_sandbox_settings_->OnCookiesCleared();

  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      1u);

  CreateBrowsingTopicsStateFile(
      std::move(preexisting_epochs),
      /*next_scheduled_calculation_time=*/start_time + kOneTestDay);

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time - kOneTestDay));
  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading.
  task_environment()->RunUntilIdle();

  EXPECT_EQ(browsing_topics_state().epochs().size(), 0u);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 0u);
  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      0u);

  histogram_tester_.ExpectTimeBucketCount(
      "BrowsingTopics.EpochTopicsCalculation.TimeBetweenCalculations",
      kTime2 - kTime1, 0);
}

TEST_F(
    BrowsingTopicsServiceImplTest,
    StartFromPreexistingState_UnexpectedNextCalculationDelay_ResetState_CalculateImmediately) {
  base::Time start_time = base::Time::Now();

  std::vector<EpochTopics> preexisting_epochs;
  preexisting_epochs.push_back(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     start_time - kOneTestDay));

  privacy_sandbox_settings_->OnCookiesCleared();

  CreateBrowsingTopicsStateFile(
      std::move(preexisting_epochs),
      /*next_scheduled_calculation_time=*/start_time + 15 * kOneTestDay);

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(6), {}},
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

  histogram_tester_.ExpectTimeBucketCount(
      "BrowsingTopics.EpochTopicsCalculation.TimeBetweenCalculations",
      kTime2 - kTime1, 0);
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
      /*next_scheduled_calculation_time=*/start_time + kOneTestDay);

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(6), {}},
                                                         {Topic(7), {}},
                                                         {Topic(8), {}},
                                                         {Topic(9), {}},
                                                         {Topic(10), {}}},
                                                        kTime2));
  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  NavigateToPage(GURL("https://www.foo.com"));

  {
    std::vector<blink::mojom::EpochTopicPtr> result;
    EXPECT_FALSE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, result));
    EXPECT_TRUE(result.empty());
    histogram_tester_.ExpectBucketCount(
        "BrowsingTopics.Result.Status",
        browsing_topics::ApiAccessResult::kStateNotReady, 1);
  }

  std::vector<ApiResultUkmMetrics> metrics_entries =
      ReadApiResultUkmMetrics(ukm_recorder);
  EXPECT_EQ(1u, metrics_entries.size());

  EXPECT_EQ(metrics_entries[0].failure_reason, ApiAccessResult::kStateNotReady);
  EXPECT_FALSE(metrics_entries[0].topic0.IsValid());
  EXPECT_FALSE(metrics_entries[0].topic1.IsValid());
  EXPECT_FALSE(metrics_entries[0].topic2.IsValid());

  EXPECT_TRUE(browsing_topics_service_->GetTopTopicsForDisplay().empty());

  base::test::TestFuture<mojom::WebUIGetBrowsingTopicsStateResultPtr> future1;
  browsing_topics_service_->GetBrowsingTopicsStateForWebUi(
      /*calculate_now=*/false, future1.GetCallback());
  EXPECT_TRUE(future1.IsReady());
  EXPECT_EQ(future1.Take()->get_override_status_message(),
            "State loading hasn't finished. Please retry shortly.");

  // Finish file loading.
  task_environment()->RunUntilIdle();

  {
    std::vector<blink::mojom::EpochTopicPtr> result;
    EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, result));
    EXPECT_FALSE(result.empty());
  }

  EXPECT_FALSE(browsing_topics_service_->GetTopTopicsForDisplay().empty());

  base::test::TestFuture<mojom::WebUIGetBrowsingTopicsStateResultPtr> future2;
  browsing_topics_service_->GetBrowsingTopicsStateForWebUi(
      /*calculate_now=*/false, future2.GetCallback());
  task_environment()->FastForwardBy(kDatabaseFetchDelay);
  EXPECT_TRUE(future2.IsReady());
  EXPECT_FALSE(future2.Take()->is_override_status_message());
}

TEST_F(BrowsingTopicsServiceImplTest,
       StartFromPreexistingState_DataDeletionOnCalculation) {
  base::Time start_time = base::Time::Now();
  topics_site_data_manager()->OnBrowsingTopicsApiUsed(
      HashMainFrameHostForStorage("a.com"), HashedDomain(1), "a.com",
      kTime1 -
          blink::features::
                  kBrowsingTopicsNumberOfEpochsOfObservationDataToUseForFiltering
                      .Get() *
              blink::features::kBrowsingTopicsTimePeriodPerEpoch.Get() -
          kCalculatorDelay);

  topics_site_data_manager()->OnBrowsingTopicsApiUsed(
      HashMainFrameHostForStorage("b.com"), HashedDomain(2), "b.com", kTime2);

  topics_site_data_manager()->OnBrowsingTopicsApiUsed(
      HashMainFrameHostForStorage("c.com"), HashedDomain(3), "c.com",
      kTime2 + kCalculatorDelay);

  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      3u);

  std::vector<EpochTopics> preexisting_epochs;
  preexisting_epochs.push_back(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     kTime1));

  preexisting_epochs.push_back(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     kTime2));
  preexisting_epochs.push_back(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     kTime2));

  CreateBrowsingTopicsStateFile(
      std::move(preexisting_epochs),
      /*next_scheduled_calculation_time=*/start_time + kOneTestDay);

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {}},
                             {Topic(2), {}},
                             {Topic(3), {}},
                             {Topic(4), {}},
                             {Topic(5), {}}},
                            start_time + kCalculatorDelay));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time + 2 * kCalculatorDelay));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 0u);

  // Finish file loading.
  task_environment()->RunUntilIdle();

  EXPECT_EQ(browsing_topics_state().epochs().size(), 3u);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 0u);

  // Advance the time to the scheduled calculation time. A calculation should
  // happen.
  task_environment()->FastForwardBy(kOneTestDay);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);
  task_environment()->FastForwardBy(kCalculatorDelay);
  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  // No data is deleted because we didn't remove any epochs with this
  // calculation.
  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      3u);
  std::map<HashedDomain, std::string> expected_context_domains(
      {{HashedDomain(1), "a.com"},
       {HashedDomain(2), "b.com"},
       {HashedDomain(3), "c.com"}});
  std::map<HashedDomain, std::string> context_domains_result =
      content::GetContextDomainsFromHashedContextDomains(
          topics_site_data_manager(),
          {HashedDomain(1), HashedDomain(2), HashedDomain(3)});
  EXPECT_EQ(context_domains_result, expected_context_domains);

  // Advance the time to the next scheduled calculation time. A calculation
  // should happen. One of the existing epochs will be replaced with a new one.
  task_environment()->FastForwardBy(kEpoch);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 2u);
  task_environment()->FastForwardBy(kCalculatorDelay);
  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  // Data prior to kTime1 - (number of epochs * epoch length) is deleted.
  std::vector<ApiUsageContext> browsing_topics_api_usage =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());
  EXPECT_EQ(browsing_topics_api_usage.size(), 2u);
  EXPECT_EQ(browsing_topics_api_usage[0].time, kTime2);
  EXPECT_EQ(browsing_topics_api_usage[1].time, kTime2 + kCalculatorDelay);
  expected_context_domains = {{HashedDomain(2), "b.com"},
                              {HashedDomain(3), "c.com"}};
  context_domains_result = content::GetContextDomainsFromHashedContextDomains(
      topics_site_data_manager(),
      {HashedDomain(1), HashedDomain(2), HashedDomain(3)});
  EXPECT_EQ(context_domains_result, expected_context_domains);
}

TEST_F(
    BrowsingTopicsServiceImplTest,
    OnTopicsDataAccessibleSinceUpdated_ResetState_ClearTopicsSiteDataStorage) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(1), {}},
                                                         {Topic(2), {}},
                                                         {Topic(3), {}},
                                                         {Topic(4), {}},
                                                         {Topic(5), {}}},
                                                        start_time));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time + kCalculatorDelay + kEpoch));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading and two calculations.
  task_environment()->FastForwardBy(2 * kCalculatorDelay + kEpoch);

  // Add some arbitrary data to site data storage. The intent is just to test
  // data deletion.
  topics_site_data_manager()->OnBrowsingTopicsApiUsed(
      HashMainFrameHostForStorage("a.com"), HashedDomain(1), "a.com",
      base::Time::Now());

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      1u);

  task_environment()->FastForwardBy(base::Microseconds(1));
  privacy_sandbox_settings_->OnCookiesCleared();

  EXPECT_EQ(browsing_topics_state().epochs().size(), 0u);
  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      0u);
}

TEST_F(BrowsingTopicsServiceImplTest, TimeoutRetry_Success) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      EpochTopics(kTime1, CalculatorResultStatus::kHangingAfterModelRequested));
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(6), {}},
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

  // Epochs were not updated, because the first calculation timed out. A retry
  // was scheduled and `next_scheduled_calculation_time` was updated.
  EXPECT_TRUE(browsing_topics_state().epochs().empty());
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            base::Time::Now() + kFirstTimeoutRetryDelay);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);

  // Forward the time right before the timeout retry.
  task_environment()->FastForwardBy(kFirstTimeoutRetryDelay -
                                    base::Microseconds(1));
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);

  // Forward the time to the timeout retry.
  task_environment()->FastForwardBy(base::Microseconds(1));
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 2u);

  // Finish the timeout retry. An epoch was added. The next calculation is
  // scheduled one epoch after.
  task_environment()->FastForwardBy(kCalculatorDelay);

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 2u);
  EXPECT_EQ(browsing_topics_state().epochs()[0].calculation_time(), kTime2);
  EXPECT_EQ(browsing_topics_state().epochs()[0].calculator_result_status(),
            CalculatorResultStatus::kSuccess);
  EXPECT_EQ(
      browsing_topics_state().next_scheduled_calculation_time(),
      start_time + 2 * kCalculatorDelay + kFirstTimeoutRetryDelay + kEpoch);
}

TEST_F(BrowsingTopicsServiceImplTest, TimeoutRetry_TimeoutAgain) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      EpochTopics(kTime1, CalculatorResultStatus::kHangingAfterModelRequested));
  mock_calculator_results.emplace(EpochTopics(
      kTime2, CalculatorResultStatus::kHangingAfterAnnotationRequested));
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(6), {}},
                                                         {Topic(7), {}},
                                                         {Topic(8), {}},
                                                         {Topic(9), {}},
                                                         {Topic(10), {}}},
                                                        kTime3));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 0u);

  // Finish file loading.
  task_environment()->RunUntilIdle();

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);
  EXPECT_TRUE(browsing_topics_state().epochs().empty());

  // Finish the calculation.
  task_environment()->FastForwardBy(kCalculatorDelay);

  // Epochs were not updated, because the first calculation timed out. A retry
  // was scheduled and `next_scheduled_calculation_time` was updated.
  EXPECT_TRUE(browsing_topics_state().epochs().empty());
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            base::Time::Now() + kFirstTimeoutRetryDelay);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);

  // Forward the time to the timeout retry.
  task_environment()->FastForwardBy(kFirstTimeoutRetryDelay);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 2u);

  // Finish the timeout retry.
  task_environment()->FastForwardBy(kCalculatorDelay);

  // Epochs were still not updated, because the calculation timed out again. A
  // retry was scheduled and `next_scheduled_calculation_time` was updated.
  EXPECT_TRUE(browsing_topics_state().epochs().empty());
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            base::Time::Now() + kFirstTimeoutRetryDelay * 2);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 2u);

  // Forward the time right before the second timeout retry.
  task_environment()->FastForwardBy(kFirstTimeoutRetryDelay * 2 -
                                    base::Microseconds(1));
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 2u);

  // Forward the time to the second timeout retry.
  task_environment()->FastForwardBy(base::Microseconds(1));
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 3u);

  // Finish the calculation. An epoch was added. The next calculation is
  // scheduled one epoch after.
  task_environment()->FastForwardBy(kCalculatorDelay);

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 3u);
  EXPECT_EQ(browsing_topics_state().epochs()[0].calculation_time(), kTime3);
  EXPECT_EQ(browsing_topics_state().epochs()[0].calculator_result_status(),
            CalculatorResultStatus::kSuccess);
  EXPECT_EQ(
      browsing_topics_state().next_scheduled_calculation_time(),
      start_time + 3 * kCalculatorDelay + kFirstTimeoutRetryDelay * 3 + kEpoch);
}

TEST_F(BrowsingTopicsServiceImplTest, TimeoutRetry_SuccessiveTimeout) {
  base::queue<EpochTopics> mock_calculator_results;
  for (int i = 0; i < 7; ++i) {
    mock_calculator_results.emplace(EpochTopics(
        kTime1, CalculatorResultStatus::kHangingAfterModelRequested));
  }

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading.
  task_environment()->RunUntilIdle();

  base::TimeDelta total_duration_after_max_exp_backoff;
  for (int i = 0; i < 5; ++i) {
    total_duration_after_max_exp_backoff +=
        kCalculatorDelay + kFirstTimeoutRetryDelay * (1 << i);
  }

  // Verify that a calculation occurs at the expected time, and verify
  // `started_calculations_count`.
  task_environment()->FastForwardBy(total_duration_after_max_exp_backoff -
                                    base::Microseconds(1));
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 5u);
  task_environment()->FastForwardBy(base::Microseconds(1));
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 6u);

  // Finish the calculation.
  task_environment()->FastForwardBy(kCalculatorDelay);

  // Verify that the next calculation occurs with kEpoch backoff delay.
  task_environment()->FastForwardBy(kEpoch - base::Microseconds(1));
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 6u);
  task_environment()->FastForwardBy(base::Microseconds(1));
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 7u);
}

TEST_F(BrowsingTopicsServiceImplTest,
       TimeoutRetry_InterruptedByHistoryDeletion) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      EpochTopics(kTime1, CalculatorResultStatus::kHangingAfterModelRequested));
  mock_calculator_results.emplace(EpochTopics(
      kTime2, CalculatorResultStatus::kHangingAfterAnnotationRequested));
  mock_calculator_results.emplace(EpochTopics(
      kTime3, CalculatorResultStatus::kHangingAfterHistoryRequested));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 0u);

  // Finish file loading.
  task_environment()->RunUntilIdle();

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);
  EXPECT_TRUE(browsing_topics_state().epochs().empty());

  // Finish the calculation.
  task_environment()->FastForwardBy(kCalculatorDelay);

  // Epochs were not updated, because the first calculation timed out. A retry
  // was scheduled and `next_scheduled_calculation_time` was updated.
  EXPECT_TRUE(browsing_topics_state().epochs().empty());
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            base::Time::Now() + kFirstTimeoutRetryDelay);

  // Forward the time to the timeout retry.
  task_environment()->FastForwardBy(kFirstTimeoutRetryDelay);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 2u);

  // Before the calculation finish, delete history. This should trigger a topics
  // re-calculation.
  task_environment()->FastForwardBy(kCalculatorDelay - base::Microseconds(1));

  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(start_time, start_time + 2 * kOneTestDay),
      /*is_from_expiration=*/false, /*deleted_rows=*/{}, /*favicon_urls=*/{},
      /*restrict_urls=*/std::nullopt);
  browsing_topics_service_->OnHistoryDeletions(history_service_.get(),
                                               deletion_info);

  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 3u);

  // The calculation shouldn't finish at the originally expected time, as it was
  // dropped and a new calculation has started.
  task_environment()->FastForwardBy(base::Microseconds(1));
  EXPECT_EQ(browsing_topics_state().epochs().size(), 0u);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 3u);

  // Finish the re-started calculation. Epochs were still not updated, because
  // the calculation timed out again. A retry was scheduled and
  // `next_scheduled_calculation_time` was updated.
  task_environment()->FastForwardBy(kCalculatorDelay - base::Microseconds(1));
  EXPECT_TRUE(browsing_topics_state().epochs().empty());
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            base::Time::Now() + kFirstTimeoutRetryDelay * 2);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 3u);

  // Forward the time by `kFirstTimeoutRetryDelay`. No new calculation has
  // started. This shows that the second retry had a delay longer than
  // `kFirstTimeoutRetryDelay`, which suggests that re-started calculation was
  // also considered as a timeout retry.
  task_environment()->FastForwardBy(kFirstTimeoutRetryDelay);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 3u);
}

TEST_F(BrowsingTopicsServiceImplTest,
       OnURLsDeleted_TimeRangeOverlapWithOneEpoch) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(1), {}},
                                                         {Topic(2), {}},
                                                         {Topic(3), {}},
                                                         {Topic(4), {}},
                                                         {Topic(5), {}}},
                                                        start_time));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time + kCalculatorDelay + kEpoch));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading and two calculations.
  task_environment()->FastForwardBy(2 * kCalculatorDelay + kEpoch);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_FALSE(browsing_topics_state().epochs()[0].empty());
  EXPECT_FALSE(browsing_topics_state().epochs()[1].empty());

  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(start_time + 5 * kOneTestDay,
                                 start_time + 6 * kOneTestDay),
      /*is_from_expiration=*/false, /*deleted_rows=*/{}, /*favicon_urls=*/{},
      /*restrict_urls=*/std::nullopt);

  browsing_topics_service_->OnHistoryDeletions(history_service_.get(),
                                               deletion_info);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_FALSE(browsing_topics_state().epochs()[0].empty());
  EXPECT_TRUE(browsing_topics_state().epochs()[1].empty());
}

TEST_F(BrowsingTopicsServiceImplTest,
       OnURLsDeleted_TimeRangeOverlapWithAllEpochs) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(1), {}},
                                                         {Topic(2), {}},
                                                         {Topic(3), {}},
                                                         {Topic(4), {}},
                                                         {Topic(5), {}}},
                                                        start_time));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time + kCalculatorDelay + kEpoch));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading and two calculations.
  task_environment()->FastForwardBy(2 * kCalculatorDelay + kEpoch);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_FALSE(browsing_topics_state().epochs()[0].empty());
  EXPECT_FALSE(browsing_topics_state().epochs()[1].empty());

  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(start_time, start_time + 2 * kOneTestDay),
      /*is_from_expiration=*/false, /*deleted_rows=*/{}, /*favicon_urls=*/{},
      /*restrict_urls=*/std::nullopt);

  browsing_topics_service_->OnHistoryDeletions(history_service_.get(),
                                               deletion_info);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);
  EXPECT_TRUE(browsing_topics_state().epochs()[0].empty());
  EXPECT_TRUE(browsing_topics_state().epochs()[1].empty());
}

TEST_F(BrowsingTopicsServiceImplTest, Recalculate) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(1), {}},
                                                         {Topic(2), {}},
                                                         {Topic(3), {}},
                                                         {Topic(4), {}},
                                                         {Topic(5), {}}},
                                                        kTime1));
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(6), {}},
                                                         {Topic(7), {}},
                                                         {Topic(8), {}},
                                                         {Topic(9), {}},
                                                         {Topic(10), {}}},
                                                        kTime2));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->FastForwardBy(kCalculatorDelay - base::Microseconds(1));

  EXPECT_EQ(browsing_topics_state().epochs().size(), 0u);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 1u);

  // History deletion during a calculation should trigger the re-calculation.
  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(start_time, start_time + 2 * kOneTestDay),
      /*is_from_expiration=*/false, /*deleted_rows=*/{}, /*favicon_urls=*/{},
      /*restrict_urls=*/std::nullopt);
  browsing_topics_service_->OnHistoryDeletions(history_service_.get(),
                                               deletion_info);

  // The calculation shouldn't finish at the originally expected time, as it was
  // dropped and a new calculation has started.
  task_environment()->FastForwardBy(base::Microseconds(1));

  EXPECT_EQ(browsing_topics_state().epochs().size(), 0u);
  EXPECT_EQ(browsing_topics_service_->started_calculations_count(), 2u);

  // Finish the re-started calculation.
  task_environment()->FastForwardBy(kCalculatorDelay - base::Microseconds(1));
  EXPECT_EQ(browsing_topics_state().epochs().size(), 1u);

  // Expect that the result comes from the re-started calculator.
  EXPECT_EQ(browsing_topics_state().epochs()[0].calculation_time(), kTime2);
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            base::Time::Now() + kEpoch);
}

TEST_F(BrowsingTopicsServiceImplTest,
       HandleTopicsWebApi_PrivacySandboxSettingsDisabled) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->RunUntilIdle();

  privacy_sandbox_settings_->SetTopicsBlockedForTesting();

  NavigateToPage(GURL("https://www.foo.com"));

  std::vector<blink::mojom::EpochTopicPtr> result;
  EXPECT_FALSE(browsing_topics_service_->HandleTopicsWebApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
      /*get_topics=*/true,
      /*observe=*/true, result));
  EXPECT_TRUE(result.empty());

  histogram_tester_.ExpectBucketCount(
      "BrowsingTopics.Result.Status",
      browsing_topics::ApiAccessResult::kAccessDisallowedBySettings, 1);

  std::vector<ApiResultUkmMetrics> metrics_entries =
      ReadApiResultUkmMetrics(ukm_recorder);
  EXPECT_EQ(1u, metrics_entries.size());

  EXPECT_EQ(metrics_entries[0].failure_reason,
            ApiAccessResult::kAccessDisallowedBySettings);
  EXPECT_FALSE(metrics_entries[0].topic0.IsValid());
  EXPECT_FALSE(metrics_entries[0].topic1.IsValid());
  EXPECT_FALSE(metrics_entries[0].topic2.IsValid());
}

TEST_F(BrowsingTopicsServiceImplTest, HandleTopicsWebApi_OneEpoch) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->FastForwardBy(kCalculatorDelay);

  NavigateToPage(GURL("https://www.foo.com"));

  {
    // Current time is before the epoch switch time.
    std::vector<blink::mojom::EpochTopicPtr> result;
    EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, result));
    EXPECT_TRUE(result.empty());
  }

  histogram_tester_.ExpectBucketCount(
      "BrowsingTopics.Result.Status",
      browsing_topics::ApiAccessResult::kSuccess, 1);
  histogram_tester_.ExpectBucketCount("BrowsingTopics.Result.RealTopicCount", 0,
                                      1);
  histogram_tester_.ExpectBucketCount(
      "BrowsingTopics.Result.FilteredTopicCount", 0, 1);
  histogram_tester_.ExpectBucketCount("BrowsingTopics.Result.FakeTopicCount", 1,
                                      0);

  std::vector<ApiResultUkmMetrics> metrics_entries =
      ReadApiResultUkmMetrics(ukm_recorder);
  EXPECT_EQ(1u, metrics_entries.size());

  // This is an empty event with no metrics.
  EXPECT_FALSE(metrics_entries[0].failure_reason);
  EXPECT_FALSE(metrics_entries[0].topic0.IsValid());
  EXPECT_FALSE(metrics_entries[0].topic1.IsValid());
  EXPECT_FALSE(metrics_entries[0].topic2.IsValid());

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(kMaxEpochIntroductionDelay);

  {
    std::vector<blink::mojom::EpochTopicPtr> result;
    EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, result));

    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0]->topic, 2);
    EXPECT_EQ(result[0]->config_version, "chrome.1");
    EXPECT_EQ(result[0]->taxonomy_version, "1");
    EXPECT_EQ(result[0]->model_version, "5000000000");
    EXPECT_EQ(result[0]->version, "chrome.1:1:5000000000");
  }
}

TEST_F(BrowsingTopicsServiceImplTest,
       HandleTopicsWebApi_EpochConfigVersionDifferentFromCurrent) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->FastForwardBy(kCalculatorDelay);

  NavigateToPage(GURL("https://www.foo.com"));

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(kMaxEpochIntroductionDelay);

  // Switch to use a non-default prioritized_topics_list, so that the current
  // configuration version is different from that derived at the epoch topics
  // calculation time.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{blink::features::kBrowsingTopics, {}},
       {blink::features::kBrowsingTopicsParameters,
        {{"time_period_per_epoch",
          base::StrCat({base::NumberToString(kEpoch.InSeconds()), "s"})},
         {"max_epoch_introduction_delay",
          base::StrCat(
              {base::NumberToString(kMaxEpochIntroductionDelay.InSeconds()),
               "s"})},
         {"epoch_retention_duration", "3650000d"},
         {"prioritized_topics_list", "1,57"}}}},
      /*disabled_features=*/{});

  std::vector<blink::mojom::EpochTopicPtr> result;
  EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
      /*get_topics=*/true,
      /*observe=*/true, result));

  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0]->topic, 2);
  EXPECT_EQ(result[0]->config_version, "chrome.1");
  EXPECT_EQ(result[0]->taxonomy_version, "1");
  EXPECT_EQ(result[0]->model_version, "5000000000");
  EXPECT_EQ(result[0]->version, "chrome.1:1:5000000000");
}

TEST_F(BrowsingTopicsServiceImplTest,
       HandleTopicsWebApi_TwoEpochsWithDifferentConfigVersions) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/5, kModelVersion,
                            /*config_version=*/2));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(2 * kCalculatorDelay + kEpoch);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 2u);

  NavigateToPage(GURL("https://www.foo.com"));

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(kMaxEpochIntroductionDelay);

  std::vector<blink::mojom::EpochTopicPtr> result;
  EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
      /*get_topics=*/true,
      /*observe=*/true, result));

  EXPECT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0]->config_version, "chrome.1");
  EXPECT_EQ(result[0]->topic, 7);
  EXPECT_EQ(result[1]->config_version, "chrome.2");
  EXPECT_EQ(result[1]->topic, 2);
}

TEST_F(BrowsingTopicsServiceImplTest, HandleTopicsWebApi_OneEpoch_Filtered) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(1), {}},
                                                         {Topic(2), {}},
                                                         {Topic(3), {}},
                                                         {Topic(4), {}},
                                                         {Topic(5), {}}},
                                                        kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->FastForwardBy(kCalculatorDelay);

  NavigateToPage(GURL("https://www.foo.com"));

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(kMaxEpochIntroductionDelay);

  std::vector<blink::mojom::EpochTopicPtr> result;
  EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
      /*get_topics=*/true,
      /*observe=*/true, result));
  EXPECT_TRUE(result.empty());

  histogram_tester_.ExpectBucketCount(
      "BrowsingTopics.Result.Status",
      browsing_topics::ApiAccessResult::kSuccess, 1);
  histogram_tester_.ExpectBucketCount("BrowsingTopics.Result.RealTopicCount", 0,
                                      1);
  histogram_tester_.ExpectBucketCount(
      "BrowsingTopics.Result.FilteredTopicCount", 1, 1);
  histogram_tester_.ExpectBucketCount("BrowsingTopics.Result.FakeTopicCount", 1,
                                      0);

  std::vector<ApiResultUkmMetrics> metrics_entries =
      ReadApiResultUkmMetrics(ukm_recorder);
  EXPECT_EQ(1u, metrics_entries.size());

  EXPECT_FALSE(metrics_entries[0].failure_reason);
  EXPECT_TRUE(metrics_entries[0].topic0.IsValid());

  EXPECT_EQ(metrics_entries[0].topic0.topic(), Topic(2));
  EXPECT_TRUE(metrics_entries[0].topic0.is_true_topic());
  EXPECT_TRUE(metrics_entries[0].topic0.should_be_filtered());
  EXPECT_EQ(metrics_entries[0].topic0.taxonomy_version(), 1);
  EXPECT_EQ(metrics_entries[0].topic0.model_version(), 5000000000LL);

  EXPECT_FALSE(metrics_entries[0].topic1.IsValid());
  EXPECT_FALSE(metrics_entries[0].topic2.IsValid());
}

TEST_F(BrowsingTopicsServiceImplTest,
       HandleTopicsWebApi_TopicNotAllowedByPrivacySandboxSettings) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->FastForwardBy(kCalculatorDelay);

  // This domain would let a random topic (Topic(130)) be returned.
  NavigateToPage(GURL("https://www.foo81.com"));

  // Current time is before the epoch switch time.
  {
    std::vector<blink::mojom::EpochTopicPtr> result;
    EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, result));
    EXPECT_TRUE(result.empty());
  }

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(kMaxEpochIntroductionDelay);

  {
    std::vector<blink::mojom::EpochTopicPtr> result;
    EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, result));

    // Some topics are returned.
    EXPECT_FALSE(result.empty());
  }

  privacy_sandbox_settings_->SetTopicAllowed(
      privacy_sandbox::CanonicalTopic(Topic(130), /*taxonomy_version=*/1),
      false);

  {
    std::vector<blink::mojom::EpochTopicPtr> result;
    EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, result));

    // The topic was blocked by the settings.
    EXPECT_TRUE(result.empty());
  }
}

TEST_F(BrowsingTopicsServiceImplTest, HandleTopicsWebApi_FourEpochs) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(11), {GetHashedDomain("bar.com")}},
                             {Topic(12), {GetHashedDomain("bar.com")}},
                             {Topic(13), {GetHashedDomain("bar.com")}},
                             {Topic(14), {GetHashedDomain("bar.com")}},
                             {Topic(15), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(16), {GetHashedDomain("bar.com")}},
                             {Topic(17), {GetHashedDomain("bar.com")}},
                             {Topic(18), {GetHashedDomain("bar.com")}},
                             {Topic(19), {GetHashedDomain("bar.com")}},
                             {Topic(20), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(4 * kCalculatorDelay + 3 * kEpoch);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  NavigateToPage(GURL("https://www.foo.com"));

  // Current time is before the epoch switch time.
  {
    std::vector<blink::mojom::EpochTopicPtr> result;
    EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, result));

    EXPECT_EQ(result.size(), 3u);
    std::set<int> result_set;
    result_set.insert(result[0]->topic);
    result_set.insert(result[1]->topic);
    result_set.insert(result[2]->topic);
    EXPECT_EQ(result_set, std::set<int>({2, 7, 12}));
  }

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(kMaxEpochIntroductionDelay);

  {
    std::vector<blink::mojom::EpochTopicPtr> result;
    EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, result));

    EXPECT_EQ(result.size(), 3u);
    std::set<int> result_set;
    result_set.insert(result[0]->topic);
    result_set.insert(result[1]->topic);
    result_set.insert(result[2]->topic);
    EXPECT_EQ(result_set, std::set<int>({7, 12, 17}));
  }
}

TEST_F(BrowsingTopicsServiceImplTest,
       HandleTopicsWebApi_DuplicateTopicsRemoved) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(4 * kCalculatorDelay + 3 * kEpoch);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  NavigateToPage(GURL("https://www.foo.com"));

  // Current time is before the epoch switch time.
  {
    std::vector<blink::mojom::EpochTopicPtr> result;
    EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, result));

    EXPECT_EQ(result.size(), 2u);
    std::set<int> result_set;
    result_set.insert(result[0]->topic);
    result_set.insert(result[1]->topic);
    EXPECT_EQ(result_set, std::set<int>({2, 7}));
  }

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(kMaxEpochIntroductionDelay);

  {
    std::vector<blink::mojom::EpochTopicPtr> result;
    EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, result));

    EXPECT_EQ(result.size(), 2u);
    std::set<int> result_set;
    result_set.insert(result[0]->topic);
    result_set.insert(result[1]->topic);
    EXPECT_EQ(result_set, std::set<int>({2, 7}));
  }

  // Ensure access has been reported to the Page Specific Content Settings.
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      web_contents()->GetPrimaryPage());
  EXPECT_TRUE(pscs->HasAccessedTopics());
  auto topics = pscs->GetAccessedTopics();
  EXPECT_EQ(2u, topics.size());

  // PSCS::GetAccessedTopics() will return sorted values.
  EXPECT_EQ(topics[0].topic_id(), Topic(2));
  EXPECT_EQ(topics[1].topic_id(), Topic(7));
}

TEST_F(BrowsingTopicsServiceImplTest,
       HandleTopicsWebApi_TopicsReturnedInSortedOrder) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(4 * kCalculatorDelay + 3 * kEpoch);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  NavigateToPage(GURL("https://www.foo.com"));

  // Current time is before the epoch switch time.

  std::vector<blink::mojom::EpochTopicPtr> result;
  EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
      /*get_topics=*/true,
      /*observe=*/true, result));

  EXPECT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0]->topic, 2);
  EXPECT_EQ(result[1]->topic, 7);
}

TEST_F(BrowsingTopicsServiceImplTest,
       HandleTopicsWebApi_TopicsReturnedInSortedOrder_DifferentVersions) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/5,
                            /*model_version=*/4));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/5,
                            /*model_version=*/3));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/5,
                            /*model_version=*/2));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/5,
                            /*model_version=*/1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(4 * kCalculatorDelay + 3 * kEpoch);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  NavigateToPage(GURL("https://www.foo.com"));

  // Current time is before the epoch switch time.

  std::vector<blink::mojom::EpochTopicPtr> result;
  EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
      /*get_topics=*/true,
      /*observe=*/true, result));

  EXPECT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0]->topic, 7);
  EXPECT_EQ(result[0]->version, "chrome.1:1:2");
  EXPECT_EQ(result[1]->topic, 2);
  EXPECT_EQ(result[1]->version, "chrome.1:1:3");
  EXPECT_EQ(result[2]->topic, 7);
  EXPECT_EQ(result[2]->version, "chrome.1:1:4");
}

TEST_F(BrowsingTopicsServiceImplTest, NumVersionsInEpochs_OneVerison) {
  base::queue<EpochTopics> mock_calculator_results;

  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(6), {}},
                                                         {Topic(7), {}},
                                                         {Topic(8), {}},
                                                         {Topic(9), {}},
                                                         {Topic(10), {}}},
                                                        kTime1));
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(6), {}},
                                                         {Topic(7), {}},
                                                         {Topic(8), {}},
                                                         {Topic(9), {}},
                                                         {Topic(10), {}}},
                                                        kTime1));
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(6), {}},
                                                         {Topic(7), {}},
                                                         {Topic(8), {}},
                                                         {Topic(9), {}},
                                                         {Topic(10), {}}},
                                                        kTime1));
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(6), {}},
                                                         {Topic(7), {}},
                                                         {Topic(8), {}},
                                                         {Topic(9), {}},
                                                         {Topic(10), {}}},
                                                        kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(4 * kCalculatorDelay + 3 * kEpoch);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  NavigateToPage(GURL("https://www.foo.com"));

  EXPECT_EQ(
      browsing_topics_service_->NumVersionsInEpochs(
          web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin()),
      1);
}

TEST_F(BrowsingTopicsServiceImplTest,
       NumVersionsInEpochs_ThreeVerisons_ClearedTopics) {
  base::queue<EpochTopics> mock_calculator_results;

  EpochTopics epoch_version1 =
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/5,
                            /*model_version=*/1);
  EpochTopics epoch_version2 =
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/5,
                            /*model_version=*/2);
  EpochTopics epoch_version3 =
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/5,
                            /*model_version=*/3);
  EpochTopics epoch_version4 =
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/5,
                            /*model_version=*/4);

  epoch_version1.ClearTopics();
  epoch_version2.ClearTopics();
  epoch_version3.ClearTopics();
  epoch_version4.ClearTopics();

  mock_calculator_results.emplace(std::move(epoch_version1));
  mock_calculator_results.emplace(std::move(epoch_version2));
  mock_calculator_results.emplace(std::move(epoch_version3));
  mock_calculator_results.emplace(std::move(epoch_version4));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(4 * kCalculatorDelay + 3 * kEpoch);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  NavigateToPage(GURL("https://www.foo.com"));

  EXPECT_EQ(
      browsing_topics_service_->NumVersionsInEpochs(
          web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin()),
      3);
}

TEST_F(BrowsingTopicsServiceImplTest, HandleTopicsWebApi_TrackedUsageContext) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Advance to the time after the epoch switch time.
  task_environment()->FastForwardBy(kCalculatorDelay +
                                    kMaxEpochIntroductionDelay);

  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      0u);

  NavigateToPage(GURL("https://www.foo.com"));

  std::vector<blink::mojom::EpochTopicPtr> result;
  EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
      /*get_topics=*/true,
      /*observe=*/true, result));
  EXPECT_EQ(result.size(), 1u);

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("www.foo.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain,
            GetHashedDomain("bar.com"));
}

TEST_F(BrowsingTopicsServiceImplTest, HandleTopicsWebApi_DoesNotObserve) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Advance to the time after the epoch switch time.
  task_environment()->FastForwardBy(kCalculatorDelay +
                                    kMaxEpochIntroductionDelay);

  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      0u);

  NavigateToPage(GURL("https://www.foo.com"));

  std::vector<blink::mojom::EpochTopicPtr> result;
  EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
      /*get_topics=*/true,
      /*observe=*/false, result));
  EXPECT_EQ(result.size(), 1u);

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());
  EXPECT_TRUE(api_usage_contexts.empty());
}

TEST_F(BrowsingTopicsServiceImplTest, HandleTopicsWebApi_DoesNotGetTopics) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Advance to the time after the epoch switch time.
  task_environment()->FastForwardBy(kCalculatorDelay +
                                    kMaxEpochIntroductionDelay);

  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      0u);

  NavigateToPage(GURL("https://www.foo.com"));

  std::vector<blink::mojom::EpochTopicPtr> result;
  EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kFetch,
      /*get_topics=*/false,
      /*observe=*/true, result));
  EXPECT_TRUE(result.empty());

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("www.foo.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain,
            GetHashedDomain("bar.com"));
}

TEST_F(
    BrowsingTopicsServiceImplTest,
    HandleTopicsWebApi_DoesNotGetTopics_SettingsDisabled_NoApiResultUkmEvent) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1));
  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Advance to the time after the epoch switch time.
  task_environment()->FastForwardBy(kCalculatorDelay +
                                    kMaxEpochIntroductionDelay);

  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      0u);

  NavigateToPage(GURL("https://www.foo.com"));

  privacy_sandbox_settings_->SetTopicsBlockedForTesting();

  std::vector<blink::mojom::EpochTopicPtr> result;
  EXPECT_FALSE(browsing_topics_service_->HandleTopicsWebApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kFetch,
      /*get_topics=*/false,
      /*observe=*/true, result));
  EXPECT_TRUE(result.empty());

  std::vector<ApiResultUkmMetrics> metrics_entries =
      ReadApiResultUkmMetrics(ukm_recorder);
  EXPECT_TRUE(metrics_entries.empty());
}

TEST_F(BrowsingTopicsServiceImplTest, ApiResultUkm_ZeroAndOneTopic) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
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

  std::vector<ApiResultUkmMetrics> metrics_entries =
      ReadApiResultUkmMetrics(ukm_recorder);
  EXPECT_EQ(0u, metrics_entries.size());

  // Current time is before the epoch switch time. Expect one ukm event without
  // any metrics.
  {
    std::vector<blink::mojom::EpochTopicPtr> result;
    EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, result));
  }

  metrics_entries = ReadApiResultUkmMetrics(ukm_recorder);
  EXPECT_EQ(1u, metrics_entries.size());

  EXPECT_FALSE(metrics_entries[0].failure_reason);
  EXPECT_FALSE(metrics_entries[0].topic0.IsValid());
  EXPECT_FALSE(metrics_entries[0].topic1.IsValid());
  EXPECT_FALSE(metrics_entries[0].topic2.IsValid());

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(kMaxEpochIntroductionDelay);

  {
    std::vector<blink::mojom::EpochTopicPtr> result;
    EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, result));
  }

  metrics_entries = ReadApiResultUkmMetrics(ukm_recorder);
  EXPECT_EQ(2u, metrics_entries.size());

  EXPECT_FALSE(metrics_entries[1].failure_reason);
  EXPECT_TRUE(metrics_entries[1].topic0.IsValid());
  EXPECT_EQ(metrics_entries[1].topic0.topic(), Topic(2));
  EXPECT_TRUE(metrics_entries[1].topic0.is_true_topic());
  EXPECT_FALSE(metrics_entries[1].topic0.should_be_filtered());
  EXPECT_EQ(metrics_entries[1].topic0.taxonomy_version(), 1);
  EXPECT_EQ(metrics_entries[1].topic0.model_version(), 5000000000LL);
  EXPECT_FALSE(metrics_entries[1].topic1.IsValid());
  EXPECT_FALSE(metrics_entries[1].topic2.IsValid());
}

TEST_F(BrowsingTopicsServiceImplTest, ApiResultUkm_3Topics) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/0));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1, /*padded_top_topics_start_index=*/0));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1, /*padded_top_topics_start_index=*/0));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(4 * kCalculatorDelay + 3 * kEpoch);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 4u);

  NavigateToPage(GURL("https://www.foo.com"));

  // Advance to the time after the epoch switch time.
  task_environment()->AdvanceClock(kMaxEpochIntroductionDelay);

  std::vector<blink::mojom::EpochTopicPtr> api_call_result;
  EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
      /*get_topics=*/true,
      /*observe=*/true, api_call_result));

  // The API returns 2 topics, but all 3 candidate topics are logged.
  EXPECT_EQ(2u, api_call_result.size());

  std::vector<ApiResultUkmMetrics> metrics_entries =
      ReadApiResultUkmMetrics(ukm_recorder);
  EXPECT_EQ(1u, metrics_entries.size());

  EXPECT_FALSE(metrics_entries[0].failure_reason);
  EXPECT_TRUE(metrics_entries[0].topic0.IsValid());
  EXPECT_EQ(metrics_entries[0].topic0.topic(), Topic(7));
  EXPECT_FALSE(metrics_entries[0].topic0.is_true_topic());
  EXPECT_FALSE(metrics_entries[0].topic0.should_be_filtered());
  EXPECT_EQ(metrics_entries[0].topic0.taxonomy_version(), 1);
  EXPECT_EQ(metrics_entries[0].topic0.model_version(), 5000000000LL);

  EXPECT_TRUE(metrics_entries[0].topic1.IsValid());
  EXPECT_EQ(metrics_entries[0].topic1.topic(), Topic(2));
  EXPECT_FALSE(metrics_entries[0].topic1.is_true_topic());
  EXPECT_FALSE(metrics_entries[0].topic1.should_be_filtered());
  EXPECT_EQ(metrics_entries[0].topic1.taxonomy_version(), 1);
  EXPECT_EQ(metrics_entries[0].topic1.model_version(), 5000000000LL);

  EXPECT_TRUE(metrics_entries[0].topic2.IsValid());
  EXPECT_EQ(metrics_entries[0].topic2.topic(), Topic(7));
  EXPECT_TRUE(metrics_entries[0].topic2.is_true_topic());
  EXPECT_FALSE(metrics_entries[0].topic2.should_be_filtered());
  EXPECT_EQ(metrics_entries[0].topic2.taxonomy_version(), 1);
  EXPECT_EQ(metrics_entries[0].topic2.model_version(), 5000000000LL);

  histogram_tester_.ExpectBucketCount(
      "BrowsingTopics.Result.Status",
      browsing_topics::ApiAccessResult::kSuccess, 1);
  histogram_tester_.ExpectBucketCount("BrowsingTopics.Result.RealTopicCount", 1,
                                      1);
  histogram_tester_.ExpectBucketCount(
      "BrowsingTopics.Result.FilteredTopicCount", 0, 1);
  histogram_tester_.ExpectBucketCount("BrowsingTopics.Result.FakeTopicCount", 2,
                                      1);
}

TEST_F(BrowsingTopicsServiceImplTest, GetTopTopicsForDisplay) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/1));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("bar.com")}},
                             {Topic(2), {GetHashedDomain("bar.com")}},
                             {Topic(3), {GetHashedDomain("bar.com")}},
                             {Topic(4), {GetHashedDomain("bar.com")}},
                             {Topic(5), {GetHashedDomain("bar.com")}}},
                            kTime1,
                            /*padded_top_topics_start_index=*/2));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("bar.com")}},
                             {Topic(7), {GetHashedDomain("bar.com")}},
                             {Topic(8), {GetHashedDomain("bar.com")}},
                             {Topic(9), {GetHashedDomain("bar.com")}},
                             {Topic(10), {GetHashedDomain("bar.com")}}},
                            kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish all calculations.
  task_environment()->FastForwardBy(4 * kCalculatorDelay + 3 * kEpoch);

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
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(1), {}},
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
            start_time + kCalculatorDelay + kEpoch);
}

TEST_F(BrowsingTopicsServiceImplTest,
       GetBrowsingTopicsStateForWebUi_CalculationNow) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(1), {}},
                                                         {Topic(2), {}},
                                                         {Topic(3), {}},
                                                         {Topic(4), {}},
                                                         {Topic(5), {}}},
                                                        start_time));

  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {}},
                             {Topic(2), {}},
                             {Topic(3), {}},
                             {Topic(4), {}},
                             {Topic(5), {}}},
                            start_time + kCalculatorDelay + kOneTestDay));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  task_environment()->FastForwardBy(kCalculatorDelay);

  EXPECT_EQ(browsing_topics_state().epochs().size(), 1u);
  EXPECT_EQ(browsing_topics_state().next_scheduled_calculation_time(),
            start_time + kCalculatorDelay + kEpoch);

  // Advance by some time smaller than the periodic update interval.
  task_environment()->FastForwardBy(kOneTestDay);

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

  // The `next_scheduled_calculation_time` is reset to an epoch after.
  EXPECT_EQ(webui_state->next_scheduled_calculation_time,
            start_time + 2 * kCalculatorDelay + kOneTestDay + kEpoch);
}

TEST_F(BrowsingTopicsServiceImplTest, GetBrowsingTopicsStateForWebUi) {
  base::Time start_time = base::Time::Now();

  // Add a database entry for HashedDomain(456) so that we can check if the
  // unhashed domain is displayed if available.
  topics_site_data_manager()->OnBrowsingTopicsApiUsed(
      HashedHost(123), HashedDomain(456), "456.com", base::Time::Now());

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {HashedDomain(123), HashedDomain(456)}},
                             {Topic(2), {}},
                             {Topic(0), {}},  // blocked
                             {Topic(4), {}},
                             {Topic(5), {}}},
                            start_time));

  // Failed calculation.
  mock_calculator_results.emplace(
      EpochTopics(start_time + kEpoch,
                  CalculatorResultStatus::kFailureAnnotationExecutionError));

  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time + 2 * kEpoch,
                            /*padded_top_topics_start_index=*/2));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading and three calculations.
  task_environment()->FastForwardBy(3 * kCalculatorDelay + 2 * kEpoch);

  base::test::TestFuture<mojom::WebUIGetBrowsingTopicsStateResultPtr> future;
  browsing_topics_service_->GetBrowsingTopicsStateForWebUi(
      /*calculate_now=*/false, future.GetCallback());
  task_environment()->FastForwardBy(kDatabaseFetchDelay);
  EXPECT_TRUE(future.IsReady());

  mojom::WebUIGetBrowsingTopicsStateResultPtr result = future.Take();
  mojom::WebUIBrowsingTopicsStatePtr& webui_state =
      result->get_browsing_topics_state();

  EXPECT_EQ(webui_state->epochs.size(), 3u);
  EXPECT_EQ(webui_state->next_scheduled_calculation_time,
            start_time + 3 * kCalculatorDelay + 3 * kEpoch);

  const mojom::WebUIEpochPtr& epoch0 = webui_state->epochs[0];
  const mojom::WebUIEpochPtr& epoch1 = webui_state->epochs[1];
  const mojom::WebUIEpochPtr& epoch2 = webui_state->epochs[2];

  EXPECT_EQ(epoch0->calculation_time, start_time + 2 * kEpoch);
  EXPECT_EQ(epoch0->model_version, "5000000000");
  EXPECT_EQ(epoch0->taxonomy_version, "1");
  EXPECT_EQ(epoch0->topics.size(), 5u);
  EXPECT_EQ(epoch0->topics[0]->topic_id, 6);
  EXPECT_EQ(epoch0->topics[0]->topic_name, u"Entertainment Industry");
  EXPECT_TRUE(epoch0->topics[0]->is_real_topic);
  EXPECT_TRUE(epoch0->topics[0]->observed_by_domains.empty());
  EXPECT_EQ(epoch0->topics[1]->topic_id, 7);
  EXPECT_EQ(epoch0->topics[1]->topic_name, u"Humor");
  EXPECT_TRUE(epoch0->topics[1]->is_real_topic);
  EXPECT_TRUE(epoch0->topics[1]->observed_by_domains.empty());
  EXPECT_EQ(epoch0->topics[2]->topic_id, 8);
  EXPECT_EQ(epoch0->topics[2]->topic_name, u"Live Comedy");
  EXPECT_FALSE(epoch0->topics[2]->is_real_topic);
  EXPECT_TRUE(epoch0->topics[2]->observed_by_domains.empty());
  EXPECT_EQ(epoch0->topics[3]->topic_id, 9);
  EXPECT_EQ(epoch0->topics[3]->topic_name, u"Live Sporting Events");
  EXPECT_FALSE(epoch0->topics[3]->is_real_topic);
  EXPECT_TRUE(epoch0->topics[3]->observed_by_domains.empty());
  EXPECT_EQ(epoch0->topics[4]->topic_id, 10);
  EXPECT_EQ(epoch0->topics[4]->topic_name, u"Magic");
  EXPECT_FALSE(epoch0->topics[4]->is_real_topic);
  EXPECT_TRUE(epoch0->topics[4]->observed_by_domains.empty());

  EXPECT_EQ(epoch1->calculation_time, start_time + kEpoch);
  EXPECT_EQ(epoch1->model_version, "0");
  EXPECT_EQ(epoch1->taxonomy_version, "0");
  EXPECT_EQ(epoch1->topics.size(), 0u);

  EXPECT_EQ(epoch2->calculation_time, start_time);
  EXPECT_EQ(epoch2->model_version, "5000000000");
  EXPECT_EQ(epoch2->taxonomy_version, "1");
  EXPECT_EQ(epoch2->topics.size(), 5u);
  EXPECT_EQ(epoch2->topics[0]->topic_id, 1);
  EXPECT_EQ(epoch2->topics[0]->topic_name, u"Arts & Entertainment");
  EXPECT_TRUE(epoch2->topics[0]->is_real_topic);
  EXPECT_EQ(epoch2->topics[0]->observed_by_domains.size(), 2u);
  // The unhashed domain for 123 is unavailable, so "123" is used.
  EXPECT_EQ(epoch2->topics[0]->observed_by_domains[0], "123");
  // "456.com" is stored in the call to OnBrowsingTopicsApiUsed above.
  EXPECT_EQ(epoch2->topics[0]->observed_by_domains[1], "456.com");
  EXPECT_EQ(epoch2->topics[1]->topic_id, 2);
  EXPECT_EQ(epoch2->topics[1]->topic_name, u"Acting & Theater");
  EXPECT_TRUE(epoch2->topics[1]->is_real_topic);
  EXPECT_TRUE(epoch2->topics[1]->observed_by_domains.empty());
  EXPECT_EQ(epoch2->topics[2]->topic_id, 0);
  EXPECT_EQ(epoch2->topics[2]->topic_name, u"Unknown");
  EXPECT_TRUE(epoch2->topics[2]->is_real_topic);
  EXPECT_TRUE(epoch2->topics[2]->observed_by_domains.empty());
  EXPECT_EQ(epoch2->topics[3]->topic_id, 4);
  EXPECT_EQ(epoch2->topics[3]->topic_name, u"Concerts & Music Festivals");
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
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(1), {}},
                                                         {Topic(2), {}},
                                                         {Topic(3), {}},
                                                         {Topic(4), {}},
                                                         {Topic(5), {}}},
                                                        start_time));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time + kCalculatorDelay + kEpoch));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading and two calculations.
  task_environment()->FastForwardBy(2 * kCalculatorDelay + kEpoch);

  // Clearing topic 7 should clear child topic 8 as well.
  browsing_topics_service_->ClearTopic(
      privacy_sandbox::CanonicalTopic(Topic(7), /*taxonomy_version=*/1));
  std::vector<privacy_sandbox::CanonicalTopic> result =
      browsing_topics_service_->GetTopTopicsForDisplay();

  EXPECT_EQ(result.size(), 8u);
  EXPECT_EQ(result[0].topic_id(), Topic(1));
  EXPECT_EQ(result[1].topic_id(), Topic(2));
  EXPECT_EQ(result[2].topic_id(), Topic(3));
  EXPECT_EQ(result[3].topic_id(), Topic(4));
  EXPECT_EQ(result[4].topic_id(), Topic(5));
  EXPECT_EQ(result[5].topic_id(), Topic(6));
  EXPECT_EQ(result[6].topic_id(), Topic(9));
  EXPECT_EQ(result[7].topic_id(), Topic(10));
}

TEST_F(BrowsingTopicsServiceImplTest, BlockTopicWithFinch) {
  base::Time start_time = base::Time::Now();

  std::vector<EpochTopics> preexisting_epochs;
  preexisting_epochs.push_back(CreateTestEpochTopics({{Topic(6), {}},
                                                      {Topic(7), {}},
                                                      {Topic(8), {}},
                                                      {Topic(9), {}},
                                                      {Topic(10), {}}},
                                                     start_time - kOneTestDay));

  CreateBrowsingTopicsStateFile(
      std::move(preexisting_epochs),
      /*next_scheduled_calculation_time=*/start_time + kOneTestDay);

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(1), {}},
                                                         {Topic(2), {}},
                                                         {Topic(3), {}},
                                                         {Topic(4), {}},
                                                         {Topic(5), {}}},
                                                        kTime2));

  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{blink::features::kBrowsingTopics, {}},
       {blink::features::kBrowsingTopicsParameters,
        {{"time_period_per_epoch",
          base::StrCat({base::NumberToString(kEpoch.InSeconds()), "s"})},
         {"max_epoch_introduction_delay",
          base::StrCat(
              {base::NumberToString(kMaxEpochIntroductionDelay.InSeconds()),
               "s"})},
         {"epoch_retention_duration", "3650000d"},
         {"disabled_topics_list", "20,10,7"}}}},
      /*disabled_features=*/{});
  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading.
  task_environment()->FastForwardBy(2 * kCalculatorDelay + kEpoch);

  std::vector<privacy_sandbox::CanonicalTopic> result =
      browsing_topics_service_->GetTopTopicsForDisplay();

  // Don't receive 7 and 10 (they are blocked by Finch) and 8 (it's blocked
  // because it's descended from 7).
  EXPECT_EQ(result.size(), 7u);
  EXPECT_EQ(result[0].topic_id(), Topic(6));
  EXPECT_EQ(result[1].topic_id(), Topic(9));
  EXPECT_EQ(result[2].topic_id(), Topic(1));
  EXPECT_EQ(result[3].topic_id(), Topic(2));
  EXPECT_EQ(result[4].topic_id(), Topic(3));
  EXPECT_EQ(result[5].topic_id(), Topic(4));
  EXPECT_EQ(result[6].topic_id(), Topic(5));
}

TEST_F(BrowsingTopicsServiceImplTest, ClearTopicBeforeLoadFinish) {
  base::Time start_time = base::Time::Now();

  std::vector<EpochTopics> preexisting_epochs;
  preexisting_epochs.push_back(CreateTestEpochTopics({{Topic(1), {}},
                                                      {Topic(2), {}},
                                                      {Topic(3), {}},
                                                      {Topic(4), {}},
                                                      {Topic(5), {}}},
                                                     start_time - kOneTestDay));

  CreateBrowsingTopicsStateFile(
      std::move(preexisting_epochs),
      /*next_scheduled_calculation_time=*/start_time + kOneTestDay);

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(6), {}},
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

  // If a topic in the settings is cleared before load finish,
  // that topic and its descendants will be cleared after load finish.
  std::vector<privacy_sandbox::CanonicalTopic> result =
      browsing_topics_service_->GetTopTopicsForDisplay();
  EXPECT_EQ(result.size(), 4u);
  EXPECT_EQ(result[0].topic_id(), Topic(1));
  EXPECT_EQ(result[1].topic_id(), Topic(2));
  EXPECT_EQ(result[2].topic_id(), Topic(4));
  EXPECT_EQ(result[3].topic_id(), Topic(5));
}

TEST_F(BrowsingTopicsServiceImplTest, ClearAllTopicsData) {
  base::Time start_time = base::Time::Now();

  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(1), {}},
                                                         {Topic(2), {}},
                                                         {Topic(3), {}},
                                                         {Topic(4), {}},
                                                         {Topic(5), {}}},
                                                        start_time));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {}},
                             {Topic(7), {}},
                             {Topic(8), {}},
                             {Topic(9), {}},
                             {Topic(10), {}}},
                            start_time + kCalculatorDelay + kEpoch));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading and two calculations.
  task_environment()->FastForwardBy(2 * kCalculatorDelay + kEpoch);

  // Add some arbitrary data to site data storage. The intent is just to test
  // data deletion.
  topics_site_data_manager()->OnBrowsingTopicsApiUsed(
      HashMainFrameHostForStorage("a.com"), HashedDomain(1), "a1.com",
      base::Time::Now());
  topics_site_data_manager()->OnBrowsingTopicsApiUsed(
      HashMainFrameHostForStorage("a.com"), HashedDomain(2), "a2.com",
      base::Time::Now());

  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      2u);

  task_environment()->FastForwardBy(base::Microseconds(1));

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
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(1), {GetHashedDomain("b.com")}},
                             {Topic(2), {GetHashedDomain("b.com")}},
                             {Topic(3), {GetHashedDomain("b.com")}},
                             {Topic(4), {GetHashedDomain("b.com")}},
                             {Topic(5), {GetHashedDomain("b.com")}}},
                            kTime1));
  mock_calculator_results.emplace(
      CreateTestEpochTopics({{Topic(6), {GetHashedDomain("b.com")}},
                             {Topic(7), {GetHashedDomain("b.com")}},
                             {Topic(8), {GetHashedDomain("b.com")}},
                             {Topic(9), {GetHashedDomain("b.com")}},
                             {Topic(10), {GetHashedDomain("b.com")}}},
                            kTime1 + kCalculatorDelay + kEpoch));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  NavigateToPage(GURL("https://a.com"));

  // Finish file loading and two calculations.
  task_environment()->FastForwardBy(2 * kCalculatorDelay + kEpoch);

  {
    std::vector<blink::mojom::EpochTopicPtr> api_call_result;
    EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.b.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, api_call_result));

    EXPECT_EQ(api_call_result.size(), 1u);
    EXPECT_EQ(api_call_result[0]->topic, 3);
  }

  // Add some arbitrary data to site data storage. The intent is just to test
  // data deletion.
  topics_site_data_manager()->OnBrowsingTopicsApiUsed(
      HashMainFrameHostForStorage("d.com"), GetHashedDomain("c.com"), "c.com",
      base::Time::Now());
  topics_site_data_manager()->OnBrowsingTopicsApiUsed(
      HashMainFrameHostForStorage("d.com"), GetHashedDomain("b.com"), "b.com, ",
      base::Time::Now());

  // The data is from both the manual `OnBrowsingTopicsApiUsed` call and the
  // usage tracking due to the previous `HandleTopicsWebApi`.
  EXPECT_EQ(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).size(),
      3u);

  task_environment()->FastForwardBy(base::Microseconds(1));

  browsing_topics_service_->ClearTopicsDataForOrigin(
      url::Origin::Create(GURL("https://b.com")));

  task_environment()->RunUntilIdle();

  // Note that this won't trigger another usage storing to the database, because
  // the same context domain was seen in the page before.
  {
    std::vector<blink::mojom::EpochTopicPtr> api_call_result;
    EXPECT_TRUE(browsing_topics_service_->HandleTopicsWebApi(
        /*context_origin=*/url::Origin::Create(GURL("https://www.b.com")),
        web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
        /*get_topics=*/true,
        /*observe=*/true, api_call_result));

    // Since the domain "b.com" is removed. The candidate topic won't be
    // returned.
    EXPECT_TRUE(api_call_result.empty());
  }

  std::vector<browsing_topics::ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());

  // The context domain "b.com" should have been cleared.
  EXPECT_EQ(api_usage_contexts.size(), 1u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("d.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain,
            GetHashedDomain("c.com"));
}

TEST_F(BrowsingTopicsServiceImplTest, MethodsFailGracefullyAfterShutdown) {
  base::queue<EpochTopics> mock_calculator_results;
  mock_calculator_results.emplace(CreateTestEpochTopics({{Topic(1), {}},
                                                         {Topic(2), {}},
                                                         {Topic(3), {}},
                                                         {Topic(4), {}},
                                                         {Topic(5), {}}},
                                                        kTime1));

  InitializeBrowsingTopicsService(std::move(mock_calculator_results));

  // Finish file loading.
  task_environment()->RunUntilIdle();

  browsing_topics_service_->Shutdown();

  std::vector<blink::mojom::EpochTopicPtr> result;
  EXPECT_FALSE(browsing_topics_service_->HandleTopicsWebApi(
      /*context_origin=*/url::Origin::Create(GURL("https://www.bar.com")),
      web_contents()->GetPrimaryMainFrame(), ApiCallerSource::kJavaScript,
      /*get_topics=*/true,
      /*observe=*/true, result));
  EXPECT_TRUE(result.empty());

  base::test::TestFuture<mojom::WebUIGetBrowsingTopicsStateResultPtr> future1;
  browsing_topics_service_->GetBrowsingTopicsStateForWebUi(
      /*calculate_now=*/false, future1.GetCallback());
  EXPECT_TRUE(future1.IsReady());
  EXPECT_EQ(future1.Take()->get_override_status_message(),
            "BrowsingTopicsService is shutting down.");

  EXPECT_TRUE(browsing_topics_service_->GetTopTopicsForDisplay().empty());

  browsing_topics_service_->ClearTopic(
      privacy_sandbox::CanonicalTopic(Topic(7), /*taxonomy_version=*/1));

  browsing_topics_service_->ClearTopicsDataForOrigin(
      url::Origin::Create(GURL("https://b.com")));

  browsing_topics_service_->ClearAllTopicsData();
}

}  // namespace browsing_topics
