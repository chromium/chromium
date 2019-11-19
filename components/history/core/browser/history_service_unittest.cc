// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History unit tests come in two flavors:
//
// 1. The more complicated style is that the unit test creates a full history
//    service. This spawns a background thread for the history backend, and
//    all communication is asynchronous. This is useful for testing more
//    complicated things or end-to-end behavior.
//
// 2. The simpler style is to create a history backend on this thread and
//    access it directly without a HistoryService object. This is much simpler
//    because communication is synchronous. Generally, sets should go through
//    the history backend (since there is a lot of logic) but gets can come
//    directly from the HistoryDatabase. This is because the backend generally
//    has no logic in the getter except threading stuff, which we don't want
//    to run.

#include "components/history/core/browser/history_service.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/test/database_test_utils.h"
#include "components/history/core/test/test_history_database.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_change_processor_wrapper_for_test.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/sync_merge_result.h"
#include "components/sync/protocol/history_delete_directive_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

class HistoryServiceTest : public testing::Test {
 public:
  HistoryServiceTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

  ~HistoryServiceTest() override {}

 protected:
  friend class BackendDelegate;

  // testing::Test
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    history_dir_ = temp_dir_.GetPath().AppendASCII("HistoryServiceTest");
    ASSERT_TRUE(base::CreateDirectory(history_dir_));
    history_service_.reset(new history::HistoryService);
    if (!history_service_->Init(
            TestHistoryDatabaseParamsForPath(history_dir_))) {
      history_service_.reset();
      ADD_FAILURE();
    }
  }

  void TearDown() override {
    if (history_service_)
      CleanupHistoryService();

    // Make sure we don't have any event pending that could disrupt the next
    // test.
    base::RunLoop().RunUntilIdle();
  }

  void CleanupHistoryService() {
    DCHECK(history_service_);

    base::RunLoop run_loop;
    history_service_->ClearCachedDataForContextID(nullptr);
    history_service_->SetOnBackendDestroyTask(run_loop.QuitClosure());
    history_service_->Cleanup();
    history_service_.reset();

    // Wait for the backend class to terminate before deleting the files and
    // moving to the next test. Note: if this never terminates, somebody is
    // probably leaking a reference to the history backend, so it never calls
    // our destroy task.
    run_loop.Run();
  }

  // Fills the query_url_result_ structures with the information about the given
  // URL and whether the operation succeeded or not.
  bool QueryURL(history::HistoryService* history, const GURL& url) {
    base::RunLoop run_loop;
    history_service_->QueryURL(
        url, true,
        base::BindLambdaForTesting([&](history::QueryURLResult result) {
          query_url_result_ = std::move(result);
          run_loop.Quit();
        }),
        &tracker_);
    run_loop.Run();  // Will be exited in SaveURLAndQuit.
    return query_url_result_.success;
  }

  // Fills in saved_redirects_ with the redirect information for the given URL,
  // returning true on success. False means the URL was not found.
  void QueryRedirectsFrom(history::HistoryService* history, const GURL& url) {
    base::RunLoop run_loop;
    history_service_->QueryRedirectsFrom(
        url,
        base::BindOnce(&HistoryServiceTest::OnRedirectQueryComplete,
                       base::Unretained(this), run_loop.QuitClosure()),
        &tracker_);
    run_loop.Run();  // Will be exited in *QueryComplete.
  }

  // Callback for QueryRedirects.
  void OnRedirectQueryComplete(base::OnceClosure done,
                               history::RedirectList redirects) {
    saved_redirects_ = std::move(redirects);
    std::move(done).Run();
  }

  void QueryMostVisitedURLs() {
    const int kResultCount = 20;
    const int kDaysBack = 90;

    base::RunLoop run_loop;
    history_service_->QueryMostVisitedURLs(
        kResultCount, kDaysBack,
        base::BindLambdaForTesting([&](MostVisitedURLList urls) {
          most_visited_urls_ = urls;
          run_loop.Quit();
        }),
        &tracker_);
    run_loop.Run();  // Will be exited in *QueryComplete.
  }

  base::ScopedTempDir temp_dir_;

  base::test::SingleThreadTaskEnvironment task_environment_;

  MostVisitedURLList most_visited_urls_;

  // When non-NULL, this will be deleted on tear down and we will block until
  // the backend thread has completed. This allows tests for the history
  // service to use this feature, but other tests to ignore this.
  std::unique_ptr<history::HistoryService> history_service_;

  // names of the database files
  base::FilePath history_dir_;

  // Set by the redirect callback when we get data. You should be sure to
  // clear this before issuing a redirect request.
  history::RedirectList saved_redirects_;

  // For history requests.
  base::CancelableTaskTracker tracker_;

  // For saving URL info after a call to QueryURL
  history::QueryURLResult query_url_result_;
};

TEST_F(HistoryServiceTest, AddPage) {
  ASSERT_TRUE(history_service_.get());
  // Add the page once from a child frame.
  const GURL test_url("http://www.google.com/");
  history_service_->AddPage(
      test_url, base::Time::Now(), nullptr, 0, GURL(), history::RedirectList(),
      ui::PAGE_TRANSITION_MANUAL_SUBFRAME, history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(0, query_url_result_.row.typed_count());
  EXPECT_TRUE(
      query_url_result_.row.hidden());  // Hidden because of child frame.

  // Add the page once from the main frame (should unhide it).
  history_service_->AddPage(test_url, base::Time::Now(), nullptr, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  EXPECT_EQ(2, query_url_result_.row.visit_count());  // Added twice.
  EXPECT_EQ(0, query_url_result_.row.typed_count());  // Never typed.
  EXPECT_FALSE(
      query_url_result_.row.hidden());  // Because loaded in main frame.
}

TEST_F(HistoryServiceTest, AddRedirect) {
  ASSERT_TRUE(history_service_.get());
  history::RedirectList first_redirects = {GURL("http://first.page.com/"),
                                           GURL("http://second.page.com/")};

  // Add the sequence of pages as a server with no referrer. Note that we need
  // to have a non-NULL page ID scope.
  history_service_->AddPage(
      first_redirects.back(), base::Time::Now(),
      reinterpret_cast<ContextID>(1), 0, GURL(), first_redirects,
      ui::PAGE_TRANSITION_LINK, history::SOURCE_BROWSED, true);

  // The first page should be added once with a link visit type (because we set
  // LINK when we added the original URL, and a referrer of nowhere (0).
  EXPECT_TRUE(QueryURL(history_service_.get(), first_redirects[0]));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  int64_t first_visit = query_url_result_.visits[0].visit_id;
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      query_url_result_.visits[0].transition,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CHAIN_START)));
  EXPECT_EQ(0, query_url_result_.visits[0].referring_visit);  // No referrer.

  // The second page should be a server redirect type with a referrer of the
  // first page.
  EXPECT_TRUE(QueryURL(history_service_.get(), first_redirects[1]));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  int64_t second_visit = query_url_result_.visits[0].visit_id;
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      query_url_result_.visits[0].transition,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_CHAIN_END)));
  EXPECT_EQ(first_visit, query_url_result_.visits[0].referring_visit);

  // Check that the redirect finding function successfully reports it.
  saved_redirects_.clear();
  QueryRedirectsFrom(history_service_.get(), first_redirects[0]);
  ASSERT_EQ(1U, saved_redirects_.size());
  EXPECT_EQ(first_redirects[1], saved_redirects_[0]);

  // Now add a client redirect from that second visit to a third, client
  // redirects are tracked by the RenderView prior to updating history,
  // so we pass in a CLIENT_REDIRECT qualifier to mock that behavior.
  history::RedirectList second_redirects = {first_redirects[1],
                                            GURL("http://last.page.com/")};
  history_service_->AddPage(second_redirects[1], base::Time::Now(),
                   reinterpret_cast<ContextID>(1), 1,
                   second_redirects[0], second_redirects,
                   ui::PageTransitionFromInt(
                       ui::PAGE_TRANSITION_LINK |
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT),
                   history::SOURCE_BROWSED, true);

  // The last page (source of the client redirect) should NOT have an
  // additional visit added, because it was a client redirect (normally it
  // would). We should only have 1 left over from the first sequence.
  EXPECT_TRUE(QueryURL(history_service_.get(), second_redirects[0]));
  EXPECT_EQ(1, query_url_result_.row.visit_count());

  // The final page should be set as a client redirect from the previous visit.
  EXPECT_TRUE(QueryURL(history_service_.get(), second_redirects[1]));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      query_url_result_.visits[0].transition,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_CLIENT_REDIRECT |
                                ui::PAGE_TRANSITION_CHAIN_END)));
  EXPECT_EQ(second_visit, query_url_result_.visits[0].referring_visit);
}

TEST_F(HistoryServiceTest, MakeIntranetURLsTyped) {
  ASSERT_TRUE(history_service_.get());

  // Add a non-typed visit to an intranet URL on an unvisited host.  This should
  // get promoted to a typed visit.
  const GURL test_url("http://intranet_host/path");
  history_service_->AddPage(test_url, base::Time::Now(), nullptr, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(1, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[0].transition, ui::PAGE_TRANSITION_TYPED));

  // Add more visits on the same host.  None of these should be promoted since
  // there is already a typed visit.

  // Different path.
  const GURL test_url2("http://intranet_host/different_path");
  history_service_->AddPage(test_url2, base::Time::Now(), nullptr, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url2));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(0, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[0].transition, ui::PAGE_TRANSITION_LINK));

  // No path.
  const GURL test_url3("http://intranet_host/");
  history_service_->AddPage(test_url3, base::Time::Now(), nullptr, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url3));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(0, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[0].transition, ui::PAGE_TRANSITION_LINK));

  // Different scheme.
  const GURL test_url4("https://intranet_host/");
  history_service_->AddPage(test_url4, base::Time::Now(), nullptr, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url4));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(0, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[0].transition, ui::PAGE_TRANSITION_LINK));

  // Different transition.
  const GURL test_url5("http://intranet_host/another_path");
  history_service_->AddPage(
      test_url5, base::Time::Now(), nullptr, 0, GURL(), history::RedirectList(),
      ui::PAGE_TRANSITION_AUTO_BOOKMARK, history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url5));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(0, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(
      ui::PageTransitionCoreTypeIs(query_url_result_.visits[0].transition,
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK));

  // Original URL.
  history_service_->AddPage(test_url, base::Time::Now(), nullptr, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  EXPECT_EQ(2, query_url_result_.row.visit_count());
  EXPECT_EQ(1, query_url_result_.row.typed_count());
  ASSERT_EQ(2U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[1].transition, ui::PAGE_TRANSITION_LINK));

  // A redirect chain with an intranet URL at the head should be promoted.
  history::RedirectList redirects1 = {GURL("http://intranet1/path"),
                                      GURL("http://second1.com/"),
                                      GURL("http://third1.com/")};
  history_service_->AddPage(redirects1.back(), base::Time::Now(), nullptr, 0,
                            GURL(), redirects1, ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), redirects1.front()));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(1, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[0].transition, ui::PAGE_TRANSITION_TYPED));

  // As should one with an intranet URL at the tail.
  history::RedirectList redirects2 = {GURL("http://first2.com/"),
                                      GURL("http://second2.com/"),
                                      GURL("http://intranet2/path")};
  history_service_->AddPage(redirects2.back(), base::Time::Now(), nullptr, 0,
                            GURL(), redirects2, ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), redirects2.back()));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(0, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[0].transition, ui::PAGE_TRANSITION_TYPED));

  // But not one with an intranet URL in the middle.
  history::RedirectList redirects3 = {GURL("http://first3.com/"),
                                      GURL("http://intranet3/path"),
                                      GURL("http://third3.com/")};
  history_service_->AddPage(redirects3.back(), base::Time::Now(), nullptr, 0,
                            GURL(), redirects3, ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), redirects3[1]));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(0, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[0].transition, ui::PAGE_TRANSITION_LINK));
}

TEST_F(HistoryServiceTest, Typed) {
  const ContextID context_id = reinterpret_cast<ContextID>(1);

  ASSERT_TRUE(history_service_.get());

  // Add the page once as typed.
  const GURL test_url("http://www.google.com/");
  history_service_->AddPage(
      test_url, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));

  // We should have the same typed & visit count.
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(1, query_url_result_.row.typed_count());

  // Add the page again not typed.
  history_service_->AddPage(
      test_url, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), ui::PAGE_TRANSITION_LINK,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));

  // The second time should not have updated the typed count.
  EXPECT_EQ(2, query_url_result_.row.visit_count());
  EXPECT_EQ(1, query_url_result_.row.typed_count());

  // Add the page again as a generated URL.
  history_service_->AddPage(
      test_url, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), ui::PAGE_TRANSITION_GENERATED,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));

  // This should have worked like a link click.
  EXPECT_EQ(3, query_url_result_.row.visit_count());
  EXPECT_EQ(1, query_url_result_.row.typed_count());

  // Add the page again as a reload.
  history_service_->AddPage(
      test_url, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), ui::PAGE_TRANSITION_RELOAD,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));

  // This should not have incremented any visit counts.
  EXPECT_EQ(3, query_url_result_.row.visit_count());
  EXPECT_EQ(1, query_url_result_.row.typed_count());
}

TEST_F(HistoryServiceTest, SetTitle) {
  ASSERT_TRUE(history_service_.get());

  // Add a URL.
  const GURL existing_url("http://www.google.com/");
  history_service_->AddPage(
      existing_url, base::Time::Now(), history::SOURCE_BROWSED);

  // Set some title.
  const base::string16 existing_title = base::UTF8ToUTF16("Google");
  history_service_->SetPageTitle(existing_url, existing_title);

  // Make sure the title got set.
  EXPECT_TRUE(QueryURL(history_service_.get(), existing_url));
  EXPECT_EQ(existing_title, query_url_result_.row.title());

  // set a title on a nonexistent page
  const GURL nonexistent_url("http://news.google.com/");
  const base::string16 nonexistent_title = base::UTF8ToUTF16("Google News");
  history_service_->SetPageTitle(nonexistent_url, nonexistent_title);

  // Make sure nothing got written.
  EXPECT_FALSE(QueryURL(history_service_.get(), nonexistent_url));
  EXPECT_EQ(base::string16(), query_url_result_.row.title());

  // TODO(brettw) this should also test redirects, which get the title of the
  // destination page.
}

TEST_F(HistoryServiceTest, MostVisitedURLs) {
  ASSERT_TRUE(history_service_.get());

  const GURL url0("http://www.google.com/url0/");
  const GURL url1("http://www.google.com/url1/");
  const GURL url2("http://www.google.com/url2/");
  const GURL url3("http://www.google.com/url3/");
  const GURL url4("http://www.google.com/url4/");

  const ContextID context_id = reinterpret_cast<ContextID>(1);

  // Add two pages.
  history_service_->AddPage(
      url0, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  history_service_->AddPage(
      url1, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);

  QueryMostVisitedURLs();

  EXPECT_EQ(2U, most_visited_urls_.size());
  EXPECT_EQ(url0, most_visited_urls_[0].url);
  EXPECT_EQ(url1, most_visited_urls_[1].url);

  // Add another page.
  history_service_->AddPage(
      url2, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);

  QueryMostVisitedURLs();

  EXPECT_EQ(3U, most_visited_urls_.size());
  EXPECT_EQ(url0, most_visited_urls_[0].url);
  EXPECT_EQ(url1, most_visited_urls_[1].url);
  EXPECT_EQ(url2, most_visited_urls_[2].url);

  // Revisit url2, making it the top URL.
  history_service_->AddPage(
      url2, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);

  QueryMostVisitedURLs();

  EXPECT_EQ(3U, most_visited_urls_.size());
  EXPECT_EQ(url2, most_visited_urls_[0].url);
  EXPECT_EQ(url0, most_visited_urls_[1].url);
  EXPECT_EQ(url1, most_visited_urls_[2].url);

  // Revisit url1, making it the top URL.
  history_service_->AddPage(
      url1, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);

  QueryMostVisitedURLs();

  EXPECT_EQ(3U, most_visited_urls_.size());
  EXPECT_EQ(url1, most_visited_urls_[0].url);
  EXPECT_EQ(url2, most_visited_urls_[1].url);
  EXPECT_EQ(url0, most_visited_urls_[2].url);

  // Visit url4 using redirects.
  history::RedirectList redirects = {url3, url4};
  history_service_->AddPage(
      url4, base::Time::Now(), context_id, 0, GURL(),
      redirects, ui::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);

  QueryMostVisitedURLs();

  EXPECT_EQ(4U, most_visited_urls_.size());
  EXPECT_EQ(url1, most_visited_urls_[0].url);
  EXPECT_EQ(url2, most_visited_urls_[1].url);
  EXPECT_EQ(url0, most_visited_urls_[2].url);
  EXPECT_EQ(url3, most_visited_urls_[3].url);
}

namespace {

// A HistoryDBTask implementation. Each time RunOnDBThread is invoked
// invoke_count is increment. When invoked kWantInvokeCount times, true is
// returned from RunOnDBThread which should stop RunOnDBThread from being
// invoked again. When DoneRunOnMainThread is invoked, done_invoked is set to
// true.
class HistoryDBTaskImpl : public HistoryDBTask {
 public:
  static const int kWantInvokeCount;

  HistoryDBTaskImpl(int* invoke_count, bool* done_invoked)
      : invoke_count_(invoke_count), done_invoked_(done_invoked) {}

  bool RunOnDBThread(HistoryBackend* backend, HistoryDatabase* db) override {
    return (++*invoke_count_ == kWantInvokeCount);
  }

  void DoneRunOnMainThread() override {
    *done_invoked_ = true;
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  int* invoke_count_;
  bool* done_invoked_;

 private:
  ~HistoryDBTaskImpl() override {}

  DISALLOW_COPY_AND_ASSIGN(HistoryDBTaskImpl);
};

// static
const int HistoryDBTaskImpl::kWantInvokeCount = 2;

}  // namespace

TEST_F(HistoryServiceTest, HistoryDBTask) {
  ASSERT_TRUE(history_service_.get());
  base::CancelableTaskTracker task_tracker;
  int invoke_count = 0;
  bool done_invoked = false;
  history_service_->ScheduleDBTask(
      FROM_HERE,
      std::unique_ptr<history::HistoryDBTask>(
          new HistoryDBTaskImpl(&invoke_count, &done_invoked)),
      &task_tracker);
  // Run the message loop. When HistoryDBTaskImpl::DoneRunOnMainThread runs,
  // it will stop the message loop. If the test hangs here, it means
  // DoneRunOnMainThread isn't being invoked correctly.
  base::RunLoop().Run();
  CleanupHistoryService();
  // WARNING: history has now been deleted.
  history_service_.reset();
  ASSERT_EQ(HistoryDBTaskImpl::kWantInvokeCount, invoke_count);
  ASSERT_TRUE(done_invoked);
}

TEST_F(HistoryServiceTest, HistoryDBTaskCanceled) {
  ASSERT_TRUE(history_service_.get());
  base::CancelableTaskTracker task_tracker;
  int invoke_count = 0;
  bool done_invoked = false;
  history_service_->ScheduleDBTask(
      FROM_HERE,
      std::unique_ptr<history::HistoryDBTask>(
          new HistoryDBTaskImpl(&invoke_count, &done_invoked)),
      &task_tracker);
  task_tracker.TryCancelAll();
  CleanupHistoryService();
  // WARNING: history has now been deleted.
  history_service_.reset();
  ASSERT_FALSE(done_invoked);
}

// Helper to add a page with specified days back in the past.
void AddPageInThePast(HistoryService* history,
                      const std::string& url_spec,
                      int days_back) {
  const GURL url(url_spec);
  base::Time time_in_the_past =
      base::Time::Now() - base::TimeDelta::FromDays(days_back);
  history->AddPage(url, time_in_the_past, nullptr, 0, GURL(),
                   history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                   history::SOURCE_BROWSED, false);
}

// Helper to contain a callback and run loop logic.
int GetMonthlyHostCountHelper(HistoryService* history,
                              base::CancelableTaskTracker* tracker) {
  base::RunLoop run_loop;
  int count = 0;
  history->CountUniqueHostsVisitedLastMonth(
      base::BindLambdaForTesting([&](HistoryCountResult result) {
        count = result.count;
        run_loop.Quit();
      }),
      tracker);
  run_loop.Run();
  return count;
}

// Counts hosts visited in the last month.
TEST_F(HistoryServiceTest, CountMonthlyVisitedHosts) {
  base::HistogramTester histogram_tester;
  HistoryService* history = history_service_.get();
  ASSERT_TRUE(history);

  AddPageInThePast(history, "http://www.google.com/", 0);
  EXPECT_EQ(1, GetMonthlyHostCountHelper(history, &tracker_));

  AddPageInThePast(history, "http://www.google.com/foo", 1);
  AddPageInThePast(history, "https://www.google.com/foo", 5);
  AddPageInThePast(history, "https://www.gmail.com/foo", 10);
  // Expect 2 because only host part of URL counts.
  EXPECT_EQ(2, GetMonthlyHostCountHelper(history, &tracker_));

  AddPageInThePast(history, "https://www.gmail.com/foo", 31);
  // Count should not change since URL added is older than a month.
  EXPECT_EQ(2, GetMonthlyHostCountHelper(history, &tracker_));

  AddPageInThePast(history, "https://www.yahoo.com/foo", 29);
  EXPECT_EQ(3, GetMonthlyHostCountHelper(history, &tracker_));

  // The time required to compute host count is reported on each computation.
  histogram_tester.ExpectTotalCount("History.DatabaseMonthlyHostCountTime", 4);
}
}  // namespace history
