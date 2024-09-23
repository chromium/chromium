// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/browsing_topics/browsing_topics_calculator.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/browsing_topics/test_util.h"
#include "components/browsing_topics/util.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browsing_topics_test_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

namespace {

constexpr int kTaxonomyVersion = 2;

constexpr char kHost1[] = "www.foo1.com";
constexpr char kHost2[] = "www.foo2.com";
constexpr char kHost3[] = "www.foo3.com";
constexpr char kHost4[] = "www.foo4.com";
constexpr char kHost5[] = "www.foo5.com";
constexpr char kHost6[] = "www.foo6.com";

Topic ExpectedRandomTopic(size_t index) {
  Topic kExpectedRandomTopicsForTaxonomyV1[5] = {
      Topic(101), Topic(102), Topic(103), Topic(104), Topic(105)};
  Topic kExpectedRandomTopicsForTaxonomyV2[5] = {
      Topic(176), Topic(177), Topic(180), Topic(183), Topic(184)};

  if (blink::features::kBrowsingTopicsTaxonomyVersion.Get() == 1) {
    return kExpectedRandomTopicsForTaxonomyV1[index];
  }

  if (blink::features::kBrowsingTopicsTaxonomyVersion.Get() == 2) {
    return kExpectedRandomTopicsForTaxonomyV2[index];
  }

  NOTREACHED();
}

class TestHistoryService : public history::HistoryService {
 public:
  void SetQueryResultDelay(base::TimeDelta query_result_delay) {
    query_result_delay_ = query_result_delay;
  }

  base::CancelableTaskTracker::TaskId QueryHistory(
      const std::u16string& text_query,
      const history::QueryOptions& options,
      QueryHistoryCallback callback,
      base::CancelableTaskTracker* tracker) override {
    auto run_callback_after_delay =
        base::BindLambdaForTesting([callback = std::move(callback), this](
                                       history::QueryResults results) mutable {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
              FROM_HERE,
              base::BindLambdaForTesting(
                  [callback = std::move(callback),
                   results = std::move(results)]() mutable {
                    std::move(callback).Run(std::move(results));
                  }),
              query_result_delay_);
        });

    return history::HistoryService::QueryHistory(
        text_query, options, std::move(run_callback_after_delay), tracker);
  }

 private:
  base::TimeDelta query_result_delay_;
};

}  // namespace

class BrowsingTopicsCalculatorTest : public testing::Test {
 public:
  BrowsingTopicsCalculatorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
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

    topics_site_data_manager_ =
        std::make_unique<content::TesterBrowsingTopicsSiteDataManager>(
            temp_dir_.GetPath());

    history_service_ = std::make_unique<TestHistoryService>();
    history_service_->Init(
        history::TestHistoryDatabaseParamsForPath(temp_dir_.GetPath()));

    task_environment_.RunUntilIdle();
  }

  ~BrowsingTopicsCalculatorTest() override {
    cookie_settings_->ShutdownOnUIThread();
    host_content_settings_map_->ShutdownOnUIThread();
    tracking_protection_settings_->Shutdown();
  }

  EpochTopics CalculateTopics(base::circular_deque<EpochTopics> epochs = {},
                              base::Time session_start_time = base::Time(),
                              int previous_timeout_count = 0) {
    EpochTopics result = EpochTopics(base::Time());

    base::RunLoop run_loop;

    TesterBrowsingTopicsCalculator topics_calculator =
        TesterBrowsingTopicsCalculator(
            privacy_sandbox_settings_.get(), history_service_.get(),
            topics_site_data_manager_.get(), &test_annotator_,
            previous_timeout_count, session_start_time, epochs,
            base::BindLambdaForTesting([&](EpochTopics epoch_topics) {
              result = std::move(epoch_topics);
              run_loop.Quit();
            }),
            /*rand_uint64_queue=*/
            base::queue<uint64_t>{{100, 101, 102, 103, 104}});

    run_loop.Run();

    return result;
  }

  TesterBrowsingTopicsCalculator CreateCalculator(
      BrowsingTopicsCalculator::CalculateCompletedCallback callback) {
    return TesterBrowsingTopicsCalculator(
        privacy_sandbox_settings_.get(), history_service_.get(),
        topics_site_data_manager_.get(), &test_annotator_,
        /*previous_timeout_count=*/0, /*session_start_time=*/base::Time(),
        /*epochs=*/base::circular_deque<EpochTopics>(), std::move(callback),
        /*rand_uint64_queue=*/
        base::queue<uint64_t>{{100, 101, 102, 103, 104}});
  }

  void AddHistoryEntries(const std::vector<std::string>& hosts,
                         base::Time time) {
    history::HistoryAddPageArgs add_page_args;
    add_page_args.time = time;
    add_page_args.context_id = 1;

    for (const std::string& host : hosts) {
      static int nav_entry_id = 0;
      ++nav_entry_id;

      add_page_args.url = GURL(base::StrCat({"https://", host}));
      add_page_args.nav_entry_id = nav_entry_id;

      history_service_->AddPage(add_page_args);
      history_service_->SetBrowsingTopicsAllowed(
          add_page_args.context_id, nav_entry_id, add_page_args.url);
    }

    task_environment_.RunUntilIdle();
  }

  void AddApiUsageContextEntries(
      const std::vector<std::pair<std::string, std::set<HashedDomain>>>&
          main_frame_hosts_with_context_domains) {
    for (auto& [main_frame_host, context_domains] :
         main_frame_hosts_with_context_domains) {
      for (const HashedDomain& context_domain : context_domains) {
        topics_site_data_manager_->OnBrowsingTopicsApiUsed(
            HashMainFrameHostForStorage(main_frame_host), context_domain,
            base::NumberToString(context_domain.value()), base::Time::Now());
      }
    }

    task_environment_.RunUntilIdle();
  }

  void ExpectResultTopicsEqual(
      const std::vector<TopicAndDomains>& result,
      std::vector<std::pair<Topic, std::set<HashedDomain>>> expected) {
    DCHECK_EQ(expected.size(), 5u);
    EXPECT_EQ(result.size(), 5u);

    for (int i = 0; i < 5; ++i) {
      EXPECT_EQ(result[i].topic(), expected[i].first);
      EXPECT_EQ(result[i].hashed_domains(), expected[i].second);
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  std::unique_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
  std::unique_ptr<privacy_sandbox::PrivacySandboxSettings>
      privacy_sandbox_settings_;
  TestAnnotator test_annotator_;

  std::unique_ptr<content::TesterBrowsingTopicsSiteDataManager>
      topics_site_data_manager_;

  std::unique_ptr<TestHistoryService> history_service_;

  base::ScopedTempDir temp_dir_;
};

TEST_F(BrowsingTopicsCalculatorTest, PermissionDenied) {
  base::HistogramTester histograms;

  privacy_sandbox_settings_->SetTopicsBlockedForTesting();

  EpochTopics result = CalculateTopics();
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(result.calculator_result_status(),
            CalculatorResultStatus::kFailurePermissionDenied);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.FirstTry.CalculatorResultStatus",
      CalculatorResultStatus::kFailurePermissionDenied,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsCalculatorTest, ApiUsageContextQueryError) {
  base::HistogramTester histograms;

  topics_site_data_manager_->SetQueryFailureOverride();

  EpochTopics result = CalculateTopics();
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(result.calculator_result_status(),
            CalculatorResultStatus::kFailureApiUsageContextQueryError);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.FirstTry.CalculatorResultStatus",
      CalculatorResultStatus::kFailureApiUsageContextQueryError,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsCalculatorTest, AnnotationExecutionError) {
  base::HistogramTester histograms;

  EpochTopics result = CalculateTopics();
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(result.calculator_result_status(),
            CalculatorResultStatus::kFailureAnnotationExecutionError);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.FirstTry.CalculatorResultStatus",
      CalculatorResultStatus::kFailureAnnotationExecutionError,
      /*expected_bucket_count=*/1);
}

class BrowsingTopicsCalculatorUnsupporedTaxonomyVersionTest
    : public BrowsingTopicsCalculatorTest {
 public:
  BrowsingTopicsCalculatorUnsupporedTaxonomyVersionTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kBrowsingTopicsParameters,
        {{"taxonomy_version", "999"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BrowsingTopicsCalculatorUnsupporedTaxonomyVersionTest,
       TaxonomyVersionNotSupportedInBinary) {
  base::HistogramTester histograms;

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());

  EpochTopics result = CalculateTopics();
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(
      result.calculator_result_status(),
      CalculatorResultStatus::kFailureTaxonomyVersionNotSupportedInBinary);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.FirstTry.CalculatorResultStatus",
      CalculatorResultStatus::kFailureTaxonomyVersionNotSupportedInBinary,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsCalculatorTest, HangingAfterApiUsageRequested) {
  base::HistogramTester histograms;

  topics_site_data_manager_->SetQueryResultDelay(base::Seconds(35));

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());

  base::ElapsedTimer timer;
  EpochTopics result = CalculateTopics();

  // At the hanging detection timeout, the calculation should fail and the
  // hanging metrics should be recorded.
  EXPECT_EQ(timer.Elapsed(), base::Seconds(30));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(result.calculator_result_status(),
            CalculatorResultStatus::kHangingAfterApiUsageRequested);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.FirstTry.CalculatorResultStatus",
      CalculatorResultStatus::kHangingAfterApiUsageRequested,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsCalculatorTest, HangingAfterHistoryRequested) {
  base::HistogramTester histograms;

  history_service_->SetQueryResultDelay(base::Seconds(35));

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());

  base::ElapsedTimer timer;
  EpochTopics result = CalculateTopics();

  // At the hanging detection timeout, the calculation should fail and the
  // hanging metrics should be recorded.
  EXPECT_EQ(timer.Elapsed(), base::Seconds(30));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(result.calculator_result_status(),
            CalculatorResultStatus::kHangingAfterHistoryRequested);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.FirstTry.CalculatorResultStatus",
      CalculatorResultStatus::kHangingAfterHistoryRequested,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsCalculatorTest, HangingAfterModelRequested) {
  base::HistogramTester histograms;

  test_annotator_.SetModelRequestDelay(base::Seconds(35));
  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());

  base::ElapsedTimer timer;
  EpochTopics result = CalculateTopics();

  // At the hanging detection timeout, the calculation should fail and the
  // hanging metrics should be recorded.
  EXPECT_EQ(timer.Elapsed(), base::Seconds(30));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(result.calculator_result_status(),
            CalculatorResultStatus::kHangingAfterModelRequested);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.FirstTry.CalculatorResultStatus",
      CalculatorResultStatus::kHangingAfterModelRequested,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsCalculatorTest, HangingAfterAnnotationRequested) {
  base::HistogramTester histograms;

  // Add some history entries, as otherwise the annotation will be skipped.
  AddHistoryEntries({kHost1, kHost2, kHost3, kHost4, kHost5},
                    base::Time::Now());
  AddApiUsageContextEntries(
      {{kHost1, {}},
       {kHost2, {}},
       {kHost3, {HashedDomain(2)}},
       {kHost4, {HashedDomain(3)}},
       {kHost5, {HashedDomain(1), HashedDomain(2), HashedDomain(3)}}});

  task_environment_.FastForwardBy(base::Seconds(1));

  test_annotator_.SetAnnotationRequestDelay(base::Seconds(35));
  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());

  base::ElapsedTimer timer;
  EpochTopics result = CalculateTopics();

  // At the hanging detection timeout, the calculation should fail and the
  // hanging metrics should be recorded.
  EXPECT_EQ(timer.Elapsed(), base::Seconds(30));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(result.calculator_result_status(),
            CalculatorResultStatus::kHangingAfterAnnotationRequested);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.FirstTry.CalculatorResultStatus",
      CalculatorResultStatus::kHangingAfterAnnotationRequested,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsCalculatorTest,
       SlowHistoryQueryAndHangingAfterModelRequested) {
  base::HistogramTester histograms;

  history_service_->SetQueryResultDelay(base::Seconds(20));

  test_annotator_.SetModelRequestDelay(base::Seconds(35));
  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());

  base::ElapsedTimer timer;
  EpochTopics result = CalculateTopics();

  // The calculation timed out at 50 seconds. This implies that the timeout
  // timer was reset after the history query.
  EXPECT_EQ(timer.Elapsed(), base::Seconds(50));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(result.calculator_result_status(),
            CalculatorResultStatus::kHangingAfterModelRequested);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.FirstTry.CalculatorResultStatus",
      CalculatorResultStatus::kHangingAfterModelRequested,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsCalculatorTest, TimeoutRetrySuccessMetrics) {
  base::HistogramTester histograms;

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());

  base::ElapsedTimer timer;
  EpochTopics result = CalculateTopics(
      /*epochs=*/{},
      /*session_start_time=*/base::Time::Now() - base::Seconds(10),
      /*previous_timeout_count=*/2);

  EXPECT_EQ(timer.Elapsed(), base::Seconds(0));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.calculator_result_status(),
            CalculatorResultStatus::kSuccess);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.Started.RetryNumber",
      /*sample=*/2, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.TimeoutRetry."
      "CalculatorResultStatus",
      CalculatorResultStatus::kSuccess,
      /*expected_bucket_count=*/1);
  histograms.ExpectTotalCount(
      "BrowsingTopics.EpochTopicsCalculation.FirstTry.CalculatorResultStatus",
      /*expected_count=*/0);
  histograms.ExpectTotalCount(
      "BrowsingTopics.EpochTopicsCalculation.Hanging.RetryNumber",
      /*expected_count=*/0);
}

TEST_F(BrowsingTopicsCalculatorTest, TimeoutRetryHangingMetrics) {
  base::HistogramTester histograms;

  test_annotator_.SetModelRequestDelay(base::Seconds(35));
  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());

  base::ElapsedTimer timer;
  EpochTopics result = CalculateTopics(
      /*epochs=*/{},
      /*session_start_time=*/base::Time::Now() - base::Seconds(10),
      /*previous_timeout_count=*/2);

  EXPECT_EQ(timer.Elapsed(), base::Seconds(30));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(result.calculator_result_status(),
            CalculatorResultStatus::kHangingAfterModelRequested);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.Started.RetryNumber",
      /*sample=*/2, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.Hanging.RetryNumber",
      /*sample=*/2, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.TimeoutRetry."
      "CalculatorResultStatus",
      CalculatorResultStatus::kHangingAfterModelRequested,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsCalculatorTest, TerminatedBeforeComplete) {
  base::HistogramTester histograms;

  history_service_->SetQueryResultDelay(base::Seconds(35));

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());

  bool completed = false;

  {
    TesterBrowsingTopicsCalculator calculator =
        CreateCalculator(base::BindLambdaForTesting(
            [&](EpochTopics epoch_topics) { completed = true; }));

    task_environment_.RunUntilIdle();

    EXPECT_FALSE(completed);
  }

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.FirstTry.CalculatorResultStatus",
      CalculatorResultStatus::kTerminated,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsCalculatorTest, TopicsMetadata) {
  base::HistogramTester histograms;
  base::Time begin_time = base::Time::Now();

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());

  EpochTopics result1 = CalculateTopics();
  EXPECT_FALSE(result1.empty());
  EXPECT_EQ(result1.calculator_result_status(),
            CalculatorResultStatus::kSuccess);
  EXPECT_EQ(result1.taxonomy_version(), kTaxonomyVersion);
  EXPECT_EQ(result1.model_version(), 1);
  EXPECT_EQ(result1.calculation_time(), begin_time);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.FirstTry.CalculatorResultStatus",
      CalculatorResultStatus::kSuccess,
      /*expected_bucket_count=*/1);

  task_environment_.AdvanceClock(base::Seconds(2));

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(50).Build());

  EpochTopics result2 = CalculateTopics();
  EXPECT_FALSE(result2.empty());
  EXPECT_EQ(result1.calculator_result_status(),
            CalculatorResultStatus::kSuccess);
  EXPECT_EQ(result2.taxonomy_version(), kTaxonomyVersion);
  EXPECT_EQ(result2.model_version(), 50);
  EXPECT_EQ(result2.calculation_time(), begin_time + base::Seconds(2));

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.FirstTry.CalculatorResultStatus",
      CalculatorResultStatus::kSuccess,
      /*expected_bucket_count=*/2);
}

// Regression test for crbug/1495959.
TEST_F(BrowsingTopicsCalculatorTest, ModelAvailableAfterDelay) {
  test_annotator_.SetModelAvailable(false);

  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost1, kHost2, kHost3, kHost4, kHost5, kHost6},
                    begin_time);

  task_environment_.AdvanceClock(base::Seconds(1));

  // This PostTask will run when the |CalculateTopics| run loop starts and will
  // signal to the calculator that the model is ready, triggering it to start.
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](TestAnnotator* annotator) {
            annotator->UseModelInfo(*optimization_guide::TestModelInfoBuilder()
                                         .SetVersion(1)
                                         .Build());
            annotator->UseAnnotations({
                {kHost1, {1, 2, 3, 4, 5, 6}},
                {kHost2, {2, 3, 4, 5, 6}},
                {kHost3, {3, 4, 5, 6}},
                {kHost4, {4, 5, 6}},
                {kHost5, {5, 6}},
                {kHost6, {6}},
            });
            annotator->SetModelAvailable(true);
          },
          &test_annotator_));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          {{Topic(6), {}},
                           {Topic(5), {}},
                           {Topic(4), {}},
                           {Topic(3), {}},
                           {Topic(2), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 5u);
}

TEST_F(BrowsingTopicsCalculatorTest, TopTopicsRankedByFrequency) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost1, kHost2, kHost3, kHost4, kHost5, kHost6},
                    begin_time);

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5, 6}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          {{Topic(6), {}},
                           {Topic(5), {}},
                           {Topic(4), {}},
                           {Topic(3), {}},
                           {Topic(2), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 5u);
}

TEST_F(BrowsingTopicsCalculatorTest, ModelHasNoTopicsForHost) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost1, kHost2, kHost3, kHost4, kHost5, kHost6},
                    begin_time);

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          {{ExpectedRandomTopic(0), {}},
                           {ExpectedRandomTopic(1), {}},
                           {ExpectedRandomTopic(2), {}},
                           {ExpectedRandomTopic(3), {}},
                           {ExpectedRandomTopic(4), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 0u);
}

TEST_F(BrowsingTopicsCalculatorTest,
       TopTopicsRankedByFrequency_AlsoAffectedByHostsCount) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost1, kHost1, kHost1, kHost1, kHost1, kHost1, kHost2,
                     kHost3, kHost4, kHost5, kHost6},
                    begin_time);

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          {{Topic(2), {}},
                           {Topic(1), {}},
                           {Topic(6), {}},
                           {Topic(5), {}},
                           {Topic(4), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 5u);
}

TEST_F(BrowsingTopicsCalculatorTest, AllTopTopicsRandomlyPadded) {
  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5, 6}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          {{ExpectedRandomTopic(0), {}},
                           {ExpectedRandomTopic(1), {}},
                           {ExpectedRandomTopic(2), {}},
                           {ExpectedRandomTopic(3), {}},
                           {ExpectedRandomTopic(4), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 0u);
}

TEST_F(BrowsingTopicsCalculatorTest, TopTopicsPartiallyPadded) {
  base::HistogramTester histograms;

  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost4, kHost5, kHost6}, begin_time);

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5, 6}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          {{Topic(6), {}},
                           {Topic(5), {}},
                           {Topic(4), {}},
                           {ExpectedRandomTopic(0), {}},
                           {ExpectedRandomTopic(1), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 3u);
}

TEST_F(BrowsingTopicsCalculatorTest, CalculationResultUkm_FailedCalculation) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  privacy_sandbox_settings_->SetTopicsBlockedForTesting();

  CalculateTopics();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::kEntryName);
  EXPECT_EQ(1u, entries.size());

  EXPECT_FALSE(ukm_recorder.GetEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
          kTopTopic0Name));
}

TEST_F(BrowsingTopicsCalculatorTest, CalculationResultUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histograms;

  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost4, kHost5, kHost6}, begin_time);

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5, 6}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  CalculateTopics();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::kEntryName);
  EXPECT_EQ(1u, entries.size());

  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
          kTopTopic0Name,
      6);
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
          kTopTopic1Name,
      5);
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
          kTopTopic2Name,
      4);
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
          kTopTopic3Name,
      ExpectedRandomTopic(0).value());
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
          kTopTopic4Name,
      ExpectedRandomTopic(1).value());
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
          kTaxonomyVersionName,
      kTaxonomyVersion);
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
          kModelVersionName,
      1);
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
          kPaddedTopicsStartIndexName,
      3);
}

TEST_F(BrowsingTopicsCalculatorTest, TopTopicsAndObservingDomains) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost1, kHost2, kHost3, kHost4, kHost5, kHost6},
                    begin_time);

  AddApiUsageContextEntries(
      {{kHost1, {}},
       {kHost2, {}},
       {kHost3, {HashedDomain(2)}},
       {kHost4, {HashedDomain(3)}},
       {kHost5, {HashedDomain(1), HashedDomain(2), HashedDomain(3)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5, 6}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(
      result.top_topics_and_observing_domains(),
      {{Topic(6), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(5), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(4), {HashedDomain(2), HashedDomain(3)}},
       {Topic(3), {HashedDomain(2)}},
       {Topic(2), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 5u);
}

TEST_F(
    BrowsingTopicsCalculatorTest,
    HistoryHostsBefore21DaysAgo_IgnoredForTopTopicsDecision_IgnoredForObservingDomainsDecision) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost1, kHost2, kHost3, kHost4, kHost5, kHost6},
                    begin_time - base::Days(21));

  AddApiUsageContextEntries(
      {{kHost1, {}},
       {kHost2, {}},
       {kHost3, {HashedDomain(2)}},
       {kHost4, {HashedDomain(3)}},
       {kHost5, {HashedDomain(1), HashedDomain(2), HashedDomain(3)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 103, 4, 5, 6}},
      {kHost2, {2, 103, 4, 5, 6}},
      {kHost3, {103, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          {{ExpectedRandomTopic(0), {}},
                           {ExpectedRandomTopic(1), {}},
                           {ExpectedRandomTopic(2), {}},
                           {ExpectedRandomTopic(3), {}},
                           {ExpectedRandomTopic(4), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 0u);
}

TEST_F(
    BrowsingTopicsCalculatorTest,
    HistoryHostsBetween7And21Days_IgnoredForTopTopicsDecision_ConsideredForObservingDomainsDecision) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost1, kHost2, kHost3, kHost4, kHost5, kHost6},
                    begin_time - base::Days(20));

  AddApiUsageContextEntries(
      {{kHost1, {}},
       {kHost2, {}},
       {kHost3, {HashedDomain(2)}},
       {kHost4, {HashedDomain(3)}},
       {kHost5, {HashedDomain(1), HashedDomain(2), HashedDomain(3)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, ExpectedRandomTopic(2).value(), 4, 5, 6}},
      {kHost2, {2, ExpectedRandomTopic(2).value(), 4, 5, 6}},
      {kHost3, {ExpectedRandomTopic(2).value(), 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          {{ExpectedRandomTopic(0), {}},
                           {ExpectedRandomTopic(1), {}},
                           {ExpectedRandomTopic(2), {HashedDomain(2)}},
                           {ExpectedRandomTopic(3), {}},
                           {ExpectedRandomTopic(4), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 0u);
}

TEST_F(BrowsingTopicsCalculatorTest,
       DataQueryBoundedByTopicsDataAccessibleSince) {
  base::Time begin_time = base::Time::Now();

  prefs_.SetTime(prefs::kPrivacySandboxTopicsDataAccessibleSince,
                 begin_time + base::Days(6));

  AddHistoryEntries({kHost1, kHost2}, begin_time);
  AddApiUsageContextEntries({{kHost1, {}}, {kHost2, {}}});

  task_environment_.AdvanceClock(base::Days(6));

  AddHistoryEntries({kHost3, kHost4, kHost5, kHost6},
                    begin_time + base::Days(6));
  AddApiUsageContextEntries(
      {{kHost3, {HashedDomain(2)}},
       {kHost4, {HashedDomain(3)}},
       {kHost5, {HashedDomain(1), HashedDomain(2), HashedDomain(3)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5, 6}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(
      result.top_topics_and_observing_domains(),
      {{Topic(6), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(5), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(4), {HashedDomain(2), HashedDomain(3)}},
       {Topic(3), {HashedDomain(2)}},
       {ExpectedRandomTopic(0), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 4u);
}

TEST_F(BrowsingTopicsCalculatorTest,
       HistoryDataBoundedByLastEpochCalculationTime) {
  base::Time begin_time = base::Time::Now();
  AddHistoryEntries({kHost1, kHost2, kHost3}, begin_time);
  AddApiUsageContextEntries({{kHost3, {HashedDomain(5)}}});

  task_environment_.AdvanceClock(base::Days(4));
  AddHistoryEntries({kHost2, kHost3}, begin_time + base::Days(4));
  AddApiUsageContextEntries({{kHost3, {HashedDomain(2)}}});

  task_environment_.AdvanceClock(base::Days(2));
  AddHistoryEntries({kHost3, kHost4, kHost5, kHost6},
                    begin_time + base::Days(6));
  AddApiUsageContextEntries(
      {{kHost4, {HashedDomain(3)}},
       {kHost5, {HashedDomain(1), HashedDomain(2), HashedDomain(3)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5, 6}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  base::circular_deque<EpochTopics> epochs;
  epochs.push_back(EpochTopics(begin_time + base::Days(6)));

  EpochTopics result = CalculateTopics(std::move(epochs));

  // The topics are only from hosts since `begin_time + base::Days(6)`. The
  // observing domains are from data since `begin_time`.
  ExpectResultTopicsEqual(
      result.top_topics_and_observing_domains(),
      {{Topic(6),
        {HashedDomain(1), HashedDomain(2), HashedDomain(3), HashedDomain(5)}},
       {Topic(5),
        {HashedDomain(1), HashedDomain(2), HashedDomain(3), HashedDomain(5)}},
       {Topic(4), {HashedDomain(2), HashedDomain(3), HashedDomain(5)}},
       {Topic(3), {HashedDomain(2), HashedDomain(5)}},
       {ExpectedRandomTopic(0), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 4u);
}

TEST_F(BrowsingTopicsCalculatorTest,
       HistoryDataAndApiUsageContextDataBoundedByPriorEpochsCalculationTime) {
  base::Time begin_time = base::Time::Now();
  AddHistoryEntries({kHost1, kHost2, kHost3}, begin_time);
  AddApiUsageContextEntries({{kHost3, {HashedDomain(5)}}});

  task_environment_.AdvanceClock(base::Days(4));
  AddHistoryEntries({kHost2, kHost3}, begin_time + base::Days(4));
  AddApiUsageContextEntries({{kHost3, {HashedDomain(2)}}});

  task_environment_.AdvanceClock(base::Days(2));
  AddHistoryEntries({kHost3, kHost4, kHost5, kHost6},
                    begin_time + base::Days(6));
  AddApiUsageContextEntries(
      {{kHost4, {HashedDomain(3)}},
       {kHost5, {HashedDomain(1), HashedDomain(2), HashedDomain(3)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5, 6}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  base::circular_deque<EpochTopics> epochs;
  epochs.push_back(EpochTopics(begin_time + base::Days(4)));
  epochs.push_back(EpochTopics(begin_time + base::Days(5)));
  epochs.push_back(EpochTopics(begin_time + base::Days(6)));

  EpochTopics result = CalculateTopics(std::move(epochs));

  // The topics are only from hosts since `begin_time + base::Days(6)`. The
  // observing domains are from data since `begin_time + base::Days(4)`.
  ExpectResultTopicsEqual(
      result.top_topics_and_observing_domains(),
      {{Topic(6), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(5), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(4), {HashedDomain(2), HashedDomain(3)}},
       {Topic(3), {HashedDomain(2)}},
       {ExpectedRandomTopic(0), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 4u);
}

TEST_F(BrowsingTopicsCalculatorTest,
       TopTopicsAndObservingDomains_DomainsSizeExceedsLimit) {
  base::Time begin_time = base::Time::Now();

  std::set<HashedDomain> large_size_domains;
  for (int i = 1; i <= 1001; ++i) {
    large_size_domains.insert(HashedDomain(i));
  }

  AddHistoryEntries({kHost1, kHost2, kHost3, kHost4, kHost5, kHost6},
                    begin_time);

  AddApiUsageContextEntries({{kHost1, {}},
                             {kHost2, {}},
                             {kHost3, {HashedDomain(2)}},
                             {kHost4, {HashedDomain(3)}},
                             {kHost5, large_size_domains}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5, 6}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  std::set<HashedDomain> expected_domains_after_capping = large_size_domains;
  expected_domains_after_capping.erase(HashedDomain(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          {{Topic(6), expected_domains_after_capping},
                           {Topic(5), expected_domains_after_capping},
                           {Topic(4), {HashedDomain(2), HashedDomain(3)}},
                           {Topic(3), {HashedDomain(2)}},
                           {Topic(2), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 5u);
}

TEST_F(BrowsingTopicsCalculatorTest, TopicBlocked) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost1, kHost2, kHost3, kHost4, kHost5, kHost6},
                    begin_time);

  AddApiUsageContextEntries(
      {{kHost1, {}},
       {kHost2, {}},
       {kHost3, {HashedDomain(2)}},
       {kHost4, {HashedDomain(3)}},
       {kHost5, {HashedDomain(1), HashedDomain(2), HashedDomain(3)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5, 6}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  privacy_sandbox_settings_->SetTopicAllowed(
      privacy_sandbox::CanonicalTopic(Topic(6), kTaxonomyVersion),
      /*allowed=*/false);
  privacy_sandbox_settings_->SetTopicAllowed(
      privacy_sandbox::CanonicalTopic(Topic(4), kTaxonomyVersion),
      /*allowed=*/false);

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(
      result.top_topics_and_observing_domains(),
      {{Topic(0), {}},
       {Topic(5), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(0), {}},
       {Topic(3), {HashedDomain(2)}},
       {Topic(2), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 5u);
}

TEST_F(BrowsingTopicsCalculatorTest, TopicBlockedByFinch) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost1, kHost2, kHost3, kHost4, kHost5, kHost6},
                    begin_time);

  AddApiUsageContextEntries(
      {{kHost1, {}},
       {kHost2, {}},
       {kHost3, {HashedDomain(2)}},
       {kHost4, {HashedDomain(3)}},
       {kHost5, {HashedDomain(1), HashedDomain(2), HashedDomain(3)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5, 6}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kBrowsingTopicsParameters,
      {{"disabled_topics_list", "6,4"}});

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(
      result.top_topics_and_observing_domains(),
      {{Topic(0), {}},
       {Topic(5), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(0), {}},
       {Topic(3), {HashedDomain(2)}},
       {Topic(2), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 5u);
}

TEST_F(BrowsingTopicsCalculatorTest, TopicsPrioritizedByFinch) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost1, kHost2, kHost3, kHost4, kHost5, kHost6},
                    begin_time);

  AddApiUsageContextEntries(
      {{kHost1, {}},
       {kHost2, {}},
       {kHost3, {HashedDomain(2)}},
       {kHost4, {HashedDomain(3)}},
       {kHost5, {HashedDomain(1), HashedDomain(2), HashedDomain(3)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {74, 2, 3, 4, 5, 6}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kBrowsingTopicsParameters,
      {{"prioritized_topics_list", "4,57"}});  // 74 is descended from 57.

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(
      result.top_topics_and_observing_domains(),
      {{Topic(4), {HashedDomain(2), HashedDomain(3)}},
       {Topic(74), {}},
       {Topic(6), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(5), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(3), {HashedDomain(2)}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 5u);
  EXPECT_EQ(result.config_version(), 2);
}

TEST_F(BrowsingTopicsCalculatorTest, PaddedTopicsDoNotDuplicate) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost4, kHost5, kHost6}, begin_time);

  AddApiUsageContextEntries(
      {{kHost1, {}},
       {kHost2, {}},
       {kHost3, {HashedDomain(2)}},
       {kHost4, {HashedDomain(3)}},
       {kHost5, {HashedDomain(1), HashedDomain(2), HashedDomain(3)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5, ExpectedRandomTopic(1).value()}},
      {kHost2, {2, 3, 4, 5, ExpectedRandomTopic(1).value()}},
      {kHost3, {3, 4, 5, ExpectedRandomTopic(1).value()}},
      {kHost4, {4, 5, ExpectedRandomTopic(1).value()}},
      {kHost5, {5, ExpectedRandomTopic(1).value()}},
      {kHost6, {ExpectedRandomTopic(1).value()}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();

  // Note that ExpectedRandomTopic(1) (i.e. Topic(177)) is a descendant of
  // ExpectedRandomTopic(0) (i.e. Topic(176)). Thus, `ExpectedRandomTopic(0)` is
  // considered to have observed all three domains as well.
  ExpectResultTopicsEqual(
      result.top_topics_and_observing_domains(),
      {{ExpectedRandomTopic(1),
        {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(5), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(4), {HashedDomain(3)}},
       {ExpectedRandomTopic(0),
        {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {ExpectedRandomTopic(2), {}}});
}

TEST_F(BrowsingTopicsCalculatorTest, Metrics_LessThan5HistoryTopics) {
  base::HistogramTester histograms;

  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost4, kHost5, kHost6}, begin_time);

  AddApiUsageContextEntries(
      {{kHost1, {}},
       {kHost2, {}},
       {kHost3, {HashedDomain(2)}},
       {kHost4, {HashedDomain(3)}},
       {kHost5, {HashedDomain(1), HashedDomain(2), HashedDomain(3)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5, 6}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(
      result.top_topics_and_observing_domains(),
      {{Topic(6), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(5), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(4), {HashedDomain(3)}},
       {ExpectedRandomTopic(0), {}},
       {ExpectedRandomTopic(1), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 3u);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.EligibleDistinctHistoryHostsCount",
      /*sample=*/3,
      /*expected_bucket_count=*/1);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.HistoryTopicsCount",
      /*sample=*/3,
      /*expected_bucket_count=*/1);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.TopTopicsCountBeforePadding",
      /*sample=*/3,
      /*expected_bucket_count=*/1);

  histograms.ExpectTotalCount(
      "BrowsingTopics.EpochTopicsCalculation."
      "ObservationContextDomainsCountPerTopTopic",
      /*count=*/5);
  histograms.ExpectBucketCount(
      "BrowsingTopics.EpochTopicsCalculation."
      "ObservationContextDomainsCountPerTopTopic",
      /*sample=*/0,
      /*expected_count=*/2);
  histograms.ExpectBucketCount(
      "BrowsingTopics.EpochTopicsCalculation."
      "ObservationContextDomainsCountPerTopTopic",
      /*sample=*/1,
      /*expected_count=*/1);
  histograms.ExpectBucketCount(
      "BrowsingTopics.EpochTopicsCalculation."
      "ObservationContextDomainsCountPerTopTopic",
      /*sample=*/3,
      /*expected_count=*/2);
}

TEST_F(BrowsingTopicsCalculatorTest, Metrics_MoreThan5HistoryTopics) {
  base::HistogramTester histograms;

  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost1, kHost2, kHost3, kHost4, kHost5, kHost6},
                    begin_time);

  AddApiUsageContextEntries(
      {{kHost1, {}},
       {kHost2, {}},
       {kHost3, {HashedDomain(2)}},
       {kHost4, {HashedDomain(3)}},
       {kHost5, {HashedDomain(1), HashedDomain(2), HashedDomain(3)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5, 6}},
      {kHost2, {2, 3, 4, 5, 6}},
      {kHost3, {3, 4, 5, 6}},
      {kHost4, {4, 5, 6}},
      {kHost5, {5, 6}},
      {kHost6, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();

  EXPECT_EQ(result.padded_top_topics_start_index(), 5u);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.HistoryTopicsCount",
      /*sample=*/6,
      /*expected_bucket_count=*/1);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.TopTopicsCountBeforePadding",
      /*sample=*/5,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsCalculatorTest, NoDescendantTopics) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries(
      {
          kHost1,
          kHost2,
          kHost3,
          kHost4,
          kHost5,
      },
      begin_time);
  AddApiUsageContextEntries({{kHost1, {HashedDomain(1)}},
                             {kHost2, {HashedDomain(2)}},
                             {kHost3, {HashedDomain(3)}},
                             {kHost4, {HashedDomain(4)}},
                             {kHost5, {HashedDomain(5)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  test_annotator_.UseAnnotations({
      {kHost1, {2, 3, 4, 5, 6}},
      {kHost2, {3, 4, 5, 6}},
      {kHost3, {4, 5, 6}},
      {kHost4, {5, 6}},
      {kHost5, {6}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  std::vector<std::pair<Topic, std::set<HashedDomain>>>
      expected_top_topics_and_observing_domains = {
          {Topic(6),
           {HashedDomain(1), HashedDomain(2), HashedDomain(3), HashedDomain(4),
            HashedDomain(5)}},
          {Topic(5),
           {HashedDomain(1), HashedDomain(2), HashedDomain(3),
            HashedDomain(4)}},
          {Topic(4), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
          {Topic(3), {HashedDomain(1), HashedDomain(2)}},
          {Topic(2), {HashedDomain(1)}}};
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          expected_top_topics_and_observing_domains);
}

TEST_F(BrowsingTopicsCalculatorTest, DescendantTopicIsBlocked) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries(
      {
          kHost1,
          kHost2,
          kHost3,
          kHost4,
          kHost5,
      },
      begin_time);
  AddApiUsageContextEntries({{kHost1, {HashedDomain(1)}},
                             {kHost2, {HashedDomain(2)}},
                             {kHost3, {HashedDomain(3)}},
                             {kHost4, {HashedDomain(4)}},
                             {kHost5, {HashedDomain(5)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  // 1 is the parent topic of 2-5.
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 5}},
      {kHost2, {2, 3, 4, 5}},
      {kHost3, {3, 4, 5}},
      {kHost4, {4, 5}},
      {kHost5, {5}},
  });

  privacy_sandbox_settings_->SetTopicAllowed(
      privacy_sandbox::CanonicalTopic(Topic(5), kTaxonomyVersion),
      /*allowed=*/false);

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  // topic 5 is cleared but topic 1 can still see its observing domains
  std::vector<std::pair<Topic, std::set<HashedDomain>>>
      expected_top_topics_and_observing_domains = {
          {Topic(), {}},
          {Topic(4),
           {HashedDomain(1), HashedDomain(2), HashedDomain(3),
            HashedDomain(4)}},
          {Topic(3), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
          {Topic(2), {HashedDomain(1), HashedDomain(2)}},
          {Topic(1),
           {HashedDomain(1), HashedDomain(2), HashedDomain(3), HashedDomain(4),
            HashedDomain(5)}}};

  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          expected_top_topics_and_observing_domains);
}

TEST_F(BrowsingTopicsCalculatorTest, TopicHasDistantDescendant) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries(
      {
          kHost1,
          kHost2,
          kHost3,
          kHost4,
          kHost5,
      },
      begin_time);
  AddApiUsageContextEntries({{kHost1, {HashedDomain(1)}},
                             {kHost2, {HashedDomain(2)}},
                             {kHost3, {HashedDomain(3)}},
                             {kHost4, {HashedDomain(4)}},
                             {kHost5, {HashedDomain(5)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  // 1 is the parent topic of 2-4, and grandparent of 21.
  test_annotator_.UseAnnotations({
      {kHost1, {1, 2, 3, 4, 21}},
      {kHost2, {2, 3, 4, 21}},
      {kHost3, {3, 4, 21}},
      {kHost4, {4, 21}},
      {kHost5, {21}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));
  EpochTopics result = CalculateTopics();

  std::vector<std::pair<Topic, std::set<HashedDomain>>>
      expected_top_topics_and_observing_domains = {
          {Topic(21),
           {HashedDomain(1), HashedDomain(2), HashedDomain(3), HashedDomain(4),
            HashedDomain(5)}},
          {Topic(4),
           {HashedDomain(1), HashedDomain(2), HashedDomain(3),
            HashedDomain(4)}},
          {Topic(3), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
          {Topic(2), {HashedDomain(1), HashedDomain(2)}},
          {Topic(1),
           {HashedDomain(1), HashedDomain(2), HashedDomain(3), HashedDomain(4),
            HashedDomain(5)}}};

  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          expected_top_topics_and_observing_domains);
}

TEST_F(BrowsingTopicsCalculatorTest, MultipleTopTopicsHaveDescendants) {
  // This test assumes no top topics prioritization.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kBrowsingTopicsParameters,
      {{"prioritized_topics_list", ""}});

  base::Time begin_time = base::Time::Now();

  AddHistoryEntries(
      {
          kHost1,
          kHost2,
          kHost3,
          kHost4,
          kHost5,
      },
      begin_time);
  AddApiUsageContextEntries({{kHost1, {HashedDomain(1)}},
                             {kHost2, {HashedDomain(2)}},
                             {kHost3, {HashedDomain(3)}},
                             {kHost4, {HashedDomain(4)}},
                             {kHost5, {HashedDomain(5)}}});

  test_annotator_.UseModelInfo(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
  // 1 is the ancestor of 21, 57 is the ancestor of 63 and 64.
  test_annotator_.UseAnnotations({
      {kHost1, {1, 57, 63, 64, 21}},
      {kHost2, {57, 63, 64, 21}},
      {kHost3, {63, 64, 21}},
      {kHost4, {64, 21}},
      {kHost5, {21}},
  });

  task_environment_.AdvanceClock(base::Seconds(1));
  EpochTopics result = CalculateTopics();

  std::vector<std::pair<Topic, std::set<HashedDomain>>>
      expected_top_topics_and_observing_domains = {
          {Topic(21),
           {HashedDomain(1), HashedDomain(2), HashedDomain(3), HashedDomain(4),
            HashedDomain(5)}},
          {Topic(64),
           {HashedDomain(1), HashedDomain(2), HashedDomain(3),
            HashedDomain(4)}},
          {Topic(63), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
          {Topic(57),
           {HashedDomain(1), HashedDomain(2), HashedDomain(3),
            HashedDomain(4)}},
          {Topic(1),
           {HashedDomain(1), HashedDomain(2), HashedDomain(3), HashedDomain(4),
            HashedDomain(5)}}};

  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          expected_top_topics_and_observing_domains);
}

}  // namespace browsing_topics
