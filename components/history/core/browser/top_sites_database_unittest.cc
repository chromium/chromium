// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/top_sites_database.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/database_test_utils.h"
#include "components/history/core/test/thumbnail-inl.h"
#include "sql/database.h"
#include "sql/recovery.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// Verify that the up-to-date database has the expected tables and
// columns.  Functional tests only check whether the things which
// should be there are, but do not check if extraneous items are
// present.  Any extraneous items have the potential to interact
// negatively with future schema changes.
void VerifyTablesAndColumns(sql::Database* db) {
  // [meta] and [top_sites].
  EXPECT_EQ(2u, sql::test::CountSQLTables(db));

  // Implicit index on [meta], index on [top_sites].
  EXPECT_EQ(2u, sql::test::CountSQLIndices(db));

  // [key] and [value].
  EXPECT_EQ(2u, sql::test::CountTableColumns(db, "meta"));

  // [url], [url_rank], [title]
  EXPECT_EQ(3u, sql::test::CountTableColumns(db, "top_sites"));
}

void VerifyDatabaseEmpty(sql::Database* db) {
  size_t rows = 0;
  EXPECT_TRUE(sql::test::CountTableRows(db, "top_sites", &rows));
  EXPECT_EQ(0u, rows);
}

void VerifyURLsEqual(const std::vector<GURL>& expected,
                     const history::MostVisitedURLList& actual) {
  EXPECT_EQ(expected.size(), actual.size());
  for (size_t i = 0; i < expected.size(); i++)
    EXPECT_EQ(expected[i], actual[i].url) << " for i = " << i;
}

}  // namespace

namespace history {

class TopSitesDatabaseTest : public testing::Test {
 protected:
  void SetUp() override {
    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_name_ = temp_dir_.GetPath().AppendASCII("TestTopSites.db");
  }

  // URLs by rank in golden files.
  const GURL kUrl0{"http://www.google.com/"};
  const GURL kUrl1{"http://www.google.com/chrome/intl/en/welcome.html"};
  const GURL kUrl2{"https://chrome.google.com/webstore?hl=en"};

  base::ScopedTempDir temp_dir_;
  base::FilePath file_name_;
};

// Version 1 is deprecated, the resulting schema should be current,
// with no data.
TEST_F(TopSitesDatabaseTest, Version1) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v1.sql"));

  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));
  VerifyTablesAndColumns(db.db_for_testing());
  VerifyDatabaseEmpty(db.db_for_testing());
}

// Version 2 is deprecated, the resulting schema should be current,
// with no data.
TEST_F(TopSitesDatabaseTest, Version2) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v2.sql"));

  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));
  VerifyTablesAndColumns(db.db_for_testing());
  VerifyDatabaseEmpty(db.db_for_testing());
}

// Version 3 is deprecated, the resulting schema should be current,
// with no data.
TEST_F(TopSitesDatabaseTest, Version3) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v3.sql"));

  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));
  VerifyTablesAndColumns(db.db_for_testing());
  VerifyDatabaseEmpty(db.db_for_testing());
}

TEST_F(TopSitesDatabaseTest, Version4) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v4.sql"));

  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));

  VerifyTablesAndColumns(db.db_for_testing());

  // Basic operational check.
  MostVisitedURLList urls = db.GetSites();
  ASSERT_EQ(3u, urls.size());
  EXPECT_EQ(kUrl0, urls[0].url);  // [0] because of url_rank.

  sql::Transaction transaction(db.db_for_testing());
  ASSERT_TRUE(transaction.Begin());
  ASSERT_TRUE(db.RemoveURLNoTransactionForTesting(urls[1]));
  transaction.Commit();

  urls = db.GetSites();
  ASSERT_EQ(2u, urls.size());
}

TEST_F(TopSitesDatabaseTest, Version5) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v5.sql"));

  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));

  VerifyTablesAndColumns(db.db_for_testing());

  // Basic operational check.
  MostVisitedURLList urls = db.GetSites();
  ASSERT_EQ(3u, urls.size());
  EXPECT_EQ(kUrl0, urls[0].url);  // [0] because of url_rank.

  sql::Transaction transaction(db.db_for_testing());
  ASSERT_TRUE(transaction.Begin());
  ASSERT_TRUE(db.RemoveURLNoTransactionForTesting(urls[1]));
  ASSERT_TRUE(transaction.Commit());

  urls = db.GetSites();
  ASSERT_EQ(2u, urls.size());
}

// Version 1 is deprecated, the resulting schema should be current, with no
// data.
TEST_F(TopSitesDatabaseTest, Recovery1) {
  // Create an example database.
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v1.sql"));

  // Corrupt the database by adjusting the header size.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(file_name_));

  // Database is unusable at the SQLite level.
  {
    sql::Database raw_db;
    {
      sql::test::ScopedErrorExpecter expecter;
      expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
      ASSERT_FALSE(raw_db.Open(file_name_));
      EXPECT_TRUE(expecter.SawExpectedErrors());
    }
    EXPECT_EQ("ok", sql::test::IntegrityCheck(raw_db));
  }

  // Corruption should be detected and recovered during Init().
  TopSitesDatabase db;
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
    ASSERT_TRUE(db.Init(file_name_));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
  VerifyTablesAndColumns(db.db_for_testing());
  VerifyDatabaseEmpty(db.db_for_testing());
}

// Version 2 is deprecated, the resulting schema should be current, with no
// data.
TEST_F(TopSitesDatabaseTest, Recovery2) {
  // Create an example database.
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v2.sql"));

  // Corrupt the database by adjusting the header.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(file_name_));

  // Database is unusable at the SQLite level.
  {
    sql::Database raw_db;
    {
      sql::test::ScopedErrorExpecter expecter;
      expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
      ASSERT_FALSE(raw_db.Open(file_name_));
      EXPECT_TRUE(expecter.SawExpectedErrors());
    }
    EXPECT_EQ("ok", sql::test::IntegrityCheck(raw_db));
  }

  // Corruption should be detected and recovered during Init().
  TopSitesDatabase db;
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
    ASSERT_TRUE(db.Init(file_name_));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
  VerifyTablesAndColumns(db.db_for_testing());
  VerifyDatabaseEmpty(db.db_for_testing());
}

TEST_F(TopSitesDatabaseTest, Recovery4_CorruptHeader) {
  // Create an example database.
  EXPECT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v4.sql"));

  // Corrupt the database by adjusting the header.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(file_name_));

  // Database is unusable at the SQLite level.
  {
    sql::Database raw_db;
    {
      sql::test::ScopedErrorExpecter expecter;
      expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
      ASSERT_FALSE(raw_db.Open(file_name_));
      EXPECT_TRUE(expecter.SawExpectedErrors());
    }
    EXPECT_EQ("ok", sql::test::IntegrityCheck(raw_db));
  }

  // Corruption should be detected and recovered during Init().
  {
    TopSitesDatabase db;
    {
      sql::test::ScopedErrorExpecter expecter;
      expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
      ASSERT_TRUE(db.Init(file_name_));
      EXPECT_TRUE(expecter.SawExpectedErrors());
    }

    MostVisitedURLList urls = db.GetSites();
    ASSERT_EQ(3u, urls.size());
    EXPECT_EQ(kUrl0, urls[0].url);  // [0] because of url_rank.
  }

  // Double-check database integrity.
  {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(file_name_));
    EXPECT_EQ("ok", sql::test::IntegrityCheck(raw_db));
  }
}

TEST_F(TopSitesDatabaseTest, Recovery5_CorruptIndex) {
  // Create an example database.
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v5.sql"));

  // Corrupt the top_sites.url auto-index.
  static const char kIndexName[] = "sqlite_autoindex_top_sites_1";
  EXPECT_TRUE(sql::test::CorruptIndexRootPage(file_name_, kIndexName));

  // SQLite can operate on the database, but notices the corruption in integrity
  // check.
  {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(file_name_));
    EXPECT_NE("ok", sql::test::IntegrityCheck(raw_db));
  }

  // Open the database and access the corrupt index.
  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(sql::SqliteResultCode::kCorrupt);

    // Accessing the index will throw SQLITE_CORRUPT. The corruption handler
    // will recover the database and poison the handle, so the outer call
    // fails.
    EXPECT_EQ(TopSitesDatabase::kRankOfNonExistingURL,
              db.GetURLRankForTesting(MostVisitedURL(kUrl1, std::u16string())));

    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  // Check that the database is recovered at the SQLite level.
  {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(file_name_));
    ASSERT_EQ("ok", sql::test::IntegrityCheck(raw_db));
  }

  // After recovery, the database accesses won't throw errors. Recovery should
  // have regenerated the index with no data loss.
  ASSERT_TRUE(db.Init(file_name_));
  VerifyTablesAndColumns(db.db_for_testing());

  EXPECT_EQ(0,
            db.GetURLRankForTesting(MostVisitedURL(kUrl0, std::u16string())));
  EXPECT_EQ(1,
            db.GetURLRankForTesting(MostVisitedURL(kUrl1, std::u16string())));
  EXPECT_EQ(2,
            db.GetURLRankForTesting(MostVisitedURL(kUrl2, std::u16string())));

  MostVisitedURLList urls = db.GetSites();
  ASSERT_EQ(3u, urls.size());
  EXPECT_EQ(kUrl0, urls[0].url);  // [0] because of url_rank.
  EXPECT_EQ(kUrl1, urls[1].url);  // [1] because of url_rank.
  EXPECT_EQ(kUrl2, urls[2].url);  // [2] because of url_rank.
}

TEST_F(TopSitesDatabaseTest, Recovery5_CorruptIndexAndLostRow) {
  // Create an example database.
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v5.sql"));

  // Delete a row.
  {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(file_name_));
    EXPECT_TRUE(
        raw_db.Execute("DELETE FROM top_sites WHERE url = "
                       "'http://www.google.com/chrome/intl/en/welcome.html'"));
  }
  // Corrupt the top_sites.url auto-index.
  static const char kIndexName[] = "sqlite_autoindex_top_sites_1";
  EXPECT_TRUE(sql::test::CorruptIndexRootPage(file_name_, kIndexName));

  // SQLite can operate on the database, but notices the corruption in integrity
  // check.
  {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(file_name_));
    EXPECT_NE("ok", sql::test::IntegrityCheck(raw_db));
  }

  // Open the database and access the corrupt index.
  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(sql::SqliteResultCode::kCorrupt);

    // Accessing the index will throw SQLITE_CORRUPT. The corruption handler
    // will recover the database and poison the handle, so the outer call
    // fails.
    EXPECT_EQ(TopSitesDatabase::kRankOfNonExistingURL,
              db.GetURLRankForTesting(MostVisitedURL(kUrl0, std::u16string())));

    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  // Check that the database is recovered at the SQLite level.
  {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(file_name_));
    ASSERT_EQ("ok", sql::test::IntegrityCheck(raw_db));
  }

  // After recovery, the database accesses won't throw errors. Recovery should
  // have regenerated the index and adjusted the ranks.
  ASSERT_TRUE(db.Init(file_name_));
  VerifyTablesAndColumns(db.db_for_testing());

  EXPECT_EQ(0,
            db.GetURLRankForTesting(MostVisitedURL(kUrl0, std::u16string())));
  EXPECT_EQ(1,
            db.GetURLRankForTesting(MostVisitedURL(kUrl2, std::u16string())));
  EXPECT_EQ(TopSitesDatabase::kRankOfNonExistingURL,
            db.GetURLRankForTesting(MostVisitedURL(kUrl1, std::u16string())));

  MostVisitedURLList urls = db.GetSites();
  ASSERT_EQ(2u, urls.size());
  EXPECT_EQ(kUrl0, urls[0].url);  // [0] because of url_rank.
  EXPECT_EQ(kUrl2, urls[1].url);  // [1] because of url_rank.
}

TEST_F(TopSitesDatabaseTest, ApplyDelta_Delete) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v4.sql"));

  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));

  TopSitesDelta delta;
  // Delete kUrl0. Now db has kUrl1 and kUrl2.
  MostVisitedURL url_to_delete(kUrl0, u"Google");
  delta.deleted.push_back(url_to_delete);

  // Update db.
  db.ApplyDelta(delta);

  // Read db and verify.
  MostVisitedURLList urls = db.GetSites();
  VerifyURLsEqual(std::vector<GURL>({kUrl1, kUrl2}), urls);
}

TEST_F(TopSitesDatabaseTest, ApplyDelta_Add) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v4.sql"));

  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));

  GURL mapsUrl = GURL("http://maps.google.com/");

  // Add a new URL, rank = 0. Now db has mapsUrl, kUrl0, kUrl1, and kUrl2.
  TopSitesDelta delta;
  MostVisitedURLWithRank url_to_add;
  url_to_add.url = MostVisitedURL(mapsUrl, u"Google Maps");
  url_to_add.rank = 0;
  delta.added.push_back(url_to_add);

  // Update db.
  db.ApplyDelta(delta);

  // Read db and verify.
  MostVisitedURLList urls = db.GetSites();
  VerifyURLsEqual(std::vector<GURL>({mapsUrl, kUrl0, kUrl1, kUrl2}), urls);
}

TEST_F(TopSitesDatabaseTest, ApplyDelta_Move) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v4.sql"));

  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));

  // Move kUrl1 by updating its rank to 2. Now db has kUrl0, kUrl2, and
  // kUrl1.
  TopSitesDelta delta;
  MostVisitedURLWithRank url_to_move;
  url_to_move.url = MostVisitedURL(kUrl1, u"Google Chrome");
  url_to_move.rank = 2;
  delta.moved.push_back(url_to_move);

  // Update db.
  db.ApplyDelta(delta);

  // Read db and verify.
  MostVisitedURLList urls = db.GetSites();
  VerifyURLsEqual(std::vector<GURL>({kUrl0, kUrl2, kUrl1}), urls);
}

TEST_F(TopSitesDatabaseTest, ApplyDelta_All) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v4.sql"));

  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));

  GURL mapsUrl = GURL("http://maps.google.com/");

  TopSitesDelta delta;
  // Delete kUrl0. Now db has kUrl1 and kUrl2.
  MostVisitedURL url_to_delete(kUrl0, u"Google");
  delta.deleted.push_back(url_to_delete);

  // Add a new URL, not forced, rank = 0. Now db has mapsUrl, kUrl1 and kUrl2.
  MostVisitedURLWithRank url_to_add;
  url_to_add.url = MostVisitedURL(mapsUrl, u"Google Maps");
  url_to_add.rank = 0;
  delta.added.push_back(url_to_add);

  // Move kUrl1 by updating its rank to 2. Now db has mapsUrl, kUrl2 and
  // kUrl1.
  MostVisitedURLWithRank url_to_move;
  url_to_move.url = MostVisitedURL(kUrl1, u"Google Chrome");
  url_to_move.rank = 2;
  delta.moved.push_back(url_to_move);

  // Update db.
  db.ApplyDelta(delta);

  // Read db and verify.
  MostVisitedURLList urls = db.GetSites();
  VerifyURLsEqual(std::vector<GURL>({mapsUrl, kUrl2, kUrl1}), urls);
}

TEST_F(TopSitesDatabaseTest, ApplyDelta_UpdatesAddedSiteTitle) {
  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));

  const GURL url_a("https://a.example");
  const GURL url_b("https://b.example");

  {
    TopSitesDelta delta;
    delta.added.push_back(MostVisitedURLWithRank{
        .url = {MostVisitedURL(url_a, u"A1")},
        .rank = 0,
    });
    delta.added.push_back(MostVisitedURLWithRank{
        .url = {MostVisitedURL(url_b, u"B")},
        .rank = 1,
    });

    db.ApplyDelta(delta);

    MostVisitedURLList urls = db.GetSites();
    ASSERT_EQ(urls.size(), 2u);

    ASSERT_EQ(urls[0].url, url_a);
    ASSERT_EQ(urls[0].title, u"A1");

    ASSERT_EQ(urls[1].url, url_b);
    ASSERT_EQ(urls[1].title, u"B");
  }

  {
    TopSitesDelta delta;
    delta.added.push_back(MostVisitedURLWithRank{
        .url = {MostVisitedURL(url_a, u"A2")},
        .rank = 0,
    });

    db.ApplyDelta(delta);

    MostVisitedURLList urls = db.GetSites();
    ASSERT_EQ(urls.size(), 2u);

    ASSERT_EQ(urls[0].url, url_a);
    ASSERT_EQ(urls[0].title, u"A2");

    ASSERT_EQ(urls[1].url, url_b);
    ASSERT_EQ(urls[1].title, u"B");
  }
}

}  // namespace history
