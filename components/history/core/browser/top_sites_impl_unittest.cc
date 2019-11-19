// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/top_sites_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_client.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/history/core/browser/visit_delegate.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history/core/test/history_unittest_base.h"
#include "components/history/core/test/test_history_database.h"
#include "components/history/core/test/wait_top_sites_loaded_observer.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ContainerEq;

namespace history {

namespace {

const char kApplicationScheme[] = "application";
const char kPrepopulatedPageURL[] =
    "http://www.google.com/int/chrome/welcome.html";

// Returns whether |url| can be added to history.
bool MockCanAddURLToHistory(const GURL& url) {
  return url.is_valid() && !url.SchemeIs(kApplicationScheme);
}

// Used for querying top sites. Either runs sequentially, or runs a nested
// nested run loop until the response is complete. The later is used when
// TopSites is queried before it finishes loading.
class TopSitesQuerier {
 public:
  TopSitesQuerier() : number_of_callbacks_(0), waiting_(false) {}

  // Queries top sites. If |wait| is true a nested run loop is run until the
  // callback is notified.
  void QueryTopSites(TopSitesImpl* top_sites, bool wait) {
    int start_number_of_callbacks = number_of_callbacks_;
    base::RunLoop run_loop;
    top_sites->GetMostVisitedURLs(
        base::Bind(&TopSitesQuerier::OnTopSitesAvailable,
                   weak_ptr_factory_.GetWeakPtr(), &run_loop));
    if (wait && start_number_of_callbacks == number_of_callbacks_) {
      waiting_ = true;
      run_loop.Run();
    }
  }

  void CancelRequest() { weak_ptr_factory_.InvalidateWeakPtrs(); }

  void set_urls(const MostVisitedURLList& urls) { urls_ = urls; }
  const MostVisitedURLList& urls() const { return urls_; }

  int number_of_callbacks() const { return number_of_callbacks_; }

 private:
  // Callback for TopSitesImpl::GetMostVisitedURLs.
  void OnTopSitesAvailable(base::RunLoop* run_loop,
                           const history::MostVisitedURLList& data) {
    urls_ = data;
    number_of_callbacks_++;
    if (waiting_) {
      run_loop->QuitWhenIdle();
      waiting_ = false;
    }
  }

  MostVisitedURLList urls_;
  int number_of_callbacks_;
  bool waiting_;
  base::WeakPtrFactory<TopSitesQuerier> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TopSitesQuerier);
};

}  // namespace

class TopSitesImplTest : public HistoryUnitTestBase {
 public:
  TopSitesImplTest() {}

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    pref_service_.reset(new TestingPrefServiceSimple);
    TopSitesImpl::RegisterPrefs(pref_service_->registry());
    history_service_.reset(
        new HistoryService(nullptr, std::unique_ptr<VisitDelegate>()));
    ASSERT_TRUE(history_service_->Init(
        TestHistoryDatabaseParamsForPath(scoped_temp_dir_.GetPath())));
    ResetTopSites();
    WaitTopSitesLoaded();
  }

  void TearDown() override {
    DestroyTopSites();
    history_service_->Shutdown();
    history_service_.reset();
    pref_service_.reset();
  }

  // Forces top sites to load top sites from history, then recreates top sites.
  // Recreating top sites makes sure the changes from history are saved and
  // loaded from the db.
  void RefreshTopSitesAndRecreate() {
    StartQueryForMostVisited();
    WaitForHistory();
    RecreateTopSitesAndBlock();
  }

  // Blocks the caller until history processes a task. This is useful if you
  // need to wait until you know history has processed a task.
  void WaitForHistory() {
    BlockUntilHistoryProcessesPendingRequests(history_service());
  }

  TopSitesImpl* top_sites() { return top_sites_impl_.get(); }

  HistoryService* history_service() { return history_service_.get(); }

  PrepopulatedPageList GetPrepopulatedPages() {
    return top_sites()->GetPrepopulatedPages();
  }

  // Returns true if the TopSitesQuerier contains the prepopulate data starting
  // at |start_index|.
  void ContainsPrepopulatePages(const TopSitesQuerier& querier,
                                size_t start_index) {
    PrepopulatedPageList prepopulate_pages = GetPrepopulatedPages();
    ASSERT_LE(start_index + prepopulate_pages.size(), querier.urls().size());
    for (size_t i = 0; i < prepopulate_pages.size(); ++i) {
      EXPECT_EQ(prepopulate_pages[i].most_visited.url.spec(),
                querier.urls()[start_index + i].url.spec())
          << " @ index " << i;
    }
  }

  // Adds a page to history.
  void AddPageToHistory(const GURL& url,
                        const base::string16& title = base::string16(),
                        base::Time time = base::Time::Now(),
                        RedirectList redirects = RedirectList()) {
    if (redirects.empty())
      redirects.emplace_back(url);
    history_service()->AddPage(url, time, reinterpret_cast<ContextID>(1), 0,
                               GURL(), redirects, ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED, false);
    if (!title.empty())
      history_service()->SetPageTitle(url, title);
  }

  // Delets a url.
  void DeleteURL(const GURL& url) { history_service()->DeleteURLs({url}); }

  // Recreates top sites. This forces top sites to reread from the db.
  void RecreateTopSitesAndBlock() {
    // Recreate TopSites and wait for it to load.
    ResetTopSites();
    WaitTopSitesLoaded();
  }

  // Wrappers that allow private TopSites functions to be called from the
  // individual tests without making them all be friends.
  void SetTopSites(const MostVisitedURLList& new_top_sites) {
    top_sites()->SetTopSites(MostVisitedURLList(new_top_sites),
                             TopSitesImpl::CALL_LOCATION_FROM_OTHER_PLACES);
  }

  void StartQueryForMostVisited() { top_sites()->StartQueryForMostVisited(); }

  bool IsTopSitesLoaded() { return top_sites()->loaded_; }

  bool AddPrepopulatedPages(MostVisitedURLList* urls) {
    return top_sites()->AddPrepopulatedPages(urls);
  }

  void EmptyThreadSafeCache() {
    base::AutoLock lock(top_sites()->lock_);
    top_sites()->thread_safe_cache_.clear();
  }

  void ResetTopSites() {
    // TopSites shutdown takes some time as it happens on the DB thread and does
    // not support the existence of two TopSitesImpl for a location (due to
    // database locking). DestroyTopSites() waits for the TopSites cleanup to
    // complete before returning.
    DestroyTopSites();
    DCHECK(!top_sites_impl_);
    PrepopulatedPageList prepopulated_pages;
    prepopulated_pages.push_back(
        PrepopulatedPage(GURL(kPrepopulatedPageURL), base::string16(), -1, 0));
    top_sites_impl_ = new TopSitesImpl(
        pref_service_.get(), history_service_.get(),
        prepopulated_pages, base::Bind(MockCanAddURLToHistory));
    top_sites_impl_->Init(scoped_temp_dir_.GetPath().Append(kTopSitesFilename));
  }

  void DestroyTopSites() {
    if (top_sites_impl_) {
      top_sites_impl_->ShutdownOnUIThread();
      top_sites_impl_ = nullptr;

      task_environment_.RunUntilIdle();
    }
  }

  void WaitTopSitesLoaded() {
    DCHECK(top_sites_impl_);
    WaitTopSitesLoadedObserver wait_top_sites_loaded_observer(top_sites_impl_);
    wait_top_sites_loaded_observer.Run();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir scoped_temp_dir_;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<HistoryService> history_service_;
  scoped_refptr<TopSitesImpl> top_sites_impl_;

  // To cancel HistoryService tasks.
  base::CancelableTaskTracker history_tracker_;

  // To cancel TopSitesBackend tasks.
  base::CancelableTaskTracker top_sites_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TopSitesImplTest);
};  // Class TopSitesImplTest

class MockTopSitesObserver : public TopSitesObserver {
 public:
  MockTopSitesObserver() {}

  // history::TopSitesObserver:
  void TopSitesLoaded(TopSites* top_sites) override {}
  void TopSitesChanged(TopSites* top_sites,
                       ChangeReason change_reason) override {
    is_notified_ = true;
  }

  void ResetIsNotifiedState() { is_notified_ = false; }
  bool is_notified() const { return is_notified_; }

 private:
  bool is_notified_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockTopSitesObserver);
};

// Tests DoTitlesDiffer.
TEST_F(TopSitesImplTest, DoTitlesDiffer) {
  GURL url_1("http://url1/");
  GURL url_2("http://url2/");
  base::string16 title_1(base::ASCIIToUTF16("title1"));
  base::string16 title_2(base::ASCIIToUTF16("title2"));

  MockTopSitesObserver observer;
  top_sites()->AddObserver(&observer);

  // TopSites has a new list of sites and should notify its observers.
  std::vector<MostVisitedURL> list_1;
  list_1.emplace_back(url_1, title_1);
  SetTopSites(list_1);
  EXPECT_TRUE(observer.is_notified());
  observer.ResetIsNotifiedState();
  EXPECT_FALSE(observer.is_notified());

  // list_1 and list_2 have different sizes. TopSites should notify its
  // observers.
  std::vector<MostVisitedURL> list_2;
  list_2.emplace_back(url_1, title_1);
  list_2.emplace_back(url_2, title_2);
  SetTopSites(list_2);
  EXPECT_TRUE(observer.is_notified());
  observer.ResetIsNotifiedState();
  EXPECT_FALSE(observer.is_notified());

  // list_1 and list_2 are exactly the same now. TopSites should not notify its
  // observers.
  list_1.emplace_back(url_2, title_2);
  SetTopSites(list_1);
  EXPECT_FALSE(observer.is_notified());

  // Change |url_2|'s title to |title_1| in list_2. The two lists are different
  // in titles now. TopSites should notify its observers.
  list_2.pop_back();
  list_2.emplace_back(url_2, title_1);
  SetTopSites(list_2);
  EXPECT_TRUE(observer.is_notified());

  top_sites()->RemoveObserver(&observer);
}

// Tests DiffMostVisited.
TEST_F(TopSitesImplTest, DiffMostVisited) {
  GURL stays_the_same("http://staysthesame/");
  GURL gets_added_1("http://getsadded1/");
  GURL gets_added_2("http://getsadded2/");
  GURL gets_deleted_1("http://getsdeleted1/");
  GURL gets_moved_1("http://getsmoved1/");

  std::vector<MostVisitedURL> old_list;
  old_list.emplace_back(stays_the_same, base::string16());  // 0  (unchanged)
  old_list.emplace_back(gets_deleted_1, base::string16());  // 1  (deleted)
  old_list.emplace_back(gets_moved_1, base::string16());    // 2  (moved to 3)

  std::vector<MostVisitedURL> new_list;
  new_list.emplace_back(stays_the_same, base::string16());  // 0  (unchanged)
  new_list.emplace_back(gets_added_1, base::string16());    // 1  (added)
  new_list.emplace_back(gets_added_2, base::string16());    // 2  (added)
  new_list.emplace_back(gets_moved_1, base::string16());    // 3  (moved from 2)

  history::TopSitesDelta delta;
  TopSitesImpl::DiffMostVisited(old_list, new_list, &delta);

  ASSERT_EQ(2u, delta.added.size());
  EXPECT_TRUE(gets_added_1 == delta.added[0].url.url);
  EXPECT_EQ(1, delta.added[0].rank);
  EXPECT_TRUE(gets_added_2 == delta.added[1].url.url);
  EXPECT_EQ(2, delta.added[1].rank);

  ASSERT_EQ(1u, delta.deleted.size());
  EXPECT_TRUE(gets_deleted_1 == delta.deleted[0].url);

  ASSERT_EQ(1u, delta.moved.size());
  EXPECT_TRUE(gets_moved_1 == delta.moved[0].url.url);
  EXPECT_EQ(3, delta.moved[0].rank);
}

// Tests GetMostVisitedURLs.
TEST_F(TopSitesImplTest, GetMostVisited) {
  GURL news("http://news.google.com/");
  GURL google("http://google.com/");

  AddPageToHistory(news);
  AddPageToHistory(google);

  StartQueryForMostVisited();
  WaitForHistory();

  TopSitesQuerier querier;
  querier.QueryTopSites(top_sites(), false);

  ASSERT_EQ(1, querier.number_of_callbacks());

  // 2 extra prepopulated URLs.
  ASSERT_EQ(2u + GetPrepopulatedPages().size(), querier.urls().size());
  EXPECT_EQ(news, querier.urls()[0].url);
  EXPECT_EQ(google, querier.urls()[1].url);
  ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 2));
}

// Tests GetMostVisitedURLs with a redirect.
TEST_F(TopSitesImplTest, GetMostVisitedWithRedirect) {
  GURL bare("http://cnn.com/");
  GURL www("https://www.cnn.com/");
  GURL edition("https://edition.cnn.com/");

  AddPageToHistory(edition, base::ASCIIToUTF16("CNN"), base::Time::Now(),
                   history::RedirectList{bare, www, edition});
  AddPageToHistory(edition);

  StartQueryForMostVisited();
  WaitForHistory();

  TopSitesQuerier querier;
  querier.QueryTopSites(top_sites(), false);

  ASSERT_EQ(1, querier.number_of_callbacks());

  // This behavior is not desirable: even though edition.cnn.com is in the list
  // of top sites, and the the bare URL cnn.com is just a redirect to it, we're
  // returning both. Even worse, the NTP will show the same title, and icon for
  // the site, so to the user it looks like we just have the same thing twice.
  // (https://crbug.com/567132)
  std::vector<GURL> expected_urls = {bare, edition};  // should be {edition}.

  for (const auto& prepopulated : GetPrepopulatedPages()) {
    expected_urls.push_back(prepopulated.most_visited.url);
  }
  std::vector<GURL> actual_urls;
  for (const auto& actual : querier.urls()) {
    actual_urls.push_back(actual.url);
  }
  EXPECT_THAT(actual_urls, ContainerEq(expected_urls));
}

// Makes sure changes done to top sites get mirrored to the db.
TEST_F(TopSitesImplTest, SaveToDB) {
  MostVisitedURL url;
  GURL asdf_url("http://asdf.com");
  base::string16 asdf_title(base::ASCIIToUTF16("ASDF"));
  GURL google_url("http://google.com");
  base::string16 google_title(base::ASCIIToUTF16("Google"));
  GURL news_url("http://news.google.com");
  base::string16 news_title(base::ASCIIToUTF16("Google News"));

  // Add asdf_url to history.
  AddPageToHistory(asdf_url, asdf_title);

  // Make TopSites reread from the db.
  StartQueryForMostVisited();
  WaitForHistory();

  RecreateTopSitesAndBlock();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);
    ASSERT_EQ(1u + GetPrepopulatedPages().size(), querier.urls().size());
    EXPECT_EQ(asdf_url, querier.urls()[0].url);
    EXPECT_EQ(asdf_title, querier.urls()[0].title);
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 1));
  }

  MostVisitedURL url2;
  url2.url = google_url;
  url2.title = google_title;

  AddPageToHistory(url2.url, url2.title);

  // Make TopSites reread from the db.
  RefreshTopSitesAndRecreate();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);
    ASSERT_EQ(2u + GetPrepopulatedPages().size(), querier.urls().size());
    EXPECT_EQ(asdf_url, querier.urls()[0].url);
    EXPECT_EQ(asdf_title, querier.urls()[0].title);
    EXPECT_EQ(google_url, querier.urls()[1].url);
    EXPECT_EQ(google_title, querier.urls()[1].title);
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 2));
  }
}

// More permutations of saving to db.
TEST_F(TopSitesImplTest, RealDatabase) {
  MostVisitedURL url;
  GURL asdf_url("http://asdf.com");
  base::string16 asdf_title(base::ASCIIToUTF16("ASDF"));
  GURL google1_url("http://google.com");
  GURL google2_url("http://google.com/redirect");
  GURL google3_url("http://www.google.com");
  base::string16 google_title(base::ASCIIToUTF16("Google"));
  GURL news_url("http://news.google.com");
  base::string16 news_title(base::ASCIIToUTF16("Google News"));

  url.url = asdf_url;
  url.title = asdf_title;

  base::Time add_time(base::Time::Now());
  AddPageToHistory(url.url, url.title, add_time);

  RefreshTopSitesAndRecreate();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    ASSERT_EQ(1u + GetPrepopulatedPages().size(), querier.urls().size());
    EXPECT_EQ(asdf_url, querier.urls()[0].url);
    EXPECT_EQ(asdf_title, querier.urls()[0].title);
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 1));
  }

  MostVisitedURL url2;
  url2.url = google3_url;
  url2.title = google_title;
  history::RedirectList url2_redirects;
  url2_redirects.push_back(google1_url);
  url2_redirects.push_back(google2_url);
  url2_redirects.push_back(google3_url);

  AddPageToHistory(google3_url, url2.title,
                   add_time - base::TimeDelta::FromMinutes(1), url2_redirects);
  // Add google twice so that it becomes the first visited site.
  AddPageToHistory(google3_url, url2.title,
                   add_time - base::TimeDelta::FromMinutes(2), url2_redirects);

  RefreshTopSitesAndRecreate();

  {
    scoped_refptr<base::RefCountedMemory> read_data;
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    ASSERT_EQ(2u + GetPrepopulatedPages().size(), querier.urls().size());
    EXPECT_EQ(google1_url, querier.urls()[0].url);
    EXPECT_EQ(google_title, querier.urls()[0].title);

    EXPECT_EQ(asdf_url, querier.urls()[1].url);
    EXPECT_EQ(asdf_title, querier.urls()[1].title);
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 2));
  }
}

TEST_F(TopSitesImplTest, DeleteNotifications) {
  GURL google1_url("http://google.com");
  GURL google2_url("http://google.com/redirect");
  GURL google3_url("http://www.google.com");
  base::string16 google_title(base::ASCIIToUTF16("Google"));
  GURL news_url("http://news.google.com");
  base::string16 news_title(base::ASCIIToUTF16("Google News"));

  AddPageToHistory(google1_url, google_title);
  AddPageToHistory(news_url, news_title);

  RefreshTopSitesAndRecreate();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    ASSERT_EQ(GetPrepopulatedPages().size() + 2, querier.urls().size());
  }

  DeleteURL(news_url);

  // Wait for history to process the deletion.
  WaitForHistory();
  // The deletion called back to TopSitesImpl (on the main thread), which
  // triggers a history query. Wait for that to complete.
  WaitForHistory();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    ASSERT_EQ(1u + GetPrepopulatedPages().size(), querier.urls().size());
    EXPECT_EQ(google_title, querier.urls()[0].title);
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 1));
  }

  // Now reload. This verifies topsites actually wrote the deletion to disk.
  RefreshTopSitesAndRecreate();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    ASSERT_EQ(1u + GetPrepopulatedPages().size(), querier.urls().size());
    EXPECT_EQ(google_title, querier.urls()[0].title);
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 1));
  }

  DeleteURL(google1_url);

  // Wait for history to process the deletion.
  WaitForHistory();
  // The deletion called back to TopSitesImpl (on the main thread), which
  // triggers a history query. Wait for that to complete.
  WaitForHistory();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    ASSERT_EQ(GetPrepopulatedPages().size(), querier.urls().size());
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 0));
  }

  // Now reload. This verifies topsites actually wrote the deletion to disk.
  RefreshTopSitesAndRecreate();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    ASSERT_EQ(GetPrepopulatedPages().size(), querier.urls().size());
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 0));
  }
}

// Verifies that callbacks are notified correctly if requested before top sites
// has loaded.
TEST_F(TopSitesImplTest, NotifyCallbacksWhenLoaded) {
  // Recreate top sites. It won't be loaded now.
  ResetTopSites();

  EXPECT_FALSE(IsTopSitesLoaded());

  TopSitesQuerier querier1;
  TopSitesQuerier querier2;
  TopSitesQuerier querier3;

  // Starts the queries.
  querier1.QueryTopSites(top_sites(), false);
  querier2.QueryTopSites(top_sites(), false);
  querier3.QueryTopSites(top_sites(), false);

  // We shouldn't have gotten a callback.
  EXPECT_EQ(0, querier1.number_of_callbacks());
  EXPECT_EQ(0, querier2.number_of_callbacks());
  EXPECT_EQ(0, querier3.number_of_callbacks());

  // Wait for loading to complete.
  WaitTopSitesLoaded();

  // Now we should have gotten the callbacks.
  EXPECT_EQ(1, querier1.number_of_callbacks());
  EXPECT_EQ(GetPrepopulatedPages().size(), querier1.urls().size());
  EXPECT_EQ(1, querier2.number_of_callbacks());
  EXPECT_EQ(GetPrepopulatedPages().size(), querier2.urls().size());
  EXPECT_EQ(1, querier3.number_of_callbacks());
  EXPECT_EQ(GetPrepopulatedPages().size(), querier3.urls().size());

  // Reset the top sites.
  MostVisitedURLList pages;
  MostVisitedURL url;
  url.url = GURL("http://1.com/");
  pages.push_back(url);
  url.url = GURL("http://2.com/");
  pages.push_back(url);
  SetTopSites(pages);

  // Recreate top sites. It won't be loaded now.
  ResetTopSites();

  EXPECT_FALSE(IsTopSitesLoaded());

  TopSitesQuerier querier4;

  // Query again.
  querier4.QueryTopSites(top_sites(), false);

  // We shouldn't have gotten a callback.
  EXPECT_EQ(0, querier4.number_of_callbacks());

  // Wait for loading to complete.
  WaitTopSitesLoaded();

  // Now we should have gotten the callbacks.
  EXPECT_EQ(1, querier4.number_of_callbacks());
  ASSERT_EQ(2u + GetPrepopulatedPages().size(), querier4.urls().size());

  EXPECT_EQ("http://1.com/", querier4.urls()[0].url.spec());
  EXPECT_EQ("http://2.com/", querier4.urls()[1].url.spec());
  ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier4, 2));

  // Reset the top sites again, this time don't reload.
  url.url = GURL("http://3.com/");
  pages.push_back(url);
  SetTopSites(pages);

  // Query again.
  TopSitesQuerier querier5;
  querier5.QueryTopSites(top_sites(), true);

  EXPECT_EQ(1, querier5.number_of_callbacks());

  ASSERT_EQ(3u + GetPrepopulatedPages().size(), querier5.urls().size());
  EXPECT_EQ("http://1.com/", querier5.urls()[0].url.spec());
  EXPECT_EQ("http://2.com/", querier5.urls()[1].url.spec());
  EXPECT_EQ("http://3.com/", querier5.urls()[2].url.spec());
  ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier5, 3));
}

// Makes sure canceled requests are not notified.
TEST_F(TopSitesImplTest, CancelingRequestsForTopSites) {
  // Recreate top sites. It won't be loaded now.
  ResetTopSites();

  EXPECT_FALSE(IsTopSitesLoaded());

  TopSitesQuerier querier1;
  TopSitesQuerier querier2;

  // Starts the queries.
  querier1.QueryTopSites(top_sites(), false);
  querier2.QueryTopSites(top_sites(), false);

  // We shouldn't have gotten a callback.
  EXPECT_EQ(0, querier1.number_of_callbacks());
  EXPECT_EQ(0, querier2.number_of_callbacks());

  querier2.CancelRequest();

  // Wait for loading to complete.
  WaitTopSitesLoaded();

  // The first callback should succeed.
  EXPECT_EQ(1, querier1.number_of_callbacks());
  EXPECT_EQ(GetPrepopulatedPages().size(), querier1.urls().size());

  // And the canceled callback should not be notified.
  EXPECT_EQ(0, querier2.number_of_callbacks());
}

// Tests variations of blacklisting without testing prepopulated page
// blacklisting.
TEST_F(TopSitesImplTest, BlacklistingWithoutPrepopulated) {
  MostVisitedURLList pages;
  MostVisitedURL url, url1;
  url.url = GURL("http://bbc.com/");
  pages.push_back(url);
  url1.url = GURL("http://google.com/");
  pages.push_back(url1);

  SetTopSites(pages);
  EXPECT_FALSE(top_sites()->IsBlacklisted(GURL("http://bbc.com/")));

  // Blacklist google.com.
  top_sites()->AddBlacklistedURL(GURL("http://google.com/"));

  EXPECT_TRUE(top_sites()->HasBlacklistedItems());
  EXPECT_TRUE(top_sites()->IsBlacklisted(GURL("http://google.com/")));
  EXPECT_FALSE(top_sites()->IsBlacklisted(GURL("http://bbc.com/")));

  // Make sure the blacklisted site isn't returned in the results.
  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
  }

  // Recreate top sites and make sure blacklisted url was correctly read.
  RecreateTopSitesAndBlock();
  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
  }

  // Mark google as no longer blacklisted.
  top_sites()->RemoveBlacklistedURL(GURL("http://google.com/"));
  EXPECT_FALSE(top_sites()->HasBlacklistedItems());
  EXPECT_FALSE(top_sites()->IsBlacklisted(GURL("http://google.com/")));

  // Make sure google is returned now.
  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    EXPECT_EQ("http://google.com/", q.urls()[1].url.spec());
  }

  // Remove all blacklisted sites.
  top_sites()->ClearBlacklistedURLs();
  EXPECT_FALSE(top_sites()->HasBlacklistedItems());

  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    EXPECT_EQ("http://google.com/", q.urls()[1].url.spec());
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(q, 2));
  }
}

// Tests variations of blacklisting including blacklisting prepopulated pages.
// This test is disable for Android because Android does not have any
// prepopulated pages.
TEST_F(TopSitesImplTest, BlacklistingWithPrepopulated) {
  MostVisitedURLList pages;
  MostVisitedURL url, url1;
  url.url = GURL("http://bbc.com/");
  pages.push_back(url);
  url1.url = GURL("http://google.com/");
  pages.push_back(url1);

  SetTopSites(pages);
  EXPECT_FALSE(top_sites()->IsBlacklisted(GURL("http://bbc.com/")));

  // Blacklist google.com.
  top_sites()->AddBlacklistedURL(GURL("http://google.com/"));

  DCHECK_GE(GetPrepopulatedPages().size(), 1u);
  GURL prepopulate_url = GetPrepopulatedPages()[0].most_visited.url;

  EXPECT_TRUE(top_sites()->HasBlacklistedItems());
  EXPECT_TRUE(top_sites()->IsBlacklisted(GURL("http://google.com/")));
  EXPECT_FALSE(top_sites()->IsBlacklisted(GURL("http://bbc.com/")));
  EXPECT_FALSE(top_sites()->IsBlacklisted(prepopulate_url));

  // Make sure the blacklisted site isn't returned in the results.
  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    ASSERT_EQ(1u + GetPrepopulatedPages().size(), q.urls().size());
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(q, 1));
  }

  // Recreate top sites and make sure blacklisted url was correctly read.
  RecreateTopSitesAndBlock();
  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    ASSERT_EQ(1u + GetPrepopulatedPages().size(), q.urls().size());
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(q, 1));
  }

  // Blacklist one of the prepopulate urls.
  top_sites()->AddBlacklistedURL(prepopulate_url);
  EXPECT_TRUE(top_sites()->HasBlacklistedItems());

  // Make sure the blacklisted prepopulate url isn't returned.
  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    ASSERT_EQ(1u + GetPrepopulatedPages().size() - 1, q.urls().size());
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    for (size_t i = 1; i < q.urls().size(); ++i)
      EXPECT_NE(prepopulate_url.spec(), q.urls()[i].url.spec());
  }

  // Mark google as no longer blacklisted.
  top_sites()->RemoveBlacklistedURL(GURL("http://google.com/"));
  EXPECT_TRUE(top_sites()->HasBlacklistedItems());
  EXPECT_FALSE(top_sites()->IsBlacklisted(GURL("http://google.com/")));

  // Make sure google is returned now.
  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    ASSERT_EQ(2u + GetPrepopulatedPages().size() - 1, q.urls().size());
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    EXPECT_EQ("http://google.com/", q.urls()[1].url.spec());
    // Android has only one prepopulated page which has been blacklisted, so
    // only 2 urls are returned.
    if (q.urls().size() > 2)
      EXPECT_NE(prepopulate_url.spec(), q.urls()[2].url.spec());
    else
      EXPECT_EQ(1u, GetPrepopulatedPages().size());
  }

  // Remove all blacklisted sites.
  top_sites()->ClearBlacklistedURLs();
  EXPECT_FALSE(top_sites()->HasBlacklistedItems());

  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    ASSERT_EQ(2u + GetPrepopulatedPages().size(), q.urls().size());
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    EXPECT_EQ("http://google.com/", q.urls()[1].url.spec());
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(q, 2));
  }
}

// Makes sure prepopulated pages exist.
TEST_F(TopSitesImplTest, AddPrepopulatedPages) {
  TopSitesQuerier q;
  q.QueryTopSites(top_sites(), true);
  EXPECT_EQ(GetPrepopulatedPages().size(), q.urls().size());
  ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(q, 0));

  MostVisitedURLList pages = q.urls();
  EXPECT_FALSE(AddPrepopulatedPages(&pages));

  EXPECT_EQ(GetPrepopulatedPages().size(), pages.size());
  q.set_urls(pages);
  ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(q, 0));
}

}  // namespace history
