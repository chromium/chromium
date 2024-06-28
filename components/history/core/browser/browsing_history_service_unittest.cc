// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/browsing_history_service.h"

#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "components/history/core/browser/browsing_history_driver.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/sync/test/mock_sync_service.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::Time;

namespace history {

using HistoryEntry = BrowsingHistoryService::HistoryEntry;

namespace {

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

struct TestResult {
  std::string url;
  int64_t hour_offset;  // Visit time in hours past the baseline time.
  HistoryEntry::EntryType type;
  std::string remote_icon_url_for_uma;
};

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
  TestWebHistoryService() : FakeWebHistoryService() {}

  void TriggerOnWebHistoryDeleted() {
    TestRequest request;
    ExpireHistoryCompletionCallback(base::DoNothing(), &request, true);
  }

 protected:
  class TestRequest : public WebHistoryService::Request {
   private:
    // WebHistoryService::Request implementation.
    bool IsPending() override { return false; }
    int GetResponseCode() override { return net::HTTP_OK; }
    const std::string& GetResponseBody() override { return body_; }
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
    std::reverse(result.begin(), result.end());
    return result;
  }
};

class TimeoutWebHistoryService : public TestWebHistoryService {
 private:
  // WebHistoryService implementation.
  Request* CreateRequest(const GURL& url,
                         CompletionCallback callback,
                         const net::PartialNetworkTrafficAnnotationTag&
                             partial_traffic_annotation) override {
    return new TestWebHistoryService::TestRequest();
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

class BrowsingHistoryServiceTest : public ::testing::Test {
 protected:
  // WebHistory API is to pass time ranges as the number of microseconds since
  // Time::UnixEpoch() as a query parameter. This becomes a problem when we use
  // Time::LocalMidnight(), which rounds _down_, and will result in a  time
  // before Time::UnixEpoch() that cannot be represented. By adding 1 day we
  // ensure all test data is after Time::UnixEpoch().
  BrowsingHistoryServiceTest()
      : baseline_time_(Time::UnixEpoch().LocalMidnight() + base::Days(1)),
        driver_(&web_history_) {
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
    return baseline_time_ + base::Hours(hour_offset);
  }

  void AddHistory(const std::vector<TestResult>& data,
                  TestWebHistoryService* web_history) {
    for (const TestResult& entry : data) {
      if (entry.type == kLocal) {
        local_history()->AddPage(GURL(entry.url),
                                 OffsetToTime(entry.hour_offset),
                                 VisitSource::SOURCE_BROWSED);
      } else if (entry.type == kRemote) {
        web_history->AddSyncedVisit(entry.url, OffsetToTime(entry.hour_offset),
                                    entry.remote_icon_url_for_uma);
      } else {
        NOTREACHED_IN_MIGRATION();
      }
    }
  }

  void AddHistory(const std::vector<TestResult>& data) {
    AddHistory(data, web_history());
  }

  void VerifyEntry(const TestResult& expected, const HistoryEntry& actual) {
    EXPECT_EQ(GURL(expected.url), actual.url);
    EXPECT_EQ(OffsetToTime(expected.hour_offset), actual.time);
    EXPECT_EQ(static_cast<int>(expected.type),
              static_cast<int>(actual.entry_type));
    EXPECT_EQ(GURL(expected.remote_icon_url_for_uma),
              actual.remote_icon_url_for_uma);
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

  void VerifyQueryResult(bool reached_beginning,
                         bool has_synced_results,
                         const std::vector<TestResult>& expected_entries,
                         TestBrowsingHistoryDriver::QueryResult result) {
    EXPECT_EQ(reached_beginning, result.second.reached_beginning);
    EXPECT_EQ(has_synced_results, result.second.has_synced_results);
    EXPECT_FALSE(result.second.sync_timed_out);
    EXPECT_EQ(expected_entries.size(), result.first.size());
    for (size_t i = 0; i < expected_entries.size(); ++i) {
      VerifyEntry(expected_entries[i], result.first[i]);
    }
  }

  HistoryService* local_history() { return local_history_.get(); }
  TestWebHistoryService* web_history() { return &web_history_; }
  syncer::MockSyncService* sync() { return &sync_service_; }
  TestBrowsingHistoryDriver* driver() { return &driver_; }
  base::MockOneShotTimer* timer() { return timer_; }
  TestBrowsingHistoryService* service() {
    return browsing_history_service_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  // Duplicates on the same day in the local timezone are removed, so set a
  // baseline time in local time.
  Time baseline_time_;

  base::ScopedTempDir history_dir_;
  std::unique_ptr<HistoryService> local_history_;
  TestWebHistoryService web_history_;
  syncer::MockSyncService sync_service_;
  TestBrowsingHistoryDriver driver_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> timer_;
  std::unique_ptr<TestBrowsingHistoryService> browsing_history_service_;
};

TEST_F(BrowsingHistoryServiceTest, QueryHistoryNoSources) {
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), nullptr, nullptr);
  VerifyQueryResult(/*reached_beginning*/ true,
                    /*has_synced_results*/ false, {}, QueryHistory());
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryJustLocal) {
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), local_history(), nullptr);
  VerifyQueryResult(/*reached_beginning*/ true,
                    /*has_synced_results*/ false, {}, QueryHistory());
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryJustLocal) {
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), local_history(), nullptr);
  AddHistory({{kUrl1, 1, kLocal}});
  VerifyQueryResult(/*reached_beginning*/ true,
                    /*has_synced_results*/ false, {{kUrl1, 1, kLocal}},
                    QueryHistory());
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryJustWeb) {
  ResetService(driver(), nullptr, nullptr);
  VerifyQueryResult(/*reached_beginning*/ true,
                    /*has_synced_results*/ true, {}, QueryHistory());
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryDelayedWeb) {
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), nullptr, sync());
  driver()->SetWebHistory(web_history());
  VerifyQueryResult(/*reached_beginning*/ true,
                    /*has_synced_results*/ true, {}, QueryHistory());
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryJustWeb) {
  ResetService(driver(), nullptr, sync());
  AddHistory({{kUrl1, 1, kRemote}});
  VerifyQueryResult(/*reached_beginning*/ true,
                    /*has_synced_results*/ true, {{kUrl1, 1, kRemote}},
                    QueryHistory());
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryBothSources) {
  ResetService(driver(), local_history(), sync());
  VerifyQueryResult(/*reached_beginning*/ true,
                    /*has_synced_results*/ true, {}, QueryHistory());
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryAllSources) {
  ResetService(driver(), local_history(), sync());
  AddHistory({{kUrl1, 1, kLocal},
              {kUrl2, 2, kLocal},
              {kUrl3, 3, kRemote},
              {kUrl1, 4, kRemote}});
  VerifyQueryResult(
      /*reached_beginning*/ true, /*has_synced_results*/ true,
      {{kUrl1, 4, kBoth}, {kUrl3, 3, kRemote}, {kUrl2, 2, kLocal}},
      QueryHistory());
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryLocalTimeRanges) {
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
  VerifyQueryResult(/*reached_beginning*/ false,
                    /*has_synced_results*/ true,
                    {{kUrl3, 3, kLocal}, {kUrl2, 2, kLocal}},
                    QueryHistory(options));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryRemoteTimeRanges) {
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl2, 2, kRemote},
              {kUrl3, 3, kRemote},
              {kUrl4, 4, kRemote}});
  QueryOptions options;
  options.begin_time = OffsetToTime(2);
  options.end_time = OffsetToTime(4);
  VerifyQueryResult(
      /*reached_beginning*/ true, /*has_synced_results*/ true,
      {{kUrl3, 3, kRemote}, {kUrl2, 2, kRemote}}, QueryHistory(options));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryHostOnlyRemote) {
  AddHistory({{kUrl8, 1, kRemote}, {kUrl9, 2, kRemote}, {kUrl10, 3, kRemote}});

  QueryOptions options;
  options.max_count = 0;
  options.host_only = false;
  VerifyQueryResult(
      /*reached_beginning*/ true,
      /*has_synced_results*/ true,
      {{kUrl10, 3, kRemote}, {kUrl9, 2, kRemote}, {kUrl8, 1, kRemote}},
      QueryHistory(u"eight.com", options));
  options.host_only = true;
  VerifyQueryResult(/*reached_beginning*/ true,
                    /*has_synced_results*/ true, {{kUrl8, 1, kRemote}},
                    QueryHistory(u"eight.com", options));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryLocalPagingPartial) {
  AddHistory({{kUrl1, 1, kLocal}, {kUrl2, 2, kLocal}, {kUrl3, 3, kLocal}});
  VerifyQueryResult(/*reached_beginning*/ false,
                    /*has_synced_results*/ true,
                    {{kUrl3, 3, kLocal}, {kUrl2, 2, kLocal}}, QueryHistory(2));
  VerifyQueryResult(
      /*reached_beginning*/ true, /*has_synced_results*/ true,
      {{kUrl1, 1, kLocal}}, ContinueQuery());
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryLocalPagingFull) {
  AddHistory({{kUrl1, 1, kLocal}, {kUrl2, 2, kLocal}, {kUrl3, 3, kLocal}});
  VerifyQueryResult(
      /*reached_beginning*/ true, /*has_synced_results*/ true,
      {{kUrl3, 3, kLocal}, {kUrl2, 2, kLocal}, {kUrl1, 1, kLocal}},
      QueryHistory(3));
  VerifyQueryResult(
      /*reached_beginning*/ true, /*has_synced_results*/ true, {},
      ContinueQuery());
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryRemotePagingPartial) {
  AddHistory({{kUrl1, 1, kRemote}, {kUrl2, 2, kRemote}, {kUrl3, 3, kRemote}});
  VerifyQueryResult(/*reached_beginning*/ false,
                    /*has_synced_results*/ true,
                    {{kUrl3, 3, kRemote}, {kUrl2, 2, kRemote}},
                    QueryHistory(2));
  VerifyQueryResult(
      /*reached_beginning*/ true, /*has_synced_results*/ true,
      {{kUrl1, 1, kRemote}}, ContinueQuery());
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryRemotePagingFull) {
  AddHistory({{kUrl1, 1, kRemote}, {kUrl2, 2, kRemote}, {kUrl3, 3, kRemote}});
  VerifyQueryResult(
      /*reached_beginning*/ true, /*has_synced_results*/ true,
      {{kUrl3, 3, kRemote}, {kUrl2, 2, kRemote}, {kUrl1, 1, kRemote}},
      QueryHistory(3));
  VerifyQueryResult(
      /*reached_beginning*/ true, /*has_synced_results*/ true, {},
      ContinueQuery());
}

TEST_F(BrowsingHistoryServiceTest, MergeDuplicatesSameDay) {
  AddHistory({{kUrl1, 0, kRemote},
              {kUrl2, 1, kRemote},
              {kUrl1, 2, kRemote},
              {kUrl1, 3, kRemote}});
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true,
                    {{kUrl1, 3, kRemote}, {kUrl2, 1, kRemote}}, QueryHistory());
}

TEST_F(BrowsingHistoryServiceTest, MergeDuplicatesNextDayNotRemoved) {
  AddHistory({{kUrl1, 0, kRemote}, {kUrl1, 23, kRemote}, {kUrl1, 24, kRemote}});
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true,
                    {{kUrl1, 24, kRemote}, {kUrl1, 23, kRemote}},
                    QueryHistory());
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
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true,
                    {{kUrl1, 27, kRemote},
                     {kUrl2, 26, kRemote},
                     {kUrl1, 3, kRemote},
                     {kUrl2, 2, kRemote}},
                    QueryHistory());
}

TEST_F(BrowsingHistoryServiceTest, MergeDuplicatesVerifyTimestamps) {
  AddHistory({{kUrl1, 0, kRemote},
              {kUrl2, 1, kRemote},
              {kUrl1, 2, kRemote},
              {kUrl1, 3, kRemote}});
  auto results = QueryHistory();
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true,
                    {{kUrl1, 3, kRemote}, {kUrl2, 1, kRemote}}, results);
  EXPECT_EQ(3U, results.first[0].all_timestamps.size());
  EXPECT_EQ(1U, results.first[1].all_timestamps.size());
}

TEST_F(BrowsingHistoryServiceTest, MergeDuplicatesKeepNonEmptyIconUrl) {
  AddHistory({{kUrl1, 0, kRemote, kIconUrl1}, {kUrl1, 1, kLocal}});
  auto results = QueryHistory();
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true,
                    {{kUrl1, 1, kBoth, kIconUrl1}}, results);

  AddHistory({{kUrl1, 0, kLocal}, {kUrl1, 1, kRemote, kIconUrl1}});
  results = QueryHistory();
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true,
                    {{kUrl1, 1, kBoth, kIconUrl1}}, results);
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryMerge) {
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl2, 2, kRemote},
              {kUrl3, 3, kLocal},
              {kUrl1, 4, kLocal}});
  VerifyQueryResult(
      /*reached_beginning*/ true, /*has_synced_results*/ true,
      {{kUrl1, 4, kBoth}, {kUrl3, 3, kLocal}, {kUrl2, 2, kRemote}},
      QueryHistory());
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryPending) {
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl2, 2, kRemote},
              {kUrl3, 3, kLocal},
              {kUrl4, 4, kLocal}});
  VerifyQueryResult(
      /*reached_beginning*/ false, /*has_synced_results*/ true,
      {{kUrl4, 4, kLocal}}, QueryHistory(1));
  VerifyQueryResult(
      /*reached_beginning*/ false, /*has_synced_results*/ true,
      {{kUrl3, 3, kLocal}, {kUrl2, 2, kRemote}}, ContinueQuery());
  VerifyQueryResult(
      /*reached_beginning*/ true, /*has_synced_results*/ true,
      {{kUrl1, 1, kRemote}}, ContinueQuery());
}

// A full request worth of local results will sit in pending, resulting in us
// being able to delete local history before our next query and we should still
// see the local entry.
TEST_F(BrowsingHistoryServiceTest, QueryHistoryFullLocalPending) {
  AddHistory({{kUrl1, 1, kLocal}, {kUrl2, 2, kRemote}, {kUrl3, 3, kRemote}});
  VerifyQueryResult(
      /*reached_beginning*/ false, /*has_synced_results*/ true,
      {{kUrl3, 3, kRemote}}, QueryHistory(1));

  local_history()->DeleteURLs({GURL(kUrl1)});
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true,
                    {{kUrl2, 2, kRemote}, {kUrl1, 1, kLocal}}, ContinueQuery());
}

// Part of a request worth of local results will sit in pending, resulting in us
// seeing extra local results on our next request.
TEST_F(BrowsingHistoryServiceTest, QueryHistoryPartialLocalPending) {
  AddHistory({{kUrl1, 1, kLocal},
              {kUrl2, 2, kLocal},
              {kUrl3, 3, kRemote},
              {kUrl4, 4, kLocal},
              {kUrl5, 5, kRemote},
              {kUrl6, 6, kRemote},
              {kUrl7, 7, kLocal}});
  VerifyQueryResult(
      /*reached_beginning*/ false, /*has_synced_results*/ true,
      {{kUrl7, 7, kLocal}, {kUrl6, 6, kRemote}, {kUrl5, 5, kRemote}},
      QueryHistory(2));
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true,
                    {{kUrl4, 4, kLocal},
                     {kUrl3, 3, kRemote},
                     {kUrl2, 2, kLocal},
                     {kUrl1, 1, kLocal}},
                    ContinueQuery());
}

// A full request worth of remote results will sit in pending, resulting in us
// being able to delete remote history before our next query and we should still
// see the remote entry.
TEST_F(BrowsingHistoryServiceTest, QueryHistoryFullRemotePending) {
  AddHistory({{kUrl1, 1, kRemote}, {kUrl2, 2, kLocal}, {kUrl3, 3, kLocal}});
  VerifyQueryResult(/*reached_beginning*/ false, /*has_synced_results*/ true,
                    {{kUrl3, 3, kLocal}}, QueryHistory(1));

  web_history()->ClearSyncedVisits();
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true,
                    {{kUrl2, 2, kLocal}, {kUrl1, 1, kRemote}}, ContinueQuery());
}

// Part of a request worth of remote results will sit in pending, resulting in
// us seeing extra remote results on our next request.
TEST_F(BrowsingHistoryServiceTest, QueryHistoryPartialRemotePending) {
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl2, 2, kRemote},
              {kUrl3, 3, kLocal},
              {kUrl4, 4, kRemote},
              {kUrl5, 5, kLocal},
              {kUrl6, 6, kLocal},
              {kUrl7, 7, kRemote}});
  VerifyQueryResult(
      /*reached_beginning*/ false, /*has_synced_results*/ true,
      {{kUrl7, 7, kRemote}, {kUrl6, 6, kLocal}, {kUrl5, 5, kLocal}},
      QueryHistory(2));
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true,
                    {{kUrl4, 4, kRemote},
                     {kUrl3, 3, kLocal},
                     {kUrl2, 2, kRemote},
                     {kUrl1, 1, kRemote}},
                    ContinueQuery());
}

TEST_F(BrowsingHistoryServiceTest, RetryOnRemoteFailureEmpty) {
  web_history()->SetupFakeResponse(false, 0);
  VerifyQueryResult(/*reached_beginning*/ false, /*has_synced_results*/ false,
                    {}, QueryHistory());
  web_history()->SetupFakeResponse(true, net::HTTP_OK);
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true, {},
                    ContinueQuery());
}

TEST_F(BrowsingHistoryServiceTest, RetryOnRemoteFailurePagingRemote) {
  AddHistory({{kUrl1, 1, kRemote}, {kUrl2, 2, kRemote}, {kUrl3, 3, kRemote}});
  VerifyQueryResult(/*reached_beginning*/ false, /*has_synced_results*/ true,
                    {{kUrl3, 3, kRemote}, {kUrl2, 2, kRemote}},
                    QueryHistory(2));

  web_history()->SetupFakeResponse(false, 0);
  VerifyQueryResult(/*reached_beginning*/ false, /*has_synced_results*/ false,
                    {}, ContinueQuery());

  web_history()->SetupFakeResponse(true, net::HTTP_OK);
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true,
                    {{kUrl1, 1, kRemote}}, ContinueQuery());
}

TEST_F(BrowsingHistoryServiceTest, RetryOnRemoteFailurePagingLocal) {
  AddHistory({{kUrl1, 1, kLocal}, {kUrl2, 2, kLocal}, {kUrl3, 3, kLocal}});
  web_history()->SetupFakeResponse(false, 0);
  VerifyQueryResult(/*reached_beginning*/ false, /*has_synced_results*/ false,
                    {{kUrl3, 3, kLocal}, {kUrl2, 2, kLocal}}, QueryHistory(2));

  web_history()->SetupFakeResponse(true, net::HTTP_OK);
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true,
                    {{kUrl1, 1, kLocal}}, ContinueQuery());
}

TEST_F(BrowsingHistoryServiceTest, WebHistoryTimeout) {
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
  EXPECT_FALSE(driver()->GetQueryResults()[0].second.has_synced_results);
  EXPECT_TRUE(driver()->GetQueryResults()[0].second.sync_timed_out);

  // WebHistoryService will DCHECK if we destroy it before the observer in
  // BrowsingHistoryService is removed, so reset our first
  // BrowsingHistoryService before `timeout` goes out of scope.
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), nullptr, nullptr);
}

TEST_F(BrowsingHistoryServiceTest, ObservingWebHistory) {
  // No need to observe SyncService since we have a WebHistory already.
  EXPECT_CALL(*sync(), AddObserver).Times(0);
  EXPECT_CALL(*sync(), RemoveObserver).Times(0);

  ResetService(driver(), nullptr, sync());

  web_history()->TriggerOnWebHistoryDeleted();
  EXPECT_EQ(1, driver()->GetHistoryDeletedCount());
}

TEST_F(BrowsingHistoryServiceTest, ObservingWebHistoryDelayedWeb) {
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

TEST_F(BrowsingHistoryServiceTest, IncorrectlyOrderedRemoteResults) {
  // Created from crbug.com/787928, where suspected MergeDuplicateResults did
  // not start with sorted data. This case originally hit a NOTREACHED.
  ReversedWebHistoryService reversed;
  driver()->SetWebHistory(&reversed);
  ResetService(driver(), local_history(), sync());
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl1, 2, kLocal},
              {kUrl3, 3, kLocal},
              {kUrl5, 4, kRemote},
              {kUrl5, 5, kLocal},
              {kUrl6, 6, kRemote}},
             &reversed);
  VerifyQueryResult(
      /*reached_beginning*/ false, /*has_synced_results*/ true,
      {{kUrl6, 6, kRemote}, {kUrl5, 5, kBoth}}, QueryHistory(2));

  // WebHistoryService will DCHECK if we destroy it before the observer in
  // BrowsingHistoryService is removed, so reset our first
  // BrowsingHistoryService before `reversed` goes out of scope.
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), nullptr, nullptr);
}

}  // namespace

}  // namespace history
