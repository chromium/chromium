// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/visited_url_ranking_service_impl.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_types.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/testing/mock_database_client.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/segmentation_platform/public/trigger.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/fetcher_config.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using segmentation_platform::HasTrainingLabel;
using segmentation_platform::MockSegmentationPlatformService;
using URLType = visited_url_ranking::FetchOptions::URLType;
using ResultOption = visited_url_ranking::FetchOptions::ResultOption;
using testing::_;

namespace visited_url_ranking {

namespace {

const segmentation_platform::TrainingRequestId kTestRequestId =
    segmentation_platform::TrainingRequestId::FromUnsafeValue(0);

segmentation_platform::AnnotatedNumericResult CreateResult(float val) {
  segmentation_platform::AnnotatedNumericResult result(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.result.mutable_output_config()
      ->mutable_predictor()
      ->mutable_generic_predictor()
      ->add_output_labels(kTabResumptionRankerKey);
  result.result.add_result(val);
  result.request_id = kTestRequestId;
  return result;
}

}  // namespace

class MockURLVisitDataFetcher : public URLVisitDataFetcher {
 public:
  MockURLVisitDataFetcher() = default;
  MockURLVisitDataFetcher(const MockURLVisitDataFetcher&) = delete;
  MockURLVisitDataFetcher& operator=(const MockURLVisitDataFetcher&) = delete;
  ~MockURLVisitDataFetcher() override = default;

  MOCK_METHOD3(FetchURLVisitData,
               void(const FetchOptions& options,
                    const FetcherConfig& config,
                    FetchResultCallback callback));
};

class MockURLVisitAggregatesTransformer : public URLVisitAggregatesTransformer {
 public:
  MockURLVisitAggregatesTransformer() = default;
  MockURLVisitAggregatesTransformer(const MockURLVisitAggregatesTransformer&) =
      delete;
  MockURLVisitAggregatesTransformer& operator=(
      const MockURLVisitAggregatesTransformer&) = delete;
  ~MockURLVisitAggregatesTransformer() override = default;

  MOCK_METHOD3(Transform,
               void(std::vector<URLVisitAggregate> aggregates,
                    const FetchOptions& options,
                    OnTransformCallback callback));
};

class VisitedURLRankingServiceImplTest : public testing::Test {
 public:
  VisitedURLRankingServiceImplTest() = default;

  URLVisitAggregate::TabData GetSampleTabData(int32_t id, const char* url) {
    return URLVisitAggregate::TabData(URLVisitAggregate::Tab(
        id,
        URLVisit(GURL(url), u"sample_title", base::Time::Now(),
                 syncer::DeviceInfo::FormFactor::kUnknown,
                 URLVisit::Source::kLocal),
        "sample_tag", "sample_session_name"));
  }

  std::unique_ptr<MockURLVisitDataFetcher>
  CreateMockTabDataFetcherWithExpectation(
      std::vector<URLVisitAggregate::TabData> data) {
    auto data_fetcher = std::make_unique<MockURLVisitDataFetcher>();
    EXPECT_CALL(*data_fetcher, FetchURLVisitData(_, _, _))
        .Times(1)
        .WillOnce(testing::Invoke(
            [data](const FetchOptions& options, const FetcherConfig& config,
                   URLVisitDataFetcher::FetchResultCallback callback) {
              std::map<URLMergeKey, URLVisitAggregate::URLVisitVariant>
                  variant_data_map = {};
              for (auto& tab_data : data) {
                variant_data_map.emplace(
                    tab_data.last_active_tab.visit.url.spec(),
                    std::move(tab_data));
              }
              std::move(callback).Run(
                  {FetchResult::Status::kSuccess, std::move(variant_data_map)});
            }));

    return data_fetcher;
  }

  std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>>
  PrepareMockDataFetchers() {
    std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>> data_fetchers = {};
    std::vector<URLVisitAggregate::TabData> tab_data_entries;
    tab_data_entries.push_back(GetSampleTabData(1, kSampleSearchUrl));
    data_fetchers.emplace(
        Fetcher::kSession,
        CreateMockTabDataFetcherWithExpectation(std::move(tab_data_entries)));
    return data_fetchers;
  }

  void InitService(
      std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>> data_fetchers,
      std::map<URLVisitAggregatesTransformType,
               std::unique_ptr<URLVisitAggregatesTransformer>> transformers) {
    database_client_ =
        std::make_unique<segmentation_platform::MockDatabaseClient>();
    segmentation_platform_service_ =
        std::make_unique<MockSegmentationPlatformService>();
    service_impl_ = std::make_unique<VisitedURLRankingServiceImpl>(
        segmentation_platform_service_.get(), std::move(data_fetchers),
        std::move(transformers),
        std::make_unique<url_deduplication::URLDeduplicationHelper>(
            url_deduplication::DeduplicationStrategy()));

    EXPECT_CALL(*segmentation_platform_service_, GetDatabaseClient())
        .WillRepeatedly(testing::Return(database_client_.get()));
  }

  ~VisitedURLRankingServiceImplTest() override {
    service_impl_ = nullptr;
    segmentation_platform_service_ = nullptr;
  }

  using GetURLResult = std::
      tuple<ResultStatus, URLVisitsMetadata, std::vector<URLVisitAggregate>>;
  GetURLResult RunFetchURLVisitAggregates(const FetchOptions& options) {
    GetURLResult result;
    std::vector<URLVisitAggregate> aggregates;
    base::RunLoop wait_loop;
    service_impl_->FetchURLVisitAggregates(
        options,
        base::BindOnce(
            [](base::OnceClosure stop_waiting, GetURLResult* result,
               ResultStatus status, URLVisitsMetadata url_visits_metadata,
               std::vector<URLVisitAggregate> aggregates) {
              std::get<0>(*result) = status;
              std::get<1>(*result) = std::move(url_visits_metadata);
              std::get<2>(*result) = std::move(aggregates);
              std::move(stop_waiting).Run();
            },
            wait_loop.QuitClosure(), &result));
    wait_loop.Run();
    return result;
  }

  using Result = std::pair<ResultStatus, std::vector<URLVisitAggregate>>;
  Result RunRankURLVisitAggregates(
      const Config& config,
      std::vector<URLVisitAggregate> visit_aggregates) {
    Result result;
    base::RunLoop wait_loop;
    service_impl_->RankURLVisitAggregates(
        config, std::move(visit_aggregates),
        base::BindOnce(
            [](base::OnceClosure stop_waiting, Result* result,
               ResultStatus status, std::vector<URLVisitAggregate> aggregates) {
              result->first = status;
              result->second = std::move(aggregates);
              std::move(stop_waiting).Run();
            },
            wait_loop.QuitClosure(), &result));
    wait_loop.Run();
    return result;
  }

  Result RunDecorateURLVisitAggregates(
      const Config& config,
      visited_url_ranking::URLVisitsMetadata url_visits_metadata,
      std::vector<URLVisitAggregate> visit_aggregates) {
    Result result;
    base::RunLoop wait_loop;
    service_impl_->DecorateURLVisitAggregates(
        config, std::move(url_visits_metadata), std::move(visit_aggregates),
        base::BindOnce(
            [](base::OnceClosure stop_waiting, Result* result,
               ResultStatus status, std::vector<URLVisitAggregate> aggregates) {
              result->first = status;
              result->second = std::move(aggregates);
              std::move(stop_waiting).Run();
            },
            wait_loop.QuitClosure(), &result));
    wait_loop.Run();
    return result;
  }

  void WaitForCollectData(
      segmentation_platform::TrainingRequestId test_request_id,
      ScoredURLUserAction action) {
    base::RunLoop wait_loop;
    EXPECT_CALL(
        *segmentation_platform_service_,
        CollectTrainingData(
            _, test_request_id,
            HasTrainingLabel("action",
                             static_cast<base::HistogramBase::Sample>(action)),
            _))
        .WillOnce([&wait_loop](
                      segmentation_platform::proto::SegmentId,
                      segmentation_platform::TrainingRequestId,
                      const segmentation_platform::TrainingLabels&,
                      segmentation_platform::SegmentationPlatformService::
                          SuccessCallback) { wait_loop.QuitClosure().Run(); });
    wait_loop.Run();
  }

 protected:
  std::unique_ptr<segmentation_platform::MockDatabaseClient> database_client_;
  std::unique_ptr<VisitedURLRankingServiceImpl> service_impl_;
  std::unique_ptr<MockSegmentationPlatformService>
      segmentation_platform_service_;
  base::SimpleTestClock clock_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(VisitedURLRankingServiceImplTest, FetchURLVisitAggregates) {
  base::HistogramTester histogram_tester;
  InitService(PrepareMockDataFetchers(), /*transformers=*/{});
  FetchOptions fetch_options = FetchOptions(
      {
          {URLType::kActiveRemoteTab, {.age_limit = base::Days(1)}},
      },
      {
          {Fetcher::kSession,
           FetchOptions::FetchSources({URLVisit::Source::kForeign})},
      },
      base::Time::Now() - base::Days(1), {});
  VisitedURLRankingServiceImplTest::GetURLResult result =
      RunFetchURLVisitAggregates(fetch_options);
  EXPECT_EQ(std::get<0>(result), ResultStatus::kSuccess);
  EXPECT_EQ(std::get<1>(result).aggregates_count_before_transforms, 1u);
  EXPECT_EQ(std::get<2>(result).size(), 1u);

  histogram_tester.ExpectUniqueSample(
      "VisitedURLRanking.Request.Step.Fetch.Status",
      static_cast<int>(VisitedURLRankingRequestStepStatus::kSuccess), 1);
  histogram_tester.ExpectUniqueSample("VisitedURLRanking.Fetch.Session.Success",
                                      static_cast<int>(Fetcher::kSession), 1);
}

TEST_F(VisitedURLRankingServiceImplTest, FetchWhenHistoryIsNotAvailable) {
  base::HistogramTester histogram_tester;
  InitService(PrepareMockDataFetchers(), /*transformers=*/{});

  ResultOption result_option{.age_limit = base::Days(1)};
  std::map<URLType, ResultOption> result_sources = {
      {URLType::kActiveRemoteTab, result_option},
      {URLType::kRemoteVisit, result_option},
  };
  FetchOptions fetch_options = FetchOptions(
      std::move(result_sources),
      {
          {Fetcher::kSession,
           FetchOptions::FetchSources({URLVisit::Source::kForeign})},
          {Fetcher::kHistory, FetchOptions::kOriginSources},
      },
      base::Time::Now() - base::Days(1), {});
  VisitedURLRankingServiceImplTest::GetURLResult result =
      RunFetchURLVisitAggregates(fetch_options);
  EXPECT_EQ(std::get<0>(result), ResultStatus::kSuccess);
  EXPECT_EQ(std::get<1>(result).aggregates_count_before_transforms, 1u);
  EXPECT_EQ(std::get<2>(result).size(), 1u);

  histogram_tester.ExpectTotalCount(
      "VisitedURLRanking.Request.Step.Fetch.Status", 2);
}

TEST_F(VisitedURLRankingServiceImplTest,
       FetchURLVisitAggregatesWithTransforms) {
  base::HistogramTester histogram_tester;
  auto mock_bookmark_transformer =
      std::make_unique<MockURLVisitAggregatesTransformer>();
  EXPECT_CALL(*mock_bookmark_transformer, Transform(_, _, _))
      .Times(1)
      .WillOnce(testing::Invoke(
          [](std::vector<URLVisitAggregate> aggregates,
             const FetchOptions& options,
             URLVisitAggregatesTransformer::OnTransformCallback callback) {
            std::erase_if(
                aggregates, [](const URLVisitAggregate& visit_aggregate) {
                  const auto& visit_variant =
                      visit_aggregate.fetcher_data_map.at(Fetcher::kSession);
                  int id = std::get<URLVisitAggregate::TabData>(visit_variant)
                               .last_active_tab.id;
                  return static_cast<bool>(id % 2);
                });
            std::move(callback).Run(
                URLVisitAggregatesTransformer::Status::kSuccess,
                std::move(aggregates));
          }));

  std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>> data_fetchers = {};
  std::vector<URLVisitAggregate::TabData> tab_data_entries;
  tab_data_entries.push_back(GetSampleTabData(1, "https://mail.google.com"));
  tab_data_entries.push_back(GetSampleTabData(2, "https://docs.google.com"));
  data_fetchers.emplace(
      Fetcher::kSession,
      CreateMockTabDataFetcherWithExpectation(std::move(tab_data_entries)));
  std::map<URLVisitAggregatesTransformType,
           std::unique_ptr<URLVisitAggregatesTransformer>>
      transformers = {};
  transformers.emplace(URLVisitAggregatesTransformType::kBookmarkData,
                       std::move(mock_bookmark_transformer));
  InitService(std::move(data_fetchers), std::move(transformers));

  FetchOptions fetch_options = FetchOptions(
      {
          {URLType::kActiveRemoteTab, {.age_limit = base::Days(1)}},
      },
      {
          {Fetcher::kSession,
           FetchOptions::FetchSources({URLVisit::Source::kForeign})},
      },
      base::Time::Now() - base::Days(1),
      {URLVisitAggregatesTransformType::kBookmarkData});
  VisitedURLRankingServiceImplTest::GetURLResult result =
      RunFetchURLVisitAggregates(fetch_options);
  EXPECT_EQ(std::get<0>(result), ResultStatus::kSuccess);
  EXPECT_EQ(std::get<1>(result).aggregates_count_before_transforms, 2u);
  EXPECT_EQ(std::get<2>(result).size(), 1u);

  histogram_tester.ExpectUniqueSample(
      "VisitedURLRanking.Request.Step.Transform.Status",
      static_cast<int>(VisitedURLRankingRequestStepStatus::kSuccess), 1);
  histogram_tester.ExpectTotalCount(
      "VisitedURLRanking.TransformType.BookmarkData.Success", 1);
  histogram_tester.ExpectUniqueSample(
      "VisitedURLRanking.TransformType.BookmarkData.InOutPercentage", 50, 1);
  histogram_tester.ExpectTotalCount(
      "VisitedURLRanking.TransformType.BookmarkData.Latency", 1);
}

TEST_F(VisitedURLRankingServiceImplTest,
       FetchURLVisitAggregatesWithMissingTransforms) {
  base::HistogramTester histogram_tester;
  std::map<URLVisitAggregatesTransformType,
           std::unique_ptr<URLVisitAggregatesTransformer>>
      transformers = {};
  InitService(PrepareMockDataFetchers(), std::move(transformers));

  FetchOptions fetch_options = FetchOptions(
      {
          {URLType::kActiveRemoteTab, {.age_limit = base::Days(1)}},
      },
      {
          {Fetcher::kSession,
           FetchOptions::FetchSources({URLVisit::Source::kForeign})},
      },
      base::Time::Now() - base::Days(1),
      {URLVisitAggregatesTransformType::kSegmentationMetricsData});
  VisitedURLRankingServiceImplTest::GetURLResult result =
      RunFetchURLVisitAggregates(fetch_options);
  EXPECT_EQ(std::get<0>(result), ResultStatus::kError);
  EXPECT_EQ(std::get<2>(result).size(), 0u);

  histogram_tester.ExpectUniqueSample(
      "VisitedURLRanking.Request.Step.Transform.Status",
      static_cast<int>(VisitedURLRankingRequestStepStatus::kFailedNotFound), 1);
}

TEST_F(VisitedURLRankingServiceImplTest,
       FetchURLVisitAggregatesWithFailedTransforms) {
  base::HistogramTester histogram_tester;
  auto mock_segmentation_metrics_transformer =
      std::make_unique<MockURLVisitAggregatesTransformer>();
  EXPECT_CALL(*mock_segmentation_metrics_transformer, Transform(_, _, _))
      .Times(1)
      .WillOnce(testing::Invoke(
          [](std::vector<URLVisitAggregate> aggregates,
             const FetchOptions& options,
             URLVisitAggregatesTransformer::OnTransformCallback callback) {
            std::move(callback).Run(
                URLVisitAggregatesTransformer::Status::kError, {});
          }));

  std::map<URLVisitAggregatesTransformType,
           std::unique_ptr<URLVisitAggregatesTransformer>>
      transformers = {};
  transformers.emplace(
      URLVisitAggregatesTransformType::kSegmentationMetricsData,
      std::move(mock_segmentation_metrics_transformer));
  InitService(PrepareMockDataFetchers(), std::move(transformers));

  FetchOptions fetch_options = FetchOptions(
      {
          {URLType::kActiveRemoteTab, {.age_limit = base::Days(1)}},
      },
      {
          {Fetcher::kSession,
           FetchOptions::FetchSources({URLVisit::Source::kForeign})},
      },
      base::Time::Now() - base::Days(1),
      {URLVisitAggregatesTransformType::kSegmentationMetricsData});
  VisitedURLRankingServiceImplTest::GetURLResult result =
      RunFetchURLVisitAggregates(fetch_options);
  EXPECT_EQ(std::get<0>(result), ResultStatus::kError);
  EXPECT_EQ(std::get<2>(result).size(), 0u);

  histogram_tester.ExpectUniqueSample(
      "VisitedURLRanking.Request.Step.Transform.Status",
      static_cast<int>(VisitedURLRankingRequestStepStatus::kFailed), 1);
}

TEST_F(VisitedURLRankingServiceImplTest, RankURLVisitAggregates) {
  base::HistogramTester histogram_tester;
  InitService(/*data_fetchers=*/{}, /*transformers=*/{});

  base::Time now = base::Time::Now();
  std::vector<URLVisitAggregate> url_visit_aggregates = {};
  const GURL kSampleUrl1 = GURL(base::StrCat({kSampleSearchUrl, "1"}));
  url_visit_aggregates.push_back(
      CreateSampleURLVisitAggregate(kSampleUrl1, 0.9f, now));
  const GURL kSampleUrl2 = GURL(base::StrCat({kSampleSearchUrl, "2"}));
  url_visit_aggregates.push_back(
      CreateSampleURLVisitAggregate(kSampleUrl2, 1.0f, now));

  testing::InSequence s;
  EXPECT_CALL(*segmentation_platform_service_,
              GetAnnotatedNumericResult(_, _, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(CreateResult(0.9f)));
  EXPECT_CALL(*segmentation_platform_service_,
              GetAnnotatedNumericResult(_, _, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(CreateResult(1.0f)));

  Config config = {.key = kTabResumptionRankerKey};
  VisitedURLRankingServiceImplTest::Result result =
      RunRankURLVisitAggregates(config, std::move(url_visit_aggregates));
  EXPECT_EQ(result.first, ResultStatus::kSuccess);
  EXPECT_EQ(result.second.size(), 2u);
  EXPECT_EQ(**result.second[0].GetAssociatedURLs().begin(), kSampleUrl2);

  histogram_tester.ExpectUniqueSample(
      "VisitedURLRanking.Request.Step.Rank.Status",
      static_cast<int>(VisitedURLRankingRequestStepStatus::kSuccess), 1);
  histogram_tester.ExpectUniqueSample("VisitedURLRanking.Rank.NumVisits", 2, 1);
}

TEST_F(VisitedURLRankingServiceImplTest, RecordAction) {
  base::HistogramTester histogram_tester;
  InitService(/*data_fetchers=*/{}, /*transformers=*/{});

  std::vector<
      std::pair<segmentation_platform::UkmEventHash,
                std::map<segmentation_platform::UkmMetricHash, int64_t>>>
      events;
  EXPECT_CALL(*database_client_, AddEvent(testing::_))
      .Times(2)
      .WillRepeatedly(testing::Invoke(
          [&events](
              const segmentation_platform::DatabaseClient::StructuredEvent&
                  structured_event) {
            events.push_back(
                std::make_pair(structured_event.event_id,
                               structured_event.metric_hash_to_value));
          }));
  segmentation_platform::TrainingRequestId test_request_id =
      segmentation_platform::TrainingRequestId::FromUnsafeValue(0);
  service_impl_->RecordAction(ScoredURLUserAction::kSeen, kSampleSearchUrl,
                              test_request_id);
  service_impl_->RecordAction(ScoredURLUserAction::kActivated, kSampleSearchUrl,
                              test_request_id);

  WaitForCollectData(test_request_id, ScoredURLUserAction::kActivated);

  clock_.Advance(
      base::Seconds(VisitedURLRankingServiceImpl::kSeenRecordDelaySec));

  WaitForCollectData(test_request_id, ScoredURLUserAction::kSeen);

  segmentation_platform::UkmMetricHash visit_id_metric_hash =
      segmentation_platform::UkmMetricHash::FromUnsafeValue(
          base::HashMetricName(kSampleSearchUrl));
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0].second.count(visit_id_metric_hash), 1u);
  EXPECT_EQ(events[1].second.count(visit_id_metric_hash), 1u);

  histogram_tester.ExpectBucketCount(
      "VisitedURLRanking.ScoredURLAction",
      static_cast<int>(ScoredURLUserAction::kSeen), 1);
  histogram_tester.ExpectBucketCount(
      "VisitedURLRanking.ScoredURLAction",
      static_cast<int>(ScoredURLUserAction::kActivated), 1);
  histogram_tester.ExpectTotalCount("VisitedURLRanking.ScoredURLAction", 2);
}

TEST_F(VisitedURLRankingServiceImplTest, RecordActionTimeout) {
  base::HistogramTester histogram_tester;
  InitService(/*data_fetchers=*/{}, /*transformers=*/{});

  std::vector<
      std::pair<segmentation_platform::UkmEventHash,
                std::map<segmentation_platform::UkmMetricHash, int64_t>>>
      events;
  EXPECT_CALL(*database_client_, AddEvent(testing::_))
      .Times(1)
      .WillRepeatedly(testing::Invoke(
          [&events](
              const segmentation_platform::DatabaseClient::StructuredEvent&
                  structured_event) {
            events.push_back(
                std::make_pair(structured_event.event_id,
                               structured_event.metric_hash_to_value));
          }));
  segmentation_platform::TrainingRequestId test_request_id =
      segmentation_platform::TrainingRequestId::FromUnsafeValue(0);
  service_impl_->RecordAction(ScoredURLUserAction::kSeen, kSampleSearchUrl,
                              test_request_id);

  clock_.Advance(
      base::Seconds(VisitedURLRankingServiceImpl::kSeenRecordDelaySec));

  WaitForCollectData(test_request_id, ScoredURLUserAction::kSeen);

  segmentation_platform::UkmMetricHash visit_id_metric_hash =
      segmentation_platform::UkmMetricHash::FromUnsafeValue(
          base::HashMetricName(kSampleSearchUrl));
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].second.count(visit_id_metric_hash), 1u);

  histogram_tester.ExpectUniqueSample(
      "VisitedURLRanking.ScoredURLAction",
      static_cast<int>(ScoredURLUserAction::kSeen), 1);
}

TEST_F(VisitedURLRankingServiceImplTest, DecorateURLVisitAggregates) {
  InitService(/*data_fetchers=*/{}, /*transformers=*/{});

  base::FieldTrialParams params = {
      {features::kVisitedURLRankingDecorationRecentlyVisitedMinutesThreshold
           .name,
       base::StringPrintf("%u", 1)}};
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{features::kVisitedURLRankingDecorations, params}}, {});

  base::Time timestamp = base::Time::Now() - base::Minutes(5);
  std::vector<URLVisitAggregate> url_visit_aggregates = {};
  const GURL kSampleUrl = GURL(kSampleSearchUrl);

  auto url_visit_aggregate1 = CreateSampleURLVisitAggregate(
      kSampleUrl, 1.0f, timestamp, {Fetcher::kHistory});
  url_visit_aggregate1.decorations.clear();
  auto* history_data = std::get_if<URLVisitAggregate::HistoryData>(
      &url_visit_aggregate1.fetcher_data_map.at(Fetcher::kHistory));
  history_data->same_time_group_visit_count = 6;
  history_data->visit_count = 6;
  url_visit_aggregates.push_back(std::move(url_visit_aggregate1));

  base::Time most_recent_timestamp = base::Time::Now();
  auto url_visit_aggregate2 = CreateSampleURLVisitAggregate(
      kSampleUrl, 1.0f, most_recent_timestamp, {Fetcher::kHistory});
  url_visit_aggregate2.decorations.clear();
  url_visit_aggregates.push_back(std::move(url_visit_aggregate2));

  visited_url_ranking::URLVisitsMetadata url_visits_metadata;
  url_visits_metadata.most_recent_timestamp = most_recent_timestamp;
  VisitedURLRankingServiceImplTest::Result result =
      RunDecorateURLVisitAggregates({}, url_visits_metadata,
                                    std::move(url_visit_aggregates));

  EXPECT_EQ(result.first, ResultStatus::kSuccess);
  EXPECT_EQ(result.second.size(), 2u);
  EXPECT_EQ(**result.second[0].GetAssociatedURLs().begin(), kSampleUrl);
  EXPECT_EQ(result.second[0].decorations.size(), 3u);
  EXPECT_EQ(GetMostRelevantDecoration(result.second[0]).GetType(),
            DecorationType::kFrequentlyVisited);
  EXPECT_EQ(GetMostRelevantDecoration(result.second[1]).GetType(),
            DecorationType::kMostRecent);
#if BUILDFLAG(IS_IOS)
  EXPECT_EQ(result.second[0].decorations[0].GetDisplayString(),
            u"You Visit Often");
  EXPECT_EQ(result.second[0].decorations[1].GetDisplayString(),
            u"You Visit Often");
  EXPECT_EQ(GetMostRelevantDecoration(result.second[1]).GetDisplayString(),
            u"Your Most Recent Tab");
  EXPECT_EQ(result.second[1].decorations[1].GetDisplayString(),
            u"You Just Visited");
#else
  EXPECT_EQ(result.second[0].decorations[0].GetDisplayString(),
            u"You visit often");
  EXPECT_EQ(result.second[0].decorations[1].GetDisplayString(),
            u"You visit often");
  EXPECT_EQ(GetMostRelevantDecoration(result.second[1]).GetDisplayString(),
            u"Your most recent tab");
  EXPECT_EQ(result.second[1].decorations[1].GetDisplayString(),
            u"You just visited");
#endif
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(result.second[0].decorations[2].GetDisplayString(),
            u"You visited 5 min ago");
#elif BUILDFLAG(IS_IOS)
  EXPECT_EQ(result.second[0].decorations[2].GetDisplayString(),
            u"You Visited 5 min ago");
#else
  EXPECT_EQ(result.second[0].decorations[2].GetDisplayString(),
            u"You visited 5 mins ago");
#endif
}

TEST_F(VisitedURLRankingServiceImplTest,
       DecorateURLVisitAggregatesVerifyNoMostRecent) {
  InitService(/*data_fetchers=*/{}, /*transformers=*/{});

  visited_url_ranking::URLVisitsMetadata url_visits_metadata;
  url_visits_metadata.most_recent_timestamp = base::Time::Now();

  std::vector<URLVisitAggregate> url_visit_aggregates = {};
  const GURL kSampleUrl = GURL(kSampleSearchUrl);

  auto url_visit_aggregate1 = CreateSampleURLVisitAggregate(
      kSampleUrl, 0.5f, base::Time::Now() - base::Minutes(1),
      {Fetcher::kHistory});
  url_visit_aggregate1.decorations.clear();

  url_visit_aggregates.push_back(std::move(url_visit_aggregate1));

  VisitedURLRankingServiceImplTest::Result result =
      RunDecorateURLVisitAggregates({}, url_visits_metadata,
                                    std::move(url_visit_aggregates));

  EXPECT_EQ(result.first, ResultStatus::kSuccess);
  EXPECT_EQ(result.second.size(), 1u);
  EXPECT_EQ(**result.second[0].GetAssociatedURLs().begin(), kSampleUrl);
  EXPECT_EQ(result.second[0].decorations.size(), 1u);
  EXPECT_EQ(GetMostRelevantDecoration(result.second[0]).GetType(),
            DecorationType::kVisitedXAgo);
}

}  // namespace visited_url_ranking
