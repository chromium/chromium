// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/in_memory_database.h"

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "components/history/core/browser/keyword_id.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/test/test_history_database.h"
#include "sql/init_status.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/test/scoped_error_expecter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history {
namespace {

constexpr KeywordID kKeywordId = 42;

// Matches a URLRow whose fields all equal those of `expected`.
testing::Matcher<const URLRow&> SameRowAs(const URLRow& expected) {
  return testing::AllOf(
      testing::Property("id", &URLRow::id, expected.id()),
      testing::Property("url", &URLRow::url, expected.url()),
      testing::Property("title", &URLRow::title, expected.title()),
      testing::Property("visit_count", &URLRow::visit_count,
                        expected.visit_count()),
      testing::Property("typed_count", &URLRow::typed_count,
                        expected.typed_count()),
      testing::Property("last_visit", &URLRow::last_visit,
                        expected.last_visit()),
      testing::Property("hidden", &URLRow::hidden, expected.hidden()));
}

// Exposes the schema manipulation needed to make the on-disk database
// unreadable.
class BreakableHistoryDatabase : public TestHistoryDatabase {
 public:
  using URLDatabase::DropKeywordSearchTermsTable;
};

class InMemoryDatabaseTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_EQ(sql::INIT_OK,
              history_db_.Init(temp_dir_.GetPath().AppendASCII("History")));
  }

  // Adds a URL to the on-disk database and returns it with its assigned id.
  URLRow AddURL(const std::string& url, int typed_count, bool hidden) {
    URLRow row(GURL{url});
    row.set_title(u"Title");
    row.set_visit_count(typed_count + 3);
    row.set_typed_count(typed_count);
    row.set_last_visit(base::Time::Now() - base::Hours(typed_count + 1));
    row.set_hidden(hidden);
    row.set_id(history_db_.AddURL(row));
    EXPECT_NE(0, row.id());
    return row;
  }

  base::ScopedTempDir temp_dir_;
  BreakableHistoryDatabase history_db_;
};

TEST_F(InMemoryDatabaseTest, InitFromEmptyDatabase) {
  InMemoryDatabase in_memory_db;
  ASSERT_TRUE(in_memory_db.InitFromUrlDatabase(history_db_));

  EXPECT_EQ(0, in_memory_db.GetRowForURL(GURL("http://example.com/"), nullptr));
}

TEST_F(InMemoryDatabaseTest, CopiesTypedURLs) {
  const URLRow typed =
      AddURL("http://typed.example/", /*typed_count=*/2, /*hidden=*/false);
  const URLRow hidden_typed =
      AddURL("http://hidden.example/", /*typed_count=*/1, /*hidden=*/true);
  const URLRow untyped =
      AddURL("http://untyped.example/", /*typed_count=*/0, /*hidden=*/false);

  InMemoryDatabase in_memory_db;
  ASSERT_TRUE(in_memory_db.InitFromUrlDatabase(history_db_));

  URLRow copied;
  ASSERT_EQ(typed.id(), in_memory_db.GetRowForURL(typed.url(), &copied));
  EXPECT_THAT(copied, SameRowAs(typed));
  ASSERT_EQ(hidden_typed.id(),
            in_memory_db.GetRowForURL(hidden_typed.url(), &copied));
  EXPECT_THAT(copied, SameRowAs(hidden_typed));
  EXPECT_EQ(0, in_memory_db.GetRowForURL(untyped.url(), nullptr));
}

TEST_F(InMemoryDatabaseTest, CopiesKeywordSearchTerms) {
  const URLRow searched = AddURL("http://search.example/?q=some+term",
                                 /*typed_count=*/0, /*hidden=*/false);
  ASSERT_TRUE(history_db_.SetKeywordSearchTermsForURL(searched.id(), kKeywordId,
                                                      u"Some  Term"));
  KeywordSearchTermRow expected;
  ASSERT_TRUE(history_db_.GetKeywordSearchTermRow(searched.id(), &expected));

  InMemoryDatabase in_memory_db;
  ASSERT_TRUE(in_memory_db.InitFromUrlDatabase(history_db_));

  URLRow copied;
  ASSERT_EQ(searched.id(), in_memory_db.GetRowForURL(searched.url(), &copied));
  EXPECT_THAT(copied, SameRowAs(searched));
  KeywordSearchTermRow term;
  ASSERT_TRUE(in_memory_db.GetKeywordSearchTermRow(searched.id(), &term));
  EXPECT_EQ(expected.keyword_id, term.keyword_id);
  EXPECT_EQ(expected.url_id, term.url_id);
  EXPECT_EQ(expected.term, term.term);
  EXPECT_EQ(expected.normalized_term, term.normalized_term);
}

TEST_F(InMemoryDatabaseTest, SkipsRowsWithInvalidURLs) {
  const URLRow before =
      AddURL("http://before.example/", /*typed_count=*/1, /*hidden=*/false);
  // AddURL() stores an empty string for a URL that does not parse. Old profiles
  // contain such rows, as well as URLs that stopped parsing when the parsing
  // rules changed.
  const URLRow invalid =
      AddURL("not a url", /*typed_count=*/1, /*hidden=*/false);
  ASSERT_FALSE(invalid.url().is_valid());
  ASSERT_EQ(invalid.id(), history_db_.GetRowForURL(invalid.url(), nullptr));
  // Added after the invalid row so that copying it proves the copy continued
  // past the skipped row.
  const URLRow after =
      AddURL("http://after.example/", /*typed_count=*/1, /*hidden=*/false);

  InMemoryDatabase in_memory_db;
  ASSERT_TRUE(in_memory_db.InitFromUrlDatabase(history_db_));

  EXPECT_EQ(0, in_memory_db.GetRowForURL(invalid.url(), nullptr));
  URLRow copied;
  ASSERT_EQ(before.id(), in_memory_db.GetRowForURL(before.url(), &copied));
  EXPECT_THAT(copied, SameRowAs(before));
  ASSERT_EQ(after.id(), in_memory_db.GetRowForURL(after.url(), &copied));
  EXPECT_THAT(copied, SameRowAs(after));
}

TEST_F(InMemoryDatabaseTest, SucceedsWhenSourceCannotBeRead) {
  const URLRow typed =
      AddURL("http://typed.example/", /*typed_count=*/1, /*hidden=*/false);
  // The query that enumerates the rows to copy references this table, so it
  // fails to prepare without it.
  ASSERT_TRUE(history_db_.DropKeywordSearchTermsTable());

  InMemoryDatabase in_memory_db;
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(sql::SqliteResultCode::kError);
    ASSERT_TRUE(in_memory_db.InitFromUrlDatabase(history_db_));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  // Populating is best effort: the cache comes up empty but usable.
  EXPECT_EQ(0, in_memory_db.GetRowForURL(typed.url(), nullptr));
  const URLID added_id = in_memory_db.AddURL(typed);
  EXPECT_NE(0, added_id);
  EXPECT_EQ(added_id, in_memory_db.GetRowForURL(typed.url(), nullptr));
}

}  // namespace
}  // namespace history
