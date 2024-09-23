// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/history_url_visit_data_fetcher.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
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
    const std::u16string& title,
    float visibility_score,
    const std::string& originator_cache_guid,
    const std::optional<std::string> app_id = std::nullopt,
    const base::Time visit_time = base::Time::Now(),
    const base::TimeDelta visit_duration = base::Minutes(1),
    history::VisitID referring_visit_id = history::kInvalidVisitID) {
  history::AnnotatedVisit annotated_visit;
  history::URLRow url_row;
  url_row.set_url(url);
  url_row.set_title(title);
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
  visit_row.visit_duration = visit_duration;
  visit_row.referring_visit = referring_visit_id;
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

class MockDeviceInfoTracker : public syncer::DeviceInfoTracker {
 public:
  MOCK_CONST_METHOD0(IsSyncing, bool());

  MOCK_CONST_METHOD1(GetDeviceInfo,
                     syncer::DeviceInfo*(const std::string& client_id));

  MOCK_CONST_METHOD0(GetAllDeviceInfo,
                     std::vector<const syncer::DeviceInfo*>());

  MOCK_CONST_METHOD0(GetAllChromeDeviceInfo,
                     std::vector<const syncer::DeviceInfo*>());

  MOCK_METHOD1(AddObserver, void(Observer* observer));

  MOCK_METHOD1(RemoveObserver, void(Observer* observer));

  MOCK_CONST_METHOD0(CountActiveDevicesByType,
                     std::map<syncer::DeviceInfo::FormFactor, int>());

  MOCK_METHOD0(ForcePulseForTest, void());

  MOCK_CONST_METHOD1(IsRecentLocalCacheGuid,
                     bool(const std::string& cache_guid));
};

class MockDeviceInfoSyncService : public syncer::DeviceInfoSyncService {
 public:
  MockDeviceInfoSyncService() = default;
  MockDeviceInfoSyncService(const MockDeviceInfoSyncService&) = delete;
  MockDeviceInfoSyncService& operator=(const MockDeviceInfoSyncService&) =
      delete;
  ~MockDeviceInfoSyncService() override = default;

  MOCK_METHOD0(GetLocalDeviceInfoProvider, syncer::LocalDeviceInfoProvider*());

  MOCK_METHOD0(GetDeviceInfoTracker, syncer::DeviceInfoTracker*());

  MOCK_METHOD0(GetControllerDelegate,
               base::WeakPtr<syncer::DataTypeControllerDelegate>());

  MOCK_METHOD0(RefreshLocalDeviceInfo, void());
};

struct HistoryScenario {
 public:
  HistoryScenario(base::Time current_time_arg,
                  std::vector<base::Time> timestamps_arg,
                  size_t expected_same_day_group_visit_count_arg,
                  size_t expected_same_time_group_visit_count_arg)
      : current_time(std::move(current_time_arg)),
        timestamps(std::move(timestamps_arg)),
        expected_same_day_group_visit_count(
            expected_same_day_group_visit_count_arg),
        expected_same_time_group_visit_count(
            expected_same_time_group_visit_count_arg) {}
  base::Time current_time;
  std::vector<base::Time> timestamps;
  size_t expected_same_day_group_visit_count = 0;
  size_t expected_same_time_group_visit_count = 0;
};

base::Time GetStartOfDay(base::Time time) {
  base::Time::Exploded time_exploded;
  time.LocalExplode(&time_exploded);
  time_exploded.hour = 0;
  time_exploded.minute = 0;
  time_exploded.second = 0;
  time_exploded.millisecond = 0;

  if (base::Time::FromLocalExploded(time_exploded, &time)) {
    return time;
  }

  return base::Time();
}

const HistoryScenario SampleScenario_OverlappingTimeGroup() {
  base::Time today_mid_of_day =
      GetStartOfDay(base::Time::Now()) + base::Hours(12);
  std::vector<base::Time> timestamps;
  timestamps.push_back(today_mid_of_day + base::Hours(1));
  timestamps.push_back(today_mid_of_day + base::Hours(2));

  return {std::move(today_mid_of_day), std::move(timestamps), 2, 2};
}

const HistoryScenario SampleScenario_NonOverlappingTimeGroup() {
  base::Time today_mid_of_day =
      GetStartOfDay(base::Time::Now()) + base::Hours(12);

  // The current day is split into four time groups of 6 hours each. The third
  // group starts at exactly 12 PM, thus, the following two timestamps will
  // belong to the prior time group.
  std::vector<base::Time> timestamps;
  timestamps.push_back(today_mid_of_day - base::Hours(1));
  timestamps.push_back(today_mid_of_day - base::Hours(2));

  return {std::move(today_mid_of_day), std::move(timestamps), 2, 0};
}

}  // namespace

namespace visited_url_ranking {

using Source = URLVisit::Source;
using URLType = visited_url_ranking::FetchOptions::URLType;
using ResultOption = visited_url_ranking::FetchOptions::ResultOption;

constexpr char kSampleForeignDeviceGUID[] = "foreign_guid";
constexpr char kSampleForeignDeviceClientName[] = "Windows PC";
const syncer::DeviceInfo kSampleForeignDeviceInfo{
    kSampleForeignDeviceGUID,
    kSampleForeignDeviceClientName,
    "",
    "",
    sync_pb::SyncEnums_DeviceType_TYPE_WIN,
    syncer::DeviceInfo::OsType::kWindows,
    syncer::DeviceInfo::FormFactor::kDesktop,
    "",
    "",
    "",
    "",
    base::Time::Now(),
    base::Seconds(1),
    false,
    sync_pb::
        SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
    std::nullopt,
    std::nullopt,
    "",
    {},
    std::nullopt};

class HistoryURLVisitDataFetcherTest : public testing::Test {
 public:
  HistoryURLVisitDataFetcherTest() {
    clock_.SetNow(base::Time::Now());

    mock_history_service_ = std::make_unique<MockHistoryService>();
    mock_device_info_tracker_ = std::make_unique<MockDeviceInfoTracker>();
    mock_device_info_sync_service_ =
        std::make_unique<MockDeviceInfoSyncService>();
    EXPECT_CALL(*mock_device_info_sync_service_, GetDeviceInfoTracker())
        .WillRepeatedly(testing::Return(mock_device_info_tracker_.get()));

    history_url_visit_fetcher_ = std::make_unique<HistoryURLVisitDataFetcher>(
        mock_history_service_.get(), mock_device_info_sync_service_.get());
  }

  FetchOptions GetSampleFetchOptions() {
    return FetchOptions(
        {
            {URLType::kLocalVisit, {.age_limit = base::Days(1)}},
            {URLType::kRemoteVisit, {.age_limit = base::Days(1)}},
        },
        {{Fetcher::kHistory, FetchOptions::kOriginSources}},
        base::Time::Now() - base::Days(1));
  }

  std::vector<history::AnnotatedVisit> GetSampleAnnotatedVisits() {
    std::vector<history::AnnotatedVisit> annotated_visits;
    annotated_visits.emplace_back(
        SampleAnnotatedVisit(1, GURL(base::StrCat({kSampleSearchUrl, "1"})),
                             u"Search 1", 1.0f, "", "sample_app_id"));
    annotated_visits.emplace_back(
        SampleAnnotatedVisit(2, GURL(base::StrCat({kSampleSearchUrl, "2"})),
                             u"Search 2", 0.75f, kSampleForeignDeviceGUID));
    return annotated_visits;
  }

  std::vector<history::AnnotatedVisit> GetSampleAnnotatedVisitsForScenario(
      const HistoryScenario& scenario) {
    std::vector<history::AnnotatedVisit> annotated_visits;
    for (size_t i = 0; i < scenario.timestamps.size(); i++) {
      annotated_visits.emplace_back(SampleAnnotatedVisit(
          i + 1, GURL(kSampleSearchUrl), base::NumberToString16(i), 1.0f, "",
          "", scenario.timestamps[i]));
    }

    return annotated_visits;
  }

  void SetDeviceInfoTrackerExpectations() {
    std::vector<const syncer::DeviceInfo*> device_infos;
    device_infos.push_back(&kSampleForeignDeviceInfo);
    EXPECT_CALL(*mock_device_info_tracker_, GetAllDeviceInfo())
        .WillOnce(testing::Return(device_infos));
    EXPECT_CALL(*mock_device_info_tracker_,
                IsRecentLocalCacheGuid(kSampleForeignDeviceGUID))
        .WillRepeatedly(testing::Return(false));
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
        options, FetcherConfig(&clock_),
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

 protected:
  base::SimpleTestClock clock_;

 private:
  base::test::TaskEnvironment task_env_;
  std::unique_ptr<MockHistoryService> mock_history_service_;
  std::unique_ptr<MockDeviceInfoTracker> mock_device_info_tracker_;
  std::unique_ptr<MockDeviceInfoSyncService> mock_device_info_sync_service_;
  std::unique_ptr<HistoryURLVisitDataFetcher> history_url_visit_fetcher_;
};

TEST_F(HistoryURLVisitDataFetcherTest, FetchURLVisitDataDefaultSources) {
  SetDeviceInfoTrackerExpectations();
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
  EXPECT_EQ(history->visit.source, URLVisit::Source::kLocal);

  const GURL visit2_url = GURL(base::StrCat({kSampleSearchUrl, "2"}));
  const auto* history_data2 = std::get_if<URLVisitAggregate::HistoryData>(
      &result.data.at(visit2_url.spec()));
  EXPECT_EQ(history_data2->visit.url, visit2_url);
  EXPECT_EQ(history_data2->visit.title, u"Search 2");
  EXPECT_EQ(history_data2->visit.client_name, kSampleForeignDeviceClientName);
  EXPECT_EQ(history_data2->visit.device_type,
            syncer::DeviceInfo::FormFactor::kDesktop);
  EXPECT_EQ(history_data2->visit.source, URLVisit::Source::kForeign);
}

TEST_F(HistoryURLVisitDataFetcherTest,
       FetchURLVisitData_SomeDefaultVisibilyScores) {
  base::HistogramTester histogram_tester;

  const float kSampleVisibilityScore = 0.75f;
  std::vector<history::AnnotatedVisit> annotated_visits;
  annotated_visits.emplace_back(SampleAnnotatedVisit(
      1, GURL(kSampleSearchUrl), u"Search 1",
      history::VisitContentModelAnnotations::kDefaultVisibilityScore,
      /*originator_cache_guid=*/""));
  annotated_visits.emplace_back(SampleAnnotatedVisit(
      2, GURL(kSampleSearchUrl), u"Search 2", kSampleVisibilityScore,
      /*originator_cache_guid=*/""));
  SetHistoryServiceExpectations(std::move(annotated_visits));

  auto result = FetchAndGetResult(GetSampleFetchOptions());
  EXPECT_EQ(result.status, FetchResult::Status::kSuccess);
  EXPECT_EQ(result.data.size(), 1u);
  const auto* history =
      std::get_if<URLVisitAggregate::HistoryData>(&result.data.begin()->second);
  EXPECT_FLOAT_EQ(history->last_visited.content_annotations.model_annotations
                      .visibility_score,
                  kSampleVisibilityScore);
  EXPECT_EQ(history->visit_count, 2u);

  histogram_tester.ExpectUniqueSample(
      "VisitedURLRanking.Fetch.History.Filter.ZeroDurationVisits."
      "InOutPercentage",
      100, 1);
}

TEST_F(HistoryURLVisitDataFetcherTest,
       FetchURLVisitData_RemoveZeroDurationVisitURLs) {
  base::HistogramTester histogram_tester;

  std::vector<history::AnnotatedVisit> annotated_visits;
  annotated_visits.emplace_back(
      SampleAnnotatedVisit(1, GURL("http://gmail.com/"), /*title=*/u"Gmail",
                           /*visibility_score=*/1.0,
                           /*originator_cache_guid=*/"",
                           /*app_id=*/std::nullopt, base::Time::Now(),
                           /*visit_duration=*/base::Milliseconds(0)));
  annotated_visits.emplace_back(SampleAnnotatedVisit(
      2, GURL("https://gmail.com/"), /*title=*/u"Gmail",
      /*visibility_score=*/1.0f,
      /*originator_cache_guid=*/"", /*app_id=*/std::nullopt, base::Time::Now(),
      /*visit_duration=*/base::Milliseconds(0),
      /*referring_visit_id=*/1));
  annotated_visits.emplace_back(SampleAnnotatedVisit(
      3, GURL("https://mail.google.com/mail/u/0/"), /*title=*/u"Gmail",
      /*visibility_score=*/1.0f,
      /*originator_cache_guid=*/"", /*app_id=*/std::nullopt, base::Time::Now(),
      /*visit_duration=*/base::Milliseconds(0),
      /*referring_visit_id=*/2));
  annotated_visits.emplace_back(SampleAnnotatedVisit(
      4, GURL("https://mail.google.com/mail/u/0/#inbox"),
      /*title=*/u"Gmail Inbox",
      /*visibility_score=*/1.0f,
      /*originator_cache_guid=*/"", /*app_id=*/std::nullopt, base::Time::Now(),
      /*visit_duration=*/base::Minutes(1),
      /*referring_visit_id=*/3));
  SetHistoryServiceExpectations(std::move(annotated_visits));

  auto result = FetchAndGetResult(GetSampleFetchOptions());
  EXPECT_EQ(result.status, FetchResult::Status::kSuccess);
  EXPECT_EQ(result.data.size(), 1u);

  histogram_tester.ExpectUniqueSample(
      "VisitedURLRanking.Fetch.History.Filter.ZeroDurationVisits."
      "InOutPercentage",
      25, 1);
}

class HistoryURLVisitDataFetcherSourcesTest
    : public HistoryURLVisitDataFetcherTest,
      public ::testing::WithParamInterface<Source> {};

INSTANTIATE_TEST_SUITE_P(All,
                         HistoryURLVisitDataFetcherSourcesTest,
                         ::testing::Values(Source::kLocal, Source::kForeign));

TEST_P(HistoryURLVisitDataFetcherSourcesTest, FetchURLVisitData) {
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

class HistoryURLVisitDataFetcherDataTest
    : public HistoryURLVisitDataFetcherTest,
      public ::testing::WithParamInterface<HistoryScenario> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    HistoryURLVisitDataFetcherDataTest,
    ::testing::Values(SampleScenario_OverlappingTimeGroup(),
                      SampleScenario_NonOverlappingTimeGroup()));

TEST_P(HistoryURLVisitDataFetcherDataTest, FetchURLVisitData_AggregateCounts) {
  const auto scenario = GetParam();
  clock_.SetNow(scenario.current_time);
  SetHistoryServiceExpectations(GetSampleAnnotatedVisitsForScenario(scenario));

  auto result = FetchAndGetResult(GetSampleFetchOptions());
  EXPECT_EQ(result.status, FetchResult::Status::kSuccess);
  EXPECT_EQ(result.data.size(), 1u);
  const auto* history =
      std::get_if<URLVisitAggregate::HistoryData>(&result.data.begin()->second);
  EXPECT_EQ(history->visit_count, scenario.timestamps.size());
  EXPECT_EQ(history->same_day_group_visit_count,
            scenario.expected_same_day_group_visit_count);
  EXPECT_EQ(history->same_time_group_visit_count,
            scenario.expected_same_time_group_visit_count);
}

}  // namespace visited_url_ranking
