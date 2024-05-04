// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/visited_url_ranking_service_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace visited_url_ranking {

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

  MOCK_METHOD2(Transform,
               void(std::vector<URLVisitAggregate> aggregates,
                    OnTransformCallback callback));
};

constexpr char kSampleSearchUrl[] = "https://www.google.com/search?q=sample";

class VisitedURLRankingServiceImplTest : public testing::Test {
 public:
  VisitedURLRankingServiceImplTest() = default;

  void InitService(
      std::map<URLVisitAggregatesTransformType,
               std::unique_ptr<URLVisitAggregatesTransformer>> transformers) {
    auto session_tab_data_fetcher = std::make_unique<MockURLVisitDataFetcher>();
    EXPECT_CALL(*session_tab_data_fetcher,
                FetchURLVisitData(testing::_, testing::_))
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
    service_impl_ = std::make_unique<VisitedURLRankingServiceImpl>(
        std::move(data_fetchers), std::move(transformers));
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

 protected:
  std::unique_ptr<VisitedURLRankingServiceImpl> service_impl_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(VisitedURLRankingServiceImplTest, FetchURLVisitAggregates) {
  InitService({});
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

TEST_F(VisitedURLRankingServiceImplTest,
       FetchURLVisitAggregatesWithTransforms) {
  auto mock_bookmark_transformer =
      std::make_unique<MockURLVisitAggregatesTransformer>();
  EXPECT_CALL(*mock_bookmark_transformer, Transform(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [](std::vector<URLVisitAggregate> aggregates,
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
  InitService(std::move(transformers));

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

}  // namespace visited_url_ranking
