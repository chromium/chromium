// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/url_database.h"

#include <limits>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/keyword_search_term_util.h"
#include "sql/database.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

namespace history {

namespace {

bool IsURLRowEqual(const URLRow& a,
                   const URLRow& b) {
  // TODO(brettw) when the database stores an actual Time value rather than
  // a time_t, do a reaul comparison. Instead, we have to do a more rough
  // comparison since the conversion reduces the precision.
  return a.title() == b.title() && a.visit_count() == b.visit_count() &&
         a.typed_count() == b.typed_count() &&
         a.last_visit() - b.last_visit() <= base::Seconds(1) &&
         a.hidden() == b.hidden();
}

}  // namespace

class URLDatabaseTest : public testing::Test,
                        public URLDatabase {
 public:
  URLDatabaseTest() = default;

  void CreateVersion33URLTable() {
    EXPECT_TRUE(GetDB().Execute("DROP TABLE urls"));

    // create a version 33 urls table
    static constexpr char kSql[] =
        "CREATE TABLE urls ("
        "id INTEGER PRIMARY KEY,"
        "url LONGVARCHAR,"
        "title LONGVARCHAR,"
        "visit_count INTEGER DEFAULT 0 NOT NULL,"
        "typed_count INTEGER DEFAULT 0 NOT NULL,"
        "last_visit_time INTEGER NOT NULL,"
        "hidden INTEGER DEFAULT 0 NOT NULL,"
        "favicon_id INTEGER DEFAULT 0 NOT NULL)";  // favicon_id is not used
                                                   // now.
    EXPECT_TRUE(GetDB().Execute(kSql));
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
  url_info1.set_title(u"Google");
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - base::Days(1));
  url_info1.set_hidden(false);
  URLID id1_initially = AddURL(url_info1);
  EXPECT_TRUE(id1_initially);

  const GURL url2("http://mail.google.com/");
  URLRow url_info2(url2);
  url_info2.set_title(u"Google Mail");
  url_info2.set_visit_count(3);
  url_info2.set_typed_count(0);
  url_info2.set_last_visit(Time::Now() - base::Days(2));
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
  url_info2.set_title(u"Google Mail Too");
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
  url_info1.set_title(u"Google Again!");
  url_info1.set_visit_count(5);
  url_info1.set_typed_count(3);
  url_info1.set_last_visit(Time::Now());
  url_info1.set_hidden(true);
  EXPECT_TRUE(InsertOrUpdateURLRowByID(url_info1));

  const GURL url4("http://maps.google.com/");
  URLRow url_info4(url4);
  url_info4.set_id(43);
  url_info4.set_title(u"Google Maps");
  url_info4.set_visit_count(7);
  url_info4.set_typed_count(6);
  url_info4.set_last_visit(Time::Now() - base::Days(3));
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

// Tests querying prefix keyword search terms.
TEST_F(URLDatabaseTest, KeywordSearchTerms_Prefix) {
  KeywordID keyword_id = 100;
  // Choose the local midnight of yesterday as the baseline for the time.
  base::Time local_midnight = Time::Now().LocalMidnight() - base::Days(1);

  // First search for "foo".
  URLRow foo_url_1(GURL("https://www.google.com/search?q=Foo&num=1"));
  foo_url_1.set_visit_count(1);
  foo_url_1.set_last_visit(local_midnight + base::Hours(1));
  URLID foo_url_1_id = AddURL(foo_url_1);
  ASSERT_NE(0, foo_url_1_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(foo_url_1_id, keyword_id, u"Foo"));

  // Second search for "foo".
  URLRow foo_url_2(GURL("https://www.google.com/search?q=FOo&num=2"));
  foo_url_2.set_visit_count(1);
  foo_url_2.set_last_visit(local_midnight + base::Hours(2));
  URLID foo_url_2_id = AddURL(foo_url_2);
  ASSERT_NE(0, foo_url_2_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(foo_url_2_id, keyword_id, u"FOo"));

  // Third search for "foo".
  URLRow foo_url_3(GURL("https://www.google.com/search?q=FOO&num=3"));
  foo_url_3.set_visit_count(1);
  foo_url_3.set_last_visit(local_midnight + base::Hours(3));
  URLID foo_url_3_id = AddURL(foo_url_3);
  ASSERT_NE(0, foo_url_3_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(foo_url_3_id, keyword_id, u"FOO"));

  // First search for "bar".
  URLRow bar_url_1(GURL("https://www.google.com/search?q=BAR&num=4"));
  bar_url_1.set_visit_count(1);
  bar_url_1.set_last_visit(local_midnight + base::Hours(4));
  URLID bar_url_1_id = AddURL(bar_url_1);
  ASSERT_NE(0, bar_url_1_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(bar_url_1_id, keyword_id, u"BAR"));

  // First search for "food".
  URLRow food_url_1(GURL("https://www.google.com/search?q=Food&num=1"));
  food_url_1.set_visit_count(1);
  food_url_1.set_last_visit(local_midnight + base::Hours(5));
  URLID food_url_1_id = AddURL(food_url_1);
  ASSERT_NE(0, food_url_1_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(food_url_1_id, keyword_id, u"Food"));

  // Make sure we get "food" and "foo" back with the last term and visit time
  // that generated the normalized search terms.
  // CreateKeywordSearchTermVisitEnumerator accumulates the visits to unique
  // normalized search terms.
  auto enumerator_1 = CreateKeywordSearchTermVisitEnumerator(keyword_id, u"f");
  ASSERT_TRUE(enumerator_1);
  KeywordSearchTermVisitList matches;
  GetAutocompleteSearchTermsFromEnumerator(*enumerator_1, /*count=*/SIZE_MAX,
                                           SearchTermRankingPolicy::kRecency,
                                           &matches);
  ASSERT_EQ(2U, matches.size());
  EXPECT_EQ(u"Food", matches[0]->term);
  EXPECT_EQ(u"food", matches[0]->normalized_term);
  EXPECT_EQ(1, matches[0]->visit_count);
  EXPECT_EQ(local_midnight + base::Hours(5), matches[0]->last_visit_time);
  EXPECT_EQ(u"FOO", matches[1]->term);
  EXPECT_EQ(u"foo", matches[1]->normalized_term);
  EXPECT_EQ(3, matches[1]->visit_count);
  EXPECT_EQ(local_midnight + base::Hours(3), matches[1]->last_visit_time);

  // Make sure we get only as many search terms as requested in the expected
  // order.
  auto enumerator_2 = CreateKeywordSearchTermVisitEnumerator(keyword_id, u"f");
  ASSERT_TRUE(enumerator_2);
  matches.clear();
  GetAutocompleteSearchTermsFromEnumerator(
      *enumerator_2, /*count=*/1U, SearchTermRankingPolicy::kRecency, &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(u"Food", matches[0]->term);
  EXPECT_EQ(u"food", matches[0]->normalized_term);
  EXPECT_EQ(1, matches[0]->visit_count);

  KeywordSearchTermRow keyword_search_term_row;
  ASSERT_TRUE(GetKeywordSearchTermRow(foo_url_3_id, &keyword_search_term_row));
  EXPECT_EQ(keyword_id, keyword_search_term_row.keyword_id);
  EXPECT_EQ(foo_url_3_id, keyword_search_term_row.url_id);
  EXPECT_EQ(u"FOO", keyword_search_term_row.term);
  ASSERT_TRUE(GetKeywordSearchTermRow(food_url_1_id, &keyword_search_term_row));
  EXPECT_EQ(keyword_id, keyword_search_term_row.keyword_id);
  EXPECT_EQ(food_url_1_id, keyword_search_term_row.url_id);
  EXPECT_EQ(u"Food", keyword_search_term_row.term);

  // Delete all the search terms for the keyword.
  DeleteAllSearchTermsForKeyword(keyword_id);

  // Make sure we get nothing back.
  auto enumerator_3 = CreateKeywordSearchTermVisitEnumerator(keyword_id, u"f");
  ASSERT_TRUE(enumerator_3);
  matches.clear();
  GetAutocompleteSearchTermsFromEnumerator(*enumerator_3, /*count=*/SIZE_MAX,
                                           SearchTermRankingPolicy::kRecency,
                                           &matches);
  ASSERT_EQ(0U, matches.size());

  ASSERT_FALSE(GetKeywordSearchTermRow(foo_url_3_id, &keyword_search_term_row));
}

// Tests querying zero-prefix keyword search terms.
TEST_F(URLDatabaseTest, KeywordSearchTerms_ZeroPrefix) {
  KeywordID keyword_id = 100;
  // Choose the local midnight of yesterday as the baseline for the time.
  base::Time local_midnight = Time::Now().LocalMidnight() - base::Days(1);

  // First search for "foo".
  URLRow foo_url_1(GURL("https://www.google.com/search?q=Foo&num=1"));
  foo_url_1.set_visit_count(1);
  foo_url_1.set_last_visit(local_midnight + base::Hours(1));
  URLID foo_url_1_id = AddURL(foo_url_1);
  ASSERT_NE(0, foo_url_1_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(foo_url_1_id, keyword_id, u"Foo"));

  // Second search for "foo".
  URLRow foo_url_2(GURL("https://www.google.com/search?q=FOo&num=2"));
  foo_url_2.set_visit_count(1);
  foo_url_2.set_last_visit(local_midnight + base::Hours(2));
  URLID foo_url_2_id = AddURL(foo_url_2);
  ASSERT_NE(0, foo_url_2_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(foo_url_2_id, keyword_id, u"FOo"));

  // Third search for "foo".
  URLRow foo_url_3(GURL("https://www.google.com/search?q=FOO&num=3"));
  foo_url_3.set_visit_count(1);
  foo_url_3.set_last_visit(local_midnight + base::Hours(3));
  URLID foo_url_3_id = AddURL(foo_url_3);
  ASSERT_NE(0, foo_url_3_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(foo_url_3_id, keyword_id, u"FOO"));

  // First search for "bar".
  URLRow bar_url_1(GURL("https://www.google.com/search?q=BAR&num=4"));
  bar_url_1.set_visit_count(1);
  bar_url_1.set_last_visit(local_midnight + base::Hours(4));
  URLID bar_url_1_id = AddURL(bar_url_1);
  ASSERT_NE(0, bar_url_1_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(bar_url_1_id, keyword_id, u"BAR"));

  // Fourth search for "foo".
  // This search will be ignored for being too close to previous search.
  URLRow foo_url_4(GURL("https://www.google.com/search?q=foo&num=4"));
  foo_url_4.set_visit_count(1);
  foo_url_4.set_last_visit(local_midnight + base::Hours(3));
  URLID foo_url_4_id = AddURL(foo_url_4);
  ASSERT_NE(0, foo_url_4_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(foo_url_4_id, keyword_id, u"foo"));

  // Make sure we get both "foo" and "bar" back. "foo" should come first since
  // it has more visits and thus a higher frecency score.
  auto enumerator_1 = CreateKeywordSearchTermVisitEnumerator(keyword_id);
  ASSERT_TRUE(enumerator_1);
  KeywordSearchTermVisitList matches;
  GetAutocompleteSearchTermsFromEnumerator(*enumerator_1, /*count=*/SIZE_MAX,
                                           SearchTermRankingPolicy::kFrecency,
                                           &matches);
  ASSERT_EQ(2U, matches.size());
  EXPECT_EQ(u"FOO", matches[0]->term);
  EXPECT_EQ(u"foo", matches[0]->normalized_term);
  EXPECT_EQ(3, matches[0]->visit_count);
  EXPECT_EQ(local_midnight + base::Hours(3), matches[0]->last_visit_time);
  EXPECT_EQ(u"BAR", matches[1]->term);
  EXPECT_EQ(u"bar", matches[1]->normalized_term);
  EXPECT_EQ(1, matches[1]->visit_count);
  EXPECT_EQ(local_midnight + base::Hours(4), matches[1]->last_visit_time);

  // Make sure we get only as many search terms as requested in the expected
  // order.
  auto enumerator_2 = CreateKeywordSearchTermVisitEnumerator(keyword_id);
  ASSERT_TRUE(enumerator_2);
  matches.clear();
  GetAutocompleteSearchTermsFromEnumerator(*enumerator_2, /*count=*/1U,
                                           SearchTermRankingPolicy::kFrecency,
                                           &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(u"FOO", matches[0]->term);
  EXPECT_EQ(u"foo", matches[0]->normalized_term);
  EXPECT_EQ(3, matches[0]->visit_count);
  EXPECT_EQ(local_midnight + base::Hours(3), matches[0]->last_visit_time);

  KeywordSearchTermRow keyword_search_term_row;
  ASSERT_TRUE(GetKeywordSearchTermRow(foo_url_3_id, &keyword_search_term_row));
  EXPECT_EQ(keyword_id, keyword_search_term_row.keyword_id);
  EXPECT_EQ(foo_url_3_id, keyword_search_term_row.url_id);
  EXPECT_EQ(u"FOO", keyword_search_term_row.term);
  ASSERT_TRUE(GetKeywordSearchTermRow(bar_url_1_id, &keyword_search_term_row));
  EXPECT_EQ(keyword_id, keyword_search_term_row.keyword_id);
  EXPECT_EQ(bar_url_1_id, keyword_search_term_row.url_id);
  EXPECT_EQ(u"BAR", keyword_search_term_row.term);

  // Delete all the search terms for the keyword.
  DeleteAllSearchTermsForKeyword(keyword_id);

  // Make sure we get nothing back.
  auto enumerator_3 = CreateKeywordSearchTermVisitEnumerator(keyword_id);
  ASSERT_TRUE(enumerator_3);
  matches.clear();
  GetAutocompleteSearchTermsFromEnumerator(*enumerator_3, /*count=*/SIZE_MAX,
                                           SearchTermRankingPolicy::kFrecency,
                                           &matches);
  ASSERT_EQ(0U, matches.size());

  ASSERT_FALSE(GetKeywordSearchTermRow(foo_url_3_id, &keyword_search_term_row));
}

// Tests querying most repeated keyword search terms.
TEST_F(URLDatabaseTest, KeywordSearchTerms_MostRepeated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      history::kOrganicRepeatableQueries,
      {{history::kRepeatableQueriesIgnoreDuplicateVisits.name, "false"},
       {history::kRepeatableQueriesMinVisitCount.name, "1"}});
  KeywordID keyword_id = 100;
  // Choose the local midnight of yesterday as the baseline for the time.
  base::Time local_midnight = Time::Now().LocalMidnight() - base::Days(1);

  // First search for "foo" - yesterday.
  URLRow foo_url_1(GURL("https://www.google.com/search?q=foo&num=1"));
  foo_url_1.set_visit_count(1);
  foo_url_1.set_last_visit(local_midnight - base::Days(1) + base::Hours(1));
  URLID foo_url_1_id = AddURL(foo_url_1);
  ASSERT_NE(0, foo_url_1_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(foo_url_1_id, keyword_id, u"foo"));

  // First search for "bar" - yesterday.
  URLRow bar_url_1(GURL("https://www.google.com/search?q=bar&num=1"));
  bar_url_1.set_visit_count(1);
  bar_url_1.set_last_visit(local_midnight - base::Days(1) + base::Hours(2));
  URLID bar_url_1_id = AddURL(bar_url_1);
  ASSERT_NE(0, bar_url_1_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(bar_url_1_id, keyword_id, u"bar"));

  // Second search for "bar" - yesterday.
  URLRow bar_url_2(GURL("https://www.google.com/search?q=Bar&num=2"));
  bar_url_2.set_visit_count(1);
  bar_url_2.set_last_visit(local_midnight - base::Days(1) + base::Hours(3));
  URLID bar_url_2_id = AddURL(bar_url_2);
  ASSERT_NE(0, bar_url_2_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(bar_url_2_id, keyword_id, u"Bar"));

  // Second search for "foo" - yesterday.
  URLRow foo_url_2(GURL("https://www.google.com/search?q=Foo&num=2"));
  foo_url_2.set_visit_count(1);
  foo_url_2.set_last_visit(local_midnight - base::Days(1) + base::Hours(4));
  URLID foo_url_2_id = AddURL(foo_url_2);
  ASSERT_NE(0, foo_url_2_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(foo_url_2_id, keyword_id, u"Foo"));

  // Third search for "bar" - today.
  // This search will be ignored for having a visit count of 0.
  URLRow bar_url_3(GURL("https://www.google.com/search?q=BAr&num=3"));
  bar_url_3.set_visit_count(0);
  bar_url_3.set_last_visit(local_midnight + base::Hours(1));
  URLID bar_url_3_id = AddURL(bar_url_3);
  ASSERT_NE(0, bar_url_3_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(bar_url_3_id, keyword_id, u"BAr"));

  // Third search for "foo" - today.
  // This search will be ignored for having a visit count of 0.
  URLRow foo_url_3(GURL("https://www.google.com/search?q=FOo&num=3"));
  foo_url_3.set_visit_count(0);
  foo_url_3.set_last_visit(local_midnight + base::Hours(2));
  URLID foo_url_3_id = AddURL(foo_url_3);
  ASSERT_NE(0, foo_url_3_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(foo_url_3_id, keyword_id, u"FOo"));

  // Fourth search for "bar" - today.
  URLRow bar_url_4(GURL("https://www.google.com/search?q=BAR&num=4"));
  bar_url_4.set_visit_count(1);
  bar_url_4.set_last_visit(local_midnight + base::Hours(3));
  URLID bar_url_4_id = AddURL(bar_url_4);
  ASSERT_NE(0, bar_url_4_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(bar_url_4_id, keyword_id, u"BAR"));

  // Fourth search for "foo" - today.
  URLRow foo_url_4(GURL("https://www.google.com/search?q=FOO&num=4"));
  foo_url_4.set_visit_count(1);
  foo_url_4.set_last_visit(local_midnight + base::Hours(4));
  URLID foo_url_4_id = AddURL(foo_url_4);
  ASSERT_NE(0, foo_url_4_id);
  ASSERT_TRUE(SetKeywordSearchTermsForURL(foo_url_4_id, keyword_id, u"FOO"));

  // Make sure we get both "foo" and "bar" back. search terms with identical
  // scores are ranked in alphabetical order.
  auto enumerator_1 = CreateKeywordSearchTermVisitEnumerator(keyword_id);
  ASSERT_TRUE(enumerator_1);
  KeywordSearchTermVisitList matches;
  GetMostRepeatedSearchTermsFromEnumerator(*enumerator_1, /*count=*/SIZE_MAX,
                                           &matches);
  ASSERT_EQ(2U, matches.size());
  EXPECT_EQ(matches[0]->score, matches[1]->score);
  EXPECT_EQ(u"BAR", matches[0]->term);
  EXPECT_EQ(u"bar", matches[0]->normalized_term);
  EXPECT_EQ(u"FOO", matches[1]->term);
  EXPECT_EQ(u"foo", matches[1]->normalized_term);

  // Make sure we get only as many search terms as requested in the expected
  // order.
  auto enumerator_2 = CreateKeywordSearchTermVisitEnumerator(keyword_id);
  ASSERT_TRUE(enumerator_2);
  matches.clear();
  GetMostRepeatedSearchTermsFromEnumerator(*enumerator_2, /*count=*/1U,
                                           &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(u"BAR", matches[0]->term);
  EXPECT_EQ(u"bar", matches[0]->normalized_term);

  KeywordSearchTermRow keyword_search_term_row;
  ASSERT_TRUE(GetKeywordSearchTermRow(foo_url_4_id, &keyword_search_term_row));
  ASSERT_TRUE(GetKeywordSearchTermRow(bar_url_4_id, &keyword_search_term_row));

  // Delete all the search terms for the keyword.
  DeleteAllSearchTermsForKeyword(keyword_id);

  ASSERT_FALSE(GetKeywordSearchTermRow(foo_url_4_id, &keyword_search_term_row));
  ASSERT_FALSE(GetKeywordSearchTermRow(bar_url_4_id, &keyword_search_term_row));

  // Make sure we get nothing back.
  auto enumerator_3 = CreateKeywordSearchTermVisitEnumerator(keyword_id);
  ASSERT_TRUE(enumerator_3);
  matches.clear();
  GetMostRepeatedSearchTermsFromEnumerator(*enumerator_3, /*count=*/SIZE_MAX,
                                           &matches);
  ASSERT_EQ(0U, matches.size());
}

// Make sure deleting a URL also deletes a keyword visit.
TEST_F(URLDatabaseTest, DeleteURLDeletesKeywordSearchTermVisit) {
  URLRow url_info1(GURL("http://www.google.com/"));
  url_info1.set_title(u"Google");
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - base::Days(1));
  url_info1.set_hidden(false);
  URLID url_id = AddURL(url_info1);
  ASSERT_NE(0, url_id);

  // Add a keyword visit.
  ASSERT_TRUE(SetKeywordSearchTermsForURL(url_id, 1, u"visit"));

  // Delete the url.
  ASSERT_TRUE(DeleteURLRow(url_id));

  // Make sure the keyword visit was deleted.
  KeywordSearchTermVisitList matches;
  auto enumerator = CreateKeywordSearchTermVisitEnumerator(1, u"visit");
  ASSERT_TRUE(enumerator);
  matches.clear();
  GetAutocompleteSearchTermsFromEnumerator(*enumerator, /*count=*/SIZE_MAX,
                                           SearchTermRankingPolicy::kRecency,
                                           &matches);
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
  url_match_last_visit2.set_last_visit(Time::Now() - base::Days(2));
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
  url_match_last_visit1.set_last_visit(Time::Now() - base::Days(1));
  EXPECT_TRUE(AddURL(url_match_last_visit1));

  URLRow url_no_match_last_visit(GURL(
      "http://www.url_no_match_last_visit.com/"));
  url_no_match_last_visit.set_last_visit(
      Time::Now() - base::Days(kLowQualityMatchAgeLimitInDays + 1));
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
  url_info1.set_title(u"Google");
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - base::Days(1));
  url_info1.set_hidden(false);
  URLID url_id1 = AddURL(url_info1);
  ASSERT_NE(0, url_id1);

  // Add a keyword visit.
  KeywordID keyword_id = 100;
  std::u16string keyword = u"visit";
  ASSERT_TRUE(SetKeywordSearchTermsForURL(url_id1, keyword_id, keyword));

  URLRow url_info2(GURL("https://www.google.com/"));
  url_info2.set_title(u"Google");
  url_info2.set_visit_count(4);
  url_info2.set_typed_count(2);
  url_info2.set_last_visit(Time::Now() - base::Days(1));
  url_info2.set_hidden(false);
  URLID url_id2 = AddURL(url_info2);
  ASSERT_NE(0, url_id2);
  // Add the same keyword for url_info2.
  ASSERT_TRUE(SetKeywordSearchTermsForURL(url_id2, keyword_id, keyword));

  // Add another URL for different keyword.
  URLRow url_info3(GURL("https://www.google.com/search"));
  url_info3.set_title(u"Google");
  url_info3.set_visit_count(4);
  url_info3.set_typed_count(2);
  url_info3.set_last_visit(Time::Now() - base::Days(1));
  url_info3.set_hidden(false);
  URLID url_id3 = AddURL(url_info3);
  ASSERT_NE(0, url_id3);
  std::u16string keyword2 = u"Search";

  ASSERT_TRUE(SetKeywordSearchTermsForURL(url_id3, keyword_id, keyword2));

  // We should get 2 rows for `keyword`.
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

  // We should get 1 row for `keyword2`.
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
  url_info1.set_title(u"Google");
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - base::Days(1));
  url_info1.set_hidden(false);
  URLID id1_initially = AddURL(url_info1);
  EXPECT_TRUE(id1_initially);

  const GURL url2("http://mail.google.com/");
  URLRow url_info2(url2);
  url_info2.set_title(u"Google Mail");
  url_info2.set_visit_count(3);
  url_info2.set_typed_count(0);
  url_info2.set_last_visit(Time::Now() - base::Days(2));
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
  url_info3.set_title(u"Google Maps");
  url_info3.set_visit_count(7);
  url_info3.set_typed_count(6);
  url_info3.set_last_visit(Time::Now() - base::Days(3));
  url_info3.set_hidden(false);
  EXPECT_TRUE(AddURL(url_info3));

  URLRow info3;
  EXPECT_TRUE(GetRowForURL(url3, &info3));
  EXPECT_TRUE(IsURLRowEqual(url_info3, info3));
  // Verify the id re-used.
  EXPECT_EQ(info2.id(), info3.id());

  // Upgrade urls table.
  RecreateURLTableWithAllContents();

  // Verify all data kept.
  EXPECT_TRUE(GetRowForURL(url1, &info1));
  EXPECT_TRUE(IsURLRowEqual(url_info1, info1));
  EXPECT_FALSE(GetRowForURL(url2, &info2));
  EXPECT_TRUE(GetRowForURL(url3, &info3));
  EXPECT_TRUE(IsURLRowEqual(url_info3, info3));

  // Add a new URL
  const GURL url4("http://plus.google.com/");
  URLRow url_info4(url4);
  url_info4.set_title(u"Google Plus");
  url_info4.set_visit_count(4);
  url_info4.set_typed_count(3);
  url_info4.set_last_visit(Time::Now() - base::Days(4));
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
  url_info5.set_title(u"Google Docs");
  url_info5.set_visit_count(9);
  url_info5.set_typed_count(2);
  url_info5.set_last_visit(Time::Now() - base::Days(5));
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

TEST_F(URLDatabaseTest, CreateTemporaryURLTableDropsExistingTable) {
  EXPECT_TRUE(CreateTemporaryURLTable());
  const GURL url("http://www.google.com/");
  URLRow url_info(url);
  url_info.set_title(u"Google");
  url_info.set_visit_count(4);
  url_info.set_typed_count(2);
  url_info.set_last_visit(Time::Now() - base::Days(1));
  url_info.set_hidden(false);
  EXPECT_TRUE(AddTemporaryURL(url_info));
  {
    sql::Statement count_statement(
        GetDB().GetUniqueStatement("SELECT COUNT(*) from temp_urls"));
    ASSERT_TRUE(count_statement.Step());
    EXPECT_EQ(1, count_statement.ColumnInt(0));
  }

  // Calling CreateTemporaryURLTable() should drop the existing table.
  CreateTemporaryURLTable();
  {
    sql::Statement count_statement(
        GetDB().GetUniqueStatement("SELECT COUNT(*) from temp_urls"));
    ASSERT_TRUE(count_statement.Step());
    EXPECT_EQ(0, count_statement.ColumnInt(0));
  }
}

}  // namespace history
