// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/url_database.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "sql/database.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace history {

namespace {

bool IsURLRowEqual(const URLRow& a,
                   const URLRow& b) {
  // TODO(brettw) when the database stores an actual Time value rather than
  // a time_t, do a reaul comparison. Instead, we have to do a more rough
  // comparison since the conversion reduces the precision.
  return a.title() == b.title() &&
      a.visit_count() == b.visit_count() &&
      a.typed_count() == b.typed_count() &&
      a.last_visit() - b.last_visit() <= TimeDelta::FromSeconds(1) &&
      a.hidden() == b.hidden();
}

}  // namespace

class URLDatabaseTest : public testing::Test,
                        public URLDatabase {
 public:
  URLDatabaseTest() {
  }

  void CreateVersion33URLTable() {
    EXPECT_TRUE(GetDB().Execute("DROP TABLE urls"));

    std::string sql;
    // create a version 33 urls table
    sql.append(
        "CREATE TABLE urls ("
        "id INTEGER PRIMARY KEY,"
        "url LONGVARCHAR,"
        "title LONGVARCHAR,"
        "visit_count INTEGER DEFAULT 0 NOT NULL,"
        "typed_count INTEGER DEFAULT 0 NOT NULL,"
        "last_visit_time INTEGER NOT NULL,"
        "hidden INTEGER DEFAULT 0 NOT NULL,"
        "favicon_id INTEGER DEFAULT 0 NOT NULL)");  // favicon_id is not used
                                                    // now.
    EXPECT_TRUE(GetDB().Execute(sql.c_str()));
  }

 protected:
  // Provided for URL/VisitDatabase.
  sql::Database& GetDB() override { return db_; }

 private:
  // Test setup.
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath db_file = temp_dir_.GetPath().AppendASCII("URLTest.db");

    EXPECT_TRUE(db_.Open(db_file));

    // Initialize the tables for this test.
    CreateURLTable(false);
    CreateMainURLIndex();
    InitKeywordSearchTermsTable();
    CreateKeywordSearchTermsIndices();
  }
  void TearDown() override { db_.Close(); }

  base::ScopedTempDir temp_dir_;
  sql::Database db_;
};

// Test add, update, upsert, and query for the URL table in the HistoryDatabase.
TEST_F(URLDatabaseTest, AddAndUpdateURL) {
  // First, add two URLs.
  const GURL url1("http://www.google.com/");
  URLRow url_info1(url1);
  url_info1.set_title(base::UTF8ToUTF16("Google"));
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info1.set_hidden(false);
  URLID id1_initially = AddURL(url_info1);
  EXPECT_TRUE(id1_initially);

  const GURL url2("http://mail.google.com/");
  URLRow url_info2(url2);
  url_info2.set_title(base::UTF8ToUTF16("Google Mail"));
  url_info2.set_visit_count(3);
  url_info2.set_typed_count(0);
  url_info2.set_last_visit(Time::Now() - TimeDelta::FromDays(2));
  url_info2.set_hidden(true);
  EXPECT_TRUE(AddURL(url_info2));

  // Query both of them.
  URLRow info;
  EXPECT_TRUE(GetRowForURL(url1, &info));
  EXPECT_TRUE(IsURLRowEqual(url_info1, info));
  URLID id2 = GetRowForURL(url2, &info);
  EXPECT_TRUE(id2);
  EXPECT_TRUE(IsURLRowEqual(url_info2, info));

  // Update the second.
  url_info2.set_title(base::UTF8ToUTF16("Google Mail Too"));
  url_info2.set_visit_count(4);
  url_info2.set_typed_count(1);
  url_info2.set_typed_count(91011);
  url_info2.set_hidden(false);
  EXPECT_TRUE(UpdateURLRow(id2, url_info2));

  // Make sure it got updated.
  URLRow info2;
  EXPECT_TRUE(GetRowForURL(url2, &info2));
  EXPECT_TRUE(IsURLRowEqual(url_info2, info2));

  // Try updating a non-existing row. This should fail and have no effects.
  const GURL url3("http://youtube.com/");
  URLRow url_info3(url3);
  url_info3.set_id(42);
  EXPECT_FALSE(UpdateURLRow(url_info3.id(), url_info3));
  EXPECT_EQ(0, GetRowForURL(url3, &info));

  // Update an existing URL and insert a new one using the upsert operation.
  url_info1.set_id(id1_initially);
  url_info1.set_title(base::UTF8ToUTF16("Google Again!"));
  url_info1.set_visit_count(5);
  url_info1.set_typed_count(3);
  url_info1.set_last_visit(Time::Now());
  url_info1.set_hidden(true);
  EXPECT_TRUE(InsertOrUpdateURLRowByID(url_info1));

  const GURL url4("http://maps.google.com/");
  URLRow url_info4(url4);
  url_info4.set_id(43);
  url_info4.set_title(base::UTF8ToUTF16("Google Maps"));
  url_info4.set_visit_count(7);
  url_info4.set_typed_count(6);
  url_info4.set_last_visit(Time::Now() - TimeDelta::FromDays(3));
  url_info4.set_hidden(false);
  EXPECT_TRUE(InsertOrUpdateURLRowByID(url_info4));

  // Query both of these as well.
  URLID id1 = GetRowForURL(url1, &info);
  EXPECT_EQ(id1_initially, id1);
  EXPECT_TRUE(IsURLRowEqual(url_info1, info));
  URLID id4 = GetRowForURL(url4, &info);
  EXPECT_EQ(43, id4);
  EXPECT_TRUE(IsURLRowEqual(url_info4, info));

  // Query a nonexistent URL.
  EXPECT_EQ(0, GetRowForURL(GURL("http://news.google.com/"), &info));

  // Delete all urls in the domain.
  // TODO(acw): test the new url based delete domain
  // EXPECT_TRUE(db.DeleteDomain(kDomainID));

  // Make sure the urls have been properly removed.
  // TODO(acw): commented out because remove no longer works.
  // EXPECT_TRUE(db.GetURLInfo(url1, NULL) == NULL);
  // EXPECT_TRUE(db.GetURLInfo(url2, NULL) == NULL);
}

// Tests adding, querying and deleting keyword visits.
TEST_F(URLDatabaseTest, KeywordSearchTermVisit) {
  URLRow url_info1(GURL("http://www.google.com/"));
  url_info1.set_title(base::UTF8ToUTF16("Google"));
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info1.set_hidden(false);
  URLID url_id = AddURL(url_info1);
  ASSERT_NE(0, url_id);

  // Add a keyword visit.
  KeywordID keyword_id = 100;
  base::string16 keyword = base::UTF8ToUTF16(" VISIT ");
  base::string16 normalized_keyword = base::UTF8ToUTF16("visit");
  ASSERT_TRUE(SetKeywordSearchTermsForURL(url_id, keyword_id, keyword));

  // Make sure we get it back.
  std::vector<KeywordSearchTermVisit> matches;
  GetMostRecentKeywordSearchTerms(keyword_id, base::UTF8ToUTF16("vi"), 10,
                                  &matches);
  ASSERT_EQ(1U, matches.size());
  ASSERT_EQ(keyword, matches[0].term);

  auto zero_prefix_matches = GetMostRecentKeywordSearchTerms(keyword_id, 10);
  ASSERT_EQ(1U, zero_prefix_matches.size());
  ASSERT_EQ(keyword, zero_prefix_matches[0].term);
  ASSERT_EQ(normalized_keyword, zero_prefix_matches[0].normalized_term);

  KeywordSearchTermRow keyword_search_term_row;
  ASSERT_TRUE(GetKeywordSearchTermRow(url_id, &keyword_search_term_row));
  EXPECT_EQ(keyword_id, keyword_search_term_row.keyword_id);
  EXPECT_EQ(url_id, keyword_search_term_row.url_id);
  EXPECT_EQ(keyword, keyword_search_term_row.term);

  // Delete the keyword visit.
  DeleteAllSearchTermsForKeyword(keyword_id);

  // Make sure we don't get it back when querying.
  matches.clear();
  GetMostRecentKeywordSearchTerms(keyword_id, keyword, 10, &matches);
  ASSERT_EQ(0U, matches.size());

  ASSERT_FALSE(GetKeywordSearchTermRow(url_id, &keyword_search_term_row));
}

// Make sure deleting a URL also deletes a keyword visit.
TEST_F(URLDatabaseTest, DeleteURLDeletesKeywordSearchTermVisit) {
  URLRow url_info1(GURL("http://www.google.com/"));
  url_info1.set_title(base::UTF8ToUTF16("Google"));
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info1.set_hidden(false);
  URLID url_id = AddURL(url_info1);
  ASSERT_NE(0, url_id);

  // Add a keyword visit.
  ASSERT_TRUE(
      SetKeywordSearchTermsForURL(url_id, 1, base::UTF8ToUTF16("visit")));

  // Delete the url.
  ASSERT_TRUE(DeleteURLRow(url_id));

  // Make sure the keyword visit was deleted.
  std::vector<KeywordSearchTermVisit> matches;
  GetMostRecentKeywordSearchTerms(1, base::UTF8ToUTF16("visit"), 10, &matches);
  ASSERT_EQ(0U, matches.size());
}

TEST_F(URLDatabaseTest, EnumeratorForSignificant) {
  // Add URLs which do and don't meet the criteria.
  URLRow url_no_match(GURL("http://www.url_no_match.com/"));
  EXPECT_TRUE(AddURL(url_no_match));

  URLRow url_match_visit_count2(GURL("http://www.url_match_visit_count.com/"));
  url_match_visit_count2.set_visit_count(kLowQualityMatchVisitLimit);
  EXPECT_TRUE(AddURL(url_match_visit_count2));

  URLRow url_match_typed_count2(GURL("http://www.url_match_typed_count.com/"));
  url_match_typed_count2.set_typed_count(kLowQualityMatchTypedLimit);
  EXPECT_TRUE(AddURL(url_match_typed_count2));

  URLRow url_match_last_visit2(GURL("http://www.url_match_last_visit2.com/"));
  url_match_last_visit2.set_last_visit(Time::Now() - TimeDelta::FromDays(2));
  EXPECT_TRUE(AddURL(url_match_last_visit2));

  URLRow url_match_typed_count1(
      GURL("http://www.url_match_higher_typed_count.com/"));
  url_match_typed_count1.set_typed_count(kLowQualityMatchTypedLimit + 1);
  EXPECT_TRUE(AddURL(url_match_typed_count1));

  URLRow url_match_visit_count1(
      GURL("http://www.url_match_higher_visit_count.com/"));
  url_match_visit_count1.set_visit_count(kLowQualityMatchVisitLimit + 1);
  EXPECT_TRUE(AddURL(url_match_visit_count1));

  URLRow url_match_last_visit1(GURL("http://www.url_match_last_visit.com/"));
  url_match_last_visit1.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  EXPECT_TRUE(AddURL(url_match_last_visit1));

  URLRow url_no_match_last_visit(GURL(
      "http://www.url_no_match_last_visit.com/"));
  url_no_match_last_visit.set_last_visit(Time::Now() -
      TimeDelta::FromDays(kLowQualityMatchAgeLimitInDays + 1));
  EXPECT_TRUE(AddURL(url_no_match_last_visit));

  URLRow url_hidden(GURL("http://www.url_match_higher_typed_count.com/hidden"));
  url_hidden.set_typed_count(kLowQualityMatchTypedLimit + 1);
  url_hidden.set_hidden(true);
  EXPECT_TRUE(AddURL(url_hidden));

  URLDatabase::URLEnumerator history_enum;
  EXPECT_TRUE(InitURLEnumeratorForSignificant(&history_enum));

  // Vector contains urls in order of significance.
  std::vector<std::string> good_urls;
  good_urls.push_back("http://www.url_match_higher_typed_count.com/");
  good_urls.push_back("http://www.url_match_typed_count.com/");
  good_urls.push_back("http://www.url_match_last_visit.com/");
  good_urls.push_back("http://www.url_match_last_visit2.com/");
  good_urls.push_back("http://www.url_match_higher_visit_count.com/");
  good_urls.push_back("http://www.url_match_visit_count.com/");
  URLRow row;
  int row_count = 0;
  for (; history_enum.GetNextURL(&row); ++row_count)
    EXPECT_EQ(good_urls[row_count], row.url().spec());
  EXPECT_EQ(6, row_count);
}

// Test GetKeywordSearchTermRows and DeleteSearchTerm
TEST_F(URLDatabaseTest, GetAndDeleteKeywordSearchTermByTerm) {
  URLRow url_info1(GURL("http://www.google.com/"));
  url_info1.set_title(base::UTF8ToUTF16("Google"));
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info1.set_hidden(false);
  URLID url_id1 = AddURL(url_info1);
  ASSERT_NE(0, url_id1);

  // Add a keyword visit.
  KeywordID keyword_id = 100;
  base::string16 keyword = base::UTF8ToUTF16("visit");
  ASSERT_TRUE(SetKeywordSearchTermsForURL(url_id1, keyword_id, keyword));

  URLRow url_info2(GURL("https://www.google.com/"));
  url_info2.set_title(base::UTF8ToUTF16("Google"));
  url_info2.set_visit_count(4);
  url_info2.set_typed_count(2);
  url_info2.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info2.set_hidden(false);
  URLID url_id2 = AddURL(url_info2);
  ASSERT_NE(0, url_id2);
  // Add the same keyword for url_info2.
  ASSERT_TRUE(SetKeywordSearchTermsForURL(url_id2, keyword_id, keyword));

  // Add another URL for different keyword.
  URLRow url_info3(GURL("https://www.google.com/search"));
  url_info3.set_title(base::UTF8ToUTF16("Google"));
  url_info3.set_visit_count(4);
  url_info3.set_typed_count(2);
  url_info3.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info3.set_hidden(false);
  URLID url_id3 = AddURL(url_info3);
  ASSERT_NE(0, url_id3);
  base::string16 keyword2 = base::UTF8ToUTF16("Search");

  ASSERT_TRUE(SetKeywordSearchTermsForURL(url_id3, keyword_id, keyword2));

  // We should get 2 rows for |keyword|.
  std::vector<KeywordSearchTermRow> rows;
  ASSERT_TRUE(GetKeywordSearchTermRows(keyword, &rows));
  ASSERT_EQ(2u, rows.size());
  if (rows[0].url_id == url_id1) {
    EXPECT_EQ(keyword, rows[0].term);
    EXPECT_EQ(keyword, rows[1].term);
    EXPECT_EQ(url_id2, rows[1].url_id);
  } else {
    EXPECT_EQ(keyword, rows[0].term);
    EXPECT_EQ(url_id1, rows[1].url_id);
    EXPECT_EQ(keyword, rows[1].term);
    EXPECT_EQ(url_id2, rows[0].url_id);
  }

  // We should get 1 row for |keyword2|.
  rows.clear();
  ASSERT_TRUE(GetKeywordSearchTermRows(keyword2, &rows));
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(keyword2, rows[0].term);
  EXPECT_EQ(url_id3, rows[0].url_id);

  // Delete all rows have keyword.
  ASSERT_TRUE(DeleteKeywordSearchTerm(keyword));
  rows.clear();
  // We should still find keyword2.
  ASSERT_TRUE(GetKeywordSearchTermRows(keyword2, &rows));
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(keyword2, rows[0].term);
  EXPECT_EQ(url_id3, rows[0].url_id);
  rows.clear();
  // No row for keyword.
  ASSERT_TRUE(GetKeywordSearchTermRows(keyword, &rows));
  EXPECT_TRUE(rows.empty());
}

// Test for migration of update URL table, verify AUTOINCREMENT is working
// properly.
TEST_F(URLDatabaseTest, MigrationURLTableForAddingAUTOINCREMENT) {
  CreateVersion33URLTable();
  // First, add two URLs.
  const GURL url1("http://www.google.com/");
  URLRow url_info1(url1);
  url_info1.set_title(base::UTF8ToUTF16("Google"));
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info1.set_hidden(false);
  URLID id1_initially = AddURL(url_info1);
  EXPECT_TRUE(id1_initially);

  const GURL url2("http://mail.google.com/");
  URLRow url_info2(url2);
  url_info2.set_title(base::UTF8ToUTF16("Google Mail"));
  url_info2.set_visit_count(3);
  url_info2.set_typed_count(0);
  url_info2.set_last_visit(Time::Now() - TimeDelta::FromDays(2));
  url_info2.set_hidden(true);
  EXPECT_TRUE(AddURL(url_info2));

  // Verify both are added.
  URLRow info1;
  EXPECT_TRUE(GetRowForURL(url1, &info1));
  EXPECT_TRUE(IsURLRowEqual(url_info1, info1));
  URLRow info2;
  EXPECT_TRUE(GetRowForURL(url2, &info2));
  EXPECT_TRUE(IsURLRowEqual(url_info2, info2));

  // Delete second URL, and add a new URL, verify id got re-used.
  EXPECT_TRUE(DeleteURLRow(info2.id()));

  const GURL url3("http://maps.google.com/");
  URLRow url_info3(url3);
  url_info3.set_title(base::UTF8ToUTF16("Google Maps"));
  url_info3.set_visit_count(7);
  url_info3.set_typed_count(6);
  url_info3.set_last_visit(Time::Now() - TimeDelta::FromDays(3));
  url_info3.set_hidden(false);
  EXPECT_TRUE(AddURL(url_info3));

  URLRow info3;
  EXPECT_TRUE(GetRowForURL(url3, &info3));
  EXPECT_TRUE(IsURLRowEqual(url_info3, info3));
  // Verify the id re-used.
  EXPECT_EQ(info2.id(), info3.id());

  // Upgrade urls table.
  RecreateURLTableWithAllContents();

  // Verify all data keeped.
  EXPECT_TRUE(GetRowForURL(url1, &info1));
  EXPECT_TRUE(IsURLRowEqual(url_info1, info1));
  EXPECT_FALSE(GetRowForURL(url2, &info2));
  EXPECT_TRUE(GetRowForURL(url3, &info3));
  EXPECT_TRUE(IsURLRowEqual(url_info3, info3));

  // Add a new URL
  const GURL url4("http://plus.google.com/");
  URLRow url_info4(url4);
  url_info4.set_title(base::UTF8ToUTF16("Google Plus"));
  url_info4.set_visit_count(4);
  url_info4.set_typed_count(3);
  url_info4.set_last_visit(Time::Now() - TimeDelta::FromDays(4));
  url_info4.set_hidden(false);
  EXPECT_TRUE(AddURL(url_info4));

  // Verify The URL are added.
  URLRow info4;
  EXPECT_TRUE(GetRowForURL(url4, &info4));
  EXPECT_TRUE(IsURLRowEqual(url_info4, info4));

  // Delete the newest URL, and add a new URL, verify id is not re-used.
  EXPECT_TRUE(DeleteURLRow(info4.id()));

  const GURL url5("http://docs.google.com/");
  URLRow url_info5(url5);
  url_info5.set_title(base::UTF8ToUTF16("Google Docs"));
  url_info5.set_visit_count(9);
  url_info5.set_typed_count(2);
  url_info5.set_last_visit(Time::Now() - TimeDelta::FromDays(5));
  url_info5.set_hidden(false);
  EXPECT_TRUE(AddURL(url_info5));

  URLRow info5;
  EXPECT_TRUE(GetRowForURL(url5, &info5));
  EXPECT_TRUE(IsURLRowEqual(url_info5, info5));
  // Verify the id is not re-used.
  EXPECT_NE(info4.id(), info5.id());
}

TEST_F(URLDatabaseTest, URLTableContainsAUTOINCREMENTTest) {
  CreateVersion33URLTable();
  EXPECT_FALSE(URLTableContainsAutoincrement());

  // Upgrade urls table.
  RecreateURLTableWithAllContents();
  EXPECT_TRUE(URLTableContainsAutoincrement());
}

}  // namespace history
