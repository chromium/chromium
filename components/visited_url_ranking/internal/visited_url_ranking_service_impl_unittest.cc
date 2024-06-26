// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/visited_url_ranking_service_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/testing/mock_database_client.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/segmentation_platform/public/trigger.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using segmentation_platform::HasTrainingLabel;
using segmentation_platform::MockSegmentationPlatformService;
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

  MOCK_METHOD2(FetchURLVisitData,
               void(const FetchOptions& options, FetchResultCallback callback));
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

  std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>>
  PrepareMockDataFetchers() {
    auto session_tab_data_fetcher = std::make_unique<MockURLVisitDataFetcher>();
    EXPECT_CALL(*session_tab_data_fetcher, FetchURLVisitData(_, _))
        .Times(1)
        .WillOnce(testing::Invoke([](const FetchOptions& options,
                                     URLVisitDataFetcher::FetchResultCallback
                                         callback) {
          std::map<URLMergeKey, URLVisitAggregate::URLVisitVariant> data = {};
          data.emplace(kSampleSearchUrl,
                       URLVisitAggregate::TabData(URLVisitAggregate::Tab(
                           1,
                           URLVisit(GURL(kSampleSearchUrl), u"sample_title",
                                    base::Time::Now(),
                                    syncer::DeviceInfo::FormFactor::kUnknown,
                                    URLVisit::Source::kLocal),
                           "sample_tag", "sample_session_name")));
          std::move(callback).Run(
              {FetchResult::Status::kSuccess, std::move(data)});
        }));

    std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>> data_fetchers = {};
    data_fetchers.emplace(Fetcher::kSession,
                          std::move(session_tab_data_fetcher));
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
        std::move(transformers));

    EXPECT_CALL(*segmentation_platform_service_, GetDatabaseClient())
        .WillRepeatedly(testing::Return(database_client_.get()));
  }

  ~VisitedURLRankingServiceImplTest() override {
    service_impl_ = nullptr;
    segmentation_platform_service_ = nullptr;
  }

  using Result = std::pair<ResultStatus, std::vector<URLVisitAggregate>>;
  Result RunFetchURLVisitAggregates(const FetchOptions& options) {
    Result result;
    std::vector<URLVisitAggregate> aggregates;
    base::RunLoop wait_loop;
    service_impl_->FetchURLVisitAggregates(
        options,
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
  InitService(PrepareMockDataFetchers(), /*transformers=*/{});
  FetchOptions fetch_options = FetchOptions(
      {
          {Fetcher::kSession, FetchOptions::kOriginSources},
      },
      base::Time::Now() - base::Days(1), {});
  VisitedURLRankingServiceImplTest::Result result =
      RunFetchURLVisitAggregates(fetch_options);
  EXPECT_EQ(result.first, ResultStatus::kSuccess);
  EXPECT_EQ(result.second.size(), 1u);
}

TEST_F(VisitedURLRankingServiceImplTest, FetchWhenHistoryIsNotAvailable) {
  InitService(PrepareMockDataFetchers(), /*transformers=*/{});
  FetchOptions fetch_options = FetchOptions(
      {
          {Fetcher::kHistory, FetchOptions::kOriginSources},
          {Fetcher::kSession, FetchOptions::kOriginSources},
      },
      base::Time::Now() - base::Days(1), {});
  VisitedURLRankingServiceImplTest::Result result =
      RunFetchURLVisitAggregates(fetch_options);
  EXPECT_EQ(result.first, ResultStatus::kSuccess);
  EXPECT_EQ(result.second.size(), 1u);
}

TEST_F(VisitedURLRankingServiceImplTest,
       FetchURLVisitAggregatesWithTransforms) {
  auto mock_bookmark_transformer =
      std::make_unique<MockURLVisitAggregatesTransformer>();
  EXPECT_CALL(*mock_bookmark_transformer, Transform(_, _, _))
      .Times(1)
      .WillOnce(testing::Invoke(
          [](std::vector<URLVisitAggregate> aggregates,
             const FetchOptions& options,
             URLVisitAggregatesTransformer::OnTransformCallback callback) {
            std::move(callback).Run(
                URLVisitAggregatesTransformer::Status::kSuccess,
                std::move(aggregates));
          }));

  std::map<URLVisitAggregatesTransformType,
           std::unique_ptr<URLVisitAggregatesTransformer>>
      transformers = {};
  transformers.emplace(URLVisitAggregatesTransformType::kBookmarkData,
                       std::move(mock_bookmark_transformer));
  InitService(PrepareMockDataFetchers(), std::move(transformers));

  FetchOptions fetch_options = FetchOptions(
      {
          {Fetcher::kSession, FetchOptions::kOriginSources},
      },
      base::Time::Now() - base::Days(1),
      {URLVisitAggregatesTransformType::kBookmarkData});
  VisitedURLRankingServiceImplTest::Result result =
      RunFetchURLVisitAggregates(fetch_options);
  EXPECT_EQ(result.first, ResultStatus::kSuccess);
  EXPECT_EQ(result.second.size(), 1u);
}

TEST_F(VisitedURLRankingServiceImplTest, RankURLVisitAggregates) {
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

}  // namespace visited_url_ranking
