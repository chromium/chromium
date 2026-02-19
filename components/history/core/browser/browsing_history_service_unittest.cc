// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/browsing_history_service.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "components/history/core/browser/browsing_history_driver.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/sync/test/mock_sync_service.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::Time;

namespace history {

using HistoryEntry = BrowsingHistoryService::HistoryEntry;

void PrintTo(const HistoryEntry& entry, std::ostream* os) {
  *os << "{url: " << entry.url << ", time: " << entry.time
      << ", entry_type: " << entry.entry_type
      << ", remote_icon_url_for_uma: " << entry.remote_icon_url_for_uma
      << ", is_actor_visit: " << entry.is_actor_visit << "}";
}

void PrintTo(const BrowsingHistoryService::QueryResultsInfo& info,
             std::ostream* os) {
  *os << "{reached_beginning: " << info.reached_beginning
      << ", sync_timed_out: " << info.sync_timed_out << "}";
}

namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;

const char kUrl1[] = "http://www.one.com";
const char kUrl2[] = "http://www.two.com";
const char kUrl3[] = "http://www.three.com";
const char kUrl4[] = "http://www.four.com";
const char kUrl5[] = "http://www.five.com";
const char kUrl6[] = "http://www.six.com";
const char kUrl7[] = "http://www.seven.com";
const char kUrl8[] = "http://eight.com";
const char kUrl9[] = "http://nine.com/eight.com";
const char kUrl10[] = "http://ten.com/eight";
const char kIconUrl1[] = "http://www.one.com/favicon.ico";

const HistoryEntry::EntryType kLocal = HistoryEntry::LOCAL_ENTRY;
const HistoryEntry::EntryType kRemote = HistoryEntry::REMOTE_ENTRY;
const HistoryEntry::EntryType kBoth = HistoryEntry::COMBINED_ENTRY;

Time OffsetToTimeWithBaseline(base::Time baseline_time, int64_t hour_offset) {
  return baseline_time + base::Hours(hour_offset);
}

struct TestResult {
  std::string url;
  int64_t hour_offset;  // Visit time in hours past the baseline time.
  HistoryEntry::EntryType type;
  std::string remote_icon_url_for_uma;
  VisitSource visit_source = VisitSource::SOURCE_BROWSED;
  bool is_actor_visit = false;
};

void PrintTo(const TestResult& result, std::ostream* os) {
  *os << "{url: " << result.url << ", hour_offset: " << result.hour_offset
      << ", type: " << result.type
      << ", remote_icon_url_for_uma: " << result.remote_icon_url_for_uma
      << ", visit_source: " << static_cast<int>(result.visit_source) << "}";
}

MATCHER_P2(MatchesHistory, baseline_time, expected, "") {
  return arg.url == GURL(expected.url) &&
         arg.time ==
             OffsetToTimeWithBaseline(baseline_time, expected.hour_offset) &&
         arg.entry_type == expected.type &&
         arg.remote_icon_url_for_uma ==
             GURL(expected.remote_icon_url_for_uma) &&
         arg.is_actor_visit ==
             (expected.visit_source == VisitSource::SOURCE_ACTOR);
}

MATCHER_P3(MatchesQueryResult,
           baseline_time,
           reached_beginning,
           expected_entries,
           "") {
  const BrowsingHistoryService::QueryResultsInfo& info = arg.second;
  if (info.reached_beginning != reached_beginning) {
    *result_listener << "where reached_beginning should be "
                     << reached_beginning;
    return false;
  }
  if (info.sync_timed_out) {
    *result_listener << "where sync_timed_out should be false";
    return false;
  }

  const std::vector<BrowsingHistoryService::HistoryEntry>& entries = arg.first;
  std::vector<testing::Matcher<const HistoryEntry&>> matchers;
  for (const auto& entry : expected_entries) {
    matchers.push_back(MatchesHistory(baseline_time, entry));
  }
  return ExplainMatchResult(ElementsAreArray(matchers), entries,
                            result_listener);
}

class TestBrowsingHistoryDriver : public BrowsingHistoryDriver {
 public:
  explicit TestBrowsingHistoryDriver(WebHistoryService* web_history)
      : web_history_(web_history) {}

  void SetWebHistory(WebHistoryService* web_history) {
    web_history_ = web_history;
  }

  using QueryResult = std::pair<std::vector<HistoryEntry>,
                                BrowsingHistoryService::QueryResultsInfo>;
  std::vector<QueryResult> GetQueryResults() { return query_results_; }

  int GetHistoryDeletedCount() { return history_deleted_count_; }

  void RunContinuation() {
    EXPECT_TRUE(continuation_closure_);
    std::move(continuation_closure_).Run();
  }

 private:
  // BrowsingHistoryDriver implementation.
  void OnQueryComplete(
      const std::vector<HistoryEntry>& results,
      const BrowsingHistoryService::QueryResultsInfo& query_results_info,
      base::OnceClosure continuation_closure) override {
    query_results_.push_back(QueryResult(results, query_results_info));
    continuation_closure_ = std::move(continuation_closure);
  }
  void OnRemoveVisitsComplete() override {}
  void OnRemoveVisitsFailed() override {}
  void OnRemoveVisits(
      const std::vector<ExpireHistoryArgs>& expire_list) override {}
  void HistoryDeleted() override { history_deleted_count_++; }
  void HasOtherFormsOfBrowsingHistory(bool has_other_forms,
                                      bool has_synced_results) override {}
  bool AllowHistoryDeletions() override { return true; }
  bool ShouldHideWebHistoryUrl(const GURL& url) override { return false; }
  WebHistoryService* GetWebHistoryService() override { return web_history_; }
  void ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      const syncer::SyncService* sync_service,
      WebHistoryService* local_history,
      base::OnceCallback<void(bool)> callback) override {}

  int history_deleted_count_ = 0;
  std::vector<QueryResult> query_results_;
  base::OnceClosure continuation_closure_;
  raw_ptr<WebHistoryService> web_history_;
};

class TestWebHistoryService : public FakeWebHistoryService {
 public:
  TestWebHistoryService() = default;

  void TriggerOnWebHistoryDeleted() {
    TestRequest request;
    ExpireHistoryCompletionCallback(base::DoNothing(), &request, true);
  }

 protected:
  class TestRequest : public WebHistoryService::Request {
   private:
    // WebHistoryService::Request implementation.
    bool IsPending() const override { return false; }
    int GetResponseCode() const override { return net::HTTP_OK; }
    const std::string& GetResponseBody() const override { return body_; }
    void SetPostData(const std::string& post_data) override {}
    void SetPostDataAndType(const std::string& post_data,
                            const std::string& mime_type) override {}
    void SetUserAgent(const std::string& user_agent) override {}
    void Start() override {}

    std::string body_ = "{}";
  };
};

class ReversedWebHistoryService : public TestWebHistoryService {
 private:
  std::vector<FakeWebHistoryService::Visit> GetVisitsBetween(
      base::Time begin,
      base::Time end,
      size_t count,
      bool* more_results_left) override {
    auto result = FakeWebHistoryService::GetVisitsBetween(begin, end, count,
                                                          more_results_left);
    std::ranges::reverse(result);
    return result;
  }
};

class TimeoutWebHistoryService : public TestWebHistoryService {
 private:
  // WebHistoryService implementation.
  std::unique_ptr<Request> CreateRequest(
      const GURL& url,
      CompletionCallback callback,
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      override {
    return std::make_unique<TestWebHistoryService::TestRequest>();
  }
};

class TestBrowsingHistoryService : public BrowsingHistoryService {
 public:
  TestBrowsingHistoryService(BrowsingHistoryDriver* driver,
                             HistoryService* local_history,
                             syncer::SyncService* sync_service,
                             std::unique_ptr<base::OneShotTimer> timer)
      : BrowsingHistoryService(driver,
                               local_history,
                               sync_service,
                               std::move(timer)) {}
};

// The param determines whether the feature `kHistoryQueryOnlyLocalFirst` is
// enabled.
class BrowsingHistoryServiceTest : public ::testing::TestWithParam<bool> {
 protected:
  // WebHistory API is to pass time ranges as the number of microseconds since
  // Time::UnixEpoch() as a query parameter. This becomes a problem when we use
  // Time::LocalMidnight(), which rounds _down_, and will result in a  time
  // before Time::UnixEpoch() that cannot be represented. By adding 1 day we
  // ensure all test data is after Time::UnixEpoch().
  BrowsingHistoryServiceTest()
      : baseline_time_(Time::UnixEpoch().LocalMidnight() + base::Days(1)),
        driver_(&web_history_) {
    features_.InitWithFeatureState(kHistoryQueryOnlyLocalFirst, GetParam());

    EXPECT_TRUE(history_dir_.CreateUniqueTempDir());
    local_history_ = CreateHistoryService(history_dir_.GetPath(), true);
    ResetService(driver(), local_history(), sync());
  }

  void ResetService(BrowsingHistoryDriver* driver,
                    HistoryService* local_history,
                    syncer::SyncService* sync_service) {
    std::unique_ptr<base::MockOneShotTimer> timer =
        std::make_unique<base::MockOneShotTimer>();
    timer_ = timer.get();
    browsing_history_service_ = std::make_unique<TestBrowsingHistoryService>(
        driver, local_history, sync_service, std::move(timer));
  }

  void BlockUntilHistoryProcessesPendingRequests() {
    history::BlockUntilHistoryProcessesPendingRequests(local_history());
  }

  Time OffsetToTime(int64_t hour_offset) {
    return OffsetToTimeWithBaseline(baseline_time_, hour_offset);
  }

  void AddHistory(const std::vector<TestResult>& data,
                  TestWebHistoryService* web_history) {
    for (const TestResult& entry : data) {
      if (entry.type == kLocal) {
        VisitSource source =
            entry.is_actor_visit ? SOURCE_ACTOR : entry.visit_source;
        local_history()->AddPage(GURL(entry.url),
                                 OffsetToTime(entry.hour_offset), source);
      } else if (entry.type == kRemote) {
        web_history->AddSyncedVisit(entry.url, OffsetToTime(entry.hour_offset),
                                    entry.remote_icon_url_for_uma);
      } else {
        NOTREACHED();
      }
    }
  }

  void AddHistory(const std::vector<TestResult>& data) {
    AddHistory(data, web_history());
  }

  HistoryEntry CreateEntry(const std::string& url,
                           const std::vector<int>& hour_offsets) {
    HistoryEntry entry;
    entry.url = GURL(url);
    for (int hour_offset : hour_offsets) {
      entry.all_timestamps[entry.url].insert(OffsetToTime(hour_offset));
    }
    return entry;
  }

  TestBrowsingHistoryDriver::QueryResult QueryHistory(size_t max_count = 0) {
    QueryOptions options;
    options.max_count = max_count;
    return QueryHistory(options);
  }

  TestBrowsingHistoryDriver::QueryResult QueryHistory(
      const QueryOptions& options) {
    return QueryHistory(std::u16string(), options);
  }

  TestBrowsingHistoryDriver::QueryResult QueryHistory(
      const std::u16string& query_text,
      const QueryOptions& options) {
    size_t previous_results_count = driver()->GetQueryResults().size();
    service()->QueryHistory(query_text, options);
    BlockUntilHistoryProcessesPendingRequests();
    const std::vector<TestBrowsingHistoryDriver::QueryResult> all_results =
        driver()->GetQueryResults();
    EXPECT_EQ(previous_results_count + 1, all_results.size());
    return *all_results.rbegin();
  }

  TestBrowsingHistoryDriver::QueryResult ContinueQuery() {
    size_t previous_results_count = driver()->GetQueryResults().size();
    driver()->RunContinuation();
    BlockUntilHistoryProcessesPendingRequests();
    const std::vector<TestBrowsingHistoryDriver::QueryResult> all_results =
        driver()->GetQueryResults();
    EXPECT_EQ(previous_results_count + 1, all_results.size());
    return *all_results.rbegin();
  }

  HistoryService* local_history() { return local_history_.get(); }
  TestWebHistoryService* web_history() { return &web_history_; }
  syncer::MockSyncService* sync() { return &sync_service_; }
  TestBrowsingHistoryDriver* driver() { return &driver_; }
  base::MockOneShotTimer* timer() { return timer_; }
  TestBrowsingHistoryService* service() {
    return browsing_history_service_.get();
  }

 protected:
  // Duplicates on the same day in the local timezone are removed, so set a
  // baseline time in local time.
  Time baseline_time_;

 private:
  base::test::ScopedFeatureList features_;

  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir history_dir_;
  std::unique_ptr<HistoryService> local_history_;
  TestWebHistoryService web_history_;
  syncer::MockSyncService sync_service_;
  TestBrowsingHistoryDriver driver_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> timer_;
  std::unique_ptr<TestBrowsingHistoryService> browsing_history_service_;
};

INSTANTIATE_TEST_SUITE_P(,
                         BrowsingHistoryServiceTest,
                         testing::Bool(),
                         [](testing::TestParamInfo<bool> param_info) {
                           return param_info.param ? "QueryLocalFirst"
                                                   : "QueryInParallel";
                         });

TEST_P(BrowsingHistoryServiceTest, QueryHistoryExcludes404s) {
  // Allow saving 404 visits to History.
  base::test::ScopedFeatureList scoped_featurelist;
  scoped_featurelist.InitAndEnableFeature(history::kVisitedLinksOn404);

  // Add a non-404 visit.
  AddHistory({{kUrl1, 1, kLocal}});

  // Add a 404 visit.
  HistoryAddPageArgs page_404_args;
  page_404_args.url = GURL(kUrl2);
  page_404_args.time = OffsetToTime(2);
  page_404_args.context_annotations = {.response_code = 404};
  local_history()->AddPage(page_404_args);

  BlockUntilHistoryProcessesPendingRequests();

  // 404s should be excluded from query results.
  EXPECT_THAT(QueryHistory(),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{{kUrl1, 1, kLocal}}));
}

TEST_P(BrowsingHistoryServiceTest, QueryHistoryNoSources) {
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), nullptr, nullptr);
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{}));
}

TEST_P(BrowsingHistoryServiceTest, EmptyQueryHistoryJustLocal) {
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), local_history(), nullptr);
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{}));
}

TEST_P(BrowsingHistoryServiceTest, QueryHistoryJustLocal) {
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), local_history(), nullptr);
  AddHistory({{kUrl1, 1, kLocal}});
  EXPECT_THAT(QueryHistory(),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{{kUrl1, 1, kLocal}}));
}

TEST_P(BrowsingHistoryServiceTest, EmptyQueryHistoryJustWeb) {
  ResetService(driver(), nullptr, nullptr);
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{}));
}

TEST_P(BrowsingHistoryServiceTest, EmptyQueryHistoryDelayedWeb) {
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), nullptr, sync());
  driver()->SetWebHistory(web_history());
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{}));
}

TEST_P(BrowsingHistoryServiceTest, QueryHistoryJustWeb) {
  ResetService(driver(), nullptr, sync());
  AddHistory({{kUrl1, 1, kRemote}});
  EXPECT_THAT(QueryHistory(),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{{kUrl1, 1, kRemote}}));
}

TEST_P(BrowsingHistoryServiceTest, EmptyQueryHistoryBothSources) {
  ResetService(driver(), local_history(), sync());
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{}));
}

TEST_P(BrowsingHistoryServiceTest, QueryHistoryAllSources) {
  ResetService(driver(), local_history(), sync());
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl2, 2, kRemote},
              {kUrl3, 3, kLocal},
              {kUrl1, 4, kLocal}});
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{
                                                     {kUrl1, 4, kBoth},
                                                     {kUrl3, 3, kLocal},
                                                     {kUrl2, 2, kRemote},
                                                 }));
}

TEST_P(BrowsingHistoryServiceTest, QueryHistoryLocalTimeRanges) {
  AddHistory({{kUrl1, 1, kLocal},
              {kUrl2, 2, kLocal},
              {kUrl3, 3, kLocal},
              {kUrl4, 4, kLocal}});
  QueryOptions options;
  options.begin_time = OffsetToTime(2);
  options.end_time = OffsetToTime(4);
  // Having a `reached_beginning` value of false here seems
  // counterintuitive. Seems to be for paging by `begin_time` instead of
  // `count`. If the local history implementation changes, feel free to update
  // this value, all this test cares about is that BrowsingHistoryService passes
  // the values through correctly.
  EXPECT_THAT(QueryHistory(options),
              MatchesQueryResult(baseline_time_,
                                 /*reached_beginning*/ false,
                                 std::vector<TestResult>{
                                     {kUrl3, 3, kLocal},
                                     {kUrl2, 2, kLocal},
                                 }));
}

TEST_P(BrowsingHistoryServiceTest, QueryHistoryRemoteTimeRanges) {
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl2, 2, kRemote},
              {kUrl3, 3, kRemote},
              {kUrl4, 4, kRemote}});
  QueryOptions options;
  options.begin_time = OffsetToTime(2);
  options.end_time = OffsetToTime(4);
  EXPECT_THAT(QueryHistory(options),
              MatchesQueryResult(baseline_time_,
                                 /*reached_beginning*/ true,
                                 std::vector<TestResult>{
                                     {kUrl3, 3, kRemote},
                                     {kUrl2, 2, kRemote},
                                 }));
}

TEST_P(BrowsingHistoryServiceTest, QueryHistoryHostOnlyRemote) {
  AddHistory({{kUrl8, 1, kRemote}, {kUrl9, 2, kRemote}, {kUrl10, 3, kRemote}});

  QueryOptions options;
  options.max_count = 0;
  options.host_only = false;
  EXPECT_THAT(QueryHistory(u"eight.com", options),
              MatchesQueryResult(baseline_time_,
                                 /*reached_beginning*/ true,
                                 std::vector<TestResult>{
                                     {kUrl10, 3, kRemote},
                                     {kUrl9, 2, kRemote},
                                     {kUrl8, 1, kRemote},
                                 }));
  options.host_only = true;
  EXPECT_THAT(QueryHistory(u"eight.com", options),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{
                                     {kUrl8, 1, kRemote},
                                 }));
}

TEST_P(BrowsingHistoryServiceTest, QueryHistoryLocalPagingPartial) {
  AddHistory({{kUrl1, 1, kLocal}, {kUrl2, 2, kLocal}, {kUrl3, 3, kLocal}});
  EXPECT_THAT(QueryHistory(2), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ false,
                                                  std::vector<TestResult>{
                                                      {kUrl3, 3, kLocal},
                                                      {kUrl2, 2, kLocal},
                                                  }));

  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{
                                                      {kUrl1, 1, kLocal},
                                                  }));
}

TEST_P(BrowsingHistoryServiceTest, QueryHistoryLocalPagingFull) {
  AddHistory({{kUrl1, 1, kLocal}, {kUrl2, 2, kLocal}, {kUrl3, 3, kLocal}});
  // With `kHistoryQueryOnlyLocalFirst`, the first query doesn't reach the
  // beginning, since there were just enough local results to fulfill the
  // request and remote hasn't been queried yet.
  bool reached_beginning =
      !base::FeatureList::IsEnabled(kHistoryQueryOnlyLocalFirst);
  EXPECT_THAT(QueryHistory(3),
              MatchesQueryResult(baseline_time_,
                                 /*reached_beginning*/ reached_beginning,
                                 std::vector<TestResult>{
                                     {kUrl3, 3, kLocal},
                                     {kUrl2, 2, kLocal},
                                     {kUrl1, 1, kLocal},
                                 }));

  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{}));
}

TEST_P(BrowsingHistoryServiceTest, QueryHistoryRemotePagingPartial) {
  AddHistory({{kUrl1, 1, kRemote}, {kUrl2, 2, kRemote}, {kUrl3, 3, kRemote}});
  EXPECT_THAT(QueryHistory(2), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ false,
                                                  std::vector<TestResult>{
                                                      {kUrl3, 3, kRemote},
                                                      {kUrl2, 2, kRemote},
                                                  }));

  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{
                                                      {kUrl1, 1, kRemote},
                                                  }));
}

TEST_P(BrowsingHistoryServiceTest, QueryHistoryRemotePagingFull) {
  AddHistory({{kUrl1, 1, kRemote}, {kUrl2, 2, kRemote}, {kUrl3, 3, kRemote}});
  // Note: As opposed to QueryHistoryLocalPagingFull, here both local and remote
  // reach the beginning. The local query returns no results, so remote gets
  // queried immediately and returns all the existing results.
  EXPECT_THAT(QueryHistory(3), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{
                                                      {kUrl3, 3, kRemote},
                                                      {kUrl2, 2, kRemote},
                                                      {kUrl1, 1, kRemote},
                                                  }));
  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{}));
}

TEST_P(BrowsingHistoryServiceTest, MergeDuplicatesSameDay) {
  AddHistory({{kUrl1, 0, kRemote},
              {kUrl2, 1, kRemote},
              {kUrl1, 2, kRemote},
              {kUrl1, 3, kRemote}});
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{
                                                     {kUrl1, 3, kRemote},
                                                     {kUrl2, 1, kRemote},
                                                 }));
}

TEST_P(BrowsingHistoryServiceTest, MergeDuplicatesNextDayNotRemoved) {
  AddHistory({{kUrl1, 0, kRemote}, {kUrl1, 23, kRemote}, {kUrl1, 24, kRemote}});
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{
                                                     {kUrl1, 24, kRemote},
                                                     {kUrl1, 23, kRemote},
                                                 }));
}

TEST_P(BrowsingHistoryServiceTest, MergeDuplicatesMultipleDays) {
  AddHistory({{kUrl2, 0, kRemote},
              {kUrl1, 1, kRemote},
              {kUrl2, 2, kRemote},
              {kUrl1, 3, kRemote},
              {kUrl2, 24, kRemote},
              {kUrl1, 25, kRemote},
              {kUrl2, 26, kRemote},
              {kUrl1, 27, kRemote}});
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{
                                                     {kUrl1, 27, kRemote},
                                                     {kUrl2, 26, kRemote},
                                                     {kUrl1, 3, kRemote},
                                                     {kUrl2, 2, kRemote},
                                                 }));
}

TEST_P(BrowsingHistoryServiceTest, MergeDuplicatesVerifyTimestamps) {
  AddHistory({{kUrl1, 0, kRemote},
              {kUrl2, 1, kRemote},
              {kUrl1, 2, kRemote},
              {kUrl1, 3, kRemote}});
  auto results = QueryHistory();
  EXPECT_THAT(results, MatchesQueryResult(baseline_time_,
                                          /*reached_beginning*/ true,
                                          std::vector<TestResult>{
                                              {kUrl1, 3, kRemote},
                                              {kUrl2, 1, kRemote},
                                          }));
  EXPECT_EQ(3U, results.first[0].all_timestamps[GURL(kUrl1)].size());
  EXPECT_EQ(1U, results.first[1].all_timestamps[GURL(kUrl2)].size());
}

TEST_P(BrowsingHistoryServiceTest, MergeDuplicatesKeepNonEmptyIconUrl) {
  AddHistory({{kUrl1, 0, kRemote, kIconUrl1}, {kUrl1, 1, kLocal}});
  EXPECT_THAT(QueryHistory(),
              MatchesQueryResult(
                  baseline_time_,
                  /*reached_beginning*/ true,
                  std::vector<TestResult>{{kUrl1, 1, kBoth, kIconUrl1}}));

  AddHistory({{kUrl1, 0, kLocal}, {kUrl1, 1, kRemote, kIconUrl1}});
  EXPECT_THAT(QueryHistory(),
              MatchesQueryResult(
                  baseline_time_,
                  /*reached_beginning*/ true,
                  std::vector<TestResult>{{kUrl1, 1, kBoth, kIconUrl1}}));
}

TEST_P(BrowsingHistoryServiceTest, QueryHistoryMerge) {
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl2, 2, kRemote},
              {kUrl3, 3, kLocal},
              {kUrl1, 4, kLocal}});
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{
                                                     {kUrl1, 4, kBoth},
                                                     {kUrl3, 3, kLocal},
                                                     {kUrl2, 2, kRemote},
                                                 }));
}

TEST_P(BrowsingHistoryServiceTest, QueryHistoryPending) {
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl2, 2, kRemote},
              {kUrl3, 3, kLocal},
              {kUrl4, 4, kLocal}});
  EXPECT_THAT(QueryHistory(1), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ false,
                                                  std::vector<TestResult>{
                                                      {kUrl4, 4, kLocal},
                                                  }));

  if (base::FeatureList::IsEnabled(kHistoryQueryOnlyLocalFirst)) {
    EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                    /*reached_beginning*/ false,
                                                    std::vector<TestResult>{
                                                        {kUrl3, 3, kLocal},
                                                    }));
    EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                    /*reached_beginning*/ false,
                                                    std::vector<TestResult>{
                                                        {kUrl2, 2, kRemote},
                                                    }));
  } else {
    // Since local and remote are queried in parallel, one result is returned
    // from each, even though only one result was requested.
    EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                    /*reached_beginning*/ false,
                                                    std::vector<TestResult>{
                                                        {kUrl3, 3, kLocal},
                                                        {kUrl2, 2, kRemote},
                                                    }));
  }
  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{
                                                      {kUrl1, 1, kRemote},
                                                  }));
}

// A full request worth of local results will sit in pending, resulting in us
// being able to delete local history before our next query and we should still
// see the local entry.
TEST_P(BrowsingHistoryServiceTest, QueryHistoryFullLocalPending) {
  if (base::FeatureList::IsEnabled(kHistoryQueryOnlyLocalFirst)) {
    // With `kHistoryQueryOnlyLocalFirst`, the situation with pending results
    // doesn't exist.
    GTEST_SKIP();
  }
  AddHistory({{kUrl1, 1, kLocal}, {kUrl2, 2, kRemote}, {kUrl3, 3, kRemote}});
  EXPECT_THAT(QueryHistory(1), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ false,
                                                  std::vector<TestResult>{
                                                      {kUrl3, 3, kRemote},
                                                  }));

  local_history()->DeleteURLs({GURL(kUrl1)});
  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{
                                                      {kUrl2, 2, kRemote},
                                                      {kUrl1, 1, kLocal},
                                                  }));
}

// Part of a request worth of local results will sit in pending, resulting in us
// seeing extra local results on our next request.
TEST_P(BrowsingHistoryServiceTest, QueryHistoryPartialLocalPending) {
  if (base::FeatureList::IsEnabled(kHistoryQueryOnlyLocalFirst)) {
    // With `kHistoryQueryOnlyLocalFirst`, the situation with pending results
    // doesn't exist.
    GTEST_SKIP();
  }
  AddHistory({{kUrl1, 1, kLocal},
              {kUrl2, 2, kLocal},
              {kUrl3, 3, kRemote},
              {kUrl4, 4, kLocal},
              {kUrl5, 5, kRemote},
              {kUrl6, 6, kRemote},
              {kUrl7, 7, kLocal}});
  EXPECT_THAT(QueryHistory(2), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ false,
                                                  std::vector<TestResult>{
                                                      {kUrl7, 7, kLocal},
                                                      {kUrl6, 6, kRemote},
                                                      {kUrl5, 5, kRemote},
                                                  }));
  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{
                                                      {kUrl4, 4, kLocal},
                                                      {kUrl3, 3, kRemote},
                                                      {kUrl2, 2, kLocal},
                                                      {kUrl1, 1, kLocal},
                                                  }));
}

// A full request worth of remote results will sit in pending, resulting in us
// being able to delete remote history before our next query and we should still
// see the remote entry.
TEST_P(BrowsingHistoryServiceTest, QueryHistoryFullRemotePending) {
  if (base::FeatureList::IsEnabled(kHistoryQueryOnlyLocalFirst)) {
    // With `kHistoryQueryOnlyLocalFirst`, the situation with pending results
    // doesn't exist.
    GTEST_SKIP();
  }
  AddHistory({{kUrl1, 1, kRemote}, {kUrl2, 2, kLocal}, {kUrl3, 3, kLocal}});
  EXPECT_THAT(QueryHistory(1), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ false,
                                                  std::vector<TestResult>{
                                                      {kUrl3, 3, kLocal},
                                                  }));

  web_history()->ClearSyncedVisits();
  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{
                                                      {kUrl2, 2, kLocal},
                                                      {kUrl1, 1, kRemote},
                                                  }));
}

// Part of a request worth of remote results will sit in pending, resulting in
// us seeing extra remote results on our next request.
TEST_P(BrowsingHistoryServiceTest, QueryHistoryPartialRemotePending) {
  if (base::FeatureList::IsEnabled(kHistoryQueryOnlyLocalFirst)) {
    // With `kHistoryQueryOnlyLocalFirst`, the situation with pending results
    // doesn't exist.
    GTEST_SKIP();
  }
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl2, 2, kRemote},
              {kUrl3, 3, kLocal},
              {kUrl4, 4, kRemote},
              {kUrl5, 5, kLocal},
              {kUrl6, 6, kLocal},
              {kUrl7, 7, kRemote}});
  EXPECT_THAT(QueryHistory(2), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ false,
                                                  std::vector<TestResult>{
                                                      {kUrl7, 7, kRemote},
                                                      {kUrl6, 6, kLocal},
                                                      {kUrl5, 5, kLocal},
                                                  }));
  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{
                                                      {kUrl4, 4, kRemote},
                                                      {kUrl3, 3, kLocal},
                                                      {kUrl2, 2, kRemote},
                                                      {kUrl1, 1, kRemote},
                                                  }));
}

TEST_P(BrowsingHistoryServiceTest, RetryOnRemoteFailureEmpty) {
  web_history()->SetupFakeResponse(false, 0);
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ false,
                                                 std::vector<TestResult>{}));
  web_history()->SetupFakeResponse(true, net::HTTP_OK);
  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{}));
}

TEST_P(BrowsingHistoryServiceTest, RetryOnRemoteFailurePagingRemote) {
  AddHistory({{kUrl1, 1, kRemote}, {kUrl2, 2, kRemote}, {kUrl3, 3, kRemote}});
  EXPECT_THAT(QueryHistory(2), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ false,
                                                  std::vector<TestResult>{
                                                      {kUrl3, 3, kRemote},
                                                      {kUrl2, 2, kRemote},
                                                  }));

  web_history()->SetupFakeResponse(false, 0);
  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ false,
                                                  std::vector<TestResult>{}));

  web_history()->SetupFakeResponse(true, net::HTTP_OK);
  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{
                                                      {kUrl1, 1, kRemote},
                                                  }));
}

TEST_P(BrowsingHistoryServiceTest, RetryOnRemoteFailurePagingLocal) {
  AddHistory({{kUrl1, 1, kLocal}, {kUrl2, 2, kLocal}, {kUrl3, 3, kLocal}});
  web_history()->SetupFakeResponse(false, 0);
  EXPECT_THAT(QueryHistory(2), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ false,
                                                  std::vector<TestResult>{
                                                      {kUrl3, 3, kLocal},
                                                      {kUrl2, 2, kLocal},
                                                  }));

  web_history()->SetupFakeResponse(true, net::HTTP_OK);
  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{
                                                      {kUrl1, 1, kLocal},
                                                  }));
}

TEST_P(BrowsingHistoryServiceTest, WebHistoryTimeout) {
  TimeoutWebHistoryService timeout;
  driver()->SetWebHistory(&timeout);
  ResetService(driver(), local_history(), sync());
  EXPECT_EQ(0U, driver()->GetQueryResults().size());
  service()->QueryHistory(std::u16string(), QueryOptions());
  EXPECT_EQ(0U, driver()->GetQueryResults().size());
  BlockUntilHistoryProcessesPendingRequests();
  timer()->Fire();
  EXPECT_EQ(1U, driver()->GetQueryResults().size());
  EXPECT_FALSE(driver()->GetQueryResults()[0].second.reached_beginning);
  EXPECT_TRUE(driver()->GetQueryResults()[0].second.sync_timed_out);

  // WebHistoryService will DCHECK if we destroy it before the observer in
  // BrowsingHistoryService is removed, so reset our first
  // BrowsingHistoryService before `timeout` goes out of scope.
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), nullptr, nullptr);
}

TEST_P(BrowsingHistoryServiceTest, ObservingWebHistory) {
  // No need to observe SyncService since we have a WebHistory already.
  EXPECT_CALL(*sync(), AddObserver).Times(0);
  EXPECT_CALL(*sync(), RemoveObserver).Times(0);

  ResetService(driver(), nullptr, sync());

  web_history()->TriggerOnWebHistoryDeleted();
  EXPECT_EQ(1, driver()->GetHistoryDeletedCount());
}

TEST_P(BrowsingHistoryServiceTest, ObservingWebHistoryDelayedWeb) {
  // Since there's no WebHistory, observations should be set up on Sync.
  EXPECT_CALL(*sync(), AddObserver);
  EXPECT_CALL(*sync(), RemoveObserver).Times(0);

  driver()->SetWebHistory(nullptr);
  ResetService(driver(), nullptr, sync());

  // OnStateChanged() is a no-op if WebHistory is still inaccessible.
  service()->OnStateChanged(sync());

  driver()->SetWebHistory(web_history());
  // Since WebHistory is currently not being observed, triggering a history
  // deletion will not be propagated to the driver.
  web_history()->TriggerOnWebHistoryDeleted();
  EXPECT_EQ(0, driver()->GetHistoryDeletedCount());

  // Once OnStateChanged() gets called, the BrowsingHistoryService switches from
  // observing SyncService to WebHistoryService. As such, RemoveObserver should
  // have been called on SyncService, so lets verify.
  testing::Mock::VerifyAndClearExpectations(sync());
  EXPECT_CALL(*sync(), AddObserver).Times(0);
  EXPECT_CALL(*sync(), RemoveObserver);
  service()->OnStateChanged(sync());

  // Only now should deletion should be propagated through.
  web_history()->TriggerOnWebHistoryDeleted();
  EXPECT_EQ(1, driver()->GetHistoryDeletedCount());
}

TEST_P(BrowsingHistoryServiceTest, IncorrectlyOrderedRemoteResults) {
  // Created from crbug.com/787928, where suspected MergeDuplicateResults did
  // not start with sorted data. This case originally hit a NOTREACHED.
  ReversedWebHistoryService reversed;
  driver()->SetWebHistory(&reversed);
  ResetService(driver(), local_history(), sync());
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl3, 2, kRemote},
              {kUrl3, 3, kLocal},
              {kUrl5, 4, kRemote},
              {kUrl5, 5, kLocal},
              {kUrl6, 6, kRemote}},
             &reversed);
  if (base::FeatureList::IsEnabled(kHistoryQueryOnlyLocalFirst)) {
    // The local query returns 5, 3. Since more results were requested, a
    // remote query is started for entries < 3, which returns 1, 2 (in this
    // order!). 2 and 3 have the same URL and day so are merged. Note that the
    // remote entries 4 and 6 are never queried. In practice, this situation
    // should be impossible - recent remote entries should always also be
    // available locally.
    EXPECT_THAT(QueryHistory(4), MatchesQueryResult(baseline_time_,
                                                    /*reached_beginning*/ true,
                                                    std::vector<TestResult>{
                                                        {kUrl5, 5, kLocal},
                                                        {kUrl3, 3, kBoth},
                                                        {kUrl1, 1, kRemote},
                                                    }));
  } else {
    // The local query returns 5, 3, and the remote one returns 4, 6 (in this
    // order!). 4 and 5 have the same URL and are merged.
    EXPECT_THAT(QueryHistory(2), MatchesQueryResult(baseline_time_,
                                                    /*reached_beginning*/ false,
                                                    std::vector<TestResult>{
                                                        {kUrl6, 6, kRemote},
                                                        {kUrl5, 5, kBoth},
                                                    }));
  }

  // WebHistoryService will DCHECK if we destroy it before the observer in
  // BrowsingHistoryService is removed, so reset our first
  // BrowsingHistoryService before `reversed` goes out of scope.
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), nullptr, nullptr);
}

TEST_P(BrowsingHistoryServiceTest, MultipleSubsequentQueries) {
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl2, 2, kRemote},
              {kUrl3, 3, kRemote},
              {kUrl4, 4, kLocal},
              {kUrl5, 5, kLocal},
              {kUrl6, 6, kLocal}});

  // First query: Two local results.
  EXPECT_THAT(QueryHistory(2), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ false,
                                                  std::vector<TestResult>{
                                                      {kUrl6, 6, kLocal},
                                                      {kUrl5, 5, kLocal},
                                                  }));
  if (base::FeatureList::IsEnabled(kHistoryQueryOnlyLocalFirst)) {
    // Second query: One local and one remote result. Under the hood, this maps
    // to two successive queries, one to the local DB and then one to the remote
    // service.
    EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                    /*reached_beginning*/ false,
                                                    std::vector<TestResult>{
                                                        {kUrl4, 4, kLocal},
                                                        {kUrl3, 3, kRemote},
                                                    }));
    // Third query: Two remote results, and done.
    EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                    /*reached_beginning*/ true,
                                                    std::vector<TestResult>{
                                                        {kUrl2, 2, kRemote},
                                                        {kUrl1, 1, kRemote},
                                                    }));
  } else {
    // Second query: One local and *two* remote results. This is sort of
    // unexpected (only two results were asked for), but is a consequence of the
    // parallel local+remote queries.
    EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                    /*reached_beginning*/ false,
                                                    std::vector<TestResult>{
                                                        {kUrl4, 4, kLocal},
                                                        {kUrl3, 3, kRemote},
                                                        {kUrl2, 2, kRemote},
                                                    }));
    // Third query: The one remaining remote result, and done.
    EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                    /*reached_beginning*/ true,
                                                    std::vector<TestResult>{
                                                        {kUrl1, 1, kRemote},
                                                    }));
  }
}

TEST_P(BrowsingHistoryServiceTest, RemoveVisitsMetric) {
  // `kUrl1` was visited 3 times on day 1, and 4 times on day 2. `kUrl2` was
  // visited once on day 1. In total, there are 3 `HistoryEntry` instances
  // (since every "entry" groups all visits to a URL for a single day).
  // Note that for this test it doesn't matter that no such history entries were
  // actually added to the service first.
  const std::vector<HistoryEntry> entries{CreateEntry(kUrl1, {1, 2, 3}),
                                          CreateEntry(kUrl2, {4}),
                                          CreateEntry(kUrl1, {25, 26, 27, 28})};

  {
    base::HistogramTester histograms;

    service()->RemoveVisits(entries);

    histograms.ExpectUniqueSample(
        "History.RemoveVisitsFromWebHistory.EntryCount", 3, 1);
  }

  {
    // Simulate that history sync is disabled, so `WebHistoryService` is null.
    driver()->SetWebHistory(nullptr);

    base::HistogramTester histograms;

    service()->RemoveVisits(entries);

    // Without WebHistoryService, nothing should be recorded.
    histograms.ExpectTotalCount("History.RemoveVisitsFromWebHistory.EntryCount",
                                0);
  }
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_P(BrowsingHistoryServiceTest, IncludeActorVisits) {
  AddHistory({
      {kUrl1, 1, kRemote},
      {kUrl2, 2, kLocal, "", VisitSource::SOURCE_ACTOR},
  });

  QueryOptions options;
  options.include_actor_visits = true;
  EXPECT_THAT(
      QueryHistory(options),
      MatchesQueryResult(baseline_time_,
                         /*reached_beginning*/ true,
                         std::vector<TestResult>{
                             {kUrl2, 2, kLocal, "", VisitSource::SOURCE_ACTOR},
                             {kUrl1, 1, kRemote},
                         }));
}

TEST_P(BrowsingHistoryServiceTest, ActorVisitsExcludedByDefault) {
  AddHistory({
      {kUrl1, 1, kRemote},
      {kUrl2, 2, kLocal, "", VisitSource::SOURCE_ACTOR},
  });

  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{
                                                     {kUrl1, 1, kRemote},
                                                 }));
}

TEST_P(BrowsingHistoryServiceTest, ActorVisitDeduplication) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBrowsingHistoryActorIntegrationM2);

  AddHistory({
      {kUrl1, 1, kRemote},
      {kUrl1, 2, kLocal},
      {kUrl1, 3, kLocal, "", VisitSource::SOURCE_ACTOR},
      {kUrl2, 4, kLocal, "", VisitSource::SOURCE_ACTOR},
      {kUrl2, 5, kLocal, "", VisitSource::SOURCE_ACTOR},
  });

  QueryOptions options;
  options.include_actor_visits = true;
  // Disable backend de-duplication to properly test the client-side
  // de-duplication.
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;

  // Duplicate actor visits take the latest visit values.
  // Actor visits are not duplicated with non-actor visits.
  EXPECT_THAT(
      QueryHistory(options),
      MatchesQueryResult(baseline_time_,
                         /*reached_beginning*/ true,
                         std::vector<TestResult>{
                             {kUrl2, 5, kLocal, "", VisitSource::SOURCE_ACTOR},
                             {kUrl1, 3, kLocal, "", VisitSource::SOURCE_ACTOR},
                             {kUrl1, 2, kBoth},
                         }));
}

TEST_P(BrowsingHistoryServiceTest, GroupSimilarVisits) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowsingHistorySimilarVisitsGrouping);

  // Add a page with a custom title.
  HistoryAddPageArgs page1;
  page1.url = GURL("http://www.a.com/1");
  page1.time = OffsetToTime(4);
  page1.title = u"Title A";
  local_history()->AddPage(page1);

  // Add a page with a different URL but the same title as page 1, this should
  // not be grouped with page 1.
  HistoryAddPageArgs page2;
  page2.url = GURL("http://www.b.com/1");
  page2.time = OffsetToTime(3);
  page2.title = u"Title B";
  local_history()->AddPage(page2);

  // Add a remote page with a different URL but the same domain and title as
  // page 1. This should be grouped with page 1.
  HistoryAddPageArgs page3;
  page3.url = GURL("http://www.a.com/2");
  page3.time = OffsetToTime(2);
  page3.title = u"Title A";
  local_history()->AddPage(page3);

  // Add a page with a different URL and title, but the same domain as page 1,
  // this should not be grouped with page 1.
  HistoryAddPageArgs page4;
  page4.url = GURL("http://www.a.com/3");
  page4.time = OffsetToTime(1);
  page4.title = u"Title C";
  local_history()->AddPage(page4);

  // Add a page with the same URL and title as page 1, this should be grouped
  // with page 1.
  HistoryAddPageArgs page5;
  page5.url = GURL("http://www.a.com/1");
  page5.time = OffsetToTime(5);
  page5.title = u"Title A";
  local_history()->AddPage(page5);

  BlockUntilHistoryProcessesPendingRequests();

  EXPECT_THAT(
      QueryHistory(),
      MatchesQueryResult(baseline_time_, /*reached_beginning=*/true,
                         std::vector<TestResult>{
                             {"http://www.a.com/1", 5, kLocal},
                             {"http://www.b.com/1", 3, kLocal},
                             {"http://www.a.com/3", 1, kLocal},
                         }));
}

TEST_P(BrowsingHistoryServiceTest, ShouldQueryActorVisitsOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      history::kBrowsingHistoryActorIntegrationM3);

  AddHistory({{kUrl1, 1, kRemote, ""},
              {kUrl2, 2, kLocal, "", VisitSource::SOURCE_BROWSED,
               true /*is_actor_visit*/}});

  QueryOptions options;
  options.include_user_visits = false;
  options.include_actor_visits = true;

  TestBrowsingHistoryDriver::QueryResult result = QueryHistory(options);

  ASSERT_EQ(1u, result.first.size());

  EXPECT_EQ(GURL(kUrl2), result.first[0].url);
  EXPECT_TRUE(result.first[0].is_actor_visit);

  for (const auto& entry : result.first) {
    EXPECT_NE(entry.entry_type, HistoryEntry::REMOTE_ENTRY);
    EXPECT_NE(entry.entry_type, HistoryEntry::COMBINED_ENTRY);
  }

  EXPECT_FALSE(result.second.sync_timed_out);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace

}  // namespace history
