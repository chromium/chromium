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
#include "components/visited_url_ranking/public/fetcher_config.h"
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
    const std::optional<std::string> app_id = std::nullopt,
    const base::Time visit_time = base::Time::Now()) {
  history::AnnotatedVisit annotated_visit;
  history::URLRow url_row;
  url_row.set_url(url);
  annotated_visit.url_row = std::move(url_row);
  history::VisitContentModelAnnotations model_annotations;
  model_annotations.visibility_score = visibility_score;
  history::VisitContentAnnotations content_annotations;
  content_annotations.model_annotations = std::move(model_annotations);
  annotated_visit.content_annotations = std::move(content_annotations);
  history::VisitContextAnnotations context_annotations;
  annotated_visit.context_annotations = std::move(context_annotations);
  history::VisitRow visit_row;
  visit_row.visit_id = visit_id;
  visit_row.visit_time = visit_time;
  visit_row.is_known_to_sync = true;
  visit_row.originator_cache_guid = originator_cache_guid;
  visit_row.app_id = app_id;
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
using URLType = visited_url_ranking::FetchOptions::URLType;
using ResultOption = visited_url_ranking::FetchOptions::ResultOption;

class HistoryURLVisitDataFetcherTest
    : public testing::Test,
      public ::testing::WithParamInterface<Source> {
 public:
  HistoryURLVisitDataFetcherTest() {
    mock_history_service_ = std::make_unique<MockHistoryService>();

    history_url_visit_fetcher_ = std::make_unique<HistoryURLVisitDataFetcher>(
        mock_history_service_.get());
  }

  std::vector<history::AnnotatedVisit> GetSampleAnnotatedVisits() {
    std::vector<history::AnnotatedVisit> annotated_visits = {};
    annotated_visits.emplace_back(
        SampleAnnotatedVisit(1, GURL(base::StrCat({kSampleSearchUrl, "1"})),
                             1.0f, "", "sample_app_id"));
    annotated_visits.emplace_back(
        SampleAnnotatedVisit(2, GURL(base::StrCat({kSampleSearchUrl, "2"})),
                             0.75f, "foreign_session_guid"));
    return annotated_visits;
  }

  void SetHistoryServiceExpectations(
      std::vector<history::AnnotatedVisit> annotated_visits) {
    EXPECT_CALL(*mock_history_service_,
                GetAnnotatedVisits(_, true, false, _, _))
        .WillOnce(testing::Invoke(
            [annotated_visits](
                const history::QueryOptions& options,
                bool compute_redirect_chain_start_properties,
                bool get_unclustered_visits_only,
                history::HistoryService::GetAnnotatedVisitsCallback callback,
                base::CancelableTaskTracker* tracker)
                -> base::CancelableTaskTracker::TaskId {
              std::move(callback).Run(std::move(annotated_visits));
              return 0;
            }));
  }

  FetchResult FetchAndGetResult(const FetchOptions& options) {
    FetchResult result = FetchResult(FetchResult::Status::kError, {});
    base::RunLoop wait_loop;
    history_url_visit_fetcher_->FetchURLVisitData(
        options, FetcherConfig(),
        base::BindOnce(
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
  SetHistoryServiceExpectations(GetSampleAnnotatedVisits());

  FetchOptions options = FetchOptions(
      {
          {URLType::kLocalVisit, {.age_limit = base::Days(1)}},
          {URLType::kRemoteVisit, {.age_limit = base::Days(1)}},
      },
      {
          {Fetcher::kHistory, {FetchOptions::kOriginSources}},
      },
      base::Time::Now() - base::Days(1));
  auto result = FetchAndGetResult(options);
  EXPECT_EQ(result.status, FetchResult::Status::kSuccess);
  EXPECT_EQ(result.data.size(), 2u);

  const auto entry_url = GURL(base::StrCat({kSampleSearchUrl, "1"}));
  const auto* history = std::get_if<URLVisitAggregate::HistoryData>(
      &result.data.at(entry_url.spec()));
  EXPECT_EQ(history->last_app_id, "sample_app_id");
  EXPECT_EQ(history->total_foreground_duration.InSeconds(), 0);
}

TEST_F(HistoryURLVisitDataFetcherTest,
       FetchURLVisitData_SomeDefaultVisibilyScores) {
  const float kSampleVisibilityScore = 0.75f;
  std::vector<history::AnnotatedVisit> annotated_visits = {};
  annotated_visits.emplace_back(SampleAnnotatedVisit(
      1, GURL(kSampleSearchUrl),
      history::VisitContentModelAnnotations::kDefaultVisibilityScore,
      /*originator_cache_guid=*/""));
  annotated_visits.emplace_back(
      SampleAnnotatedVisit(2, GURL(kSampleSearchUrl), kSampleVisibilityScore,
                           /*originator_cache_guid=*/""));
  SetHistoryServiceExpectations(std::move(annotated_visits));

  FetchOptions options = FetchOptions(
      {
          {URLType::kLocalVisit, {.age_limit = base::Days(1)}},
          {URLType::kRemoteVisit, {.age_limit = base::Days(1)}},
      },
      {{Fetcher::kHistory, FetchOptions::kOriginSources}},
      base::Time::Now() - base::Days(1));
  auto result = FetchAndGetResult(options);
  EXPECT_EQ(result.status, FetchResult::Status::kSuccess);
  EXPECT_EQ(result.data.size(), 1u);
  const auto* history =
      std::get_if<URLVisitAggregate::HistoryData>(&result.data.begin()->second);
  EXPECT_FLOAT_EQ(history->last_visited.content_annotations.model_annotations
                      .visibility_score,
                  kSampleVisibilityScore);
}

INSTANTIATE_TEST_SUITE_P(All,
                         HistoryURLVisitDataFetcherTest,
                         ::testing::Values(Source::kLocal, Source::kForeign));

TEST_P(HistoryURLVisitDataFetcherTest, FetchURLVisitData) {
  SetHistoryServiceExpectations(GetSampleAnnotatedVisits());

  const auto source = GetParam();
  ResultOption result_option{.age_limit = base::Days(1)};
  std::map<URLType, ResultOption> result_sources = {};
  if (source == Source::kLocal) {
    result_sources.emplace(URLType::kLocalVisit, std::move(result_option));
  } else if (source == Source::kForeign) {
    result_sources.emplace(URLType::kRemoteVisit, std::move(result_option));
  }
  std::map<Fetcher, FetchOptions::FetchSources> fetcher_sources;
  fetcher_sources.emplace(Fetcher::kHistory,
                          FetchOptions::FetchSources({source}));
  FetchOptions options =
      FetchOptions(std::move(result_sources), std::move(fetcher_sources),
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
