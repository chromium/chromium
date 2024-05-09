// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/history_url_visit_data_fetcher.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {

constexpr char kSampleSearchUrl[] = "https://www.google.com/search?q=";

history::AnnotatedVisit SampleAnnotatedVisit(
    history::VisitID visit_id,
    const GURL& url,
    float visibility_score,
    const std::string& originator_cache_guid,
    const base::Time visit_time = base::Time::Now()) {
  history::VisitContentModelAnnotations model_annotations;
  model_annotations.visibility_score = visibility_score;
  history::VisitContentAnnotations content_annotations;
  content_annotations.model_annotations = std::move(model_annotations);
  history::URLRow url_row;
  url_row.set_url(url);
  history::VisitRow visit_row;
  visit_row.visit_id = visit_id;
  visit_row.visit_time = visit_time;
  visit_row.is_known_to_sync = true;
  visit_row.originator_cache_guid = originator_cache_guid;

  history::AnnotatedVisit annotated_visit;
  annotated_visit.url_row = std::move(url_row);
  annotated_visit.content_annotations = std::move(content_annotations);
  annotated_visit.visit_row = std::move(visit_row);

  return annotated_visit;
}

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService() = default;
  MockHistoryService(const MockHistoryService&) = delete;
  MockHistoryService& operator=(const MockHistoryService&) = delete;
  ~MockHistoryService() override = default;

  MOCK_CONST_METHOD5(GetAnnotatedVisits,
                     base::CancelableTaskTracker::TaskId(
                         const history::QueryOptions& options,
                         bool compute_redirect_chain_start_properties,
                         bool get_unclustered_visits_only,
                         HistoryService::GetAnnotatedVisitsCallback callback,
                         base::CancelableTaskTracker* tracker));
};

}  // namespace

namespace visited_url_ranking {

using Source = URLVisit::Source;

class HistoryURLVisitDataFetcherTest
    : public testing::Test,
      public ::testing::WithParamInterface<Source> {
 public:
  HistoryURLVisitDataFetcherTest() {
    mock_history_service_ = std::make_unique<MockHistoryService>();
    EXPECT_CALL(*mock_history_service_,
                GetAnnotatedVisits(_, true, false, _, _))
        .WillOnce(testing::Invoke(
            [](const history::QueryOptions& options,
               bool compute_redirect_chain_start_properties,
               bool get_unclustered_visits_only,
               history::HistoryService::GetAnnotatedVisitsCallback callback,
               base::CancelableTaskTracker* tracker)
                -> base::CancelableTaskTracker::TaskId {
              std::vector<history::AnnotatedVisit> annotated_visits = {};
              annotated_visits.emplace_back(SampleAnnotatedVisit(
                  1, GURL(base::StrCat({kSampleSearchUrl, "1"})), 1.0f, ""));
              annotated_visits.emplace_back(SampleAnnotatedVisit(
                  2, GURL(base::StrCat({kSampleSearchUrl, "2"})), 0.75f,
                  "foreign_session_guid"));
              std::move(callback).Run(std::move(annotated_visits));
              return 0;
            }));
    history_url_visit_fetcher_ = std::make_unique<HistoryURLVisitDataFetcher>(
        mock_history_service_->AsWeakPtr());
  }

  FetchResult FetchAndGetResult(const FetchOptions& options) {
    FetchResult result = FetchResult(FetchResult::Status::kError, {});
    base::RunLoop wait_loop;
    history_url_visit_fetcher_->FetchURLVisitData(
        options, base::BindOnce(
                     [](base::OnceClosure stop_waiting, FetchResult* result,
                        FetchResult result_arg) {
                       result->status = result_arg.status;
                       result->data = std::move(result_arg.data);
                       std::move(stop_waiting).Run();
                     },
                     wait_loop.QuitClosure(), &result));
    wait_loop.Run();
    return result;
  }

 private:
  base::test::TaskEnvironment task_env_;
  std::unique_ptr<MockHistoryService> mock_history_service_;
  std::unique_ptr<HistoryURLVisitDataFetcher> history_url_visit_fetcher_;
};

TEST_F(HistoryURLVisitDataFetcherTest, FetchURLVisitDataDefaultSources) {
  FetchOptions options =
      FetchOptions({{Fetcher::kHistory, FetchOptions::kOriginSources}},
                   base::Time::Now() - base::Days(1));
  auto result = FetchAndGetResult(options);
  EXPECT_EQ(result.status, FetchResult::Status::kSuccess);
  EXPECT_EQ(result.data.size(), 2u);
}

INSTANTIATE_TEST_SUITE_P(All,
                         HistoryURLVisitDataFetcherTest,
                         ::testing::Values(Source::kLocal, Source::kForeign));

TEST_P(HistoryURLVisitDataFetcherTest, FetchURLVisitData) {
  const auto source = GetParam();
  FetchOptions options = FetchOptions({{Fetcher::kHistory, {source}}},
                                      base::Time::Now() - base::Days(1));
  auto result = FetchAndGetResult(options);
  EXPECT_EQ(result.status, FetchResult::Status::kSuccess);
  EXPECT_EQ(result.data.size(), 1u);
  const auto* history =
      std::get_if<URLVisitAggregate::HistoryData>(&result.data.begin()->second);
  EXPECT_EQ(history->last_visited.visit_row.originator_cache_guid.empty(),
            source == Source::kLocal);
}

}  // namespace visited_url_ranking
