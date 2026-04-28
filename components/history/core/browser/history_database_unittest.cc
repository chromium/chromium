// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_database.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/database_test_utils.h"
#include "components/history/core/test/test_history_database.h"
#include "sql/init_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history {

TEST(HistoryDatabaseTest, DropBookmarks) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;

  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.GetPath().AppendASCII("DropBookmarks.db");
  sql::Database::Delete(db_file);

  // Copy db file over that contains starred URLs.
  base::FilePath old_history_path;
  EXPECT_TRUE(GetTestDataHistoryDir(&old_history_path));
  old_history_path =
      old_history_path.Append(FILE_PATH_LITERAL("History_with_starred"));
  base::CopyFile(old_history_path, db_file);

  // Load the DB twice. The first time it should migrate. Make sure that the
  // migration leaves it in a state fit to load again later.
  for (int i = 0; i < 2; ++i) {
    TestHistoryDatabase history_db;
    ASSERT_EQ(sql::INIT_OK, history_db.Init(db_file));
    HistoryDatabase::URLEnumerator url_enumerator;
    ASSERT_TRUE(history_db.InitURLEnumeratorForEverything(&url_enumerator));
    int num_urls = 0;
    URLRow url_row;
    while (url_enumerator.GetNextURL(&url_row)) {
      ++num_urls;
    }
    ASSERT_EQ(5, num_urls);
  }
}

class VisitedLinkWithUrlEnumeratorTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_file_ = temp_dir_.GetPath().AppendASCII("VisitedLinkWithUrlTest.db");
    sql::Database::Delete(db_file_);
    ASSERT_EQ(sql::INIT_OK, db_.Init(db_file_));
  }

  void TearDown() override {}

  URLID AddTestUrl(const GURL& url) {
    URLRow url_row(url);
    url_row.set_title(u"Test");
    url_row.set_visit_count(1);
    url_row.set_typed_count(0);
    url_row.set_last_visit(base::Time::Now());
    url_row.set_hidden(false);
    return db_.AddURL(url_row);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath db_file_;
  TestHistoryDatabase db_;
};

TEST_F(VisitedLinkWithUrlEnumeratorTest, BasicJoinedEnumeration) {
  // Add URLs to the url table.
  const GURL link_url1("http://www.example.com/page1");
  const GURL link_url2("http://www.example.com/page2");
  URLID url_id1 = AddTestUrl(link_url1);
  URLID url_id2 = AddTestUrl(link_url2);
  ASSERT_TRUE(url_id1);
  ASSERT_TRUE(url_id2);

  // Add visited link rows referencing those URLs.
  const GURL top_level("http://www.top.com/");
  const GURL frame("http://www.frame.com/");
  ASSERT_TRUE(db_.AddVisitedLink(url_id1, top_level, frame, 3));
  ASSERT_TRUE(db_.AddVisitedLink(url_id2, top_level, frame, 5));

  // Enumerate using the joined enumerator.
  HistoryDatabase::VisitedLinkWithUrlEnumerator iter;
  ASSERT_TRUE(db_.InitVisitedLinkWithUrlEnumeratorForEverything(iter));

  int count = 0;
  VisitedLinkRow row;
  GURL link_url;
  while (iter.GetNextVisitedLink(row, link_url)) {
    EXPECT_TRUE(link_url.is_valid());
    EXPECT_TRUE(link_url == link_url1 || link_url == link_url2);
    EXPECT_EQ(row.link_url_id, link_url == link_url1 ? url_id1 : url_id2);
    count++;
  }
  EXPECT_EQ(2, count);
}

TEST_F(VisitedLinkWithUrlEnumeratorTest, OrphanedVisitedLinkSkipped) {
  // Add a URL and a visited link referencing it.
  const GURL link_url("http://www.example.com/exists");
  URLID url_id = AddTestUrl(link_url);
  ASSERT_TRUE(url_id);

  const GURL top_level("http://www.top.com/");
  const GURL frame("http://www.frame.com/");
  ASSERT_TRUE(db_.AddVisitedLink(url_id, top_level, frame, 1));

  // Add a visited link with a link_url_id that doesn't exist in the urls table.
  URLID orphaned_id = url_id + 9999;
  ASSERT_TRUE(db_.AddVisitedLink(orphaned_id, top_level, frame, 1));

  // The INNER JOIN should skip the orphaned row.
  HistoryDatabase::VisitedLinkWithUrlEnumerator iter;
  ASSERT_TRUE(db_.InitVisitedLinkWithUrlEnumeratorForEverything(iter));

  int count = 0;
  VisitedLinkRow row;
  GURL url;
  while (iter.GetNextVisitedLink(row, url)) {
    EXPECT_EQ(url, link_url);
    count++;
  }
  EXPECT_EQ(1, count);
}

TEST_F(VisitedLinkWithUrlEnumeratorTest, EmptyTable) {
  // No visited links added - enumerator should return no results.
  HistoryDatabase::VisitedLinkWithUrlEnumerator iter;
  ASSERT_TRUE(db_.InitVisitedLinkWithUrlEnumeratorForEverything(iter));

  VisitedLinkRow row;
  GURL url;
  EXPECT_FALSE(iter.GetNextVisitedLink(row, url));
}

// Tests for GetBatchRecentVisitsForSignificantURLs ----------------------------

class BatchRecentVisitsTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_file_ = temp_dir_.GetPath().AppendASCII("BatchRecentVisitsTest.db");
    sql::Database::Delete(db_file_);
    ASSERT_EQ(sql::INIT_OK, db_.Init(db_file_));
  }

  // Adds a URL to the urls table and returns its URLID.
  URLID AddTestUrl(const GURL& url,
                   int visit_count = 5,
                   int typed_count = 1,
                   bool hidden = false) {
    URLRow url_row(url);
    url_row.set_title(u"Test");
    url_row.set_visit_count(visit_count);
    url_row.set_typed_count(typed_count);
    url_row.set_last_visit(base::Time::Now());
    url_row.set_hidden(hidden);
    return db_.AddURL(url_row);
  }

  // Adds a visit to the visits table for the given URL.
  VisitID AddTestVisit(
      URLID url_id,
      base::Time visit_time,
      ui::PageTransition transition = ui::PAGE_TRANSITION_LINK) {
    VisitRow visit(url_id, visit_time, /*arg_referring_visit=*/0, transition,
                   /*arg_segment_id=*/0,
                   /*arg_incremented_omnibox_typed_score=*/false,
                   /*arg_opener_visit=*/0);
    visit.source = SOURCE_BROWSED;
    EXPECT_TRUE(db_.AddVisit(&visit));
    return visit.visit_id;
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath db_file_;
  TestHistoryDatabase db_;
};

TEST_F(BatchRecentVisitsTest, ReturnsVisitsForSignificantURLs) {
  // Add a significant URL (visit_count >= 4, typed_count >= 1).
  const GURL url1("http://www.example.com/");
  URLID url_id1 = AddTestUrl(url1, /*visit_count=*/5, /*typed_count=*/1);
  ASSERT_TRUE(url_id1);

  // Add 3 visits for this URL at different times.
  base::Time now = base::Time::Now();
  AddTestVisit(url_id1, now - base::Hours(3), ui::PAGE_TRANSITION_TYPED);
  AddTestVisit(url_id1, now - base::Hours(2), ui::PAGE_TRANSITION_LINK);
  AddTestVisit(url_id1, now - base::Hours(1), ui::PAGE_TRANSITION_TYPED);

  auto result =
      db_.GetBatchRecentVisitsForSignificantURLs(/*max_visits_per_url=*/10);

  // Should have visits for url_id1.
  ASSERT_TRUE(result.contains(url_id1));
  EXPECT_EQ(3u, result[url_id1].size());
  // Visits should be ordered by visit_time descending (most recent first).
  EXPECT_GE(result[url_id1][0].first, result[url_id1][1].first);
  EXPECT_GE(result[url_id1][1].first, result[url_id1][2].first);
}

TEST_F(BatchRecentVisitsTest, ExcludesHiddenURLs) {
  // Add a hidden URL — should not appear in results.
  const GURL hidden_url("http://www.hidden.com/");
  URLID hidden_id =
      AddTestUrl(hidden_url, /*visit_count=*/10, /*typed_count=*/5,
                 /*hidden=*/true);
  ASSERT_TRUE(hidden_id);
  AddTestVisit(hidden_id, base::Time::Now());

  auto result =
      db_.GetBatchRecentVisitsForSignificantURLs(/*max_visits_per_url=*/10);
  EXPECT_FALSE(result.contains(hidden_id));
}

TEST_F(BatchRecentVisitsTest, ExcludesInsignificantURLs) {
  // Add a URL that doesn't meet significance thresholds (visit_count < 4,
  // typed_count < 1, and last_visit too old).
  const GURL url("http://www.insignificant.com/");
  URLRow url_row(url);
  url_row.set_title(u"Insignificant");
  url_row.set_visit_count(1);
  url_row.set_typed_count(0);
  url_row.set_last_visit(base::Time::Now() - base::Days(30));
  url_row.set_hidden(false);
  URLID url_id = db_.AddURL(url_row);
  ASSERT_TRUE(url_id);
  AddTestVisit(url_id, base::Time::Now() - base::Days(30));

  auto result =
      db_.GetBatchRecentVisitsForSignificantURLs(/*max_visits_per_url=*/10);
  EXPECT_FALSE(result.contains(url_id));
}

TEST_F(BatchRecentVisitsTest, RespectsMaxVisitsPerUrl) {
  const GURL url("http://www.example.com/");
  URLID url_id = AddTestUrl(url);
  ASSERT_TRUE(url_id);

  // Add 5 visits.
  base::Time now = base::Time::Now();
  for (int i = 0; i < 5; i++) {
    AddTestVisit(url_id, now - base::Hours(i));
  }

  // Request only 2 visits per URL.
  auto result =
      db_.GetBatchRecentVisitsForSignificantURLs(/*max_visits_per_url=*/2);
  ASSERT_TRUE(result.contains(url_id));
  EXPECT_EQ(2u, result[url_id].size());
}

TEST_F(BatchRecentVisitsTest, MultipleURLsWithVisits) {
  const GURL url1("http://www.one.com/");
  const GURL url2("http://www.two.com/");
  URLID id1 = AddTestUrl(url1);
  URLID id2 = AddTestUrl(url2);
  ASSERT_TRUE(id1);
  ASSERT_TRUE(id2);

  base::Time now = base::Time::Now();
  AddTestVisit(id1, now - base::Hours(1));
  AddTestVisit(id1, now - base::Hours(2));
  AddTestVisit(id2, now - base::Hours(3));

  auto result =
      db_.GetBatchRecentVisitsForSignificantURLs(/*max_visits_per_url=*/10);
  ASSERT_TRUE(result.contains(id1));
  ASSERT_TRUE(result.contains(id2));
  EXPECT_EQ(2u, result[id1].size());
  EXPECT_EQ(1u, result[id2].size());
}

TEST_F(BatchRecentVisitsTest, EmptyDatabase) {
  auto result =
      db_.GetBatchRecentVisitsForSignificantURLs(/*max_visits_per_url=*/10);
  EXPECT_TRUE(result.empty());
}

}  // namespace history
