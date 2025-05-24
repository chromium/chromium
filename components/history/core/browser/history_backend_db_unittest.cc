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

#include <stdint.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>

#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/case_conversion.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/page_usage_data.h"
#include "components/history/core/test/history_backend_db_base_test.h"
#include "components/history/core/test/test_history_database.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {
namespace {

// This must be outside the anonymous namespace for the friend statement in
// HistoryBackend to work.
class HistoryBackendDBTest : public HistoryBackendDBBaseTest {
 public:
  HistoryBackendDBTest() = default;
  ~HistoryBackendDBTest() override = default;
};

TEST_F(HistoryBackendDBTest, ClearBrowsingData_Downloads) {
  CreateBackendAndDatabase();

  // Initially there should be nothing in the downloads database.
  std::vector<DownloadRow> downloads;
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());

  // Add a download, test that it was added correctly, remove it, test that it
  // was removed.
  base::Time now = base::Time();
  uint32_t id = 1;
  EXPECT_TRUE(AddDownload(id,
                          "BC5E3854-7B1D-4DE0-B619-B0D99C8B18B4",
                          DownloadState::COMPLETE,
                          base::Time()));
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(1U, downloads.size());

  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("current-path")),
            downloads[0].current_path);
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("target-path")),
            downloads[0].target_path);
  EXPECT_EQ(1UL, downloads[0].url_chain.size());
  EXPECT_EQ(GURL("foo-url"), downloads[0].url_chain[0]);
  EXPECT_EQ(std::string("http://referrer.example.com/"),
            downloads[0].referrer_url.spec());
  EXPECT_EQ(std::string("http://tab-url.example.com/"),
            downloads[0].tab_url.spec());
  EXPECT_EQ(std::string("http://tab-referrer-url.example.com/"),
            downloads[0].tab_referrer_url.spec());
  EXPECT_EQ(now, downloads[0].start_time);
  EXPECT_EQ(now, downloads[0].end_time);
  EXPECT_EQ(0, downloads[0].received_bytes);
  EXPECT_EQ(512, downloads[0].total_bytes);
  EXPECT_EQ(DownloadState::COMPLETE, downloads[0].state);
  EXPECT_EQ(DownloadDangerType::NOT_DANGEROUS, downloads[0].danger_type);
  EXPECT_EQ(kTestDownloadInterruptReasonNone, downloads[0].interrupt_reason);
  EXPECT_FALSE(downloads[0].opened);
  EXPECT_EQ("by_ext_id", downloads[0].by_ext_id);
  EXPECT_EQ("by_ext_name", downloads[0].by_ext_name);
  EXPECT_EQ("by_web_app_id", downloads[0].by_web_app_id);
  EXPECT_EQ("application/vnd.oasis.opendocument.text", downloads[0].mime_type);
  EXPECT_EQ("application/octet-stream", downloads[0].original_mime_type);

  db_->QueryDownloads(&downloads);
  EXPECT_EQ(1U, downloads.size());
  db_->RemoveDownload(id);
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());
}

TEST_F(HistoryBackendDBTest, MigrateDownloadByWebApp) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(63));

  // Precondition: Open the old version of the DB and make sure the new column
  // doesn't exist yet.
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    ASSERT_FALSE(db.DoesColumnExist("downloads", "by_web_app_id"));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads ("
          "    id, guid, current_path, target_path, start_time, received_bytes,"
          "    total_bytes, state, danger_type, interrupt_reason, hash,"
          "    end_time, opened, last_access_time, transient, referrer, "
          "    site_url, embedder_download_data, tab_url, tab_referrer_url, "
          "    http_method, by_ext_id, by_ext_name, etag, last_modified, "
          "    mime_type, original_mime_type)"
          "VALUES("
          "    1, '435A5C7A-F6B7-4DF2-8696-22E4FCBA3EB2', 'foo.txt', 'foo.txt',"
          "    13104873187307670, 11, 11, 1, 0, 0, X'', 13104873187521021, 0, "
          "    13104873187521021, 0, 'http://example.com/dl/',"
          "    'http://example.com', '', '', '', '', 'extension-id',"
          "    'extension-name', '', '', 'text/plain', 'text/plain')"));
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(1, 0, 'https://example.com')"));
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(64, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      // The downloads table should have the by_ext_id column unmodified,
      // and should have the new by_web_app_id column initialized to empty
      // string.
      sql::Statement s(db.GetUniqueStatement(
          "SELECT by_ext_id, by_web_app_id from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ("extension-id", s.ColumnString(0));
      EXPECT_EQ("", s.ColumnString(1));
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateClustersAndVisitsAddInteractionState) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(64));

  constexpr int64_t kTestClusterId = 39;
  constexpr VisitID kTestVisitId = 42;

  ClusterVisit visit;
  visit.score = 0.4;
  visit.engagement_score = 0.9;
  visit.url_for_deduping = GURL("https://url_for_deduping_test.com/");
  visit.normalized_url = GURL("https://norm_url.com/");
  visit.url_for_display = u"urlfordisplay";

  const char kInsertStatement[] =
      "INSERT INTO clusters_and_visits "
      "(cluster_id,visit_id,score,engagement_score,url_for_deduping,"
      "normalized_url,url_for_display) VALUES (?,?,?,?,?,?,?)";

  // Open the old version of the DB and make sure the new columns don't exist
  // yet.
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    ASSERT_FALSE(
        db.DoesColumnExist("clusters_and_visits", "interaction_state"));

    // Add legacy entry to visits.
    sql::Statement s(db.GetUniqueStatement(kInsertStatement));
    s.BindInt64(0, kTestClusterId);
    s.BindInt64(1, kTestVisitId);
    s.BindDouble(2, visit.score);
    s.BindDouble(3, visit.engagement_score);
    s.BindString(4, visit.url_for_deduping.spec());
    s.BindString(5, visit.normalized_url.spec());
    s.BindString16(6, visit.url_for_display);

    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 65);

  ClusterVisit visit_received = db_->GetClusterVisit(kTestVisitId);
  EXPECT_EQ(visit.score, visit_received.score);
  EXPECT_EQ(visit.engagement_score, visit_received.engagement_score);
  EXPECT_EQ(visit.url_for_deduping, visit_received.url_for_deduping);
  EXPECT_EQ(visit.normalized_url, visit_received.normalized_url);
  EXPECT_EQ(visit.url_for_display, visit.url_for_display);

  DeleteBackend();

  // Open the db manually again and make sure the new columns exist.
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    EXPECT_TRUE(db.DoesColumnExist("clusters_and_visits", "interaction_state"));
  }
}

TEST_F(HistoryBackendDBTest, MigrateVisitsAddExternalReferrerUrlColumn) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(65));

  const VisitID visit_id = 1;
  const URLID url_id = 2;
  const base::Time visit_time(base::Time::Now());
  const ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  const base::TimeDelta visit_duration(base::Seconds(45));

  const char kInsertStatement[] =
      "INSERT INTO visits "
      "(id, url, visit_time, transition, visit_duration) "
      "VALUES (?, ?, ?, ?, ?)";

  // Open the old version of the DB and make sure the new column doesn't exist
  // yet.
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    ASSERT_FALSE(db.DoesColumnExist("visits", "external_referrer_url"));

    // Add entry to visits.
    sql::Statement s(db.GetUniqueStatement(kInsertStatement));
    s.BindInt64(0, visit_id);
    s.BindInt64(1, url_id);
    s.BindInt64(2, visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
    s.BindInt64(3, transition);
    s.BindInt64(4, visit_duration.InMicroseconds());

    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 66);

  VisitRow visit_row;
  db_->GetRowForVisit(visit_id, &visit_row);
  EXPECT_TRUE(visit_row.external_referrer_url.is_empty());

  DeleteBackend();

  // Open the db manually again and make sure the new columns exist.
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    EXPECT_TRUE(db.DoesColumnExist("visits", "external_referrer_url"));
  }
}

TEST_F(HistoryBackendDBTest, MigrateVisitsAddVisitedLinkIdColumn) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(66));

  const VisitID visit_id = 1;
  const URLID url_id = 2;
  const base::Time visit_time(base::Time::Now());
  const ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  const base::TimeDelta visit_duration(base::Seconds(45));

  const char kInsertStatement[] =
      "INSERT INTO visits "
      "(id, url, visit_time, transition, visit_duration) "
      "VALUES (?, ?, ?, ?, ?)";

  // Open the old version of the DB and make sure the new column doesn't exist
  // yet.
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    ASSERT_FALSE(db.DoesColumnExist("visits", "visited_link_id"));

    // Add entry to visits.
    sql::Statement s(db.GetUniqueStatement(kInsertStatement));
    s.BindInt64(0, visit_id);
    s.BindInt64(1, url_id);
    s.BindInt64(2, visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
    s.BindInt64(3, transition);
    s.BindInt64(4, visit_duration.InMicroseconds());

    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 67);

  VisitRow visit_row;
  db_->GetRowForVisit(visit_id, &visit_row);
  EXPECT_EQ(visit_row.visited_link_id, 0);

  DeleteBackend();

  // Open the db manually again and make sure the new columns exist.
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    EXPECT_TRUE(db.DoesColumnExist("visits", "visited_link_id"));
  }
}

TEST_F(HistoryBackendDBTest, MigrateRemoveTypedUrlMetadataTable) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(67));

  // Open the old version of the DB and make sure the "typed_url_sync_metadata"
  // table exists.
  const char kTypedUrlMetadataTable[] = "typed_url_sync_metadata";
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    ASSERT_TRUE(db.DoesTableExist(kTypedUrlMetadataTable));
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 68);

  DeleteBackend();

  // Open the db manually again and make sure the table does not exist anymore.
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    EXPECT_FALSE(db.DoesTableExist(kTypedUrlMetadataTable));
  }
}

TEST_F(HistoryBackendDBTest, MigrateVisitsAddAppId) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(68));

  const VisitID visit_id = 1;
  const URLID url_id = 2;
  const base::Time visit_time(base::Time::Now());
  const ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  const base::TimeDelta visit_duration(base::Seconds(45));

  const char kInsertStatement[] =
      "INSERT INTO visits "
      "(id, url, visit_time, transition, visit_duration) "
      "VALUES (?, ?, ?, ?, ?)";

  // Open the old version of the DB and make sure the new column doesn't exist
  // yet.
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    ASSERT_FALSE(db.DoesColumnExist("visits", "app_id"));

    // Add entry to visits.
    sql::Statement s(db.GetUniqueStatement(kInsertStatement));
    s.BindInt64(0, visit_id);
    s.BindInt64(1, url_id);
    s.BindInt64(2, visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
    s.BindInt64(3, transition);
    s.BindInt64(4, visit_duration.InMicroseconds());
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 70);

  VisitRow visit_row;
  db_->GetRowForVisit(visit_id, &visit_row);
  EXPECT_FALSE(visit_row.app_id);

  DeleteBackend();

  // Open the db manually again and make sure the new columns exist.
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    EXPECT_TRUE(db.DoesColumnExist("visits", "app_id"));
  }
}

// ^^^ NEW MIGRATION TESTS GO HERE ^^^

// Preparation for the next DB migration: This test verifies that the test DB
// file for the current version exists and can be loaded.
// In the past, we only added a history.57.sql file to the repo while adding a
// migration to the NEXT version 58. That's confusing because then the developer
// has to reverse engineer what the migration for 57 was. This test looks like
// a no-op, but verifies that the test file for the current version always
// pre-exists, so adding the NEXT migration doesn't require reverse engineering.
// If you introduce a new migration, add a test for it above, and add a new
// history.n.sql file for the new DB layout so that this test keeps passing.
// SQL schemas can change without migrations, so make sure to verify the
// history.n-1.sql is up-to-date by re-creating. The flow to create a migration
// n should be:
// 1) There should already exist history.n-1.sql.
// 2) Re-create history.n-1.sql to make sure it hasn't changed since it was
//    created.
// 3) Add a migration test beginning with `CreateDBVersion(n-1)` and ending with
//    `ASSERT_GE(HistoryDatabase::GetCurrentVersion(), n);`
// 4) Create history.n.sql.
TEST_F(HistoryBackendDBTest, VerifyTestSQLFileForCurrentVersionAlreadyExists) {
  ASSERT_NO_FATAL_FAILURE(
      CreateDBVersion(HistoryDatabase::GetCurrentVersion()));
  CreateBackendAndDatabase();
}

bool FilterURL(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS();
}

TEST_F(HistoryBackendDBTest, QuerySegmentUsage) {
  CreateBackendAndDatabase();

  const GURL url1("file://bar");
  const GURL url2("http://www.foo.com");
  const int visit_count1 = 10;
  const int visit_count2 = 5;

  URLID url_id1 = db_->AddURL(URLRow(url1));
  ASSERT_NE(0, url_id1);
  URLID url_id2 = db_->AddURL(URLRow(url2));
  ASSERT_NE(0, url_id2);

  SegmentID segment_id1 = db_->CreateSegment(
      url_id1, VisitSegmentDatabase::ComputeSegmentName(url1));
  ASSERT_NE(0, segment_id1);
  SegmentID segment_id2 = db_->CreateSegment(
      url_id2, VisitSegmentDatabase::ComputeSegmentName(url2));
  ASSERT_NE(0, segment_id2);

  const base::Time now(base::Time::Now());
  ASSERT_TRUE(db_->UpdateSegmentVisitCount(segment_id1, now, visit_count1));
  ASSERT_TRUE(db_->UpdateSegmentVisitCount(segment_id2, now, visit_count2));
  const base::Time two_days_ago = now - base::Days(2);
  ASSERT_TRUE(
      db_->UpdateSegmentVisitCount(segment_id1, two_days_ago, visit_count1));
  ASSERT_TRUE(
      db_->UpdateSegmentVisitCount(segment_id2, two_days_ago, visit_count2));

  // Without a filter, the "file://" URL should win.
  std::vector<std::unique_ptr<PageUsageData>> results =
      db_->QuerySegmentUsage(/*max_result_count=*/1, base::NullCallback());
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(url1, results[0]->GetURL());
  EXPECT_EQ(segment_id1, results[0]->GetID());
  EXPECT_EQ(now.LocalMidnight(), results[0]->GetLastVisitTimeslot());
  EXPECT_EQ(visit_count1 * 2, results[0]->GetVisitCount());

  // With the filter, the "file://" URL should be filtered out, so the "http://"
  // URL should win instead.
  std::vector<std::unique_ptr<PageUsageData>> results2 = db_->QuerySegmentUsage(
      /*max_result_count=*/1, base::BindRepeating(&FilterURL));
  ASSERT_EQ(1u, results2.size());
  EXPECT_EQ(url2, results2[0]->GetURL());
  EXPECT_EQ(segment_id2, results2[0]->GetID());
  EXPECT_EQ(now.LocalMidnight(), results2[0]->GetLastVisitTimeslot());
  EXPECT_EQ(visit_count2 * 2, results2[0]->GetVisitCount());
}

TEST_F(HistoryBackendDBTest, QuerySegmentUsageReturnsNothingForZeroVisits) {
  CreateBackendAndDatabase();

  const GURL url("http://www.foo.com");
  const base::Time time(base::Time::Now());

  URLID url_id = db_->AddURL(URLRow(url));
  ASSERT_NE(0, url_id);

  SegmentID segment_id =
      db_->CreateSegment(url_id, VisitSegmentDatabase::ComputeSegmentName(url));
  ASSERT_NE(0, segment_id);
  ASSERT_TRUE(db_->UpdateSegmentVisitCount(segment_id, time, 0));

  std::vector<std::unique_ptr<PageUsageData>> results =
      db_->QuerySegmentUsage(/*max_result_count=*/1, base::NullCallback());
  EXPECT_TRUE(results.empty());
}

TEST_F(HistoryBackendDBTest,
       QuerySegmentUsageWithWindowSecondarySortsByLastVisit) {
  CreateBackendAndDatabase();

  const GURL url1("http://www.bar.com");
  const GURL url2("http://www.foo.com");
  const GURL url3("http://www.cat.com");
  const GURL url4("http://www.relevantsite.com");
  const GURL url5("http://www.anotherone.com");
  const int visit_count1 = 2;
  const int visit_count2 = 1;
  const int visit_count3 = 3;
  const int visit_count4 = 5;
  const int visit_count5 = 8;

  URLID url_id1 = db_->AddURL(URLRow(url1));
  ASSERT_NE(0, url_id1);
  URLID url_id2 = db_->AddURL(URLRow(url2));
  ASSERT_NE(0, url_id2);
  URLID url_id3 = db_->AddURL(URLRow(url3));
  ASSERT_NE(0, url_id3);
  URLID url_id4 = db_->AddURL(URLRow(url4));
  ASSERT_NE(0, url_id4);
  URLID url_id5 = db_->AddURL(URLRow(url5));
  ASSERT_NE(0, url_id5);

  SegmentID segment_id1 = db_->CreateSegment(
      url_id1, VisitSegmentDatabase::ComputeSegmentName(url1));
  ASSERT_NE(0, segment_id1);
  SegmentID segment_id2 = db_->CreateSegment(
      url_id2, VisitSegmentDatabase::ComputeSegmentName(url2));
  ASSERT_NE(0, segment_id2);
  SegmentID segment_id3 = db_->CreateSegment(
      url_id3, VisitSegmentDatabase::ComputeSegmentName(url3));
  ASSERT_NE(0, segment_id3);
  SegmentID segment_id4 = db_->CreateSegment(
      url_id4, VisitSegmentDatabase::ComputeSegmentName(url4));
  ASSERT_NE(0, segment_id4);
  SegmentID segment_id5 = db_->CreateSegment(
      url_id5, VisitSegmentDatabase::ComputeSegmentName(url5));
  ASSERT_NE(0, segment_id5);

  // Since times are normalized to local midnight, make sure these are over
  // a day apart.
  const base::Time url_time_1(base::Time::FromTimeT(200000));
  const base::Time url_time_2(base::Time::FromTimeT(100000));
  const base::Time url_time_3(base::Time::FromTimeT(300000));
  const base::Time url_time_4(base::Time::Now());
  const base::Time url_time_5(base::Time::Now());
  ASSERT_TRUE(
      db_->UpdateSegmentVisitCount(segment_id1, url_time_1, visit_count1));
  ASSERT_TRUE(
      db_->UpdateSegmentVisitCount(segment_id2, url_time_2, visit_count2));
  ASSERT_TRUE(
      db_->UpdateSegmentVisitCount(segment_id3, url_time_3, visit_count3));
  ASSERT_TRUE(
      db_->UpdateSegmentVisitCount(segment_id4, url_time_4, visit_count4));
  ASSERT_TRUE(
      db_->UpdateSegmentVisitCount(segment_id5, url_time_5, visit_count5));

  std::vector<std::unique_ptr<PageUsageData>> results = db_->QuerySegmentUsage(
      /*max_result_count=*/5, base::NullCallback(),
      /*recency_factor_name=*/std::nullopt,
      /*recency_window_days=*/0);

  ASSERT_EQ(5u, results.size());

  // Sites older than recency window should be scored 0.
  EXPECT_THAT(results[2]->GetScore(), 0);
  EXPECT_THAT(results[3]->GetScore(), 0);
  EXPECT_THAT(results[4]->GetScore(), 0);

  EXPECT_THAT(results[0]->GetURL(), url5);
  EXPECT_THAT(results[1]->GetURL(), url4);
  // 0 scored sites should be sorted based on `last_visit_time`.
  EXPECT_THAT(results[2]->GetURL(), url3);
  EXPECT_THAT(results[3]->GetURL(), url1);
  EXPECT_THAT(results[4]->GetURL(), url2);
}

}  // namespace
}  // namespace history
