// Copyright 2012 The Chromium Authors
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
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_client.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/visit_delegate.h"
#include "components/history/core/test/database_test_utils.h"
#include "components/history/core/test/test_history_database.h"
#include "components/visitedlink/core/visited_link.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

class TestVisitDelegate : public VisitDelegate {
 public:
  TestVisitDelegate() = default;
  ~TestVisitDelegate() override = default;

  // Implementation of VisitDelegate.
  bool Init(HistoryService* history_service) override { return true; }
  void AddURL(const GURL& url) override {}
  void AddURLs(const std::vector<GURL>& urls) override {}
  void DeleteURLs(const std::vector<GURL>& urls) override {}
  void DeleteAllURLs() override {}
  void AddVisitedLink(const VisitedLink& link) override;
  void DeleteVisitedLinks(const std::vector<VisitedLink>& links) override {}
  void DeleteAllVisitedLinks() override {}
  std::optional<uint64_t> GetOrAddOriginSalt(
      const url::Origin& origin) override {
    return std::nullopt;
  }

  bool visit_delegate_was_called() { return visit_delegate_was_called_; }

  base::WeakPtr<TestVisitDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  std::vector<VisitedLink> get_added_links() { return links_; }

  void set_add_complete_task(base::OnceClosure task) {
    DCHECK(add_complete_task_.is_null());
    add_complete_task_ = std::move(task);
  }

 private:
  // Set to true once `AddVisitedLink` is called.
  bool visit_delegate_was_called_ = false;
  // A mock of our partitioned hashtable.
  std::vector<VisitedLink> links_;
  // Task to be called once `AddVisitedLink` is called.
  base::OnceClosure add_complete_task_;
  base::WeakPtrFactory<TestVisitDelegate> weak_factory_{this};
};

void TestVisitDelegate::AddVisitedLink(const VisitedLink& link) {
  visit_delegate_was_called_ = true;
  links_.push_back(link);
  // Notify the unit test that the AddVisitedLink task has completed.
  if (!add_complete_task_.is_null()) {
    std::move(add_complete_task_).Run();
  }
}

class HistoryServiceTest : public testing::Test {
 public:
  HistoryServiceTest() = default;
  ~HistoryServiceTest() override = default;

 protected:
  friend class BackendDelegate;

  // testing::Test
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    history_dir_ = temp_dir_.GetPath().AppendASCII("HistoryServiceTest");
    ASSERT_TRUE(base::CreateDirectory(history_dir_));
    history_service_ = std::make_unique<history::HistoryService>();
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
    history_service_->ClearCachedDataForContextID(0);
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
  bool QueryURL(const GURL& url) {
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
  void QueryRedirectsFrom(const GURL& url) {
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

    base::RunLoop run_loop;
    history_service_->QueryMostVisitedURLs(
        kResultCount, base::BindLambdaForTesting([&](MostVisitedURLList urls) {
          most_visited_urls_ = urls;
          run_loop.Quit();
        }),
        &tracker_);
    run_loop.Run();  // Will be exited in callback on query complete.
  }

  void QueryMostRepeatedQueriesForKeyword(KeywordID keyword_id,
                                          size_t result_count) {
    base::RunLoop run_loop;
    history_service_->QueryMostRepeatedQueriesForKeyword(
        keyword_id, result_count,
        base::BindLambdaForTesting([&](KeywordSearchTermVisitList queries) {
          most_repeated_queries_ = std::move(queries);
          run_loop.Quit();
        }),
        &tracker_);
    run_loop.Run();  // Will be exited in callback on query complete.
  }

  base::ScopedTempDir temp_dir_;

  base::test::TaskEnvironment task_environment_;

  MostVisitedURLList most_visited_urls_;

  KeywordSearchTermVisitList most_repeated_queries_;

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

// Simple test that removes a bookmark. This test exercises the code paths in
// History that block till BookmarkModel is loaded.
TEST_F(HistoryServiceTest, RemoveNotification) {
  ASSERT_TRUE(history_service_.get());

  // Add a URL.
  GURL url("http://www.google.com");

  history_service_->AddPage(url, base::Time::Now(), 0, 1, GURL(),
                            RedirectList(), ui::PAGE_TRANSITION_TYPED,
                            SOURCE_BROWSED, false);

  // This won't actually delete the URL, rather it'll empty out the visits.
  // This triggers blocking on the BookmarkModel.
  history_service_->DeleteURLs({url});
}

TEST_F(HistoryServiceTest, AddPage) {
  ASSERT_TRUE(history_service_.get());
  // Add the page once from a child frame.
  const GURL test_url("http://www.google.com/");
  history_service_->AddPage(
      test_url, base::Time::Now(), 0, 0, GURL(), history::RedirectList(),
      ui::PAGE_TRANSITION_MANUAL_SUBFRAME, history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(test_url));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(0, query_url_result_.row.typed_count());
  EXPECT_TRUE(
      query_url_result_.row.hidden());  // Hidden because of child frame.

  // Add the page once from the main frame (should unhide it).
  history_service_->AddPage(test_url, base::Time::Now(), 0, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(test_url));
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
  history_service_->AddPage(first_redirects.back(), base::Time::Now(), 1, 0,
                            GURL(), first_redirects, ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, true);

  // The first page should be added once with a link visit type (because we set
  // LINK when we added the original URL, and a referrer of nowhere (0).
  EXPECT_TRUE(QueryURL(first_redirects[0]));
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
  EXPECT_TRUE(QueryURL(first_redirects[1]));
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
  QueryRedirectsFrom(first_redirects[0]);
  ASSERT_EQ(1U, saved_redirects_.size());
  EXPECT_EQ(first_redirects[1], saved_redirects_[0]);

  // Now add a client redirect from that second visit to a third, client
  // redirects are tracked by the RenderView prior to updating history,
  // so we pass in a CLIENT_REDIRECT qualifier to mock that behavior.
  history::RedirectList second_redirects = {first_redirects[1],
                                            GURL("http://last.page.com/")};
  history_service_->AddPage(
      second_redirects[1], base::Time::Now(), 1, 1, second_redirects[0],
      second_redirects,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      history::SOURCE_BROWSED, true);

  // The last page (source of the client redirect) should NOT have an
  // additional visit added, because it was a client redirect (normally it
  // would). We should only have 1 left over from the first sequence.
  EXPECT_TRUE(QueryURL(second_redirects[0]));
  EXPECT_EQ(1, query_url_result_.row.visit_count());

  // The final page should be set as a client redirect from the previous visit.
  EXPECT_TRUE(QueryURL(second_redirects[1]));
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
  history_service_->AddPage(test_url, base::Time::Now(), 0, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(test_url));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(1, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[0].transition, ui::PAGE_TRANSITION_TYPED));

  // Add more visits on the same host.  None of these should be promoted since
  // there is already a typed visit.

  // Different path.
  const GURL test_url2("http://intranet_host/different_path");
  history_service_->AddPage(test_url2, base::Time::Now(), 0, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(test_url2));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(0, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[0].transition, ui::PAGE_TRANSITION_LINK));

  // No path.
  const GURL test_url3("http://intranet_host/");
  history_service_->AddPage(test_url3, base::Time::Now(), 0, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(test_url3));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(0, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[0].transition, ui::PAGE_TRANSITION_LINK));

  // Different scheme.
  const GURL test_url4("https://intranet_host/");
  history_service_->AddPage(test_url4, base::Time::Now(), 0, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(test_url4));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(0, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[0].transition, ui::PAGE_TRANSITION_LINK));

  // Different transition.
  const GURL test_url5("http://intranet_host/another_path");
  history_service_->AddPage(
      test_url5, base::Time::Now(), 0, 0, GURL(), history::RedirectList(),
      ui::PAGE_TRANSITION_AUTO_BOOKMARK, history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(test_url5));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(0, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(
      ui::PageTransitionCoreTypeIs(query_url_result_.visits[0].transition,
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK));

  // Original URL.
  history_service_->AddPage(test_url, base::Time::Now(), 0, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(test_url));
  EXPECT_EQ(2, query_url_result_.row.visit_count());
  EXPECT_EQ(1, query_url_result_.row.typed_count());
  ASSERT_EQ(2U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[1].transition, ui::PAGE_TRANSITION_LINK));

  // A redirect chain with an intranet URL at the head should be promoted.
  history::RedirectList redirects1 = {GURL("http://intranet1/path"),
                                      GURL("http://second1.com/"),
                                      GURL("http://third1.com/")};
  history_service_->AddPage(redirects1.back(), base::Time::Now(), 0, 0, GURL(),
                            redirects1, ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(redirects1.front()));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(1, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[0].transition, ui::PAGE_TRANSITION_TYPED));

  // As should one with an intranet URL at the tail.
  history::RedirectList redirects2 = {GURL("http://first2.com/"),
                                      GURL("http://second2.com/"),
                                      GURL("http://intranet2/path")};
  history_service_->AddPage(redirects2.back(), base::Time::Now(), 0, 0, GURL(),
                            redirects2, ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(redirects2.back()));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(0, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[0].transition, ui::PAGE_TRANSITION_TYPED));

  // But not one with an intranet URL in the middle.
  history::RedirectList redirects3 = {GURL("http://first3.com/"),
                                      GURL("http://intranet3/path"),
                                      GURL("http://third3.com/")};
  history_service_->AddPage(redirects3.back(), base::Time::Now(), 0, 0, GURL(),
                            redirects3, ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(redirects3[1]));
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(0, query_url_result_.row.typed_count());
  ASSERT_EQ(1U, query_url_result_.visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      query_url_result_.visits[0].transition, ui::PAGE_TRANSITION_LINK));
}

TEST_F(HistoryServiceTest, Typed) {
  const ContextID context_id = 1;

  ASSERT_TRUE(history_service_.get());

  // Add the page once as typed.
  const GURL test_url("http://www.google.com/");
  history_service_->AddPage(test_url, base::Time::Now(), context_id, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(test_url));

  // We should have the same typed & visit count.
  EXPECT_EQ(1, query_url_result_.row.visit_count());
  EXPECT_EQ(1, query_url_result_.row.typed_count());

  // Add the page again not typed.
  history_service_->AddPage(test_url, base::Time::Now(), context_id, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(test_url));

  // The second time should not have updated the typed count.
  EXPECT_EQ(2, query_url_result_.row.visit_count());
  EXPECT_EQ(1, query_url_result_.row.typed_count());

  // Add the page again as a generated URL.
  history_service_->AddPage(test_url, base::Time::Now(), context_id, 0, GURL(),
                            history::RedirectList(),
                            ui::PAGE_TRANSITION_GENERATED,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(test_url));

  // This should have worked like a link click.
  EXPECT_EQ(3, query_url_result_.row.visit_count());
  EXPECT_EQ(1, query_url_result_.row.typed_count());

  // Add the page again as a reload.
  history_service_->AddPage(test_url, base::Time::Now(), context_id, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_RELOAD,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(test_url));

  // This should not have incremented any visit counts.
  EXPECT_EQ(3, query_url_result_.row.visit_count());
  EXPECT_EQ(1, query_url_result_.row.typed_count());
}

TEST_F(HistoryServiceTest, SetTitle) {
  ASSERT_TRUE(history_service_.get());

  // Add a URL.
  const GURL existing_url("http://www.google.com/");
  history_service_->AddPage(existing_url, base::Time::Now(),
                            history::SOURCE_BROWSED);

  // Set some title.
  const std::u16string existing_title = u"Google";
  history_service_->SetPageTitle(existing_url, existing_title);

  // Make sure the title got set.
  EXPECT_TRUE(QueryURL(existing_url));
  EXPECT_EQ(existing_title, query_url_result_.row.title());

  // set a title on a nonexistent page
  const GURL nonexistent_url("http://news.google.com/");
  const std::u16string nonexistent_title = u"Google News";
  history_service_->SetPageTitle(nonexistent_url, nonexistent_title);

  // Make sure nothing got written.
  EXPECT_FALSE(QueryURL(nonexistent_url));
  EXPECT_EQ(std::u16string(), query_url_result_.row.title());

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

  const ContextID context_id = 1;

  // Add two pages.
  history_service_->AddPage(url0, base::Time::Now(), context_id, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
                            history::SOURCE_BROWSED, false);
  history_service_->AddPage(url1, base::Time::Now(), context_id, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
                            history::SOURCE_BROWSED, false);

  QueryMostVisitedURLs();

  EXPECT_EQ(2U, most_visited_urls_.size());
  EXPECT_EQ(url0, most_visited_urls_[0].url);
  EXPECT_EQ(url1, most_visited_urls_[1].url);

  // Add another page.
  history_service_->AddPage(url2, base::Time::Now(), context_id, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
                            history::SOURCE_BROWSED, false);

  QueryMostVisitedURLs();

  EXPECT_EQ(3U, most_visited_urls_.size());
  EXPECT_EQ(url0, most_visited_urls_[0].url);
  EXPECT_EQ(url1, most_visited_urls_[1].url);
  EXPECT_EQ(url2, most_visited_urls_[2].url);

  // Revisit url2, making it the top URL.
  history_service_->AddPage(url2, base::Time::Now(), context_id, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
                            history::SOURCE_BROWSED, false);

  QueryMostVisitedURLs();

  EXPECT_EQ(3U, most_visited_urls_.size());
  EXPECT_EQ(url2, most_visited_urls_[0].url);
  EXPECT_EQ(url0, most_visited_urls_[1].url);
  EXPECT_EQ(url1, most_visited_urls_[2].url);

  // Revisit url1, making it the top URL.
  history_service_->AddPage(url1, base::Time::Now(), context_id, 0, GURL(),
                            history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
                            history::SOURCE_BROWSED, false);

  QueryMostVisitedURLs();

  EXPECT_EQ(3U, most_visited_urls_.size());
  EXPECT_EQ(url1, most_visited_urls_[0].url);
  EXPECT_EQ(url2, most_visited_urls_[1].url);
  EXPECT_EQ(url0, most_visited_urls_[2].url);

  // Visit url4 using redirects.
  history::RedirectList redirects = {url3, url4};
  history_service_->AddPage(url4, base::Time::Now(), context_id, 0, GURL(),
                            redirects, ui::PAGE_TRANSITION_TYPED,
                            history::SOURCE_BROWSED, false);

  QueryMostVisitedURLs();

  EXPECT_EQ(4U, most_visited_urls_.size());
  EXPECT_EQ(url1, most_visited_urls_[0].url);
  EXPECT_EQ(url2, most_visited_urls_[1].url);
  EXPECT_EQ(url0, most_visited_urls_[2].url);
  EXPECT_EQ(url3, most_visited_urls_[3].url);
}

TEST_F(HistoryServiceTest, QueryMostRepeatedQueriesForKeyword) {
  ASSERT_TRUE(history_service_.get());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      history::kOrganicRepeatableQueries,
      {{history::kRepeatableQueriesMaxAgeDays.name, "4"},
       {history::kRepeatableQueriesMinVisitCount.name, "1"},
       {history::kRepeatableQueriesIgnoreDuplicateVisits.name, "true"}});

  const KeywordID first_keyword_id = 1;
  const KeywordID second_keyword_id = 2;

  struct PageData {
    const GURL url;
    const std::u16string term;
    base::Time time;
    const KeywordID keyword_id;
  };

  PageData page_1 = {GURL("http://www.search.com/?q=First"), u"First",
                     base::Time::Now() - base::Days(4), first_keyword_id};
  PageData page_2 = {GURL("http://www.search.com/?q=Second"), u"Second",
                     base::Time::Now() - base::Days(3), first_keyword_id};
  PageData page_3 = {GURL("http://www.search.com/?q=Second&foo=bar"), u"Second",
                     base::Time::Now() - base::Days(3), first_keyword_id};
  PageData page_4 = {GURL("http://www.search.com/?q=Fourth"), u"Fourth",
                     base::Time::Now() - base::Days(2), first_keyword_id};
  PageData page_5 = {GURL("http://www.search.com/?q=Fifth"), u"Fifth",
                     base::Time::Now() - base::Days(1), second_keyword_id};

  // Add first page from them first keyword.
  history_service_->AddPage(page_1.url, page_1.time, history::SOURCE_BROWSED);
  history_service_->SetKeywordSearchTermsForURL(page_1.url, page_1.keyword_id,
                                                page_1.term);

  // Add second page from the first keyword.
  history_service_->AddPage(page_2.url, page_2.time, history::SOURCE_BROWSED);
  history_service_->SetKeywordSearchTermsForURL(page_2.url, page_2.keyword_id,
                                                page_2.term);

  {
    base::HistogramTester histogram_tester;
    QueryMostRepeatedQueriesForKeyword(first_keyword_id, 1);

    ASSERT_EQ(1U, most_repeated_queries_.size());
    EXPECT_EQ(u"second", most_repeated_queries_[0]->normalized_term);
    EXPECT_EQ(1, most_repeated_queries_[0]->visit_count);

    histogram_tester.ExpectTotalCount("History.QueryMostRepeatedQueriesTimeV2",
                                      1);
  }

  // Add fourth page from the first keyword.
  history_service_->AddPage(page_4.url, page_4.time, history::SOURCE_BROWSED);
  history_service_->SetKeywordSearchTermsForURL(page_4.url, page_4.keyword_id,
                                                page_4.term);

  {
    base::HistogramTester histogram_tester;
    QueryMostRepeatedQueriesForKeyword(first_keyword_id, 1);

    ASSERT_EQ(1U, most_repeated_queries_.size());
    EXPECT_EQ(u"fourth", most_repeated_queries_[0]->normalized_term);
    EXPECT_EQ(1, most_repeated_queries_[0]->visit_count);

    histogram_tester.ExpectTotalCount("History.QueryMostRepeatedQueriesTimeV2",
                                      1);
  }

  // Revisit second page from the first keyword, making it the top page.
  history_service_->AddPage(page_2.url, page_2.time, history::SOURCE_BROWSED);
  history_service_->SetKeywordSearchTermsForURL(page_2.url, page_2.keyword_id,
                                                page_2.term);

  {
    base::HistogramTester histogram_tester;
    QueryMostRepeatedQueriesForKeyword(first_keyword_id, 1);

    ASSERT_EQ(1U, most_repeated_queries_.size());
    EXPECT_EQ(u"second", most_repeated_queries_[0]->normalized_term);
    EXPECT_EQ(2, most_repeated_queries_[0]->visit_count);

    histogram_tester.ExpectTotalCount("History.QueryMostRepeatedQueriesTimeV2",
                                      1);
  }

  // Add third page from the first keyword. This is considered a duplicative
  // vist and will be ignored. This does not change the top page.
  history_service_->AddPage(page_3.url, page_3.time, history::SOURCE_BROWSED);
  history_service_->SetKeywordSearchTermsForURL(page_3.url, page_3.keyword_id,
                                                page_3.term);

  {
    base::HistogramTester histogram_tester;
    QueryMostRepeatedQueriesForKeyword(first_keyword_id, 1);

    ASSERT_EQ(1U, most_repeated_queries_.size());
    EXPECT_EQ(u"second", most_repeated_queries_[0]->normalized_term);
    EXPECT_EQ(2, most_repeated_queries_[0]->visit_count);

    histogram_tester.ExpectTotalCount("History.QueryMostRepeatedQueriesTimeV2",
                                      1);
  }

  // Add fifth page from the second keyword. This does not change the top page.
  history_service_->AddPage(page_5.url, page_5.time, history::SOURCE_BROWSED);
  history_service_->SetKeywordSearchTermsForURL(page_5.url, page_5.keyword_id,
                                                page_5.term);

  {
    base::HistogramTester histogram_tester;
    QueryMostRepeatedQueriesForKeyword(first_keyword_id, 1);

    ASSERT_EQ(1U, most_repeated_queries_.size());
    EXPECT_EQ(u"second", most_repeated_queries_[0]->normalized_term);
    EXPECT_EQ(2, most_repeated_queries_[0]->visit_count);

    histogram_tester.ExpectTotalCount("History.QueryMostRepeatedQueriesTimeV2",
                                      1);
  }
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

  HistoryDBTaskImpl(int* invoke_count,
                    bool* done_invoked,
                    base::OnceClosure quit_closure)
      : invoke_count_(invoke_count),
        done_invoked_(done_invoked),
        quit_closure_(std::move(quit_closure)) {}

  HistoryDBTaskImpl(const HistoryDBTaskImpl&) = delete;
  HistoryDBTaskImpl& operator=(const HistoryDBTaskImpl&) = delete;

  bool RunOnDBThread(HistoryBackend* backend, HistoryDatabase* db) override {
    return (++*invoke_count_ == kWantInvokeCount);
  }

  void DoneRunOnMainThread() override {
    *done_invoked_ = true;
    std::move(quit_closure_).Run();
  }

  raw_ptr<int> invoke_count_;
  raw_ptr<bool> done_invoked_;

 private:
  ~HistoryDBTaskImpl() override = default;
  base::OnceClosure quit_closure_;
};

// static
const int HistoryDBTaskImpl::kWantInvokeCount = 2;

}  // namespace

TEST_F(HistoryServiceTest, HistoryDBTask) {
  ASSERT_TRUE(history_service_.get());
  base::CancelableTaskTracker task_tracker;
  int invoke_count = 0;
  bool done_invoked = false;
  base::RunLoop loop;
  history_service_->ScheduleDBTask(
      FROM_HERE,
      std::unique_ptr<history::HistoryDBTask>(new HistoryDBTaskImpl(
          &invoke_count, &done_invoked, loop.QuitWhenIdleClosure())),
      &task_tracker);
  // Run the message loop. When HistoryDBTaskImpl::DoneRunOnMainThread runs,
  // it will stop the message loop. If the test hangs here, it means
  // DoneRunOnMainThread isn't being invoked correctly.
  loop.Run();
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
      std::unique_ptr<history::HistoryDBTask>(new HistoryDBTaskImpl(
          &invoke_count, &done_invoked, base::DoNothing())),
      &task_tracker);
  task_tracker.TryCancelAll();
  CleanupHistoryService();
  // WARNING: history has now been deleted.
  history_service_.reset();
  ASSERT_FALSE(done_invoked);
}

// Helper to add a page at specified point of time.
void AddPageAtTime(HistoryService* history,
                   const std::string& url_spec,
                   base::Time time_in_the_past) {
  const GURL url(url_spec);
  history->AddPage(url, time_in_the_past, 0, 0, GURL(), history::RedirectList(),
                   ui::PAGE_TRANSITION_LINK, history::SOURCE_BROWSED, false);
}

void AddPageInThePast(HistoryService* history,
                      const std::string& url_spec,
                      int days_back) {
  base::Time time_in_the_past = base::Time::Now() - base::Days(days_back);
  AddPageAtTime(history, url_spec, time_in_the_past);
}

// Helper to add a synced page at a specified day in the past.
void AddSyncedPageInThePast(HistoryService* history,
                            const std::string& url_spec,
                            int days_back) {
  base::Time time_in_the_past = base::Time::Now() - base::Days(days_back);
  history->AddPage(GURL(url_spec), time_in_the_past, 0, 0, GURL(),
                   history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                   history::SOURCE_SYNCED, false);
}

// Helper to add a page with specified days back in the past.
base::Time GetTimeInThePast(base::Time base_time,
                            int days_back,
                            int hours_since_midnight,
                            int minutes = 0,
                            int seconds = 0) {
  base::Time past_midnight = MidnightNDaysLater(base_time, -days_back);

  return past_midnight + base::Hours(hours_since_midnight) +
         base::Minutes(minutes) + base::Seconds(seconds);
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

std::pair<DomainDiversityResults, DomainDiversityResults>
GetDomainDiversityHelper(HistoryService* history,
                         base::Time begin_time,
                         base::Time end_time,
                         DomainMetricBitmaskType metric_type_bitmask,
                         base::CancelableTaskTracker* tracker) {
  base::RunLoop run_loop;
  base::TimeDelta dst_rounding_offset = base::Hours(4);

  // Compute the number of days to report metrics for.
  int number_of_days = 0;
  if (begin_time < end_time) {
    number_of_days = (end_time.LocalMidnight() - begin_time.LocalMidnight() +
                      dst_rounding_offset)
                         .InDaysFloored();
  }

  std::pair<DomainDiversityResults, DomainDiversityResults> results;
  history->GetDomainDiversity(
      end_time, number_of_days, metric_type_bitmask,
      base::BindLambdaForTesting([&](std::pair<DomainDiversityResults,
                                               DomainDiversityResults> result) {
        results = result;
        run_loop.Quit();
      }),
      tracker);
  run_loop.Run();
  return results;
}

// Test one domain visit metric. A negative value indicates that an invalid
// metric is expected.
void TestDomainMetric(const std::optional<DomainMetricCountType>& metric,
                      int expected) {
  if (expected >= 0) {
    ASSERT_TRUE(metric.has_value());
    EXPECT_EQ(expected, metric.value().count);
  } else {
    EXPECT_FALSE(metric.has_value());
  }
}

// Test a set of 1-day, 7-day and 28-day domain visit metrics.
void TestDomainMetricSet(const DomainMetricSet& metric_set,
                         int expected_one_day_metric,
                         int expected_seven_day_metric,
                         int expected_twenty_eight_day_metric) {
  TestDomainMetric(metric_set.one_day_metric, expected_one_day_metric);
  TestDomainMetric(metric_set.seven_day_metric, expected_seven_day_metric);
  TestDomainMetric(metric_set.twenty_eight_day_metric,
                   expected_twenty_eight_day_metric);
}

// Counts hosts visited in the last month.
TEST_F(HistoryServiceTest, CountMonthlyVisitedHosts) {
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
}

TEST_F(HistoryServiceTest, GetDomainDiversityShortBasetimeRange) {
  HistoryService* history = history_service_.get();
  ASSERT_TRUE(history);

  base::Time query_time = base::Time::Now();

  // Make sure `query_time` is at least some time past the midnight so that
  // some domain visits can be inserted between `query_time` and midnight
  // for testing.
  query_time =
      std::max(query_time.LocalMidnight() + base::Minutes(10), query_time);

  AddPageAtTime(history, "http://www.google.com/",
                GetTimeInThePast(query_time, /*days_back=*/2,
                                 /*hours_since_midnight=*/12));
  AddPageAtTime(history, "http://www.gmail.com/",
                GetTimeInThePast(query_time, 2, 13));
  AddPageAtTime(history, "http://www.gmail.com/foo",
                GetTimeInThePast(query_time, 2, 14));
  AddPageAtTime(history, "http://images.google.com/foo",
                GetTimeInThePast(query_time, 1, 7));

  // Domains visited on the query day will not be included in the result.
  AddPageAtTime(history, "http://www.youtube.com/", query_time.LocalMidnight());
  AddPageAtTime(history, "http://www.chromium.com/",
                query_time.LocalMidnight() + base::Minutes(5));
  AddPageAtTime(history, "http://www.youtube.com/", query_time);

  // IP addresses, empty strings, non-TLD's should not be counted
  // as domains.
  AddPageAtTime(history, "127.0.0.1", GetTimeInThePast(query_time, 1, 8));
  AddPageAtTime(history, "", GetTimeInThePast(query_time, 1, 13));
  AddPageAtTime(history, "http://localhost/",
                GetTimeInThePast(query_time, 1, 8));
  AddPageAtTime(history, "http://ak/", GetTimeInThePast(query_time, 1, 14));

  // Should return empty result if `begin_time` == `end_time`.
  auto [local_res, all_res] = GetDomainDiversityHelper(
      history, query_time, query_time,
      history::kEnableLast1DayMetric | history::kEnableLast7DayMetric |
          history::kEnableLast28DayMetric,
      &tracker_);
  EXPECT_EQ(0u, local_res.size());
  EXPECT_EQ(0u, all_res.size());

  // Metrics will be computed for each of the 4 continuous midnights.
  std::tie(local_res, all_res) = GetDomainDiversityHelper(
      history, GetTimeInThePast(query_time, 4, 0), query_time,
      history::kEnableLast1DayMetric | history::kEnableLast7DayMetric |
          history::kEnableLast28DayMetric,
      &tracker_);

  ASSERT_EQ(4u, local_res.size());
  ASSERT_EQ(4u, all_res.size());

  TestDomainMetricSet(local_res[0], 1, 2, 2);
  TestDomainMetricSet(local_res[1], 2, 2, 2);
  TestDomainMetricSet(local_res[2], 0, 0, 0);
  TestDomainMetricSet(local_res[3], 0, 0, 0);

  TestDomainMetricSet(all_res[0], 1, 2, 2);
  TestDomainMetricSet(all_res[1], 2, 2, 2);
  TestDomainMetricSet(all_res[2], 0, 0, 0);
  TestDomainMetricSet(all_res[3], 0, 0, 0);
}

TEST_F(HistoryServiceTest, GetDomainDiversityLongBasetimeRange) {
  HistoryService* history = history_service_.get();
  ASSERT_TRUE(history);

  base::Time query_time = base::Time::Now();

  AddPageAtTime(history, "http://www.google.com/",
                GetTimeInThePast(query_time, /*days_back=*/90,
                                 /*hours_since_midnight=*/6));
  AddPageAtTime(history, "http://maps.google.com/",
                GetTimeInThePast(query_time, 34, 6));
  AddPageAtTime(history, "http://www.google.com/",
                GetTimeInThePast(query_time, 31, 4));
  AddPageAtTime(history, "https://www.google.co.uk/",
                GetTimeInThePast(query_time, 14, 5));
  AddPageAtTime(history, "http://www.gmail.com/",
                GetTimeInThePast(query_time, 10, 13));
  AddPageAtTime(history, "http://www.chromium.org/foo",
                GetTimeInThePast(query_time, 7, 14));
  AddPageAtTime(history, "https://www.youtube.com/",
                GetTimeInThePast(query_time, 2, 12));
  AddPageAtTime(history, "https://www.youtube.com/foo",
                GetTimeInThePast(query_time, 2, 12));
  AddPageAtTime(history, "https://www.chromium.org/",
                GetTimeInThePast(query_time, 1, 13));
  AddPageAtTime(history, "https://www.google.com/",
                GetTimeInThePast(query_time, 1, 13));

  auto [local_res, all_res] = GetDomainDiversityHelper(
      history, GetTimeInThePast(query_time, 10, 12), query_time,
      history::kEnableLast1DayMetric | history::kEnableLast7DayMetric |
          history::kEnableLast28DayMetric,
      &tracker_);
  // Only up to seven days will be considered.
  ASSERT_EQ(7u, local_res.size());
  ASSERT_EQ(7u, all_res.size());

  TestDomainMetricSet(local_res[0], 2, 3, 5);
  TestDomainMetricSet(local_res[1], 1, 2, 4);
  TestDomainMetricSet(local_res[2], 0, 1, 3);
  TestDomainMetricSet(local_res[3], 0, 2, 4);
  TestDomainMetricSet(local_res[4], 0, 2, 4);
  TestDomainMetricSet(local_res[5], 0, 2, 4);
  TestDomainMetricSet(local_res[6], 1, 2, 4);

  TestDomainMetricSet(all_res[0], 2, 3, 5);
  TestDomainMetricSet(all_res[1], 1, 2, 4);
  TestDomainMetricSet(all_res[2], 0, 1, 3);
  TestDomainMetricSet(all_res[3], 0, 2, 4);
  TestDomainMetricSet(all_res[4], 0, 2, 4);
  TestDomainMetricSet(all_res[5], 0, 2, 4);
  TestDomainMetricSet(all_res[6], 1, 2, 4);
}

TEST_F(HistoryServiceTest, GetDomainDiversityBitmaskTest) {
  HistoryService* history = history_service_.get();
  ASSERT_TRUE(history);

  base::Time query_time = base::Time::Now();

  AddPageAtTime(history, "http://www.google.com/",
                GetTimeInThePast(query_time, /*days_back=*/28,
                                 /*hours_since_midnight=*/6));
  AddPageAtTime(history, "http://www.youtube.com/",
                GetTimeInThePast(query_time, 7, 6));
  AddPageAtTime(history, "http://www.chromium.com/",
                GetTimeInThePast(query_time, 1, 4));

  auto [local_res, all_res] = GetDomainDiversityHelper(
      history, GetTimeInThePast(query_time, 7, 12), query_time,
      history::kEnableLast1DayMetric | history::kEnableLast7DayMetric,
      &tracker_);
  ASSERT_EQ(7u, local_res.size());
  ASSERT_EQ(7u, all_res.size());

  TestDomainMetricSet(local_res[0], 1, 2, -1);
  TestDomainMetricSet(local_res[1], 0, 1, -1);
  TestDomainMetricSet(local_res[2], 0, 1, -1);
  TestDomainMetricSet(local_res[3], 0, 1, -1);
  TestDomainMetricSet(local_res[4], 0, 1, -1);
  TestDomainMetricSet(local_res[5], 0, 1, -1);
  TestDomainMetricSet(local_res[6], 1, 1, -1);

  TestDomainMetricSet(all_res[0], 1, 2, -1);
  TestDomainMetricSet(all_res[1], 0, 1, -1);
  TestDomainMetricSet(all_res[2], 0, 1, -1);
  TestDomainMetricSet(all_res[3], 0, 1, -1);
  TestDomainMetricSet(all_res[4], 0, 1, -1);
  TestDomainMetricSet(all_res[5], 0, 1, -1);
  TestDomainMetricSet(all_res[6], 1, 1, -1);

  std::tie(local_res, all_res) = GetDomainDiversityHelper(
      history, GetTimeInThePast(query_time, 6, 12), query_time,
      history::kEnableLast28DayMetric | history::kEnableLast7DayMetric,
      &tracker_);

  ASSERT_EQ(6u, local_res.size());
  ASSERT_EQ(6u, all_res.size());

  TestDomainMetricSet(local_res[0], -1, 2, 3);
  TestDomainMetricSet(local_res[1], -1, 1, 2);
  TestDomainMetricSet(local_res[2], -1, 1, 2);
  TestDomainMetricSet(local_res[3], -1, 1, 2);
  TestDomainMetricSet(local_res[4], -1, 1, 2);
  TestDomainMetricSet(local_res[5], -1, 1, 2);

  TestDomainMetricSet(all_res[0], -1, 2, 3);
  TestDomainMetricSet(all_res[1], -1, 1, 2);
  TestDomainMetricSet(all_res[2], -1, 1, 2);
  TestDomainMetricSet(all_res[3], -1, 1, 2);
  TestDomainMetricSet(all_res[4], -1, 1, 2);
  TestDomainMetricSet(all_res[5], -1, 1, 2);
}

// Gets unique local and synced domains visited and the last visited domain
// within a time range.
TEST_F(HistoryServiceTest, GetUniqueDomainsVisited) {
  base::Time base_time = base::Time::Now();
  HistoryService* history = history_service_.get();
  ASSERT_TRUE(history);

  // Add local visits to history database at specific days back.
  AddPageInThePast(history, "http://www.test1.com/", 1);
  AddPageInThePast(history, "http://www.test2.com/test", 2);
  AddPageInThePast(history, "http://www.test2.com/", 3);
  AddPageInThePast(history, "http://www.test3.com/", 4);

  // Add synced visits to history database at specific days back.
  AddSyncedPageInThePast(history, "http://www.test3.com/", 3);
  AddSyncedPageInThePast(history, "http://www.test4.com/", 5);

  {
    // DomainsVisitedResult should be empty when no domains in range.
    base::test::TestFuture<DomainsVisitedResult> future;

    history->GetUniqueDomainsVisited(
        /*begin_time=*/base_time - base::Days(10),
        /*end_time=*/base_time - base::Days(5), future.GetCallback(),
        &tracker_);

    DomainsVisitedResult result = future.Take();

    EXPECT_EQ(0u, result.locally_visited_domains.size());
    EXPECT_EQ(0u, result.all_visited_domains.size());
  }

  {
    // DomainsVisitedResult should include unique domains in range in
    // reverse-chronological order.
    base::test::TestFuture<DomainsVisitedResult> future;

    history->GetUniqueDomainsVisited(
        /*begin_time=*/base_time - base::Days(2), /*end_time=*/base_time,
        future.GetCallback(), &tracker_);

    std::vector<std::string> expectedLocalResult({"test1.com", "test2.com"});
    std::vector<std::string> expectedSyncedResult({"test1.com", "test2.com"});

    DomainsVisitedResult result = future.Take();

    EXPECT_EQ(expectedLocalResult, result.locally_visited_domains);
    EXPECT_EQ(expectedSyncedResult, result.all_visited_domains);
  }

  {
    // DomainsVisitedResult should not include duplicate domains in range.
    base::test::TestFuture<DomainsVisitedResult> future;

    history->GetUniqueDomainsVisited(
        /*begin_time=*/base_time - base::Days(4), /*end_time=*/base_time,
        future.GetCallback(), &tracker_);

    std::vector<std::string> expectedLocalResult(
        {"test1.com", "test2.com", "test3.com"});
    std::vector<std::string> expectedSyncedResult(
        {"test1.com", "test2.com", "test3.com"});

    DomainsVisitedResult result = future.Take();

    EXPECT_EQ(expectedLocalResult, result.locally_visited_domains);
    EXPECT_EQ(expectedSyncedResult, result.all_visited_domains);
  }

  {
    // local domains should not include synced visits in range.
    base::test::TestFuture<DomainsVisitedResult> future;

    history->GetUniqueDomainsVisited(
        /*begin_time=*/base_time - base::Days(5), /*end_time=*/base_time,
        future.GetCallback(), &tracker_);

    std::vector<std::string> expectedLocalResult(
        {"test1.com", "test2.com", "test3.com"});
    std::vector<std::string> expectedSyncedResult(
        {"test1.com", "test2.com", "test3.com", "test4.com"});

    DomainsVisitedResult result = future.Take();

    EXPECT_EQ(expectedLocalResult, result.locally_visited_domains);
    EXPECT_EQ(expectedSyncedResult, result.all_visited_domains);
  }
}

namespace {

class AddSyncedVisitTask : public HistoryDBTask {
 public:
  AddSyncedVisitTask(base::RunLoop* run_loop,
                     const GURL& url,
                     const VisitRow& visit)
      : run_loop_(run_loop), url_(url), visit_(visit) {}

  AddSyncedVisitTask(const AddSyncedVisitTask&) = delete;
  AddSyncedVisitTask& operator=(const AddSyncedVisitTask&) = delete;

  ~AddSyncedVisitTask() override = default;

  bool RunOnDBThread(HistoryBackend* backend, HistoryDatabase* db) override {
    VisitID visit_id = backend->AddSyncedVisit(
        url_, u"Title", /*hidden=*/false, visit_, std::nullopt, std::nullopt);
    EXPECT_NE(visit_id, kInvalidVisitID);
    LOG(ERROR) << "Added visit!";
    return true;
  }

  void DoneRunOnMainThread() override { run_loop_->QuitWhenIdle(); }

 private:
  raw_ptr<base::RunLoop> run_loop_;

  GURL url_;
  VisitRow visit_;
};

}  // namespace

TEST_F(HistoryServiceTest, GetDomainDiversityLocalVsSynced) {
  HistoryService* history = history_service_.get();
  ASSERT_TRUE(history);

  base::Time query_time = base::Time::Now();

  // Make sure `query_time` is at least some time past the midnight so that
  // some domain visits can be inserted between `query_time` and midnight
  // for testing.
  query_time =
      std::max(query_time.LocalMidnight() + base::Minutes(10), query_time);

  // Add a local visit.
  history->AddPage(GURL("https://www.local.com/"),
                   GetTimeInThePast(query_time, /*days_back=*/1,
                                    /*hours_since_midnight=*/12),
                   0, 0, GURL(), history::RedirectList(),
                   ui::PAGE_TRANSITION_LINK, history::SOURCE_BROWSED, false);

  // Add a synced visit, as it would be created by HISTORY sync. The API to do
  // this isn't exposed in HistoryService (only HistoryBackend).
  {
    VisitRow visit;
    visit.visit_time = GetTimeInThePast(query_time, /*days_back=*/1,
                                        /*hours_since_midnight=*/14);
    visit.originator_cache_guid = "some_originator";
    visit.transition = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_START |
        ui::PAGE_TRANSITION_CHAIN_END);
    visit.is_known_to_sync = true;

    base::RunLoop run_loop;
    history->ScheduleDBTask(
        FROM_HERE,
        std::make_unique<AddSyncedVisitTask>(
            &run_loop, GURL("https://www.synced.com/"), visit),
        &tracker_);
    run_loop.Run();
  }

  auto [local_res, all_res] = GetDomainDiversityHelper(
      history, GetTimeInThePast(query_time, 1, 0), query_time,
      history::kEnableLast1DayMetric, &tracker_);

  ASSERT_EQ(1u, local_res.size());
  ASSERT_EQ(1u, all_res.size());

  // The "local" result should only count the local visit.
  TestDomainMetricSet(local_res[0], 1, -1, -1);
  // The "all" result should also include the synced visit.
  TestDomainMetricSet(all_res[0], 2, -1, -1);
}

TEST_F(HistoryServiceTest, GetMostRecentVisitsForGurl) {
  HistoryService* history = history_service_.get();
  ASSERT_TRUE(history);

  // Should not return older visits.
  AddPageInThePast(history, "http://www.google.com/", 6);
  // Should not return visits to a different URL.
  AddPageInThePast(history, "http://www.not-google.com/", 1);
  AddPageInThePast(history, "http://www.google.com/", 1);
  // Should return visits in order of visit time.
  AddPageInThePast(history, "http://www.google.com/", 3);
  AddPageInThePast(history, "http://www.google.com/", 2);
  // Should not return older visits.
  AddPageInThePast(history, "http://www.google.com/", 6);

  base::test::TestFuture<QueryURLResult> future;
  history->GetMostRecentVisitsForGurl(GURL("http://www.google.com/"), 3,
                                      future.GetCallback(), &tracker_);
  const auto result = future.Take();
  EXPECT_EQ(result.row.id(), 1);
  EXPECT_THAT(result.visits,
              testing::ElementsAre(
                  testing::AllOf(testing::Field(&VisitRow::url_id, 1),
                                 testing::Field(&VisitRow::visit_id, 3)),
                  testing::AllOf(testing::Field(&VisitRow::url_id, 1),
                                 testing::Field(&VisitRow::visit_id, 5)),
                  testing::AllOf(testing::Field(&VisitRow::url_id, 1),
                                 testing::Field(&VisitRow::visit_id, 4))));
}

// This class mocks the VisitDelegate in HistoryService to ensure that
// partitioned visited links are not added immediately, but rather are posted to
// the HistoryBackend before notifying the VisitDelegate.
class OrderingHistoryServiceTest : public HistoryServiceTest {
 public:
  OrderingHistoryServiceTest() = default;
  ~OrderingHistoryServiceTest() override = default;

 protected:
  friend class BackendDelegate;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    history_dir_ = temp_dir_.GetPath().AppendASCII("HistoryServiceTest");
    ASSERT_TRUE(base::CreateDirectory(history_dir_));

    // Override the VisitDelegate with our mock instance.
    std::unique_ptr<TestVisitDelegate> visit_delegate =
        std::make_unique<TestVisitDelegate>();
    // Wait for the AddVisitedLink task to post to the DB thread and contact the
    // TestVisitDelegate on the main thread. The task will terminate
    // the message loop when the build is done.
    visit_delegate->set_add_complete_task(run_loop_.QuitClosure());
    // Store a weak instance of the VisitDelegate so we can query it in tests.
    weak_visit_delegate_ = visit_delegate->GetWeakPtr();

    // Set up the HistoryService.
    history_service_ = std::make_unique<history::HistoryService>(
        nullptr, std::move(visit_delegate));
    if (!history_service_->Init(
            TestHistoryDatabaseParamsForPath(history_dir_))) {
      history_service_.reset();
      ADD_FAILURE();
    }
  }

  base::RunLoop run_loop_;
  base::WeakPtr<TestVisitDelegate> weak_visit_delegate_;
};

TEST_F(OrderingHistoryServiceTest, EnsureCorrectOrder) {
  // Create the components required for our visited link.
  const GURL frame_url("https://local1.url");
  const GURL top_level_url("https://local2.url");
  const GURL server_redirect_url("http://ads.google.com");
  const GURL client_redirect_url("http://google.com");
  base::Time visit_time = base::Time::Now() - base::Days(1);
  const ContextID context_id1 = 1;

  // Create a VisitedLinkRow containing the top-level site and frame origin.
  VisitedLinkRow deleted_visited_link_row;
  deleted_visited_link_row.top_level_url = top_level_url;
  deleted_visited_link_row.frame_url = frame_url;

  // Create a VisitedLink deletion notification for that same VisitedLink.
  DeletedVisitedLink deleted_visited_link;
  deleted_visited_link.link_url = client_redirect_url;
  deleted_visited_link.visited_link_row = deleted_visited_link_row;

  // Create a Visit deletion notification for that same VisitedLink and a mock
  // corresponding Visit.
  VisitRow deleted_visit_row = VisitRow();
  deleted_visit_row.visit_time = visit_time;
  DeletedVisit deleted_visit(deleted_visit_row, deleted_visited_link);
  std::vector<DeletedVisit> deleted_visits = {deleted_visit};

  // Prepare a mock `AddPage` request for the VisitedLink.
  HistoryAddPageArgs request(
      client_redirect_url, base::Time::Now() - base::Seconds(1), context_id1, 0,
      std::nullopt, frame_url,
      /*redirects=*/{}, ui::PAGE_TRANSITION_LINK, false, SOURCE_BROWSED, false,
      true, std::nullopt, top_level_url);

  // Simulate a user clicking on our VistedLink.
  history_service_->AddPage(request);

  // Check that the visit delegate is not called immediately.
  ASSERT_TRUE(weak_visit_delegate_);
  EXPECT_FALSE(weak_visit_delegate_->visit_delegate_was_called());

  // Wait for the visit delegate to resolve.
  run_loop_.Run();

  // Determine what VisitedLink should be in our mock hashtable.
  VisitedLink expected_link = {client_redirect_url,
                               net::SchemefulSite(top_level_url),
                               url::Origin::Create(frame_url)};
  std::vector<VisitedLink> expected_links = {expected_link};

  // Ensure that we have notified out visit delegate of the added link.
  ASSERT_TRUE(weak_visit_delegate_);
  EXPECT_TRUE(weak_visit_delegate_->visit_delegate_was_called());
  EXPECT_EQ(weak_visit_delegate_->get_added_links(), expected_links);
}

}  // namespace history
