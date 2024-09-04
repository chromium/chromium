// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/history/core/browser/expire_history_backend.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/favicon/core/favicon_database.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_backend_notifier.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/core/browser/top_sites_impl.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/history/core/test/history_client_fake_bookmarks.h"
#include "components/history/core/test/test_history_database.h"
#include "components/history/core/test/thumbnail.h"
#include "components/history/core/test/wait_top_sites_loaded_observer.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::UnorderedElementsAre;

// The test must be in the history namespace for the gtest forward declarations
// to work. It also eliminates a bunch of ugly "history::".
namespace history {

namespace {

const std::string kTestAppId = "org.chromium.dino";

base::Time PretendNow() {
  static constexpr base::Time::Exploded kReferenceTime = {.year = 2015,
                                                          .month = 1,
                                                          .day_of_week = 5,
                                                          .day_of_month = 2,
                                                          .hour = 11};
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(kReferenceTime, &out_time));
  return out_time;
}

// Returns whether `url` can be added to history.
bool MockCanAddURLToHistory(const GURL& url) {
  return url.is_valid();
}

base::Time GetOldFaviconThreshold() {
  return PretendNow() - base::Days(internal::kOnDemandFaviconIsOldAfterDays);
}

}  // namespace

// ExpireHistoryTest -----------------------------------------------------------

class ExpireHistoryTest : public testing::Test, public HistoryBackendNotifier {
 public:
  ExpireHistoryTest()
      : backend_client_(history_client_.CreateBackendClient()),
        expirer_(this,
                 backend_client_.get(),
                 task_environment_.GetMainThreadTaskRunner()) {}

 protected:
  // Called by individual tests when they want data populated.
  void AddExampleData(URLID url_ids[3],
                      base::Time visit_times[4],
                      bool set_app_id = false);

  // Returns true if the given favicon has an entry in the DB.
  bool HasFavicon(favicon_base::FaviconID favicon_id);

  favicon_base::FaviconID GetFavicon(const GURL& page_url,
                                     favicon_base::IconType icon_type);

  // EXPECTs that each URL-specific history thing (basically, everything but
  // favicons) is gone, the reason being either that it was automatically
  // `expired`, or manually deleted.
  void EnsureURLInfoGone(const URLRow& row, bool expired);

  const DeletionInfo* GetLastDeletionInfo() {
    if (urls_deleted_notifications_.empty())
      return nullptr;
    return &urls_deleted_notifications_.back();
  }

  // Returns whether HistoryBackendNotifier::NotifyURLsModified was
  // called for `url`.
  bool ModifiedNotificationSentDueToExpiry(const GURL& url);
  bool ModifiedNotificationSentDueToUserAction(const GURL& url);

  // Clears the list of notifications received.
  void ClearLastNotifications() {
    urls_modified_notifications_.clear();
    urls_deleted_notifications_.clear();
  }

  void StarURL(const GURL& url) { history_client_.AddBookmark(url); }

  // Returns the path the db files are created in.
  const base::FilePath& path() const { return tmp_dir_.GetPath(); }

  // This must be destroyed last.
  base::ScopedTempDir tmp_dir_;

  base::test::TaskEnvironment task_environment_;

  HistoryClientFakeBookmarks history_client_;
  std::unique_ptr<HistoryBackendClient> backend_client_;

  ExpireHistoryBackend expirer_;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<HistoryDatabase> main_db_;
  std::unique_ptr<favicon::FaviconDatabase> thumb_db_;
  scoped_refptr<TopSitesImpl> top_sites_;

  typedef std::vector<std::pair<bool, URLRows>> URLsModifiedNotificationList;
  URLsModifiedNotificationList urls_modified_notifications_;

  typedef std::vector<DeletionInfo> URLsDeletedNotificationList;
  URLsDeletedNotificationList urls_deleted_notifications_;

 private:
  void SetUp() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());

    base::FilePath history_name = path().Append(kHistoryFilename);
    main_db_ = std::make_unique<TestHistoryDatabase>();
    if (main_db_->Init(history_name) != sql::INIT_OK)
      main_db_.reset();

    base::FilePath thumb_name = path().Append(kFaviconsFilename);
    thumb_db_ = std::make_unique<favicon::FaviconDatabase>();
    if (thumb_db_->Init(thumb_name) != sql::INIT_OK)
      thumb_db_.reset();

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    TopSitesImpl::RegisterPrefs(pref_service_->registry());

    expirer_.SetDatabases(main_db_.get(), thumb_db_.get());
    top_sites_ = new TopSitesImpl(pref_service_.get(), nullptr, nullptr,
                                  PrepopulatedPageList(),
                                  base::BindRepeating(MockCanAddURLToHistory));
    WaitTopSitesLoadedObserver wait_top_sites_observer(top_sites_);
    top_sites_->Init(path().Append(kTopSitesFilename));
    wait_top_sites_observer.Run();
  }

  void TearDown() override {
    ClearLastNotifications();

    expirer_.SetDatabases(nullptr, nullptr);

    main_db_.reset();
    thumb_db_.reset();

    top_sites_->ShutdownOnUIThread();
    top_sites_ = nullptr;

    if (base::CurrentThread::Get())
      base::RunLoop().RunUntilIdle();

    pref_service_.reset();
  }

  bool ModifiedNotificationSent(const GURL& url,
                                bool should_be_from_expiration);

  // HistoryBackendNotifier:
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override {}
  void NotifyURLVisited(const URLRow& url_row,
                        const VisitRow& visit_row,
                        std::optional<int64_t> local_navigation_id) override {}
  void NotifyURLsModified(const URLRows& rows,
                          bool is_from_expiration) override {
    urls_modified_notifications_.push_back(
        std::make_pair(is_from_expiration, rows));
  }
  void NotifyDeletions(DeletionInfo deletion_info) override {
    urls_deleted_notifications_.push_back(std::move(deletion_info));
  }
  void NotifyVisitUpdated(const VisitRow& visit,
                          VisitUpdateReason reason) override {}
  void NotifyVisitsDeleted(const std::vector<DeletedVisit>& visits) override {}
};

// The example data consists of 4 visits. The middle two visits are to the
// same URL, while the first and last are for unique ones. This allows a test
// for the oldest or newest to include both a URL that should get totally
// deleted (the one on the end) with one that should only get a visit deleted
// (with the one in the middle) when it picks the proper threshold time.
//
// Each visit has indexed data, each URL has thumbnail. The first two URLs will
// share the same favicon, while the last one will have a unique favicon. The
// second visit for the middle URL is typed.
//
// The IDs of the added URLs, and the times of the four added visits will be
// added to the given arrays. If set_app_id is true, set the app_id to the
// 2nd/3rd row for testing.
void ExpireHistoryTest::AddExampleData(URLID url_ids[3],
                                       base::Time visit_times[4],
                                       bool set_app_id) {
  if (!main_db_)
    return;

  // Four times for each visit.
  visit_times[3] = PretendNow();
  visit_times[2] = visit_times[3] - base::Days(1);
  visit_times[1] = visit_times[3] - base::Days(2);
  visit_times[0] = visit_times[3] - base::Days(3);

  // Two favicons. The first two URLs will share the same one, while the last
  // one will have a unique favicon.
  favicon_base::FaviconID favicon1 = thumb_db_->AddFavicon(
      GURL("http://favicon/url1"), favicon_base::IconType::kFavicon);
  favicon_base::FaviconID favicon2 = thumb_db_->AddFavicon(
      GURL("http://favicon/url2"), favicon_base::IconType::kFavicon);

  // Three URLs.
  URLRow url_row1(GURL("http://www.google.com/1"));
  url_row1.set_last_visit(visit_times[0]);
  url_row1.set_visit_count(1);
  url_ids[0] = main_db_->AddURL(url_row1);
  thumb_db_->AddIconMapping(url_row1.url(), favicon1);

  URLRow url_row2(GURL("http://www.google.com/2"));
  url_row2.set_last_visit(visit_times[2]);
  url_row2.set_visit_count(2);
  url_row2.set_typed_count(1);
  url_ids[1] = main_db_->AddURL(url_row2);
  thumb_db_->AddIconMapping(url_row2.url(), favicon1);

  URLRow url_row3(GURL("http://www.google.com/3"));
  url_row3.set_last_visit(visit_times[3]);
  url_row3.set_visit_count(1);
  url_ids[2] = main_db_->AddURL(url_row3);
  thumb_db_->AddIconMapping(url_row3.url(), favicon2);

  // Four visits.
  VisitRow visit_row1;
  visit_row1.url_id = url_ids[0];
  visit_row1.visit_time = visit_times[0];
  main_db_->AddVisit(&visit_row1, SOURCE_BROWSED);

  VisitRow visit_row2;
  visit_row2.url_id = url_ids[1];
  visit_row2.visit_time = visit_times[1];
  if (set_app_id) {
    visit_row2.app_id = kTestAppId;
  }
  main_db_->AddVisit(&visit_row2, SOURCE_BROWSED);

  VisitRow visit_row3;
  visit_row3.url_id = url_ids[1];
  visit_row3.visit_time = visit_times[2];
  visit_row3.transition = ui::PAGE_TRANSITION_TYPED;
  visit_row3.incremented_omnibox_typed_score = true;
  if (set_app_id) {
    visit_row3.app_id = kTestAppId;
  }
  main_db_->AddVisit(&visit_row3, SOURCE_BROWSED);

  VisitRow visit_row4;
  visit_row4.url_id = url_ids[2];
  visit_row4.visit_time = visit_times[3];
  main_db_->AddVisit(&visit_row4, SOURCE_BROWSED);
}

bool ExpireHistoryTest::HasFavicon(favicon_base::FaviconID favicon_id) {
  if (!thumb_db_ || favicon_id == 0)
    return false;
  return thumb_db_->GetFaviconHeader(favicon_id, nullptr, nullptr);
}

favicon_base::FaviconID ExpireHistoryTest::GetFavicon(
    const GURL& page_url,
    favicon_base::IconType icon_type) {
  std::vector<favicon::IconMapping> icon_mappings;
  if (thumb_db_->GetIconMappingsForPageURL(page_url, {icon_type},
                                           &icon_mappings)) {
    return icon_mappings[0].icon_id;
  }
  return 0;
}

void ExpireHistoryTest::EnsureURLInfoGone(const URLRow& row, bool expired) {
  // The passed in `row` must originate from `main_db_` so that its ID will be
  // set to what had been in effect in `main_db_` before the deletion.
  ASSERT_NE(0, row.id());

  // Verify the URL no longer exists.
  URLRow temp_row;
  EXPECT_FALSE(main_db_->GetURLRow(row.id(), &temp_row));

  // There should be no visits.
  VisitVector visits;
  main_db_->GetVisitsForURL(row.id(), &visits);
  EXPECT_EQ(0U, visits.size());

  bool found_delete_notification = false;
  for (const auto& info : urls_deleted_notifications_) {
    EXPECT_EQ(expired, info.is_from_expiration());
    const history::URLRows& rows(info.deleted_rows());
    auto it_row =
        base::ranges::find_if(rows, history::URLRow::URLRowHasURL(row.url()));
    if (it_row != rows.end()) {
      // Further verify that the ID is set to what had been in effect in the
      // main database before the deletion. The InMemoryHistoryBackend relies
      // on this to delete its cached copy of the row.
      EXPECT_EQ(row.id(), it_row->id());
      found_delete_notification = true;
    }
  }
  for (const auto& pair : urls_modified_notifications_) {
    const auto& rows = pair.second;
    EXPECT_TRUE(
        base::ranges::none_of(rows, history::URLRow::URLRowHasURL(row.url())));
  }
  EXPECT_TRUE(found_delete_notification);
}

bool ExpireHistoryTest::ModifiedNotificationSentDueToExpiry(const GURL& url) {
  return ModifiedNotificationSent(url,
                                  /*should_be_from_expiration=*/true);
}
bool ExpireHistoryTest::ModifiedNotificationSentDueToUserAction(
    const GURL& url) {
  return ModifiedNotificationSent(url,
                                  /*should_be_from_expiration=*/false);
}

bool ExpireHistoryTest::ModifiedNotificationSent(
    const GURL& url,
    bool should_be_from_expiration) {
  for (const auto& pair : urls_modified_notifications_) {
    const bool is_from_expiration = pair.first;
    const auto& rows = pair.second;
    if (is_from_expiration == should_be_from_expiration &&
        base::ranges::any_of(rows, history::URLRow::URLRowHasURL(url))) {
      return true;
    }
  }
  return false;
}

TEST_F(ExpireHistoryTest, DeleteFaviconsIfPossible) {
  // Add a favicon record.
  const GURL favicon_url("http://www.google.com/favicon.ico");
  favicon_base::FaviconID icon_id =
      thumb_db_->AddFavicon(favicon_url, favicon_base::IconType::kFavicon);
  EXPECT_TRUE(icon_id);
  EXPECT_TRUE(HasFavicon(icon_id));

  // The favicon should be deletable with no users.
  {
    ExpireHistoryBackend::DeleteEffects effects;
    effects.affected_favicons.insert(icon_id);
    expirer_.DeleteFaviconsIfPossible(&effects);
    EXPECT_FALSE(HasFavicon(icon_id));
    EXPECT_EQ(1U, effects.deleted_favicons.size());
    EXPECT_EQ(1U, effects.deleted_favicons.count(favicon_url));
  }

  // Add back the favicon.
  icon_id =
      thumb_db_->AddFavicon(favicon_url, favicon_base::IconType::kTouchIcon);
  EXPECT_TRUE(icon_id);
  EXPECT_TRUE(HasFavicon(icon_id));

  // Add a page that references the favicon.
  URLRow row(GURL("http://www.google.com/2"));
  row.set_visit_count(1);
  EXPECT_TRUE(main_db_->AddURL(row));
  thumb_db_->AddIconMapping(row.url(), icon_id);

  // Favicon should not be deletable.
  {
    ExpireHistoryBackend::DeleteEffects effects;
    effects.affected_favicons.insert(icon_id);
    expirer_.DeleteFaviconsIfPossible(&effects);
    EXPECT_TRUE(HasFavicon(icon_id));
    EXPECT_TRUE(effects.deleted_favicons.empty());
  }
}

// Deletes a URL with a favicon that it is the last referencer of, so that it
// should also get deleted.
// This test failed near end of month.
// Please comment on http://crbug.com/15724 if it fails again.
TEST_F(ExpireHistoryTest, DeleteURLAndFavicon) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  // Verify things are the way we expect with a URL row, favicon.
  URLRow last_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &last_row));
  favicon_base::FaviconID favicon_id =
      GetFavicon(last_row.url(), favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());

  // Delete the URL and its dependencies.
  expirer_.DeleteURL(last_row.url(), base::Time::Max());

  // All the normal data + the favicon should be gone.
  EnsureURLInfoGone(last_row, false);
  EXPECT_FALSE(GetFavicon(last_row.url(), favicon_base::IconType::kFavicon));
  EXPECT_FALSE(HasFavicon(favicon_id));
}

// Deletes visits to a URL with a time bound. The url, favicon and the second
// visit should not get deleted.
TEST_F(ExpireHistoryTest, DeleteURLWithTimeBound) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  // Remove the first url because it shares the favicon with the second url.
  URLRow first_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[0], &first_row));
  expirer_.DeleteURL(first_row.url(), base::Time::Max());

  // Verify things are the way we expect with a URL row, favicon.
  URLRow second_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &second_row));
  favicon_base::FaviconID favicon_id =
      GetFavicon(second_row.url(), favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  ASSERT_EQ(2U, visits.size());

  // Delete the first visit but not the URL and dependencies.
  expirer_.DeleteURL(second_row.url(), visits[0].visit_time);
  // The second visit, URL and favicon should still be there.
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &second_row));
  VisitVector visits_after_deletion;
  main_db_->GetVisitsForURL(url_ids[1], &visits_after_deletion);
  ASSERT_EQ(1U, visits_after_deletion.size());
  EXPECT_EQ(visits[1].visit_time, visits_after_deletion[0].visit_time);
  EXPECT_TRUE(GetFavicon(second_row.url(), favicon_base::IconType::kFavicon));
  EXPECT_TRUE(HasFavicon(favicon_id));

  // Delete the second visit.
  expirer_.DeleteURL(second_row.url(), visits[1].visit_time);
  // All the normal data + the favicon should be gone.
  EnsureURLInfoGone(second_row, false);
  EXPECT_FALSE(GetFavicon(second_row.url(), favicon_base::IconType::kFavicon));
  EXPECT_FALSE(HasFavicon(favicon_id));
}

// Deletes a URL with a favicon that other URLs reference, so that the favicon
// should not get deleted. This also tests deleting more than one visit.
TEST_F(ExpireHistoryTest, DeleteURLWithoutFavicon) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  // Verify things are the way we expect with a URL row, favicon.
  URLRow last_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &last_row));
  favicon_base::FaviconID favicon_id =
      GetFavicon(last_row.url(), favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  EXPECT_EQ(2U, visits.size());

  // Delete the URL and its dependencies.
  expirer_.DeleteURL(last_row.url(), base::Time::Max());

  // All the normal data except the favicon should be gone.
  EnsureURLInfoGone(last_row, false);
  EXPECT_TRUE(HasFavicon(favicon_id));
}

// Deletes a URL with context annotations attached to the visits. Verifies the
// context annotations are also deleted.
TEST_F(ExpireHistoryTest, DeleteURLAndContextAnnotations) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  // Add some stub context annotations for the last URL row.
  URLRow last_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &last_row));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());
  int test_visit_id = visits[0].visit_id;
  main_db_->AddContextAnnotationsForVisit(test_visit_id, {});

  // Verify that the context annotation is there for that visit.
  VisitContextAnnotations unused;
  EXPECT_TRUE(main_db_->GetContextAnnotationsForVisit(test_visit_id, &unused));

  // Delete the URL and its dependencies.
  expirer_.DeleteURL(last_row.url(), base::Time::Max());

  // All the normal data + the favicon should be gone.
  EnsureURLInfoGone(last_row, false);
  EXPECT_FALSE(main_db_->GetContextAnnotationsForVisit(test_visit_id, &unused));
}

// DeleteURL should delete the history of starred urls, but the URL should
// remain starred and its favicon should remain too.
TEST_F(ExpireHistoryTest, DeleteStarredVisitedURL) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row));

  // Star the last URL.
  StarURL(url_row.url());

  // Attempt to delete the url.
  expirer_.DeleteURL(url_row.url(), base::Time::Max());

  // Verify it no longer exists.
  GURL url = url_row.url();
  ASSERT_FALSE(main_db_->GetRowForURL(url, &url_row));
  EnsureURLInfoGone(url_row, false);

  // Yet the favicon should exist.
  favicon_base::FaviconID favicon_id =
      GetFavicon(url, favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id));
}

// DeleteURL should not delete the favicon of bookmarked URLs.
TEST_F(ExpireHistoryTest, DeleteStarredUnvisitedURL) {
  // Create a bookmark associated with a favicon.
  const GURL url("http://www.google.com/starred");
  favicon_base::FaviconID favicon = thumb_db_->AddFavicon(
      GURL("http://favicon/url1"), favicon_base::IconType::kFavicon);
  thumb_db_->AddIconMapping(url, favicon);
  StarURL(url);

  // Delete it.
  expirer_.DeleteURL(url, base::Time::Max());

  // The favicon should exist.
  favicon_base::FaviconID favicon_id =
      GetFavicon(url, favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id));

  // Unstar the URL and try again to delete it.
  history_client_.ClearAllBookmarks();
  expirer_.DeleteURL(url, base::Time::Max());

  // The favicon should be gone.
  favicon_id = GetFavicon(url, favicon_base::IconType::kFavicon);
  EXPECT_FALSE(HasFavicon(favicon_id));
}

// Deletes multiple URLs at once.  The favicon for the third one but
// not the first two should be deleted.
TEST_F(ExpireHistoryTest, DeleteURLs) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  // Verify things are the way we expect with URL rows, favicons.
  URLRow rows[3];
  favicon_base::FaviconID favicon_ids[3];
  std::vector<GURL> urls;
  // Push back a bogus URL (which shouldn't change anything).
  urls.push_back(GURL());
  for (size_t i = 0; i < std::size(rows); ++i) {
    ASSERT_TRUE(main_db_->GetURLRow(url_ids[i], &rows[i]));
    favicon_ids[i] =
        GetFavicon(rows[i].url(), favicon_base::IconType::kFavicon);
    EXPECT_TRUE(HasFavicon(favicon_ids[i]));
    urls.push_back(rows[i].url());
  }

  StarURL(rows[0].url());

  // Delete the URLs and their dependencies.
  expirer_.DeleteURLs(urls, base::Time::Max());

  EnsureURLInfoGone(rows[0], false);
  EnsureURLInfoGone(rows[1], false);
  EnsureURLInfoGone(rows[2], false);
  EXPECT_TRUE(HasFavicon(favicon_ids[0]));
  EXPECT_TRUE(HasFavicon(favicon_ids[1]));
  EXPECT_FALSE(HasFavicon(favicon_ids[2]));
}

// Expires all URLs more recent than a given time, with no starred items.
// Our time threshold is such that one URL should be updated (we delete one of
// the two visits) and one is deleted.
TEST_F(ExpireHistoryTest, FlushRecentURLsUnstarred) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());

  // This should delete the last two visits.
  std::set<GURL> restrict_urls;
  expirer_.ExpireHistoryBetween(restrict_urls, kNoAppIdFilter, visit_times[2],
                                base::Time(),
                                /*user_initiated*/ true);
  EXPECT_EQ(GetLastDeletionInfo()->time_range().begin(), visit_times[2]);
  EXPECT_EQ(GetLastDeletionInfo()->time_range().end(), base::Time());
  EXPECT_EQ(DeletionInfo::Reason::kOther,
            GetLastDeletionInfo()->deletion_reason());
  EXPECT_THAT(GetLastDeletionInfo()->deleted_visit_ids(),
              UnorderedElementsAre(3, 4));

  // Verify that the middle URL had its last visit deleted only.
  visits.clear();
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  EXPECT_EQ(1U, visits.size());

  // Verify that the middle URL visit time and visit counts were updated.
  EXPECT_TRUE(ModifiedNotificationSentDueToUserAction(url_row1.url()));
  URLRow temp_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(visit_times[2] == url_row1.last_visit());  // Previous value.
  EXPECT_TRUE(visit_times[1] == temp_row.last_visit());  // New value.
  EXPECT_EQ(2, url_row1.visit_count());
  EXPECT_EQ(1, temp_row.visit_count());
  EXPECT_EQ(1, url_row1.typed_count());
  EXPECT_EQ(0, temp_row.typed_count());

  // Verify that the middle URL's favicon is still there.
  favicon_base::FaviconID favicon_id =
      GetFavicon(url_row1.url(), favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id));

  // Verify that the last URL was deleted.
  favicon_base::FaviconID favicon_id2 =
      GetFavicon(url_row2.url(), favicon_base::IconType::kFavicon);
  EnsureURLInfoGone(url_row2, false);
  EXPECT_FALSE(HasFavicon(favicon_id2));
}

// Expires all URLs visited between two given times, with no starred items.
TEST_F(ExpireHistoryTest, FlushURLsUnstarredBetweenTwoTimestamps) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row0, url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[0], &url_row0));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[0], &visits);
  ASSERT_EQ(1U, visits.size());
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  ASSERT_EQ(2U, visits.size());
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());

  // This should delete the two visits of the url_ids[1].
  std::set<GURL> restrict_urls;
  expirer_.ExpireHistoryBetween(restrict_urls, kNoAppIdFilter, visit_times[1],
                                visit_times[3],
                                /*user_initiated*/ true);

  main_db_->GetVisitsForURL(url_ids[0], &visits);
  EXPECT_EQ(1U, visits.size());
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  EXPECT_EQ(0U, visits.size());
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  EXPECT_EQ(1U, visits.size());

  // Verify that the url_ids[1] was deleted.
  favicon_base::FaviconID favicon_id1 =
      GetFavicon(url_row1.url(), favicon_base::IconType::kFavicon);
  EnsureURLInfoGone(url_row1, false);
  EXPECT_FALSE(HasFavicon(favicon_id1));

  // Verify that the url_ids[0]'s favicon are still there.
  favicon_base::FaviconID favicon_id0 =
      GetFavicon(url_row0.url(), favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id0));

  // Verify that the url_ids[2]'s favicon are still there.
  favicon_base::FaviconID favicon_id2 =
      GetFavicon(url_row2.url(), favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id2));
}

// Expires all URLs more recent than a given time, with no starred items.
// Same as FlushRecentURLsUnstarred test but with base::Time::Max() as end_time.
TEST_F(ExpireHistoryTest, FlushRecentURLsUnstarredWithMaxTime) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());

  // Use base::Time::Max() instead of base::Time().
  // This should delete the last two visits.
  std::set<GURL> restrict_urls;
  expirer_.ExpireHistoryBetween(restrict_urls, kNoAppIdFilter, visit_times[2],
                                base::Time::Max(),
                                /*user_initiated*/ true);

  // Verify that the middle URL had its last visit deleted only.
  visits.clear();
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  EXPECT_EQ(1U, visits.size());

  // Verify that the middle URL visit time and visit counts were updated.
  EXPECT_TRUE(ModifiedNotificationSentDueToUserAction(url_row1.url()));
  URLRow temp_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(visit_times[2] == url_row1.last_visit());  // Previous value.
  EXPECT_TRUE(visit_times[1] == temp_row.last_visit());  // New value.
  EXPECT_EQ(2, url_row1.visit_count());
  EXPECT_EQ(1, temp_row.visit_count());
  EXPECT_EQ(1, url_row1.typed_count());
  EXPECT_EQ(0, temp_row.typed_count());

  // Verify that the middle URL's favicon is still there.
  favicon_base::FaviconID favicon_id =
      GetFavicon(url_row1.url(), favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id));

  // Verify that the last URL was deleted.
  favicon_base::FaviconID favicon_id2 =
      GetFavicon(url_row2.url(), favicon_base::IconType::kFavicon);
  EnsureURLInfoGone(url_row2, false);
  EXPECT_FALSE(HasFavicon(favicon_id2));
}

// Expires all URLs with a given app ID, more recent than a given time, with no
// starred items.
TEST_F(ExpireHistoryTest, FlushRecentURLsWithAppIdUnstarredWithMaxTime) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times, /*set_app_id=*/true);

  URLRow url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());

  // Use base::Time::Max() instead of base::Time().
  // This should delete the 2nd visit (one with app id) of the last two visits.
  std::set<GURL> restrict_urls;
  expirer_.ExpireHistoryBetween(restrict_urls, kTestAppId, visit_times[2],
                                base::Time::Max(),
                                /*user_initiated*/ true);

  // Verify that the middle URL had its last visit deleted only.
  visits.clear();
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  EXPECT_EQ(1U, visits.size());

  // Verify that the middle URL visit time and visit counts were updated.
  EXPECT_TRUE(ModifiedNotificationSentDueToUserAction(url_row1.url()));
  URLRow temp_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(visit_times[2] == url_row1.last_visit());  // Previous value.
  EXPECT_TRUE(visit_times[1] == temp_row.last_visit());  // New value.
  EXPECT_EQ(2, url_row1.visit_count());
  EXPECT_EQ(1, temp_row.visit_count());
  EXPECT_EQ(1, url_row1.typed_count());
  EXPECT_EQ(0, temp_row.typed_count());

  // Verify that the middle URL's favicon is still there.
  favicon_base::FaviconID favicon_id =
      GetFavicon(url_row1.url(), favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id));

  // Verify that the last URL's favicon is still there.
  favicon_base::FaviconID favicon_id2 =
      GetFavicon(url_row2.url(), favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id2));
}

// Expires all URLs with no starred items.
TEST_F(ExpireHistoryTest, FlushAllURLsUnstarred) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());

  // This should delete all URL visits.
  std::set<GURL> restrict_urls;
  expirer_.ExpireHistoryBetween(restrict_urls, kNoAppIdFilter, base::Time(),
                                base::Time::Max(),
                                /*user_initiated*/ true);

  // Verify that all URL visits deleted.
  visits.clear();
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  EXPECT_EQ(0U, visits.size());
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  EXPECT_EQ(0U, visits.size());

  // Verify that all URLs were deleted.
  favicon_base::FaviconID favicon_id1 =
      GetFavicon(url_row1.url(), favicon_base::IconType::kFavicon);
  EnsureURLInfoGone(url_row1, false);
  EXPECT_FALSE(HasFavicon(favicon_id1));

  favicon_base::FaviconID favicon_id2 =
      GetFavicon(url_row2.url(), favicon_base::IconType::kFavicon);
  EnsureURLInfoGone(url_row2, false);
  EXPECT_FALSE(HasFavicon(favicon_id2));
}

// Expires all URLs with times in a given set.
TEST_F(ExpireHistoryTest, FlushURLsForTimes) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());

  // This should delete the last two visits.
  std::vector<base::Time> times;
  times.push_back(visit_times[3]);
  times.push_back(visit_times[2]);
  expirer_.ExpireHistoryForTimes(times);
  EXPECT_FALSE(GetLastDeletionInfo()->time_range().IsValid());
  EXPECT_EQ(DeletionInfo::Reason::kOther,
            GetLastDeletionInfo()->deletion_reason());
  EXPECT_THAT(GetLastDeletionInfo()->deleted_visit_ids(),
              UnorderedElementsAre(3, 4));

  // Verify that the middle URL had its last visit deleted only.
  visits.clear();
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  EXPECT_EQ(1U, visits.size());

  // Verify that the middle URL visit time and visit counts were updated.
  EXPECT_TRUE(ModifiedNotificationSentDueToUserAction(url_row1.url()));
  URLRow temp_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(visit_times[2] == url_row1.last_visit());  // Previous value.
  EXPECT_TRUE(visit_times[1] == temp_row.last_visit());  // New value.
  EXPECT_EQ(2, url_row1.visit_count());
  EXPECT_EQ(1, temp_row.visit_count());
  EXPECT_EQ(1, url_row1.typed_count());
  EXPECT_EQ(0, temp_row.typed_count());

  // Verify that the middle URL's favicon is still there.
  favicon_base::FaviconID favicon_id =
      GetFavicon(url_row1.url(), favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id));

  // Verify that the last URL was deleted.
  favicon_base::FaviconID favicon_id2 =
      GetFavicon(url_row2.url(), favicon_base::IconType::kFavicon);
  EnsureURLInfoGone(url_row2, false);
  EXPECT_FALSE(HasFavicon(favicon_id2));
}

// Expires only a specific URLs more recent than a given time, with no starred
// items.  Our time threshold is such that the URL should be updated (we delete
// one of the two visits).
TEST_F(ExpireHistoryTest, FlushRecentURLsUnstarredRestricted) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());

  // This should delete only visit 3, because of the URL restriction.
  std::set<GURL> restrict_urls = {url_row1.url()};
  expirer_.ExpireHistoryBetween(restrict_urls, kNoAppIdFilter, visit_times[2],
                                base::Time(),
                                /*user_initiated*/ true);
  EXPECT_EQ(GetLastDeletionInfo()->time_range().begin(), visit_times[2]);
  EXPECT_EQ(GetLastDeletionInfo()->time_range().end(), base::Time());
  EXPECT_EQ(GetLastDeletionInfo()->deleted_rows().size(), 0U);
  EXPECT_EQ(GetLastDeletionInfo()->restrict_urls()->size(), 1U);
  EXPECT_EQ(DeletionInfo::Reason::kOther,
            GetLastDeletionInfo()->deletion_reason());
  EXPECT_THAT(GetLastDeletionInfo()->deleted_visit_ids(),
              UnorderedElementsAre(3));

  // Verify that the middle URL had its last visit deleted only.
  visits.clear();
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  EXPECT_EQ(1U, visits.size());

  // Verify that the middle URL visit time and visit counts were updated.
  EXPECT_TRUE(ModifiedNotificationSentDueToUserAction(url_row1.url()));
  URLRow temp_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(visit_times[2] == url_row1.last_visit());  // Previous value.
  EXPECT_TRUE(visit_times[1] == temp_row.last_visit());  // New value.
  EXPECT_EQ(2, url_row1.visit_count());
  EXPECT_EQ(1, temp_row.visit_count());
  EXPECT_EQ(1, url_row1.typed_count());
  EXPECT_EQ(0, temp_row.typed_count());

  // Verify that the middle URL's favicon is still there.
  favicon_base::FaviconID favicon_id =
      GetFavicon(url_row1.url(), favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id));

  // Verify that the last URL was not touched.
  EXPECT_TRUE(main_db_->GetURLRow(url_ids[2], &temp_row));
  EXPECT_TRUE(HasFavicon(favicon_id));
}

// Expire a starred URL, it shouldn't get deleted
TEST_F(ExpireHistoryTest, FlushRecentURLsStarred) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  // Star the last two URLs.
  StarURL(url_row1.url());
  StarURL(url_row2.url());

  // This should delete the last two visits.
  std::set<GURL> restrict_urls;
  expirer_.ExpireHistoryBetween(restrict_urls, kNoAppIdFilter, visit_times[2],
                                base::Time(),
                                /*user_initiated*/ true);

  // The URL rows should still exist.
  URLRow new_url_row1, new_url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &new_url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &new_url_row2));

  // The visit times should be updated.
  EXPECT_TRUE(new_url_row1.last_visit() == visit_times[1]);
  EXPECT_TRUE(new_url_row2.last_visit().is_null());  // No last visit time.

  // Visit/typed count should be updated.
  EXPECT_TRUE(ModifiedNotificationSentDueToUserAction(url_row1.url()));
  EXPECT_TRUE(ModifiedNotificationSentDueToUserAction(url_row2.url()));
  EXPECT_EQ(0, new_url_row1.typed_count());
  EXPECT_EQ(1, new_url_row1.visit_count());
  EXPECT_EQ(0, new_url_row2.typed_count());
  EXPECT_EQ(0, new_url_row2.visit_count());

  // Favicons should still exist.
  favicon_base::FaviconID favicon_id =
      GetFavicon(url_row1.url(), favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id));
  favicon_id = GetFavicon(url_row1.url(), favicon_base::IconType::kFavicon);
  EXPECT_TRUE(HasFavicon(favicon_id));
}

TEST_F(ExpireHistoryTest, ExpireHistoryBetweenPropagatesUserInitiated) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);
  std::set<GURL> restrict_urls;

  expirer_.ExpireHistoryBetween(restrict_urls, kNoAppIdFilter, visit_times[3],
                                base::Time(),
                                /*user_initiated*/ true);
  EXPECT_FALSE(GetLastDeletionInfo()->is_from_expiration());
  EXPECT_EQ(DeletionInfo::Reason::kOther,
            GetLastDeletionInfo()->deletion_reason());
  EXPECT_THAT(GetLastDeletionInfo()->deleted_visit_ids(),
              UnorderedElementsAre(4));

  expirer_.ExpireHistoryBetween(restrict_urls, kNoAppIdFilter, visit_times[1],
                                base::Time(),
                                /*user_initiated*/ false);
  EXPECT_TRUE(GetLastDeletionInfo()->is_from_expiration());
  EXPECT_EQ(DeletionInfo::Reason::kOther,
            GetLastDeletionInfo()->deletion_reason());
  EXPECT_THAT(GetLastDeletionInfo()->deleted_visit_ids(),
              UnorderedElementsAre(2, 3));
}

TEST_F(ExpireHistoryTest, ExpireHistoryBeforeUnstarred) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row0, url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[0], &url_row0));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  // Expire the oldest two visits.
  expirer_.ExpireHistoryBeforeForTesting(visit_times[1]);

  // The first URL should be deleted along with its sole visit. The second URL
  // itself should not be affected, as there is still one more visit to it, but
  // its first visit should be deleted.
  URLRow temp_row;
  EnsureURLInfoGone(url_row0, true);
  EXPECT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(ModifiedNotificationSentDueToExpiry(url_row1.url()));
  VisitVector visits;
  main_db_->GetVisitsForURL(temp_row.id(), &visits);
  EXPECT_EQ(1U, visits.size());
  EXPECT_EQ(visit_times[2], visits[0].visit_time);
  EXPECT_TRUE(main_db_->GetURLRow(url_ids[2], &temp_row));

  // Now expire one more visit so that the second URL should be removed. The
  // third URL and its visit should be intact.
  ClearLastNotifications();
  expirer_.ExpireHistoryBeforeForTesting(visit_times[2]);
  EnsureURLInfoGone(url_row1, true);
  EXPECT_TRUE(main_db_->GetURLRow(url_ids[2], &temp_row));
  main_db_->GetVisitsForURL(temp_row.id(), &visits);
  EXPECT_EQ(1U, visits.size());
}

TEST_F(ExpireHistoryTest, ExpireHistoryBeforeStarred) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row0, url_row1;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[0], &url_row0));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));

  // Star the URLs.
  StarURL(url_row0.url());
  StarURL(url_row1.url());

  // Now expire the first three visits (first two URLs). The first three visits
  // should be deleted, but the URL records themselves should not, as they are
  // starred.
  expirer_.ExpireHistoryBeforeForTesting(visit_times[2]);

  URLRow temp_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[0], &temp_row));
  EXPECT_TRUE(ModifiedNotificationSentDueToExpiry(url_row0.url()));
  VisitVector visits;
  main_db_->GetVisitsForURL(temp_row.id(), &visits);
  EXPECT_EQ(0U, visits.size());

  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(ModifiedNotificationSentDueToExpiry(url_row1.url()));
  main_db_->GetVisitsForURL(temp_row.id(), &visits);
  EXPECT_EQ(0U, visits.size());

  // The third URL should be unchanged.
  EXPECT_TRUE(main_db_->GetURLRow(url_ids[2], &temp_row));
  EXPECT_FALSE(ModifiedNotificationSentDueToExpiry(temp_row.url()));
  main_db_->GetVisitsForURL(temp_row.id(), &visits);
  EXPECT_EQ(1U, visits.size());
}

// Tests the return values from ExpireSomeOldHistory. The rest of the
// functionality of this function is tested by the ExpireHistoryBefore*
// tests which use this function internally.
TEST_F(ExpireHistoryTest, ExpireSomeOldHistory) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);
  const ExpiringVisitsReader* reader = expirer_.GetAllVisitsReader();

  // Deleting a time range with no URLs should return false (nothing found).
  EXPECT_FALSE(expirer_.ExpireSomeOldHistory(visit_times[0] - base::Days(100),
                                             reader, 1));
  EXPECT_EQ(nullptr, GetLastDeletionInfo());

  // Deleting a time range with not up the the max results should also return
  // false (there will only be one visit deleted in this range).
  EXPECT_FALSE(expirer_.ExpireSomeOldHistory(visit_times[0], reader, 2));
  EXPECT_EQ(1U, GetLastDeletionInfo()->deleted_rows().size());
  EXPECT_FALSE(GetLastDeletionInfo()->time_range().IsValid());
  EXPECT_EQ(DeletionInfo::Reason::kOther,
            GetLastDeletionInfo()->deletion_reason());
  EXPECT_THAT(GetLastDeletionInfo()->deleted_visit_ids(),
              UnorderedElementsAre(1));
  ClearLastNotifications();

  // Deleting a time range with the max number of results should return true
  // (max deleted).
  EXPECT_TRUE(expirer_.ExpireSomeOldHistory(visit_times[2], reader, 1));
  ASSERT_TRUE(GetLastDeletionInfo());
  EXPECT_THAT(GetLastDeletionInfo()->deleted_visit_ids(),
              UnorderedElementsAre(2));
}

TEST_F(ExpireHistoryTest, ExpiringVisitsReader) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  const ExpiringVisitsReader* all = expirer_.GetAllVisitsReader();
  const ExpiringVisitsReader* auto_subframes =
      expirer_.GetAutoSubframeVisitsReader();

  VisitVector visits;
  base::Time now = PretendNow();

  // Verify that the early expiration threshold, stored in the meta table is
  // initialized.
  EXPECT_TRUE(main_db_->GetEarlyExpirationThreshold() ==
              base::Time::FromInternalValue(1L));

  // First, attempt reading AUTO_SUBFRAME visits. We should get none.
  EXPECT_FALSE(auto_subframes->Read(now, main_db_.get(), &visits, 1));
  EXPECT_EQ(0U, visits.size());

  // Verify that the early expiration threshold was updated, since there are no
  // AUTO_SUBFRAME visits in the given time range.
  EXPECT_TRUE(now <= main_db_->GetEarlyExpirationThreshold());

  // Now, read all visits and verify that there's at least one.
  EXPECT_TRUE(all->Read(now, main_db_.get(), &visits, 1));
  EXPECT_EQ(1U, visits.size());
}

// Test that ClearOldOnDemandFaviconsIfPossible() deletes favicons associated
// only to unstarred page URLs.
TEST_F(ExpireHistoryTest, ClearOldOnDemandFaviconsDoesDeleteUnstarred) {
  // The blob does not encode any real bitmap, obviously.
  const unsigned char kBlob[] = "0";
  scoped_refptr<base::RefCountedBytes> favicon(
      new base::RefCountedBytes(kBlob));

  // Icon: old and not bookmarked case.
  GURL url("http://google.com/favicon.ico");
  favicon_base::FaviconID icon_id = thumb_db_->AddFavicon(
      url, favicon_base::IconType::kFavicon, favicon,
      favicon::FaviconBitmapType::ON_DEMAND,
      GetOldFaviconThreshold() - base::Seconds(1), gfx::Size());
  ASSERT_NE(0, icon_id);
  GURL page_url("http://google.com/");
  ASSERT_NE(0, thumb_db_->AddIconMapping(page_url, icon_id));

  expirer_.ClearOldOnDemandFaviconsIfPossible(GetOldFaviconThreshold());

  // The icon gets deleted.
  EXPECT_FALSE(thumb_db_->GetIconMappingsForPageURL(page_url, nullptr));
  EXPECT_FALSE(thumb_db_->GetFaviconHeader(icon_id, nullptr, nullptr));
  EXPECT_FALSE(thumb_db_->GetFaviconBitmaps(icon_id, nullptr));
}

// Test that ClearOldOnDemandFaviconsIfPossible() deletes favicons associated to
// at least one starred page URL.
TEST_F(ExpireHistoryTest, ClearOldOnDemandFaviconsDoesNotDeleteStarred) {
  // The blob does not encode any real bitmap, obviously.
  const unsigned char kBlob[] = "0";
  scoped_refptr<base::RefCountedBytes> favicon(
      new base::RefCountedBytes(kBlob));

  // Icon: old but bookmarked case.
  GURL url("http://google.com/favicon.ico");
  favicon_base::FaviconID icon_id = thumb_db_->AddFavicon(
      url, favicon_base::IconType::kFavicon, favicon,
      favicon::FaviconBitmapType::ON_DEMAND,
      GetOldFaviconThreshold() - base::Seconds(1), gfx::Size());
  ASSERT_NE(0, icon_id);
  GURL page_url1("http://google.com/1");
  ASSERT_NE(0, thumb_db_->AddIconMapping(page_url1, icon_id));
  StarURL(page_url1);
  GURL page_url2("http://google.com/2");
  ASSERT_NE(0, thumb_db_->AddIconMapping(page_url2, icon_id));

  expirer_.ClearOldOnDemandFaviconsIfPossible(GetOldFaviconThreshold());

  // Nothing gets deleted.
  EXPECT_TRUE(thumb_db_->GetFaviconHeader(icon_id, nullptr, nullptr));
  std::vector<favicon::FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(thumb_db_->GetFaviconBitmaps(icon_id, &favicon_bitmaps));
  EXPECT_EQ(1u, favicon_bitmaps.size());
  std::vector<favicon::IconMapping> icon_mapping;
  EXPECT_TRUE(thumb_db_->GetIconMappingsForPageURL(page_url1, &icon_mapping));
  EXPECT_TRUE(thumb_db_->GetIconMappingsForPageURL(page_url2, &icon_mapping));
  EXPECT_EQ(2u, icon_mapping.size());
  EXPECT_EQ(icon_id, icon_mapping[0].icon_id);
  EXPECT_EQ(icon_id, icon_mapping[1].icon_id);
}

// Test that ClearOldOnDemandFaviconsIfPossible() has effect if the last
// clearing was long time age (such as 2 days ago).
TEST_F(ExpireHistoryTest, ClearOldOnDemandFaviconsDoesDeleteAfterLongDelay) {
  // Previous clearing (2 days ago).
  expirer_.ClearOldOnDemandFaviconsIfPossible(GetOldFaviconThreshold() -
                                              base::Days(2));

  // The blob does not encode any real bitmap, obviously.
  const unsigned char kBlob[] = "0";
  scoped_refptr<base::RefCountedBytes> favicon(
      new base::RefCountedBytes(kBlob));

  // Icon: old and not bookmarked case.
  GURL url("http://google.com/favicon.ico");
  favicon_base::FaviconID icon_id = thumb_db_->AddFavicon(
      url, favicon_base::IconType::kFavicon, favicon,
      favicon::FaviconBitmapType::ON_DEMAND,
      GetOldFaviconThreshold() - base::Seconds(1), gfx::Size());
  ASSERT_NE(0, icon_id);
  GURL page_url("http://google.com/");
  ASSERT_NE(0, thumb_db_->AddIconMapping(page_url, icon_id));

  expirer_.ClearOldOnDemandFaviconsIfPossible(GetOldFaviconThreshold());

  // The icon gets deleted.
  EXPECT_FALSE(thumb_db_->GetIconMappingsForPageURL(page_url, nullptr));
  EXPECT_FALSE(thumb_db_->GetFaviconHeader(icon_id, nullptr, nullptr));
  EXPECT_FALSE(thumb_db_->GetFaviconBitmaps(icon_id, nullptr));
}

// Test that ClearOldOnDemandFaviconsIfPossible() deletes favicons associated to
// at least one starred page URL.
TEST_F(ExpireHistoryTest,
       ClearOldOnDemandFaviconsDoesNotDeleteAfterShortDelay) {
  // Previous clearing (5 minutes ago).
  expirer_.ClearOldOnDemandFaviconsIfPossible(GetOldFaviconThreshold() -
                                              base::Minutes(5));

  // The blob does not encode any real bitmap, obviously.
  const unsigned char kBlob[] = "0";
  scoped_refptr<base::RefCountedBytes> favicon(
      new base::RefCountedBytes(kBlob));

  // Icon: old but bookmarked case.
  GURL url("http://google.com/favicon.ico");
  favicon_base::FaviconID icon_id = thumb_db_->AddFavicon(
      url, favicon_base::IconType::kFavicon, favicon,
      favicon::FaviconBitmapType::ON_DEMAND,
      GetOldFaviconThreshold() - base::Seconds(1), gfx::Size());
  ASSERT_NE(0, icon_id);
  GURL page_url1("http://google.com/1");
  ASSERT_NE(0, thumb_db_->AddIconMapping(page_url1, icon_id));
  GURL page_url2("http://google.com/2");
  ASSERT_NE(0, thumb_db_->AddIconMapping(page_url2, icon_id));

  expirer_.ClearOldOnDemandFaviconsIfPossible(GetOldFaviconThreshold());

  // Nothing gets deleted.
  EXPECT_TRUE(thumb_db_->GetFaviconHeader(icon_id, nullptr, nullptr));
  std::vector<favicon::FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(thumb_db_->GetFaviconBitmaps(icon_id, &favicon_bitmaps));
  EXPECT_EQ(1u, favicon_bitmaps.size());
  std::vector<favicon::IconMapping> icon_mapping;
  EXPECT_TRUE(thumb_db_->GetIconMappingsForPageURL(page_url1, &icon_mapping));
  EXPECT_TRUE(thumb_db_->GetIconMappingsForPageURL(page_url2, &icon_mapping));
  EXPECT_EQ(2u, icon_mapping.size());
  EXPECT_EQ(icon_id, icon_mapping[0].icon_id);
  EXPECT_EQ(icon_id, icon_mapping[1].icon_id);
}

// Test that all visits that are redirect parents of specified visits are also
// removed. See crbug.com/786878.
TEST_F(ExpireHistoryTest, DeleteVisitAndRedirects) {
  // Set up the example data.
  base::Time now = PretendNow();
  URLRow url_row1(GURL("http://google.com/1"));
  url_row1.set_last_visit(now - base::Days(1));
  url_row1.set_visit_count(1);
  URLID url1 = main_db_->AddURL(url_row1);

  URLRow url_row2(GURL("http://www.google.com/1"));
  url_row2.set_last_visit(now);
  url_row2.set_visit_count(1);
  URLID url2 = main_db_->AddURL(url_row2);

  // Add a visit to "http://google.com/1" that is redirected to
  // "http://www.google.com/1".
  VisitRow visit_row1;
  visit_row1.url_id = url1;
  visit_row1.visit_time = now - base::Days(1);
  visit_row1.transition = ui::PAGE_TRANSITION_CHAIN_START;

  main_db_->AddVisit(&visit_row1, SOURCE_BROWSED);

  VisitRow visit_row2;
  visit_row2.url_id = url2;
  visit_row2.visit_time = now;
  visit_row2.referring_visit = visit_row1.visit_id;
  visit_row1.transition = ui::PAGE_TRANSITION_CHAIN_END;
  main_db_->AddVisit(&visit_row2, SOURCE_BROWSED);

  // Expiring visit_row2 should also expire visit_row1 which is its redirect
  // parent.
  expirer_.ExpireVisits({visit_row2}, DeletionInfo::Reason::kOther);

  VisitRow v;
  EXPECT_FALSE(main_db_->GetRowForVisit(visit_row1.visit_id, &v));
  EXPECT_FALSE(main_db_->GetRowForVisit(visit_row2.visit_id, &v));
  URLRow u;
  EXPECT_FALSE(main_db_->GetURLRow(url1, &u));
  EXPECT_FALSE(main_db_->GetURLRow(url2, &u));
}

// Test that loops in redirect parents are handled. See crbug.com/798234.
TEST_F(ExpireHistoryTest, DeleteVisitAndRedirectsWithLoop) {
  // Set up the example data.
  base::Time now = PretendNow();
  URLRow url_row1(GURL("http://google.com/1"));
  url_row1.set_last_visit(now - base::Days(1));
  url_row1.set_visit_count(1);
  URLID url1 = main_db_->AddURL(url_row1);

  URLRow url_row2(GURL("http://www.google.com/1"));
  url_row2.set_last_visit(now);
  url_row2.set_visit_count(1);
  URLID url2 = main_db_->AddURL(url_row2);

  // Add a visit to "http://google.com/1" that is redirected to
  // "http://www.google.com/1".
  VisitRow visit_row1;
  visit_row1.url_id = url1;
  visit_row1.visit_time = now - base::Days(1);
  visit_row1.transition = ui::PAGE_TRANSITION_CHAIN_START;
  main_db_->AddVisit(&visit_row1, SOURCE_BROWSED);

  VisitRow visit_row2;
  visit_row2.url_id = url2;
  visit_row2.visit_time = now;
  visit_row2.referring_visit = visit_row1.visit_id;
  visit_row1.transition = ui::PAGE_TRANSITION_CHAIN_END;
  main_db_->AddVisit(&visit_row2, SOURCE_BROWSED);

  // Set the first visit to be redirect parented to the second visit.
  visit_row1.referring_visit = visit_row2.visit_id;
  main_db_->UpdateVisitRow(visit_row1);

  // Expiring visit_row2 should also expire visit_row1 which is its redirect
  // parent, without infinite looping.
  expirer_.ExpireVisits({visit_row2}, DeletionInfo::Reason::kOther);

  VisitRow v;
  EXPECT_FALSE(main_db_->GetRowForVisit(visit_row1.visit_id, &v));
  EXPECT_FALSE(main_db_->GetRowForVisit(visit_row2.visit_id, &v));
  URLRow u;
  EXPECT_FALSE(main_db_->GetURLRow(url1, &u));
  EXPECT_FALSE(main_db_->GetURLRow(url2, &u));
}

// Test that visits that are referers but not part of a redirect chain don't
// get deleted. See crbug.com/919488.
TEST_F(ExpireHistoryTest, DeleteVisitButNotActualReferers) {
  // Set up the example data.
  base::Time now = PretendNow();
  URLRow url_row1(GURL("http://google.com/1"));
  url_row1.set_last_visit(now - base::Days(1));
  url_row1.set_visit_count(1);
  URLID url1 = main_db_->AddURL(url_row1);

  URLRow url_row2(GURL("http://www.google.com/1"));
  url_row2.set_last_visit(now);
  url_row2.set_visit_count(1);
  URLID url2 = main_db_->AddURL(url_row2);

  // Add a visit to "http://google.com/1" that is a referer to
  // "http://www.google.com/1". But both are separate redirect chains.
  VisitRow visit_row1;
  visit_row1.url_id = url1;
  visit_row1.visit_time = now - base::Days(1);
  visit_row1.transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_CHAIN_START | ui::PAGE_TRANSITION_CHAIN_END);
  main_db_->AddVisit(&visit_row1, SOURCE_BROWSED);

  VisitRow visit_row2;
  visit_row2.url_id = url2;
  visit_row2.visit_time = now;
  visit_row2.referring_visit = visit_row1.visit_id;
  visit_row2.transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_CHAIN_START | ui::PAGE_TRANSITION_CHAIN_END);
  main_db_->AddVisit(&visit_row2, SOURCE_BROWSED);

  // Expiring visit_row2 should not expire visit_row1 which is its referer
  // parent.
  expirer_.ExpireVisits({visit_row2}, DeletionInfo::Reason::kOther);

  VisitRow v;
  EXPECT_TRUE(main_db_->GetRowForVisit(visit_row1.visit_id, &v));
  EXPECT_FALSE(main_db_->GetRowForVisit(visit_row2.visit_id, &v));
  URLRow u;
  EXPECT_TRUE(main_db_->GetURLRow(url1, &u));
  EXPECT_FALSE(main_db_->GetURLRow(url2, &u));
}

TEST_F(ExpireHistoryTest, DeleteVisitsWithoutDeletingURLs) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row1;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  ASSERT_EQ(2U, visits.size());

  // This should delete just one visit from url_row1, but leave the URL alone,
  // because it still has one other visit remaining.
  expirer_.ExpireHistoryForTimes({visit_times[2]});

  ASSERT_TRUE(GetLastDeletionInfo())
      << "A deletion notification should have been issued.";
  EXPECT_FALSE(GetLastDeletionInfo()->time_range().IsValid());
  EXPECT_EQ(DeletionInfo::Reason::kOther,
            GetLastDeletionInfo()->deletion_reason());
  EXPECT_THAT(GetLastDeletionInfo()->deleted_visit_ids(),
              UnorderedElementsAre(3));

  EXPECT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1))
      << "URL should still exist";
}

// TODO(brettw) add some visits with no URL to make sure everything is updated
// properly. Have the visits also refer to nonexistent FTS rows.
//
// Maybe also refer to invalid favicons.

}  // namespace history
