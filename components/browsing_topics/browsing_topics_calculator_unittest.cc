// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_calculator.h"

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
#include "components/optimization_guide/content/browser/test_page_content_annotations_service.h"
#include "components/optimization_guide/content/browser/test_page_content_annotator.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browsing_topics_test_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

namespace {

constexpr size_t kTaxonomySize = 349;
constexpr int kTaxonomyVersion = 1;

constexpr char kHost1[] = "www.foo1.com";
constexpr char kHost2[] = "www.foo2.com";
constexpr char kHost3[] = "www.foo3.com";
constexpr char kHost4[] = "www.foo4.com";
constexpr char kHost5[] = "www.foo5.com";
constexpr char kHost6[] = "www.foo6.com";

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
    cookie_settings_ = base::MakeRefCounted<content_settings::CookieSettings>(
        host_content_settings_map_.get(), &prefs_, false, "chrome-extension");
    auto privacy_sandbox_delegate = std::make_unique<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>();
    privacy_sandbox_delegate->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/false);
    privacy_sandbox_delegate->SetUpIsIncognitoProfileResponse(
        /*incognito=*/false);
    privacy_sandbox_settings_ =
        std::make_unique<privacy_sandbox::PrivacySandboxSettingsImpl>(
            std::move(privacy_sandbox_delegate),
            host_content_settings_map_.get(), cookie_settings_, &prefs_);
    privacy_sandbox_settings_->SetAllPrivacySandboxAllowedForTesting();

    topics_site_data_manager_ =
        std::make_unique<content::TesterBrowsingTopicsSiteDataManager>(
            temp_dir_.GetPath());

    history_service_ = std::make_unique<history::HistoryService>();
    history_service_->Init(
        history::TestHistoryDatabaseParamsForPath(temp_dir_.GetPath()));

    page_content_annotations_service_ =
        optimization_guide::TestPageContentAnnotationsService::Create(
            /*optimization_guide_model_provider=*/nullptr,
            history_service_.get());

    page_content_annotations_service_->OverridePageContentAnnotatorForTesting(
        &test_page_content_annotator_);

    task_environment_.RunUntilIdle();
  }

  ~BrowsingTopicsCalculatorTest() override {
    host_content_settings_map_->ShutdownOnUIThread();
  }

  EpochTopics CalculateTopics(base::circular_deque<EpochTopics> epochs = {}) {
    EpochTopics result = EpochTopics(base::Time());

    base::RunLoop run_loop;

    TesterBrowsingTopicsCalculator topics_calculator =
        TesterBrowsingTopicsCalculator(
            privacy_sandbox_settings_.get(), history_service_.get(),
            topics_site_data_manager_.get(),
            page_content_annotations_service_.get(), epochs,
            base::BindLambdaForTesting([&](EpochTopics epoch_topics) {
              result = std::move(epoch_topics);
              run_loop.Quit();
            }),
            /*rand_uint64_queue=*/
            base::queue<uint64_t>{{100, 101, 102, 103, 104}});

    run_loop.Run();

    return result;
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
      topics_site_data_manager_->OnBrowsingTopicsApiUsed(
          HashMainFrameHostForStorage(main_frame_host),
          base::flat_set<HashedDomain>(context_domains.begin(),
                                       context_domains.end()),
          base::Time::Now());
    }

    task_environment_.RunUntilIdle();
  }

  std::vector<optimization_guide::WeightedIdentifier> TopicsAndWeight(
      const std::vector<int32_t>& topics,
      double weight) {
    std::vector<optimization_guide::WeightedIdentifier> result;
    for (int32_t topic : topics) {
      result.emplace_back(
          optimization_guide::WeightedIdentifier(topic, weight));
    }

    return result;
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
  std::unique_ptr<privacy_sandbox::PrivacySandboxSettings>
      privacy_sandbox_settings_;

  std::unique_ptr<content::TesterBrowsingTopicsSiteDataManager>
      topics_site_data_manager_;

  std::unique_ptr<history::HistoryService> history_service_;

  std::unique_ptr<optimization_guide::PageContentAnnotationsService>
      page_content_annotations_service_;

  optimization_guide::TestPageContentAnnotator test_page_content_annotator_;

  base::ScopedTempDir temp_dir_;
};

TEST_F(BrowsingTopicsCalculatorTest, PermissionDenied) {
  base::HistogramTester histograms;

  privacy_sandbox_settings_->SetTopicsBlockedForTesting();

  EpochTopics result = CalculateTopics();
  EXPECT_TRUE(result.empty());

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.CalculatorResultStatus",
      /*kFailurePermissionDenied*/ 1,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsCalculatorTest, ApiUsageContextQueryError) {
  base::HistogramTester histograms;

  topics_site_data_manager_->SetQueryFailureOverride();

  EpochTopics result = CalculateTopics();
  EXPECT_TRUE(result.empty());

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.CalculatorResultStatus",
      /*kFailureApiUsageContextQueryError*/ 2,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsCalculatorTest, AnnotationExecutionError) {
  base::HistogramTester histograms;

  EpochTopics result = CalculateTopics();
  EXPECT_TRUE(result.empty());

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.CalculatorResultStatus",
      /*kFailureAnnotationExecutionError*/ 3,
      /*expected_bucket_count=*/1);
}

class BrowsingTopicsCalculatorUnsupporedTaxonomyVersionTest
    : public BrowsingTopicsCalculatorTest {
 public:
  BrowsingTopicsCalculatorUnsupporedTaxonomyVersionTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kBrowsingTopics, {{"taxonomy_version", "999"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BrowsingTopicsCalculatorUnsupporedTaxonomyVersionTest,
       TaxonomyVersionNotSupportedInBinary) {
  base::HistogramTester histograms;

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(), {});

  EpochTopics result = CalculateTopics();
  EXPECT_TRUE(result.empty());

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.CalculatorResultStatus",
      /*kFailureTaxonomyVersionNotSupportedInBinary*/ 4,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsCalculatorTest, TopicsMetadata) {
  base::HistogramTester histograms;
  base::Time begin_time = base::Time::Now();

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(), {});

  EpochTopics result1 = CalculateTopics();
  EXPECT_FALSE(result1.empty());
  EXPECT_EQ(result1.taxonomy_size(), kTaxonomySize);
  EXPECT_EQ(result1.taxonomy_version(), kTaxonomyVersion);
  EXPECT_EQ(result1.model_version(), 1);
  EXPECT_EQ(result1.calculation_time(), begin_time);

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.CalculatorResultStatus",
      /*kSuccess*/ 0,
      /*expected_bucket_count=*/1);

  task_environment_.AdvanceClock(base::Seconds(2));

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(50).Build(), {});

  EpochTopics result2 = CalculateTopics();
  EXPECT_FALSE(result2.empty());
  EXPECT_EQ(result2.taxonomy_size(), kTaxonomySize);
  EXPECT_EQ(result2.taxonomy_version(), kTaxonomyVersion);
  EXPECT_EQ(result2.model_version(), 50);
  EXPECT_EQ(result2.calculation_time(), begin_time + base::Seconds(2));

  histograms.ExpectUniqueSample(
      "BrowsingTopics.EpochTopicsCalculation.CalculatorResultStatus",
      /*kSuccess*/ 0,
      /*expected_bucket_count=*/2);
}

TEST_F(BrowsingTopicsCalculatorTest, TopTopicsRankedByFrequency) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost1, kHost2, kHost3, kHost4, kHost5, kHost6},
                    begin_time);

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 3, 4, 5, 6}, 0.1)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

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

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, {}},
       {kHost2, {}},
       {kHost3, {}},
       {kHost4, {}},
       {kHost5, {}},
       {kHost6, {}}});

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          {{Topic(101), {}},
                           {Topic(102), {}},
                           {Topic(103), {}},
                           {Topic(104), {}},
                           {Topic(105), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 0u);
}

TEST_F(BrowsingTopicsCalculatorTest,
       TopTopicsRankedByFrequency_AlsoAffectedByHostsCount) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost1, kHost1, kHost1, kHost1, kHost1, kHost1, kHost2,
                     kHost3, kHost4, kHost5, kHost6},
                    begin_time);

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2}, 0.1)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

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

TEST_F(BrowsingTopicsCalculatorTest,
       TopTopicsRankingNotAffectedByAnnotationWeight) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost1, kHost2, kHost3, kHost4, kHost5, kHost6},
                    begin_time);

  // Setting the weight for Topic(1) and Topic(2) to 0.9. This weight shouldn't
  // affect the top topics ordering.
  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2}, 0.9)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

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

TEST_F(BrowsingTopicsCalculatorTest, AllTopTopicsRandomlyPadded) {
  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 3, 4, 5, 6}, 0.1)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          {{Topic(101), {}},
                           {Topic(102), {}},
                           {Topic(103), {}},
                           {Topic(104), {}},
                           {Topic(105), {}}});

  EXPECT_EQ(result.padded_top_topics_start_index(), 0u);
}

TEST_F(BrowsingTopicsCalculatorTest, TopTopicsPartiallyPadded) {
  base::HistogramTester histograms;

  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost4, kHost5, kHost6}, begin_time);

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 3, 4, 5, 6}, 0.1)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          {{Topic(6), {}},
                           {Topic(5), {}},
                           {Topic(4), {}},
                           {Topic(101), {}},
                           {Topic(102), {}}});

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

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 3, 4, 5, 6}, 0.1)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

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
      101);
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
          kTopTopic4Name,
      102);
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
          kTaxonomyVersionName,
      1);
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

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 3, 4, 5, 6}, 0.1)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

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

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 103, 4, 5, 6}, 0.1)},
       {kHost2, TopicsAndWeight({2, 103, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({103, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          {{Topic(101), {}},
                           {Topic(102), {}},
                           {Topic(103), {}},
                           {Topic(104), {}},
                           {Topic(105), {}}});

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

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 103, 4, 5, 6}, 0.1)},
       {kHost2, TopicsAndWeight({2, 103, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({103, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(result.top_topics_and_observing_domains(),
                          {{Topic(101), {}},
                           {Topic(102), {}},
                           {Topic(103), {HashedDomain(2)}},
                           {Topic(104), {}},
                           {Topic(105), {}}});

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

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 3, 4, 5, 6}, 0.1)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(
      result.top_topics_and_observing_domains(),
      {{Topic(6), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(5), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(4), {HashedDomain(2), HashedDomain(3)}},
       {Topic(3), {HashedDomain(2)}},
       {Topic(101), {}}});

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

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 3, 4, 5, 6}, 0.1)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

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
       {Topic(101), {}}});

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

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 3, 4, 5, 6}, 0.1)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

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
       {Topic(101), {}}});

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

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 3, 4, 5, 6}, 0.1)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

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

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 3, 4, 5, 6}, 0.1)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

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

TEST_F(BrowsingTopicsCalculatorTest, PaddedTopicsDoNotDuplicate) {
  base::Time begin_time = base::Time::Now();

  AddHistoryEntries({kHost4, kHost5, kHost6}, begin_time);

  AddApiUsageContextEntries(
      {{kHost1, {}},
       {kHost2, {}},
       {kHost3, {HashedDomain(2)}},
       {kHost4, {HashedDomain(3)}},
       {kHost5, {HashedDomain(1), HashedDomain(2), HashedDomain(3)}}});

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 3, 4, 5, 102}, 0.1)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 102}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 102}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 102}, 0.1)},
       {kHost5, TopicsAndWeight({5, 102}, 0.1)},
       {kHost6, TopicsAndWeight({102}, 0.1)}});

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(
      result.top_topics_and_observing_domains(),
      {{Topic(102), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(5), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(4), {HashedDomain(3)}},
       {Topic(101), {}},
       {Topic(103), {}}});
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

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 3, 4, 5, 6}, 0.1)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

  task_environment_.AdvanceClock(base::Seconds(1));

  EpochTopics result = CalculateTopics();
  ExpectResultTopicsEqual(
      result.top_topics_and_observing_domains(),
      {{Topic(6), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(5), {HashedDomain(1), HashedDomain(2), HashedDomain(3)}},
       {Topic(4), {HashedDomain(3)}},
       {Topic(101), {}},
       {Topic(102), {}}});

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

  test_page_content_annotator_.UsePageTopics(
      *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
      {{kHost1, TopicsAndWeight({1, 2, 3, 4, 5, 6}, 0.1)},
       {kHost2, TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
       {kHost3, TopicsAndWeight({3, 4, 5, 6}, 0.1)},
       {kHost4, TopicsAndWeight({4, 5, 6}, 0.1)},
       {kHost5, TopicsAndWeight({5, 6}, 0.1)},
       {kHost6, TopicsAndWeight({6}, 0.1)}});

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

}  // namespace browsing_topics
