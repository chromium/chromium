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
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "components/critical_actions/core/browser/critical_action_provider.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/critical_actions/core/browser/fake_critical_action_provider.h"
#include "components/critical_actions/core/browser/features.h"
#include "components/history/core/browser/browsing_history_driver.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

using base::Time;

namespace history {

using HistoryEntry = BrowsingHistoryService::HistoryEntry;
using ClientGroup = BrowsingHistoryService::ClientGroup;

void PrintTo(const HistoryEntry& entry, std::ostream* os) {
  *os << "{url: " << entry.url << ", time: " << entry.time
      << ", entry_type: " << entry.entry_type
      << ", remote_icon_url_for_uma: " << entry.remote_icon_url_for_uma
      << ", is_actor_visit: " << entry.is_actor_visit << "}";
}

void PrintTo(const BrowsingHistoryService::QueryResultsInfo& info,
             std::ostream* os) {
  *os << "{reached_beginning: " << info.reached_beginning
      << ", sync_timed_out: " << info.sync_timed_out
      << ", queried_critical_actions: " << info.queried_critical_actions << "}";
}

namespace {

using ::testing::AllOf;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

const char kUrl1[] = "http://www.one.com";
const char kUrl2[] = "http://www.two.com";
const char kUrl3[] = "http://www.three.com";
const char kUrl4[] = "http://www.four.com";
const char kUrl5[] = "http://www.five.com";
const char kUrl6[] = "http://www.six.com";
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
  TestResult(std::string url,
             int64_t hour_offset,
             HistoryEntry::EntryType type = HistoryEntry::LOCAL_ENTRY,
             std::string remote_icon_url_for_uma = "",
             VisitSource visit_source = VisitSource::SOURCE_BROWSED,
             bool is_actor_visit = false,
             std::string client_id = "")
      : url(std::move(url)),
        hour_offset(hour_offset),
        type(type),
        remote_icon_url_for_uma(std::move(remote_icon_url_for_uma)),
        visit_source(visit_source),
        is_actor_visit(is_actor_visit),
        client_id(client_id.empty() && type == HistoryEntry::REMOTE_ENTRY
                      ? FakeWebHistoryService::kDefaultClientId
                      : std::move(client_id)) {}

  std::string url;
  int64_t hour_offset;  // Visit time in hours past the baseline time.
  HistoryEntry::EntryType type;
  std::string remote_icon_url_for_uma;
  VisitSource visit_source = VisitSource::SOURCE_BROWSED;
  bool is_actor_visit = false;
  std::string client_id;
};

void PrintTo(const TestResult& result, std::ostream* os) {
  *os << "{url: " << result.url << ", hour_offset: " << result.hour_offset
      << ", type: " << result.type
      << ", remote_icon_url_for_uma: " << result.remote_icon_url_for_uma
      << ", visit_source: " << static_cast<int>(result.visit_source)
      << ", client_id: " << result.client_id << "}";
}

MATCHER_P2(MatchesHistory, baseline_time, expected, "") {
  return arg.url == GURL(expected.url) &&
         arg.time ==
             OffsetToTimeWithBaseline(baseline_time, expected.hour_offset) &&
         arg.entry_type == expected.type &&
         arg.remote_icon_url_for_uma ==
             GURL(expected.remote_icon_url_for_uma) &&
         arg.is_actor_visit ==
             (expected.visit_source == VisitSource::SOURCE_ACTOR) &&
         arg.client_id == expected.client_id;
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
  TestBrowsingHistoryDriver() = default;

  using QueryResult = std::pair<std::vector<HistoryEntry>,
                                BrowsingHistoryService::QueryResultsInfo>;
  std::vector<QueryResult> GetQueryResults() { return query_results_; }

  int GetHistoryDeletedCount() { return history_deleted_count_; }

  const std::vector<ExpireHistoryArgs>& GetExpireHistoryArgs() const {
    return expire_list_;
  }
  void ClearExpireHistoryArgs() { expire_list_.clear(); }

  void RunContinuation() {
    EXPECT_TRUE(continuation_closure_);
    std::move(continuation_closure_).Run();
  }

  void SetQueryCompleteCallback(base::OnceClosure callback) {
    query_complete_callback_ = std::move(callback);
  }

 private:
  // BrowsingHistoryDriver implementation.
  void OnQueryComplete(
      const std::vector<HistoryEntry>& results,
      const BrowsingHistoryService::QueryResultsInfo& query_results_info,
      base::OnceClosure continuation_closure) override {
    query_results_.push_back(QueryResult(results, query_results_info));
    continuation_closure_ = std::move(continuation_closure);
    if (query_complete_callback_) {
      std::move(query_complete_callback_).Run();
    }
  }
  void OnRemoveVisitsComplete() override {}
  void OnRemoveVisitsFailed() override {}
  void OnRemoveVisits(
      const std::vector<ExpireHistoryArgs>& expire_list) override {
    expire_list_ = expire_list;
  }
  void HistoryDeleted() override { history_deleted_count_++; }
  void HasOtherFormsOfBrowsingHistory(bool has_other_forms,
                                      bool has_synced_results) override {}
  bool AllowHistoryDeletions() override { return true; }
  bool ShouldHideWebHistoryUrl(const GURL& url) override { return false; }
  void ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      const syncer::SyncService* sync_service,
      WebHistoryService* local_history,
      base::OnceCallback<void(bool)> callback) override {}

  int history_deleted_count_ = 0;
  std::vector<ExpireHistoryArgs> expire_list_;
  std::vector<QueryResult> query_results_;
  base::OnceClosure continuation_closure_;
  base::OnceClosure query_complete_callback_;
};

class TestWebHistoryService : public FakeWebHistoryService {
 public:
  TestWebHistoryService() = default;

  void TriggerOnWebHistoryDeleted() {
    TestRequest request;
    ExpireHistoryCompletionCallback(base::DoNothing(), &request, true);
  }

  void SetIsEnabled(bool enabled) { is_enabled_ = enabled; }

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
      const std::vector<std::string>& client_ids,
      bool* more_results_left) override {
    auto result = FakeWebHistoryService::GetVisitsBetween(
        begin, end, count, client_ids, more_results_left);
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
  TestBrowsingHistoryService(
      BrowsingHistoryDriver* driver,
      HistoryService* local_history,
      WebHistoryService* web_history,
      syncer::SyncService* sync_service,
      std::unique_ptr<base::OneShotTimer> timer,
      syncer::DeviceInfoTracker* device_info_tracker,
      critical_actions::CriticalActionProvider* critical_action_provider)
      : BrowsingHistoryService(driver,
                               local_history,
                               web_history,
                               sync_service,
                               std::move(timer),
                               device_info_tracker,
                               critical_action_provider) {}
};

class BrowsingHistoryServiceTest : public ::testing::Test {
 protected:
  // WebHistory API is to pass time ranges as the number of microseconds since
  // Time::UnixEpoch() as a query parameter. This becomes a problem when we use
  // Time::LocalMidnight(), which rounds _down_, and will result in a  time
  // before Time::UnixEpoch() that cannot be represented. By adding 1 day we
  // ensure all test data is after Time::UnixEpoch().
  BrowsingHistoryServiceTest()
      : baseline_time_(Time::UnixEpoch().LocalMidnight() + base::Days(1)) {
    EXPECT_TRUE(history_dir_.CreateUniqueTempDir());
    local_history_ = CreateHistoryService(history_dir_.GetPath(), true);
    ResetService(driver(), local_history(), web_history(), sync(),
                 device_info_tracker(), /*critical_action_provider=*/nullptr);
  }

  void ResetService(
      BrowsingHistoryDriver* driver,
      HistoryService* local_history,
      WebHistoryService* web_history,
      syncer::SyncService* sync_service,
      syncer::DeviceInfoTracker* device_info_tracker,
      critical_actions::CriticalActionProvider* critical_action_provider) {
    std::unique_ptr<base::MockOneShotTimer> timer =
        std::make_unique<base::MockOneShotTimer>();
    timer_ = timer.get();
    browsing_history_service_ = std::make_unique<TestBrowsingHistoryService>(
        driver, local_history, web_history, sync_service, std::move(timer),
        device_info_tracker, critical_action_provider);
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
                                    entry.remote_icon_url_for_uma,
                                    entry.client_id);
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
  syncer::FakeDeviceInfoTracker* device_info_tracker() {
    return &device_info_tracker_;
  }
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
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir history_dir_;
  std::unique_ptr<HistoryService> local_history_;
  TestWebHistoryService web_history_;
  syncer::MockSyncService sync_service_;
  syncer::FakeDeviceInfoTracker device_info_tracker_;
  TestBrowsingHistoryDriver driver_;
  std::unique_ptr<TestBrowsingHistoryService> browsing_history_service_;
  raw_ptr<base::MockOneShotTimer> timer_;
};

TEST_F(BrowsingHistoryServiceTest, QueryHistoryIncludes404s) {
  // Add a non-404 visit.
  AddHistory({{kUrl1, 1, kLocal}});

  // Add a 404 visit.
  HistoryAddPageArgs page_404_args;
  page_404_args.url = GURL(kUrl2);
  page_404_args.time = OffsetToTime(2);
  page_404_args.context_annotations = {.response_code = 404};
  local_history()->AddPage(page_404_args);

  BlockUntilHistoryProcessesPendingRequests();

  // 404s should be included in the query results by default.
  EXPECT_THAT(QueryHistory(),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{{kUrl2, 2, kLocal},
                                                         {kUrl1, 1, kLocal}}));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryNoSources) {
  ResetService(driver(), /*local_history=*/nullptr, /*web_history=*/nullptr,
               /*sync_service=*/nullptr, device_info_tracker(),
               /*critical_action_provider=*/nullptr);
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{}));
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryJustLocal) {
  ResetService(driver(), local_history(), /*web_history=*/nullptr,
               /*sync_service=*/nullptr, device_info_tracker(),
               /*critical_action_provider=*/nullptr);
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{}));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryJustLocal) {
  ResetService(driver(), local_history(), /*web_history=*/nullptr,
               /*sync_service=*/nullptr, device_info_tracker(),
               /*critical_action_provider=*/nullptr);
  AddHistory({{kUrl1, 1, kLocal}});
  EXPECT_THAT(QueryHistory(),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{{kUrl1, 1, kLocal}}));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryWithAppIdFilter) {
  ResetService(driver(), local_history(), web_history(), sync(),
               device_info_tracker(), /*critical_action_provider=*/nullptr);
  AddHistory({{kUrl1, 1, kLocal}});

  QueryOptions options;
  options.app_id = "org.chromium.test";

  EXPECT_THAT(QueryHistory(options),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{}));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryWithClientIdFilter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kWebHistoryUseNewApi);

  AddHistory(
      {{kUrl1, 1, kRemote, "", VisitSource::SOURCE_BROWSED, false, "client_1"},
       {kUrl2, 2, kRemote, "", VisitSource::SOURCE_BROWSED, false, "client_2"},
       {kUrl3, 3, kRemote, "", VisitSource::SOURCE_BROWSED, false, "client_1"},
       {kUrl4, 4, kLocal}});

  QueryOptions options;
  options.client_ids = {"client_1"};

  EXPECT_THAT(QueryHistory(options),
              MatchesQueryResult(
                  baseline_time_, /*reached_beginning*/ true,
                  std::vector<TestResult>{
                      {kUrl3, 3, kRemote, "", VisitSource::SOURCE_BROWSED,
                       false, "client_1"},
                      {kUrl1, 1, kRemote, "", VisitSource::SOURCE_BROWSED,
                       false, "client_1"}}));

  options.client_ids = {"client_1", ""};

  EXPECT_THAT(QueryHistory(options),
              MatchesQueryResult(
                  baseline_time_, /*reached_beginning*/ true,
                  std::vector<TestResult>{
                      {kUrl4, 4, kLocal},
                      {kUrl3, 3, kRemote, "", VisitSource::SOURCE_BROWSED,
                       false, "client_1"},
                      {kUrl1, 1, kRemote, "", VisitSource::SOURCE_BROWSED,
                       false, "client_1"}}));
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryJustWeb) {
  ResetService(driver(), /*local_history=*/nullptr, web_history(),
               /*sync_service=*/nullptr, device_info_tracker(),
               /*critical_action_provider=*/nullptr);
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{}));
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryDelayedWeb) {
  web_history()->SetIsEnabled(false);
  ResetService(driver(), /*local_history=*/nullptr, web_history(), sync(),
               device_info_tracker(), /*critical_action_provider=*/nullptr);
  web_history()->SetIsEnabled(true);
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{}));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryJustWeb) {
  ResetService(driver(), /*local_history=*/nullptr, web_history(), sync(),
               device_info_tracker(), /*critical_action_provider=*/nullptr);
  AddHistory({{kUrl1, 1, kRemote}});
  EXPECT_THAT(QueryHistory(),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{{kUrl1, 1, kRemote}}));
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryBothSources) {
  ResetService(driver(), local_history(), web_history(), sync(),
               device_info_tracker(), /*critical_action_provider=*/nullptr);
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{}));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryAllSources) {
  ResetService(driver(), local_history(), web_history(), sync(),
               device_info_tracker(), /*critical_action_provider=*/nullptr);
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

TEST_F(BrowsingHistoryServiceTest, QueryHistoryLocalTimeRanges) {
  AddHistory({{kUrl1, 1, kLocal},
              {kUrl2, 2, kLocal},
              {kUrl3, 3, kLocal},
              {kUrl4, 4, kLocal}});
  QueryOptions options;
  options.begin_time = OffsetToTime(2);
  options.end_time = OffsetToTime(4);
  // All visits in [begin_time, end_time) fit in one page, so the beginning of
  // the queried range is reached even though an older visit exists before
  // `begin_time`.
  EXPECT_THAT(QueryHistory(options),
              MatchesQueryResult(baseline_time_,
                                 /*reached_beginning*/ true,
                                 std::vector<TestResult>{
                                     {kUrl3, 3, kLocal},
                                     {kUrl2, 2, kLocal},
                                 }));
}

// Tests that continuing a query with `begin_time` past the end of its range
// doesn't start over from the newest visit and return duplicates.
TEST_F(BrowsingHistoryServiceTest, QueryHistoryLocalTimeRangeEmptyPage) {
  // The visit at hour 0 is before `begin_time` and must never be returned.
  AddHistory({{kUrl1, 0, kLocal}, {kUrl2, 2, kLocal}, {kUrl3, 3, kLocal}});
  QueryOptions options;
  options.begin_time = OffsetToTime(1);

  for (bool group_visits : {false, true}) {
    SCOPED_TRACE(testing::Message() << "group_visits: " << group_visits);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(kBrowsingHistorySimilarVisitsGrouping,
                                      group_visits);

    EXPECT_THAT(QueryHistory(options),
                MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                   std::vector<TestResult>{
                                       {kUrl3, 3, kLocal},
                                       {kUrl2, 2, kLocal},
                                   }));
    EXPECT_THAT(ContinueQuery(),
                MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                   std::vector<TestResult>{}));
    EXPECT_THAT(ContinueQuery(),
                MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                   std::vector<TestResult>{}));
  }
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryRemoteTimeRanges) {
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

TEST_F(BrowsingHistoryServiceTest, QueryHistoryHostOnlyRemote) {
  AddHistory({{kUrl8, 1, kRemote}, {kUrl9, 2, kRemote}, {kUrl10, 3, kRemote}});

  QueryOptions options;
  options.max_count = 0;
  EXPECT_THAT(QueryHistory(u"eight.com", options),
              MatchesQueryResult(baseline_time_,
                                 /*reached_beginning*/ true,
                                 std::vector<TestResult>{
                                     {kUrl10, 3, kRemote},
                                     {kUrl9, 2, kRemote},
                                     {kUrl8, 1, kRemote},
                                 }));
  options.hostname_suffix = "eight.com";
  EXPECT_THAT(QueryHistory(u"", options),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{
                                     {kUrl8, 1, kRemote},
                                 }));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryHostOnlyRemoteSuffixMatching) {
  const char kSubdomainUrl[] = "http://sub.eight.com";
  AddHistory({{kUrl8, 1, kRemote}, {kSubdomainUrl, 2, kRemote}});

  QueryOptions options;
  options.max_count = 0;
  options.hostname_suffix = "eight.com";

  // Both sub.eight.com and eight.com match, and matching is case-insensitive.
  EXPECT_THAT(QueryHistory(u"", options),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{
                                     {kSubdomainUrl, 2, kRemote},
                                     {kUrl8, 1, kRemote},
                                 }));
  options.hostname_suffix = "EiGhT.cOm";
  EXPECT_THAT(QueryHistory(u"", options),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{
                                     {kSubdomainUrl, 2, kRemote},
                                     {kUrl8, 1, kRemote},
                                 }));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryLocalPagingPartial) {
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

TEST_F(BrowsingHistoryServiceTest, QueryHistoryLocalPagingFull) {
  AddHistory({{kUrl1, 1, kLocal}, {kUrl2, 2, kLocal}, {kUrl3, 3, kLocal}});
  // The first query doesn't reach the beginning, since there were just enough
  // local results to fulfill the request and remote hasn't been queried yet.
  EXPECT_THAT(QueryHistory(3), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ false,
                                                  std::vector<TestResult>{
                                                      {kUrl3, 3, kLocal},
                                                      {kUrl2, 2, kLocal},
                                                      {kUrl1, 1, kLocal},
                                                  }));

  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{}));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryRemotePagingPartial) {
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

TEST_F(BrowsingHistoryServiceTest, QueryHistoryRemotePagingFull) {
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

TEST_F(BrowsingHistoryServiceTest, MergeDuplicatesSameDay) {
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

TEST_F(BrowsingHistoryServiceTest, MergeDuplicatesNextDayNotRemoved) {
  AddHistory({{kUrl1, 0, kRemote}, {kUrl1, 23, kRemote}, {kUrl1, 24, kRemote}});
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{
                                                     {kUrl1, 24, kRemote},
                                                     {kUrl1, 23, kRemote},
                                                 }));
}

TEST_F(BrowsingHistoryServiceTest, MergeDuplicatesMultipleDays) {
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

TEST_F(BrowsingHistoryServiceTest, MergeDuplicatesVerifyTimestamps) {
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

TEST_F(BrowsingHistoryServiceTest, MergeDuplicatesKeepNonEmptyIconUrl) {
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

TEST_F(BrowsingHistoryServiceTest, QueryHistoryMerge) {
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

TEST_F(BrowsingHistoryServiceTest, QueryHistoryPending) {
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl2, 2, kRemote},
              {kUrl3, 3, kLocal},
              {kUrl4, 4, kLocal}});
  EXPECT_THAT(QueryHistory(1), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ false,
                                                  std::vector<TestResult>{
                                                      {kUrl4, 4, kLocal},
                                                  }));

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
  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{
                                                      {kUrl1, 1, kRemote},
                                                  }));
}

TEST_F(BrowsingHistoryServiceTest, RetryOnRemoteFailureEmpty) {
  web_history()->SetupFakeResponse(false, 0);
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ false,
                                                 std::vector<TestResult>{}));
  web_history()->SetupFakeResponse(true, net::HTTP_OK);
  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{}));
}

TEST_F(BrowsingHistoryServiceTest, RetryOnRemoteFailurePagingRemote) {
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

TEST_F(BrowsingHistoryServiceTest, RetryOnRemoteFailurePagingLocal) {
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

TEST_F(BrowsingHistoryServiceTest, WebHistoryTimeout) {
  TimeoutWebHistoryService timeout;
  ResetService(driver(), local_history(), /*web_history=*/&timeout, sync(),
               device_info_tracker(), /*critical_action_provider=*/nullptr);
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
  ResetService(driver(), /*local_history=*/nullptr, /*web_history=*/nullptr,
               /*sync_service=*/nullptr, device_info_tracker(),
               /*critical_action_provider=*/nullptr);
}

TEST_F(BrowsingHistoryServiceTest, ObservingWebHistory) {
  ResetService(driver(), /*local_history=*/nullptr, web_history(), sync(),
               device_info_tracker(), /*critical_action_provider=*/nullptr);

  web_history()->TriggerOnWebHistoryDeleted();
  EXPECT_EQ(1, driver()->GetHistoryDeletedCount());
}

TEST_F(BrowsingHistoryServiceTest, IncorrectlyOrderedRemoteResults) {
  // Created from crbug.com/787928, where suspected MergeDuplicateResults did
  // not start with sorted data. This case originally hit a NOTREACHED.
  ReversedWebHistoryService reversed;
  ResetService(driver(), local_history(), /*web_history=*/&reversed, sync(),
               device_info_tracker(), /*critical_action_provider=*/nullptr);
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl3, 2, kRemote},
              {kUrl3, 3, kLocal},
              {kUrl5, 4, kRemote},
              {kUrl5, 5, kLocal},
              {kUrl6, 6, kRemote}},
             &reversed);

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

  // WebHistoryService will DCHECK if we destroy it before the observer in
  // BrowsingHistoryService is removed, so reset our first
  // BrowsingHistoryService before `reversed` goes out of scope.
  ResetService(driver(), /*local_history=*/nullptr, /*web_history=*/nullptr,
               /*sync_service=*/nullptr, device_info_tracker(),
               /*critical_action_provider=*/nullptr);
}

TEST_F(BrowsingHistoryServiceTest, MultipleSubsequentQueries) {
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
}

TEST_F(BrowsingHistoryServiceTest, RemoveVisitsMetric) {
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
    web_history()->SetIsEnabled(false);

    base::HistogramTester histograms;

    service()->RemoveVisits(entries);

    // Without WebHistoryService, nothing should be recorded.
    histograms.ExpectTotalCount("History.RemoveVisitsFromWebHistory.EntryCount",
                                0);
  }
}

TEST_F(BrowsingHistoryServiceTest, RemoveVisitsDeduplicatedEntries) {
  std::vector<HistoryEntry> entries;

  {
    HistoryEntry entry;
    entry.url = GURL("http://www.a.com");
    entry.all_timestamps[GURL("http://www.a.com")] = {
        OffsetToTime(1), OffsetToTime(2), OffsetToTime(3)};
    entry.all_timestamps[GURL("http://www.a.com/1")] = {OffsetToTime(4),
                                                        OffsetToTime(5)};
    entry.all_timestamps[GURL("http://www.a.com/2")] = {OffsetToTime(6)};
    entries.push_back(std::move(entry));
  }
  {
    HistoryEntry entry;
    entry.url = GURL("http://www.b.com");
    entry.all_timestamps[GURL("http://www.b.com")] = {OffsetToTime(1)};
    entry.all_timestamps[GURL("http://www.b.com/2")] = {
        OffsetToTime(2), OffsetToTime(3), OffsetToTime(4)};
    entries.push_back(std::move(entry));
  }

  service()->RemoveVisits(entries);

  std::vector<ExpireHistoryArgs> expire_list = driver()->GetExpireHistoryArgs();
  EXPECT_EQ(2u, expire_list.size());

  // Each entry's ExpireHistoryArgs should contain all unique URLs from
  // `all_timestamps`.
  EXPECT_THAT(expire_list,
              ElementsAre(Field(&ExpireHistoryArgs::urls,
                                ElementsAre(GURL("http://www.a.com"),
                                            GURL("http://www.a.com/1"),
                                            GURL("http://www.a.com/2"))),
                          Field(&ExpireHistoryArgs::urls,
                                ElementsAre(GURL("http://www.b.com"),
                                            GURL("http://www.b.com/2")))));

  EXPECT_THAT(expire_list,
              Each(AllOf(Field(&ExpireHistoryArgs::begin_time, baseline_time_),
                         Field(&ExpireHistoryArgs::end_time,
                               baseline_time_ + base::Days(1)))));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(BrowsingHistoryServiceTest, IncludeActorVisits) {
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

TEST_F(BrowsingHistoryServiceTest, ActorVisitsExcludedByDefault) {
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

TEST_F(BrowsingHistoryServiceTest, ActorVisitDeduplication) {
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

TEST_F(BrowsingHistoryServiceTest, GroupSimilarVisits) {
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
  page2.title = u"Title A";
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

  EXPECT_THAT(QueryHistory(),
              MatchesQueryResult(baseline_time_, /*reached_beginning=*/true,
                                 std::vector<TestResult>{
                                     {"http://www.a.com/1", 5, kLocal},
                                     {"http://www.b.com/1", 3, kLocal},
                                     {"http://www.a.com/3", 1, kLocal},
                                 }));
}

TEST_F(BrowsingHistoryServiceTest, ShouldQueryActorVisitsOnly) {
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
  // Remote is never queried for actor-only queries, so it must not keep the
  // query from reaching the beginning.
  EXPECT_TRUE(result.second.reached_beginning);
}

// Web History is never queried for actor-only queries. Once local history is
// exhausted, there are no more results, so `reached_beginning` must be true,
// even with Web History enabled. Otherwise callers keep requesting more pages.
TEST_F(BrowsingHistoryServiceTest, ActorVisitsOnlyPagingReachesBeginning) {
  ResetService(driver(), local_history(), web_history(), sync(),
               device_info_tracker(), /*critical_action_provider=*/nullptr);
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl2, 2, kLocal, "", VisitSource::SOURCE_ACTOR},
              {kUrl3, 3, kLocal, "", VisitSource::SOURCE_ACTOR},
              {kUrl4, 4, kLocal, "", VisitSource::SOURCE_ACTOR}});

  QueryOptions options;
  options.include_user_visits = false;
  options.include_actor_visits = true;
  options.max_count = 2;

  // There are more local actor visits than `max_count`.
  EXPECT_THAT(
      QueryHistory(options),
      MatchesQueryResult(baseline_time_, /*reached_beginning*/ false,
                         std::vector<TestResult>{
                             {kUrl4, 4, kLocal, "", VisitSource::SOURCE_ACTOR},
                             {kUrl3, 3, kLocal, "", VisitSource::SOURCE_ACTOR},
                         }));

  // The continuation returns the last local actor visit. Web History is never
  // queried, so this is the last page.
  EXPECT_THAT(
      ContinueQuery(),
      MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                         std::vector<TestResult>{
                             {kUrl2, 2, kLocal, "", VisitSource::SOURCE_ACTOR},
                         }));
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(BrowsingHistoryServiceTest, QueryDurationHistogram) {
  base::HistogramTester histogram_tester;

  AddHistory({{kUrl1, 1, kLocal}, {kUrl2, 2, kLocal}, {kUrl3, 3, kLocal}});

  histogram_tester.ExpectTotalCount("History.BrowsingHistory.QueryDuration", 0);

  // Initial query with max_count = 2 to leave remaining entries for
  // continuation.
  QueryHistory(2);
  histogram_tester.ExpectTotalCount("History.BrowsingHistory.QueryDuration", 1);

  // Continuation query to fetch the remaining history entry.
  ContinueQuery();
  histogram_tester.ExpectTotalCount("History.BrowsingHistory.QueryDuration", 2);
}

TEST_F(BrowsingHistoryServiceTest, NoSyncedDevicesWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kWebHistoryUseNewApi},
                                {kBrowsingHistoryFilterByDevice});

  EXPECT_CALL(*sync()->GetMockUserSettings(), GetSelectedTypes())
      .WillRepeatedly(testing::Return(
          syncer::UserSelectableTypeSet{syncer::UserSelectableType::kHistory}));

  device_info_tracker()->Add(syncer::TestDeviceInfoBuilder()
                                 .WithGuid("guid1")
                                 .WithClientName("Device A")
                                 .Build());
  device_info_tracker()->Add(syncer::TestDeviceInfoBuilder()
                                 .WithGuid("guid2")
                                 .WithClientName("Device B")
                                 .Build());

  EXPECT_THAT(service()->GetSyncedDevicesForFiltering(), IsEmpty());
}

TEST_F(BrowsingHistoryServiceTest,
       NoSyncedDevicesWhenWebHistoryNewApiDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kBrowsingHistoryFilterByDevice},
                                {kWebHistoryUseNewApi});

  EXPECT_CALL(*sync()->GetMockUserSettings(), GetSelectedTypes())
      .WillRepeatedly(testing::Return(
          syncer::UserSelectableTypeSet{syncer::UserSelectableType::kHistory}));

  device_info_tracker()->Add(syncer::TestDeviceInfoBuilder()
                                 .WithGuid("guid1")
                                 .WithClientName("Device A")
                                 .Build());

  EXPECT_THAT(service()->GetSyncedDevicesForFiltering(), IsEmpty());
}

TEST_F(BrowsingHistoryServiceTest, NoSyncedDevicesWhenHistorySyncIsOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kWebHistoryUseNewApi, kBrowsingHistoryFilterByDevice}, {});

  EXPECT_CALL(*sync()->GetMockUserSettings(), GetSelectedTypes())
      .WillRepeatedly(testing::Return(syncer::UserSelectableTypeSet{}));

  device_info_tracker()->Add(syncer::TestDeviceInfoBuilder()
                                 .WithGuid("guid1")
                                 .WithClientName("Device A")
                                 .Build());
  device_info_tracker()->Add(syncer::TestDeviceInfoBuilder()
                                 .WithGuid("guid2")
                                 .WithClientName("Device B")
                                 .Build());

  EXPECT_THAT(service()->GetSyncedDevicesForFiltering(), IsEmpty());
}

TEST_F(BrowsingHistoryServiceTest, NoSyncedDevicesWhenTrackerIsNull) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kWebHistoryUseNewApi, kBrowsingHistoryFilterByDevice}, {});

  EXPECT_CALL(*sync()->GetMockUserSettings(), GetSelectedTypes())
      .WillRepeatedly(testing::Return(
          syncer::UserSelectableTypeSet{syncer::UserSelectableType::kHistory}));

  ResetService(driver(), local_history(), web_history(), sync(),
               /*device_info_tracker=*/nullptr,
               /*critical_action_provider=*/nullptr);

  EXPECT_THAT(service()->GetSyncedDevicesForFiltering(), IsEmpty());
}

TEST_F(BrowsingHistoryServiceTest,
       SyncedDevicesAreDeduplicatedByNameAndIsLocalStatus) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kWebHistoryUseNewApi, kBrowsingHistoryFilterByDevice}, {});

  EXPECT_CALL(*sync()->GetMockUserSettings(), GetSelectedTypes())
      .WillRepeatedly(testing::Return(
          syncer::UserSelectableTypeSet{syncer::UserSelectableType::kHistory}));

  device_info_tracker()->Add(syncer::TestDeviceInfoBuilder()
                                 .WithGuid("guid1")
                                 .WithClientName("Device A")
                                 .Build());
  device_info_tracker()->Add(syncer::TestDeviceInfoBuilder()
                                 .WithGuid("guid2")
                                 .WithClientName("Device A")
                                 .Build());
  device_info_tracker()->Add(syncer::TestDeviceInfoBuilder()
                                 .WithGuid("guid3")
                                 .WithClientName("Device B")
                                 .Build());
  device_info_tracker()->Add(syncer::TestDeviceInfoBuilder()
                                 .WithGuid("guid4")
                                 .WithClientName("Device B")
                                 .Build());
  device_info_tracker()->Add(syncer::TestDeviceInfoBuilder()
                                 .WithGuid("guid5")
                                 .WithClientName("Device C")
                                 .Build());

  // When the local device shares a name with another device ("Device A"),
  // "(this browser)" is appended to disambiguate it.
  device_info_tracker()->SetLocalCacheGuid("guid1");

  EXPECT_THAT(
      service()->GetSyncedDevicesForFiltering(),
      UnorderedElementsAre(
          AllOf(Field(&ClientGroup::name, "Device A (this browser)"),
                Field(&ClientGroup::is_local_device, true),
                Field(&ClientGroup::client_ids, ElementsAre("guid1", ""))),
          AllOf(Field(&ClientGroup::name, "Device A"),
                Field(&ClientGroup::is_local_device, false),
                Field(&ClientGroup::client_ids, ElementsAre("guid2"))),
          AllOf(Field(&ClientGroup::name, "Device B"),
                Field(&ClientGroup::is_local_device, false),
                Field(&ClientGroup::client_ids, ElementsAre("guid3", "guid4"))),
          AllOf(Field(&ClientGroup::name, "Device C"),
                Field(&ClientGroup::is_local_device, false),
                Field(&ClientGroup::client_ids, ElementsAre("guid5")))));

  // When the local device has a unique name ("Device C"), "(this browser)" is
  // not appended.
  device_info_tracker()->SetLocalCacheGuid("guid5");

  EXPECT_THAT(
      service()->GetSyncedDevicesForFiltering(),
      UnorderedElementsAre(
          AllOf(Field(&ClientGroup::name, "Device A"),
                Field(&ClientGroup::is_local_device, false),
                Field(&ClientGroup::client_ids, ElementsAre("guid1", "guid2"))),
          AllOf(Field(&ClientGroup::name, "Device B"),
                Field(&ClientGroup::is_local_device, false),
                Field(&ClientGroup::client_ids, ElementsAre("guid3", "guid4"))),
          AllOf(Field(&ClientGroup::name, "Device C"),
                Field(&ClientGroup::is_local_device, true),
                Field(&ClientGroup::client_ids, ElementsAre("guid5", "")))));
}

// Tests local queries with `KEEP_ALL_DUPLICATES` (as used by chrome://history),
// where BrowsingHistoryService alone merges same-day visits, with
// `kBrowsingHistorySimilarVisitsGrouping` both enabled and disabled.
class BrowsingHistoryServiceKeepAllDuplicatesTest
    : public BrowsingHistoryServiceTest,
      public testing::WithParamInterface<bool> {
 public:
  BrowsingHistoryServiceKeepAllDuplicatesTest() {
    feature_list_.InitWithFeatureState(kBrowsingHistorySimilarVisitsGrouping,
                                       GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that same-day visits to a URL are merged into one entry whose
// `all_visit_ids` lists every visit, newest first, and that the URL's lifetime
// visit count isn't added up once per merged visit.
TEST_P(BrowsingHistoryServiceKeepAllDuplicatesTest,
       SameDayVisitsMergedWithAllVisitIds) {
  // Visit IDs follow insertion order: 1, 2, 3. Hours 1 and 2 are on the first
  // day, hour 26 is on the next day.
  AddHistory({
      {kUrl1, 1, kLocal},
      {kUrl1, 2, kLocal},
      {kUrl1, 26, kLocal},
  });

  QueryOptions options;
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  TestBrowsingHistoryDriver::QueryResult result = QueryHistory(options);

  ASSERT_EQ(2u, result.first.size());
  EXPECT_EQ(GURL(kUrl1), result.first[0].url);
  EXPECT_THAT(result.first[0].all_visit_ids, ElementsAre(3));
  EXPECT_EQ(GURL(kUrl1), result.first[1].url);
  EXPECT_THAT(result.first[1].all_visit_ids, ElementsAre(2, 1));
  // `kUrl1` has 3 visits in total; merging two entries must not double it.
  EXPECT_EQ(3, result.first[1].visit_count);
}

// Tests that when a URL's same-day visits span two pages, each visit ID is
// attributed to exactly one entry.
TEST_P(BrowsingHistoryServiceKeepAllDuplicatesTest,
       VisitIdsAttributedOnceAcrossPages) {
  // Same day, newest first: kUrl1@3 (ID 3), kUrl2@2 (ID 2), kUrl1@1 (ID 1),
  // kUrl3@0 (ID 4).
  AddHistory({
      {kUrl1, 1, kLocal},
      {kUrl2, 2, kLocal},
      {kUrl1, 3, kLocal},
      {kUrl3, 0, kLocal},
  });

  QueryOptions options;
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  options.max_count = 2;

  TestBrowsingHistoryDriver::QueryResult page1 = QueryHistory(options);
  ASSERT_EQ(2u, page1.first.size());
  EXPECT_EQ(GURL(kUrl1), page1.first[0].url);
  EXPECT_THAT(page1.first[0].all_visit_ids, ElementsAre(3));
  EXPECT_EQ(GURL(kUrl2), page1.first[1].url);
  EXPECT_THAT(page1.first[1].all_visit_ids, ElementsAre(2));

  // Visit 1 is only returned, and attributed, on the second page.
  TestBrowsingHistoryDriver::QueryResult page2 = ContinueQuery();
  ASSERT_EQ(2u, page2.first.size());
  EXPECT_EQ(GURL(kUrl1), page2.first[0].url);
  EXPECT_THAT(page2.first[0].all_visit_ids, ElementsAre(1));
  EXPECT_EQ(GURL(kUrl3), page2.first[1].url);
  EXPECT_THAT(page2.first[1].all_visit_ids, ElementsAre(4));
}

// Tests that same-day typed visits to a URL are merged without adding up the
// URL's lifetime typed count once per merged visit.
TEST_P(BrowsingHistoryServiceKeepAllDuplicatesTest,
       SameDayVisitsKeepUrlTypedCount) {
  for (int hour_offset : {1, 2}) {
    HistoryAddPageArgs args;
    args.url = GURL(kUrl1);
    args.time = OffsetToTime(hour_offset);
    args.transition = ui::PAGE_TRANSITION_TYPED;
    local_history()->AddPage(args);
  }

  QueryOptions options;
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  TestBrowsingHistoryDriver::QueryResult result = QueryHistory(options);

  ASSERT_EQ(1u, result.first.size());
  // `kUrl1` was typed twice in total; merging two entries must not double it.
  EXPECT_EQ(2, result.first[0].typed_count);
}

INSTANTIATE_TEST_SUITE_P(,
                         BrowsingHistoryServiceKeepAllDuplicatesTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "GroupingEnabled"
                                             : "GroupingDisabled";
                         });

// Tests that when similar visits to different URLs are grouped, the entry's
// visit and typed counts add up each URL's lifetime counts exactly once.
TEST_F(BrowsingHistoryServiceTest, GroupSimilarVisitsAddsCountsOncePerUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowsingHistorySimilarVisitsGrouping);

  // All visits have the same host and title, so they are grouped into one
  // entry. http://www.a.com/1 is typed twice, http://www.a.com/2 is visited
  // once through a link.
  static constexpr struct {
    const char* url;
    int hour_offset;
    ui::PageTransition transition;
  } kVisits[] = {
      {"http://www.a.com/1", 1, ui::PAGE_TRANSITION_TYPED},
      {"http://www.a.com/1", 2, ui::PAGE_TRANSITION_TYPED},
      {"http://www.a.com/2", 3, ui::PAGE_TRANSITION_LINK},
  };
  for (const auto& visit : kVisits) {
    HistoryAddPageArgs args;
    args.url = GURL(visit.url);
    args.time = OffsetToTime(visit.hour_offset);
    args.title = u"Title A";
    args.transition = visit.transition;
    local_history()->AddPage(args);
  }

  QueryOptions options;
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  TestBrowsingHistoryDriver::QueryResult result = QueryHistory(options);

  ASSERT_EQ(1u, result.first.size());
  // 2 visits to /1 plus 1 visit to /2; /1 is counted once, not once per visit.
  EXPECT_EQ(3, result.first[0].visit_count);
  EXPECT_EQ(2, result.first[0].typed_count);
}

// Tests that a URL's local counts are kept when it's grouped with a remote
// visit to the same URL, which always reports zero counts. Remote visits are
// only queried past the oldest local visit, so they're always older.
TEST_F(BrowsingHistoryServiceTest,
       GroupSimilarVisitsKeepsLocalCountsWithRemoteVisit) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowsingHistorySimilarVisitsGrouping);

  AddHistory({{kUrl1, 2, kLocal}, {kUrl1, 1, kRemote}});

  QueryOptions options;
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  TestBrowsingHistoryDriver::QueryResult result = QueryHistory(options);

  ASSERT_EQ(1u, result.first.size());
  EXPECT_EQ(kBoth, result.first[0].entry_type);
  EXPECT_EQ(1, result.first[0].visit_count);
}

TEST_F(BrowsingHistoryServiceTest,
       SyncedDevicesDisambiguatedByChannelWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kWebHistoryUseNewApi, kBrowsingHistoryFilterByDevice,
       syncer::kSyncDisambiguateDeviceNamesWithChannel},
      {});

  EXPECT_CALL(*sync()->GetMockUserSettings(), GetSelectedTypes())
      .WillRepeatedly(testing::Return(
          syncer::UserSelectableTypeSet{syncer::UserSelectableType::kHistory}));

  device_info_tracker()->Add(syncer::TestDeviceInfoBuilder(
                                 syncer::DeviceInfo::OsType::kAndroid)
                                 .WithGuid("guid1")
                                 .WithClientName("")
                                 .WithManufacturerName("Google")
                                 .WithModelName("Pixel 6")
                                 .WithSyncUserAgent("Mozilla/5.0 channel(stable)")
                                 .Build());

  // Assert baseline state with a single device before adding a duplicate model
  // on a non-stable channel, and capture its base display name.
  std::vector<ClientGroup> initial_devices =
      service()->GetSyncedDevicesForFiltering();
  ASSERT_THAT(
      initial_devices,
      ElementsAre(AllOf(Field(&ClientGroup::is_local_device, false),
                        Field(&ClientGroup::client_ids, ElementsAre("guid1")))));
  const std::string base_name = initial_devices[0].name;
  ASSERT_FALSE(base_name.empty());

  device_info_tracker()->Add(syncer::TestDeviceInfoBuilder(
                                 syncer::DeviceInfo::OsType::kAndroid)
                                 .WithGuid("guid2")
                                 .WithClientName("")
                                 .WithManufacturerName("Google")
                                 .WithModelName("Pixel 6")
                                 .WithSyncUserAgent("Mozilla/5.0 channel(canary)")
                                 .Build());

  // With syncer::GetDeviceDisplayNames(), duplicate base names on different
  // channels receive release channel disambiguation labels and are un-grouped
  // into separate client groups rather than merged under `base_name`.
  EXPECT_THAT(
      service()->GetSyncedDevicesForFiltering(),
      UnorderedElementsAre(
          AllOf(Field(&ClientGroup::name, base_name),
                Field(&ClientGroup::is_local_device, false),
                Field(&ClientGroup::client_ids, ElementsAre("guid1"))),
          AllOf(Field(&ClientGroup::name, base_name + " (Canary)"),
                Field(&ClientGroup::is_local_device, false),
                Field(&ClientGroup::client_ids, ElementsAre("guid2")))));
}

critical_actions::CriticalActionEntry CreateCriticalActionEntry(
    std::string id,
    Time timestamp,
    VisitID visit_id,
    critical_actions::ActionType type = critical_actions::ActionType::kFormFill,
    std::string conversation_id = "",
    std::string actor_task_id = "",
    std::string metadata = "") {
  critical_actions::CriticalActionEntry entry;
  entry.critical_action_id = std::move(id);
  entry.timestamp = timestamp;
  entry.visit_id = visit_id;
  entry.action_type = type;
  entry.action_source = critical_actions::ActionSource::kActor;
  entry.conversation_id = std::move(conversation_id);
  entry.actor_task_id = std::move(actor_task_id);
  entry.metadata = std::move(metadata);
  return entry;
}

std::vector<std::string> GetCriticalActionIds(const HistoryEntry& entry) {
  std::vector<std::string> ids;
  for (const auto& action : entry.critical_actions) {
    ids.push_back(action.critical_action_id);
  }
  return ids;
}

class BrowsingHistoryServiceCriticalActionsTest
    : public BrowsingHistoryServiceTest {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        critical_actions::features::kCriticalActionHistory);
    ASSERT_TRUE(critical_actions_dir_.CreateUniqueTempDir());
    backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    critical_action_service_ =
        std::make_unique<critical_actions::CriticalActionService>(
            critical_actions_dir_.GetPath().AppendASCII("CriticalActions.db"),
            backend_task_runner_, local_history());
    ResetService(driver(), local_history(), /*web_history=*/nullptr,
                 /*sync_service=*/nullptr, device_info_tracker(),
                 critical_action_service_.get());
  }

  void TearDown() override {
    // Shut down and destroy CriticalActionService before BrowsingHistoryService
    // to mirror Profile teardown order (CriticalActionFactory depends on
    // HistoryServiceFactory) and verify no dangling pointers occur.
    if (critical_action_service_) {
      critical_action_service_->Shutdown();
      critical_action_service_.reset();
    }
    // Wait for pending backend tasks (e.g. Init() and ~CriticalActionBackend())
    // to finish before `critical_actions_dir_` is deleted by ~ScopedTempDir().
    if (backend_task_runner_) {
      base::test::TestFuture<void> backend_flushed;
      backend_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                             backend_flushed.GetCallback());
      ASSERT_TRUE(backend_flushed.Wait());
    }
  }

  VisitID AddVisitAndGetId(const GURL& url,
                           Time time,
                           bool is_actor_visit = true,
                           const std::u16string& title = u"Visit") {
    HistoryAddPageArgs args;
    args.url = url;
    args.time = time;
    args.title = title;
    args.visit_source = is_actor_visit ? SOURCE_ACTOR : SOURCE_BROWSED;
    local_history()->AddPage(args);

    base::CancelableTaskTracker tracker;
    base::test::TestFuture<QueryURLAndVisitsResult> future;
    local_history()->QueryURLAndVisits(url, VisitQuery404sPolicy::kInclude404s,
                                       future.GetCallback(), &tracker);
    const QueryURLAndVisitsResult& result = future.Get();
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.visits.empty());
    return result.visits.back().visit_id;
  }

  static QueryOptions ActorAndUserOptions() {
    QueryOptions options;
    options.duplicate_policy = QueryOptions::REMOVE_DUPLICATES_PER_DAY;
    options.include_actor_visits = true;
    return options;
  }

  TestBrowsingHistoryDriver::QueryResult QueryHistoryAndWait(
      const QueryOptions& options = ActorAndUserOptions()) {
    size_t previous_count = driver()->GetQueryResults().size();
    base::test::TestFuture<void> future;
    driver()->SetQueryCompleteCallback(future.GetCallback());
    service()->QueryHistory(std::u16string(), options);
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(previous_count + 1, driver()->GetQueryResults().size());
    return driver()->GetQueryResults().back();
  }

  critical_actions::CriticalActionService* critical_action_service() {
    return critical_action_service_.get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir critical_actions_dir_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  std::unique_ptr<critical_actions::CriticalActionService>
      critical_action_service_;
};

TEST_F(BrowsingHistoryServiceCriticalActionsTest,
       CriticalActionsPopulatedForActorAndUserVisits) {
  base::HistogramTester histogram_tester;
  const VisitID user_visit_id = AddVisitAndGetId(
      GURL(kUrl1), OffsetToTime(1), /*is_actor_visit=*/false, u"User Visit");
  const VisitID actor_visit_id = AddVisitAndGetId(
      GURL(kUrl2), OffsetToTime(2), /*is_actor_visit=*/true, u"Actor Visit");

  critical_action_service()->AddCriticalAction(CreateCriticalActionEntry(
      "user-action", OffsetToTime(1), user_visit_id,
      critical_actions::ActionType::kCredentialAccess));
  critical_action_service()->AddCriticalAction(
      CreateCriticalActionEntry("actor-action", OffsetToTime(2), actor_visit_id,
                                critical_actions::ActionType::kDownload));

  TestBrowsingHistoryDriver::QueryResult result = QueryHistoryAndWait();
  EXPECT_TRUE(result.second.queried_critical_actions);
  ASSERT_EQ(2u, result.first.size());

  EXPECT_TRUE(result.first[0].is_actor_visit);
  EXPECT_THAT(GetCriticalActionIds(result.first[0]),
              ElementsAre("actor-action"));

  EXPECT_FALSE(result.first[1].is_actor_visit);
  EXPECT_THAT(GetCriticalActionIds(result.first[1]),
              ElementsAre("user-action"));

  histogram_tester.ExpectTotalCount("HistoryPage.CriticalActionsQueryTime", 1);
  histogram_tester.ExpectUniqueSample(
      "HistoryPage.CriticalActionsPerVisitCount", 1, 2);
}

TEST_F(BrowsingHistoryServiceCriticalActionsTest,
       CriticalActionsNotQueriedWhenFeatureDisabled) {
  base::test::ScopedFeatureList disabled;
  disabled.InitAndDisableFeature(
      critical_actions::features::kCriticalActionHistory);
  base::HistogramTester histogram_tester;

  const VisitID visit_id = AddVisitAndGetId(GURL(kUrl1), OffsetToTime(1));
  critical_action_service()->AddCriticalAction(
      CreateCriticalActionEntry("action-1", OffsetToTime(1), visit_id));

  TestBrowsingHistoryDriver::QueryResult result = QueryHistoryAndWait();
  EXPECT_FALSE(result.second.queried_critical_actions);
  ASSERT_EQ(1u, result.first.size());
  EXPECT_TRUE(result.first[0].critical_actions.empty());
  histogram_tester.ExpectTotalCount("HistoryPage.CriticalActionsQueryTime", 0);
  histogram_tester.ExpectTotalCount("HistoryPage.CriticalActionsPerVisitCount",
                                    0);
}

TEST_F(BrowsingHistoryServiceCriticalActionsTest,
       CriticalActionsNotQueriedWhenProviderNull) {
  ResetService(driver(), local_history(), /*web_history=*/nullptr,
               /*sync_service=*/nullptr, device_info_tracker(),
               /*critical_action_provider=*/nullptr);
  base::HistogramTester histogram_tester;

  const VisitID visit_id = AddVisitAndGetId(GURL(kUrl1), OffsetToTime(1));
  critical_action_service()->AddCriticalAction(
      CreateCriticalActionEntry("action-1", OffsetToTime(1), visit_id));

  TestBrowsingHistoryDriver::QueryResult result = QueryHistoryAndWait();
  EXPECT_FALSE(result.second.queried_critical_actions);
  ASSERT_EQ(1u, result.first.size());
  EXPECT_TRUE(result.first[0].critical_actions.empty());
  histogram_tester.ExpectTotalCount("HistoryPage.CriticalActionsQueryTime", 0);
  histogram_tester.ExpectTotalCount("HistoryPage.CriticalActionsPerVisitCount",
                                    0);
}

TEST_F(BrowsingHistoryServiceCriticalActionsTest,
       CriticalActionsNotQueriedForRemoteOnlyVisits) {
  ResetService(driver(), local_history(), web_history(), sync(),
               device_info_tracker(), critical_action_service());
  base::HistogramTester histogram_tester;

  AddHistory({{kUrl1, 1, kRemote}});

  TestBrowsingHistoryDriver::QueryResult result = QueryHistoryAndWait();
  EXPECT_FALSE(result.second.queried_critical_actions);
  ASSERT_EQ(1u, result.first.size());
  EXPECT_TRUE(result.first[0].critical_actions.empty());
  histogram_tester.ExpectTotalCount("HistoryPage.CriticalActionsQueryTime", 0);
}

TEST_F(BrowsingHistoryServiceCriticalActionsTest,
       CriticalActionsDistinguishesSeparateVisitsSameUrl) {
  const Time day1_visit = OffsetToTime(1);
  const Time day2_visit = OffsetToTime(26);
  const VisitID visit_id1 = AddVisitAndGetId(GURL(kUrl1), day1_visit);
  const VisitID visit_id2 = AddVisitAndGetId(GURL(kUrl1), day2_visit);

  critical_action_service()->AddCriticalAction(CreateCriticalActionEntry(
      "action-visit-1", day1_visit + base::Seconds(5), visit_id1,
      critical_actions::ActionType::kCredentialAccess));
  critical_action_service()->AddCriticalAction(CreateCriticalActionEntry(
      "action-visit-2", day2_visit + base::Seconds(5), visit_id2,
      critical_actions::ActionType::kDownload));

  TestBrowsingHistoryDriver::QueryResult result = QueryHistoryAndWait();
  ASSERT_EQ(2u, result.first.size());
  EXPECT_THAT(GetCriticalActionIds(result.first[0]),
              ElementsAre("action-visit-2"));
  EXPECT_THAT(GetCriticalActionIds(result.first[1]),
              ElementsAre("action-visit-1"));
}

TEST_F(BrowsingHistoryServiceCriticalActionsTest,
       CriticalActionsAttachedToMergedSameDayVisits) {
  base::test::ScopedFeatureList disable_grouping;
  disable_grouping.InitAndDisableFeature(kBrowsingHistorySimilarVisitsGrouping);
  base::HistogramTester histogram_tester;

  const Time t1 = baseline_time_ + base::Hours(1);
  const Time t2 = baseline_time_ + base::Hours(2);
  const VisitID visit_id1 = AddVisitAndGetId(GURL(kUrl1), t1);
  const VisitID visit_id2 = AddVisitAndGetId(GURL(kUrl1), t2);

  critical_action_service()->AddCriticalAction(CreateCriticalActionEntry(
      "action-visit-1", t1 + base::Seconds(5), visit_id1,
      critical_actions::ActionType::kCredentialAccess));
  critical_action_service()->AddCriticalAction(CreateCriticalActionEntry(
      "action-visit-2", t2 + base::Seconds(5), visit_id2,
      critical_actions::ActionType::kDownload));

  QueryOptions options = ActorAndUserOptions();
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  TestBrowsingHistoryDriver::QueryResult result = QueryHistoryAndWait(options);
  ASSERT_EQ(1u, result.first.size());
  EXPECT_THAT(GetCriticalActionIds(result.first[0]),
              ElementsAre("action-visit-2", "action-visit-1"));
  histogram_tester.ExpectUniqueSample(
      "HistoryPage.CriticalActionsPerVisitCount", 2, 1);
}

TEST_F(BrowsingHistoryServiceCriticalActionsTest,
       CriticalActionsAttachedToGroupedSimilarVisits) {
  base::test::ScopedFeatureList enable_grouping;
  enable_grouping.InitAndEnableFeature(kBrowsingHistorySimilarVisitsGrouping);

  const Time t1 = baseline_time_ + base::Hours(1);
  const Time t2 = baseline_time_ + base::Hours(2);
  const VisitID visit_id1 = AddVisitAndGetId(
      GURL("http://www.a.com/1"), t1, /*is_actor_visit=*/true, u"Shared Title");
  const VisitID visit_id2 = AddVisitAndGetId(
      GURL("http://www.a.com/2"), t2, /*is_actor_visit=*/true, u"Shared Title");

  critical_action_service()->AddCriticalAction(
      CreateCriticalActionEntry("action-1", t1, visit_id1));
  critical_action_service()->AddCriticalAction(
      CreateCriticalActionEntry("action-2", t2, visit_id2));

  QueryOptions options = ActorAndUserOptions();
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  TestBrowsingHistoryDriver::QueryResult result = QueryHistoryAndWait(options);
  ASSERT_EQ(1u, result.first.size());
  EXPECT_THAT(GetCriticalActionIds(result.first[0]),
              ElementsAre("action-2", "action-1"));
}

// Only verifies that BrowsingHistoryService deduplicates the fetched actions;
// the deduplication rules are covered by critical_action_history_utils
// tests.
TEST_F(BrowsingHistoryServiceCriticalActionsTest, CriticalActionsDeduplicated) {
  const Time visit_time = OffsetToTime(2);
  const VisitID visit_id = AddVisitAndGetId(GURL(kUrl1), visit_time);

  critical_action_service()->AddCriticalAction(CreateCriticalActionEntry(
      "latest", visit_time + base::Seconds(1), visit_id));
  critical_action_service()->AddCriticalAction(
      CreateCriticalActionEntry("duplicate", visit_time, visit_id));

  TestBrowsingHistoryDriver::QueryResult result = QueryHistoryAndWait();
  ASSERT_EQ(1u, result.first.size());
  EXPECT_THAT(GetCriticalActionIds(result.first[0]), ElementsAre("latest"));
}

TEST_F(BrowsingHistoryServiceCriticalActionsTest,
       SupersedingQueryCancelsInFlightCriticalActionsFetch) {
  critical_actions::FakeCriticalActionProvider fake_provider;
  ResetService(driver(), local_history(), /*web_history=*/nullptr,
               /*sync_service=*/nullptr, device_info_tracker(), &fake_provider);

  const VisitID visit_id1 = AddVisitAndGetId(GURL(kUrl1), OffsetToTime(1));
  const VisitID visit_id2 = AddVisitAndGetId(GURL(kUrl2), OffsetToTime(2));

  // Start query 1 and wait until it requests critical actions.
  base::test::TestFuture<void> request1_future;
  fake_provider.SetOnRequestCallback(request1_future.GetCallback());
  QueryOptions options1 = ActorAndUserOptions();
  options1.end_time = OffsetToTime(2);  // Excludes visit_id2.
  service()->QueryHistory(std::u16string(), options1);
  ASSERT_TRUE(request1_future.Wait());
  ASSERT_EQ(1u, fake_provider.request_count());
  EXPECT_TRUE(driver()->GetQueryResults().empty());

  // Start query 2 while query 1's critical action fetch is still in flight.
  base::test::TestFuture<void> request2_future;
  fake_provider.SetOnRequestCallback(request2_future.GetCallback());
  service()->QueryHistory(std::u16string(), ActorAndUserOptions());
  ASSERT_TRUE(request2_future.Wait());
  ASSERT_EQ(2u, fake_provider.request_count());

  // Resolving query 1's stale callback now must be ignored.
  fake_provider.ResolveCallback(
      0,
      {CreateCriticalActionEntry("stale-action", OffsetToTime(1), visit_id1)});
  EXPECT_TRUE(driver()->GetQueryResults().empty());

  // Resolving query 2's callback delivers query 2's results.
  fake_provider.ResolveCallback(
      1,
      {CreateCriticalActionEntry("fresh-action", OffsetToTime(2), visit_id2)});
  ASSERT_EQ(1u, driver()->GetQueryResults().size());
  ASSERT_EQ(2u, driver()->GetQueryResults()[0].first.size());
  EXPECT_THAT(GetCriticalActionIds(driver()->GetQueryResults()[0].first[0]),
              ElementsAre("fresh-action"));
}

TEST_F(BrowsingHistoryServiceCriticalActionsTest,
       QueryDurationRecordedBeforeCriticalActionsFetch) {
  critical_actions::FakeCriticalActionProvider fake_provider;
  ResetService(driver(), local_history(), /*web_history=*/nullptr,
               /*sync_service=*/nullptr, device_info_tracker(), &fake_provider);
  base::HistogramTester histogram_tester;

  const VisitID visit_id = AddVisitAndGetId(GURL(kUrl1), OffsetToTime(1));

  // Start the query and wait until it requests critical actions.
  base::test::TestFuture<void> request_future;
  fake_provider.SetOnRequestCallback(request_future.GetCallback());
  service()->QueryHistory(std::u16string(), ActorAndUserOptions());
  ASSERT_TRUE(request_future.Wait());
  ASSERT_EQ(1u, fake_provider.request_count());
  EXPECT_TRUE(driver()->GetQueryResults().empty());

  // The query duration must already be recorded, so that it excludes the
  // critical actions lookup and has the same semantics regardless of whether
  // `kCriticalActionHistory` is enabled.
  histogram_tester.ExpectTotalCount("History.BrowsingHistory.QueryDuration", 1);
  histogram_tester.ExpectTotalCount("HistoryPage.CriticalActionsQueryTime", 0);

  // Completing the critical actions lookup must not record it again.
  fake_provider.ResolveCallback(
      0, {CreateCriticalActionEntry("action-1", OffsetToTime(1), visit_id)});
  ASSERT_EQ(1u, driver()->GetQueryResults().size());
  histogram_tester.ExpectTotalCount("History.BrowsingHistory.QueryDuration", 1);
  histogram_tester.ExpectTotalCount("HistoryPage.CriticalActionsQueryTime", 1);
}

TEST_F(BrowsingHistoryServiceCriticalActionsTest,
       ProviderShutdownSkipsCriticalActionsWithoutDangling) {
  auto provider =
      std::make_unique<critical_actions::FakeCriticalActionProvider>();
  ResetService(driver(), local_history(), /*web_history=*/nullptr,
               /*sync_service=*/nullptr, device_info_tracker(), provider.get());

  AddVisitAndGetId(GURL(kUrl1), OffsetToTime(1));

  // Shut down and destroy the provider before BrowsingHistoryService queries.
  provider->Shutdown();
  provider.reset();

  TestBrowsingHistoryDriver::QueryResult result = QueryHistoryAndWait();
  ASSERT_EQ(1u, result.first.size());
  EXPECT_TRUE(result.first[0].critical_actions.empty());
}

}  // namespace

}  // namespace history
