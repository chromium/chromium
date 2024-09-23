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

#include "components/history/core/browser/history_backend.h"

#include <stdint.h>

#include <cstdint>
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
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/page_usage_data.h"
#include "components/history/core/test/history_backend_db_base_test.h"
#include "components/history/core/test/test_history_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {
namespace {

// This must be outside the anonymous namespace for the friend statement in
// HistoryBackend to work.
class HistoryBackendDBTest : public HistoryBackendDBBaseTest {
 public:
  HistoryBackendDBTest() {}
  ~HistoryBackendDBTest() override {}
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

TEST_F(HistoryBackendDBTest, MigrateDownloadsState) {
  // Create the db we want.
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));
  {
    // Open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Manually insert corrupted rows; there's infrastructure in place now to
    // make this impossible, at least according to the test above.
    for (int state = 0; state < 5; ++state) {
      sql::Statement s(db.GetUniqueStatement(
            "INSERT INTO downloads (id, full_path, url, start_time, "
            "received_bytes, total_bytes, state, end_time, opened) VALUES "
            "(?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1 + state);
      s.BindString(1, "path");
      s.BindString(2, "url");
      s.BindInt64(3, base::Time::Now().ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, state);
      s.BindInt64(7, base::Time::Now().ToTimeT());
      s.BindInt(8, state % 2);
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db using the HistoryDatabase, which should migrate from version
  // 22 to the current version, fixing just the row whose state was 3.
  // Then close the db so that we can re-open it directly.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      // The version should have been updated.
      int cur_version = HistoryDatabase::GetCurrentVersion();
      ASSERT_LT(22, cur_version);
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement statement(db.GetUniqueStatement(
          "SELECT id, state, opened "
          "FROM downloads "
          "ORDER BY id"));
      int counter = 0;
      while (statement.Step()) {
        EXPECT_EQ(1 + counter, statement.ColumnInt64(0));
        // The only thing that migration should have changed was state from 3 to
        // 4.
        EXPECT_EQ(((counter == 3) ? 4 : counter), statement.ColumnInt(1));
        EXPECT_EQ(counter % 2, statement.ColumnInt(2));
        ++counter;
      }
      EXPECT_EQ(5, counter);
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadsReasonPathsAndDangerType) {
  base::Time now(base::Time::Now());

  // Create the db we want.  The schema didn't change from 22->23, so just
  // re-use the v22 file.
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));
  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Manually insert some rows.
    sql::Statement s(db.GetUniqueStatement(
        "INSERT INTO downloads (id, full_path, url, start_time, "
        "received_bytes, total_bytes, state, end_time, opened) VALUES "
        "(?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    int64_t id = 0;
    // Null path.
    s.BindInt64(0, ++id);
    s.BindString(1, std::string());
    s.BindString(2, "http://whatever.com/index.html");
    s.BindInt64(3, now.ToTimeT());
    s.BindInt64(4, 100);
    s.BindInt64(5, 100);
    s.BindInt(6, 1);
    s.BindInt64(7, now.ToTimeT());
    s.BindInt(8, 1);
    ASSERT_TRUE(s.Run());
    s.Reset(true);

    // Non-null path.
    s.BindInt64(0, ++id);
    s.BindString(1, "/path/to/some/file");
    s.BindString(2, "http://whatever.com/index1.html");
    s.BindInt64(3, now.ToTimeT());
    s.BindInt64(4, 100);
    s.BindInt64(5, 100);
    s.BindInt(6, 1);
    s.BindInt64(7, now.ToTimeT());
    s.BindInt(8, 1);
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db using the HistoryDatabase, which should migrate from version
  // 23 to 24, creating the new tables and creating the new path, reason,
  // and danger columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      // The version should have been updated.
      int cur_version = HistoryDatabase::GetCurrentVersion();
      ASSERT_LT(23, cur_version);
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      base::Time nowish(base::Time::FromTimeT(now.ToTimeT()));

      // Confirm downloads table is valid.
      sql::Statement statement(db.GetUniqueStatement(
          "SELECT id, interrupt_reason, current_path, target_path, "
          "       danger_type, start_time, end_time "
          "FROM downloads ORDER BY id"));
      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(1, statement.ColumnInt64(0));
      EXPECT_EQ(DownloadInterruptReasonToInt(kTestDownloadInterruptReasonNone),
                statement.ColumnInt(1));
      EXPECT_EQ("", statement.ColumnString(2));
      EXPECT_EQ("", statement.ColumnString(3));
      // Implicit dependence on value of kDangerTypeNotDangerous from
      // download_database.cc.
      EXPECT_EQ(0, statement.ColumnInt(4));
      EXPECT_EQ(nowish, statement.ColumnTime(5));
      EXPECT_EQ(nowish, statement.ColumnTime(6));

      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(2, statement.ColumnInt64(0));
      EXPECT_EQ(DownloadInterruptReasonToInt(kTestDownloadInterruptReasonNone),
                statement.ColumnInt(1));
      EXPECT_EQ("/path/to/some/file", statement.ColumnString(2));
      EXPECT_EQ("/path/to/some/file", statement.ColumnString(3));
      EXPECT_EQ(0, statement.ColumnInt(4));
      EXPECT_EQ(nowish, statement.ColumnTime(5));
      EXPECT_EQ(nowish, statement.ColumnTime(6));

      EXPECT_FALSE(statement.Step());
    }
    {
      // Confirm downloads_url_chains table is valid.
      sql::Statement statement(db.GetUniqueStatement(
          "SELECT id, chain_index, url FROM downloads_url_chains "
          " ORDER BY id, chain_index"));
      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(1, statement.ColumnInt64(0));
      EXPECT_EQ(0, statement.ColumnInt(1));
      EXPECT_EQ("http://whatever.com/index.html", statement.ColumnString(2));

      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(2, statement.ColumnInt64(0));
      EXPECT_EQ(0, statement.ColumnInt(1));
      EXPECT_EQ("http://whatever.com/index1.html", statement.ColumnString(2));

      EXPECT_FALSE(statement.Step());
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateReferrer) {
  base::Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement s(db.GetUniqueStatement(
        "INSERT INTO downloads (id, full_path, url, start_time, "
        "received_bytes, total_bytes, state, end_time, opened) VALUES "
        "(?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    int64_t db_handle = 0;
    s.BindInt64(0, ++db_handle);
    s.BindString(1, "full_path");
    s.BindString(2, "http://whatever.com/index.html");
    s.BindInt64(3, now.ToTimeT());
    s.BindInt64(4, 100);
    s.BindInt64(5, 100);
    s.BindInt(6, 1);
    s.BindInt64(7, now.ToTimeT());
    s.BindInt(8, 1);
    ASSERT_TRUE(s.Run());
  }
  // Re-open the db using the HistoryDatabase, which should migrate to version
  // 26, creating the referrer column.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(26, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT referrer from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadedByExtension) {
  base::Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(26));
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads (id, current_path, target_path, start_time, "
          "received_bytes, total_bytes, state, danger_type, interrupt_reason, "
          "end_time, opened, referrer) VALUES "
          "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1);
      s.BindString(1, "current_path");
      s.BindString(2, "target_path");
      s.BindInt64(3, now.ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, 1);
      s.BindInt(7, 0);
      s.BindInt(8, 0);
      s.BindInt64(9, now.ToTimeT());
      s.BindInt(10, 1);
      s.BindString(11, "referrer");
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(?, ?, ?)"));
      s.BindInt64(0, 4);
      s.BindInt64(1, 0);
      s.BindString(2, "url");
      ASSERT_TRUE(s.Run());
    }
  }
  // Re-open the db using the HistoryDatabase, which should migrate to version
  // 27, creating the by_ext_id and by_ext_name columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(27, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT by_ext_id, by_ext_name from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadValidators) {
  base::Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(27));
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads (id, current_path, target_path, start_time, "
          "received_bytes, total_bytes, state, danger_type, interrupt_reason, "
          "end_time, opened, referrer, by_ext_id, by_ext_name) VALUES "
          "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1);
      s.BindString(1, "current_path");
      s.BindString(2, "target_path");
      s.BindInt64(3, now.ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, 1);
      s.BindInt(7, 0);
      s.BindInt(8, 0);
      s.BindInt64(9, now.ToTimeT());
      s.BindInt(10, 1);
      s.BindString(11, "referrer");
      s.BindString(12, "by extension ID");
      s.BindString(13, "by extension name");
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(?, ?, ?)"));
      s.BindInt64(0, 4);
      s.BindInt64(1, 0);
      s.BindString(2, "url");
      ASSERT_TRUE(s.Run());
    }
  }
  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version, creating the etag and last_modified columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(28, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT etag, last_modified from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadMimeType) {
  base::Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(28));
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads (id, current_path, target_path, start_time, "
          "received_bytes, total_bytes, state, danger_type, interrupt_reason, "
          "end_time, opened, referrer, by_ext_id, by_ext_name, etag, "
          "last_modified) VALUES "
          "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1);
      s.BindString(1, "current_path");
      s.BindString(2, "target_path");
      s.BindInt64(3, now.ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, 1);
      s.BindInt(7, 0);
      s.BindInt(8, 0);
      s.BindInt64(9, now.ToTimeT());
      s.BindInt(10, 1);
      s.BindString(11, "referrer");
      s.BindString(12, "by extension ID");
      s.BindString(13, "by extension name");
      s.BindString(14, "etag");
      s.BindInt64(15, now.ToTimeT());
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(?, ?, ?)"));
      s.BindInt64(0, 4);
      s.BindInt64(1, 0);
      s.BindString(2, "url");
      ASSERT_TRUE(s.Run());
    }
  }
  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version, creating the mime_type abd original_mime_type columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(29, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT mime_type, original_mime_type from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

bool IsValidRFC4122Ver4GUID(const std::string& guid) {
  // `base::Uuid::ParseCaseInsensitive().is_valid()` doesn't restrict its
  // validation to version (or subtype) 4 GUIDs as described in RFC 4122. So we
  // check if `base::Uuid::ParseCaseInsensitive().is_valid()` thinks it's a
  // valid GUID first, and then check the additional constraints.
  //
  // * Bits 4-7 of time_hi_and_version should be set to 0b0100 == 4
  //   => guid[14] == '4'
  //
  // * Bits 6-7 of clk_seq_hi_res should be set to 0b10
  //   => guid[19] in {'8','9','A','B','a','b'}
  //
  // * All other bits should be random or pseudo random.
  //   => http://dilbert.com/strip/2001-10-25
  return base::Uuid::ParseCaseInsensitive(guid).is_valid() && guid[14] == '4' &&
         (guid[19] == '8' || guid[19] == '9' || guid[19] == 'A' ||
          guid[19] == 'B' || guid[19] == 'a' || guid[19] == 'b');
}

TEST_F(HistoryBackendDBTest, MigrateHashHttpMethodAndGenerateGuids) {
  const size_t kDownloadCount = 100;
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(29));
  base::Time now(base::Time::Now());
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // In testing, it appeared that constructing a query where all rows are
    // specified (i.e. looks like "INSERT INTO foo (...) VALUES (...),(...)")
    // performs much better than executing a cached query multiple times where
    // the query inserts only a single row per run (i.e. looks like "INSERT INTO
    // (...) VALUES (...)"). For 100 records, the latter took 19s on a
    // developer machine while the former inserted 100 records in ~400ms.
    std::string download_insert_query =
        "INSERT INTO downloads (id, current_path, target_path, start_time, "
        "received_bytes, total_bytes, state, danger_type, interrupt_reason, "
        "end_time, opened, referrer, by_ext_id, by_ext_name, etag, "
        "last_modified, mime_type, original_mime_type) VALUES ";
    std::string url_insert_query =
        "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES ";

    for (uint32_t i = 0; i < kDownloadCount; ++i) {
      uint32_t download_id = i * 13321;
      if (i != 0)
        download_insert_query += ",";
      download_insert_query += base::StringPrintf(
          "(%" PRId64 ", 'current_path', 'target_path', %" PRId64
          ", 100, 100, 1, 0, 0, %" PRId64
          ", 1, 'referrer', 'by extension ID','by extension name', 'etag', "
          "'last modified', 'mime/type', 'original/mime-type')",
          static_cast<int64_t>(download_id),
          static_cast<int64_t>(now.ToTimeT()),
          static_cast<int64_t>(now.ToTimeT()));
      if (i != 0)
        url_insert_query += ",";
      url_insert_query += base::StringPrintf("(%" PRId64 ", 0, 'url')",
                                             static_cast<int64_t>(download_id));
    }
    ASSERT_TRUE(db.Execute(download_insert_query));
    ASSERT_TRUE(db.Execute(url_insert_query));
  }

  CreateBackendAndDatabase();
  DeleteBackend();

  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(30, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement("SELECT guid, id from downloads"));
      std::unordered_set<std::string> guids;
      while (s.Step()) {
        std::string guid = s.ColumnString(0);
        uint32_t id = static_cast<uint32_t>(s.ColumnInt64(1));
        EXPECT_TRUE(IsValidRFC4122Ver4GUID(guid));
        // Id is used as time_low in RFC 4122 to guarantee unique GUIDs
        EXPECT_EQ(guid.substr(0, 8), base::StringPrintf("%08" PRIX32, id));
        guids.insert(guid);
      }
      EXPECT_TRUE(s.Succeeded());
      EXPECT_EQ(kDownloadCount, guids.size());
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateTabUrls) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(30));
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads ("
          "    id, guid, current_path, target_path, start_time, received_bytes,"
          "    total_bytes, state, danger_type, interrupt_reason, hash,"
          "    end_time, opened, referrer, http_method, by_ext_id, by_ext_name,"
          "    etag, last_modified, mime_type, original_mime_type)"
          "VALUES("
          "    1, '435A5C7A-F6B7-4DF2-8696-22E4FCBA3EB2', 'foo.txt', 'foo.txt',"
          "    13104873187307670, 11, 11, 1, 0, 0, X'', 13104873187521021, 0,"
          "    'http://example.com/dl/', '', '', '', '', '', 'text/plain',"
          "    'text/plain')"));
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(1, 0, 'url')"));
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version, creating the tab_url and tab_referrer_url columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(31, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT tab_url, tab_referrer_url from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadSiteInstanceUrl) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(31));
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads ("
          "    id, guid, current_path, target_path, start_time, received_bytes,"
          "    total_bytes, state, danger_type, interrupt_reason, hash,"
          "    end_time, opened, referrer, tab_url, tab_referrer_url,"
          "    http_method, by_ext_id, by_ext_name, etag, last_modified,"
          "    mime_type, original_mime_type)"
          "VALUES("
          "    1, '435A5C7A-F6B7-4DF2-8696-22E4FCBA3EB2', 'foo.txt', 'foo.txt',"
          "    13104873187307670, 11, 11, 1, 0, 0, X'', 13104873187521021, 0,"
          "    'http://example.com/dl/', '', '', '', '', '', '', '',"
          "    'text/plain', 'text/plain')"));
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(1, 0, 'url')"));
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version, creating the site_url column.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(31, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement("SELECT site_url from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateEmbedderDownloadData) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(50));
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads ("
          "    id, guid, current_path, target_path, start_time, received_bytes,"
          "    total_bytes, state, danger_type, interrupt_reason, hash,"
          "    end_time, opened, last_access_time, transient, referrer, "
          "    site_url, tab_url, tab_referrer_url, http_method, by_ext_id, "
          "    by_ext_name, etag, last_modified, mime_type, original_mime_type)"
          "VALUES("
          "    1, '435A5C7A-F6B7-4DF2-8696-22E4FCBA3EB2', 'foo.txt', 'foo.txt',"
          "    13104873187307670, 11, 11, 1, 0, 0, X'', 13104873187521021, 0, "
          "    13104873187521021, 1, 'http://example.com/dl/',"
          "    'http://example.com', '', '', '', '', '', '', '',"
          "    'text/plain', 'text/plain')"));
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version, creating the embedder_download_data column.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(51, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT guid, embedder_download_data from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ("435A5C7A-F6B7-4DF2-8696-22E4FCBA3EB2", s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

// Tests that downloads_slices table are automatically added when migrating to
// version 33.
TEST_F(HistoryBackendDBTest, MigrateDownloadsSlicesTable) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(32));
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
  }

  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version, creating the downloads_slices table.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(32, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      // The downloads_slices table should be ready for use.
      sql::Statement s1(db.GetUniqueStatement(
          "SELECT COUNT(*) from downloads_slices"));
      EXPECT_TRUE(s1.Step());
      EXPECT_EQ(0, s1.ColumnInt(0));
      const char kInsertStatement[] = "INSERT INTO downloads_slices "
          "(download_id, offset, received_bytes) VALUES (1, 0, 100)";
      ASSERT_TRUE(db.Execute(kInsertStatement));
    }
  }
}

// Tests that last access time and transient is automatically added when
// migrating to version 36.
TEST_F(HistoryBackendDBTest, MigrateDownloadsLastAccessTimeAndTransient) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(32));
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
  }

  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(35, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      // The downloads table should have last_access_time and transient
      // initialized to zero.
      sql::Statement s(db.GetUniqueStatement(
          "SELECT last_access_time, transient from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(base::Time(), s.ColumnTime(0));
      EXPECT_EQ(0, s.ColumnInt(1));
    }
  }
}

TEST_F(HistoryBackendDBTest, DownloadCreateAndQuery) {
  CreateBackendAndDatabase();

  ASSERT_EQ(0u, db_->CountDownloads());

  std::vector<GURL> url_chain;
  url_chain.push_back(GURL("http://example.com/a"));
  url_chain.push_back(GURL("http://example.com/b"));
  url_chain.push_back(GURL("http://example.com/c"));

  base::Time start_time(base::Time::Now());
  base::Time end_time(start_time + base::Hours(1));
  base::Time last_access_time;

  DownloadRow download_A;
  download_A.current_path = base::FilePath(FILE_PATH_LITERAL("/path/1"));
  download_A.target_path = base::FilePath(FILE_PATH_LITERAL("/path/2"));
  download_A.url_chain = url_chain;
  download_A.referrer_url = GURL("http://example.com/referrer");
  download_A.site_url = GURL("http://example.com");
  download_A.embedder_download_data = "embedder_download_data";
  download_A.tab_url = GURL("http://example.com/tab-url");
  download_A.tab_referrer_url = GURL("http://example.com/tab-referrer");
  download_A.http_method = "GET";
  download_A.mime_type = "mime/type";
  download_A.original_mime_type = "original/mime-type";
  download_A.start_time = start_time;
  download_A.end_time = end_time;
  download_A.etag = "etag1";
  download_A.last_modified = "last_modified_1";
  download_A.received_bytes = 100;
  download_A.total_bytes = 1000;
  download_A.state = DownloadState::INTERRUPTED;
  download_A.danger_type = DownloadDangerType::NOT_DANGEROUS;
  download_A.interrupt_reason = kTestDownloadInterruptReasonCrash;
  download_A.hash = "hash-value1";
  download_A.id = 1;
  download_A.guid = "FE672168-26EF-4275-A149-FEC25F6A75F9";
  download_A.opened = false;
  download_A.last_access_time = last_access_time;
  download_A.transient = true;
  download_A.by_ext_id = "extension-id";
  download_A.by_ext_name = "extension-name";
  download_A.by_web_app_id = "web-app-id";

  ASSERT_TRUE(db_->CreateDownload(download_A));

  url_chain.push_back(GURL("http://example.com/d"));

  base::Time start_time2(start_time + base::Hours(10));
  base::Time end_time2(end_time + base::Hours(10));
  base::Time last_access_time2(start_time2 + base::Hours(5));

  DownloadRow download_B;
  download_B.current_path = base::FilePath(FILE_PATH_LITERAL("/path/3"));
  download_B.target_path = base::FilePath(FILE_PATH_LITERAL("/path/4"));
  download_B.url_chain = url_chain;
  download_B.referrer_url = GURL("http://example.com/referrer2");
  download_B.site_url = GURL("http://2.example.com");
  download_B.embedder_download_data = "embedder_download_data2";
  download_B.tab_url = GURL("http://example.com/tab-url2");
  download_B.tab_referrer_url = GURL("http://example.com/tab-referrer2");
  download_B.http_method = "POST";
  download_B.mime_type = "mime/type2";
  download_B.original_mime_type = "original/mime-type2";
  download_B.start_time = start_time2;
  download_B.end_time = end_time2;
  download_B.etag = "etag2";
  download_B.last_modified = "last_modified_2";
  download_B.received_bytes = 1001;
  download_B.total_bytes = 1001;
  download_B.state = DownloadState::COMPLETE;
  download_B.danger_type = DownloadDangerType::DANGEROUS_FILE;
  download_B.interrupt_reason = kTestDownloadInterruptReasonNone;
  download_B.id = 2;
  download_B.guid = "b70f3869-7d75-4878-acb4-4caf7026d12b";
  download_B.opened = false;
  download_B.last_access_time = last_access_time2;
  download_B.transient = true;
  download_B.by_ext_id = "extension-id";
  download_B.by_ext_name = "extension-name";
  download_B.by_ext_name = "web-app-id";

  ASSERT_TRUE(db_->CreateDownload(download_B));

  EXPECT_EQ(2u, db_->CountDownloads());

  std::vector<DownloadRow> results;
  db_->QueryDownloads(&results);

  ASSERT_EQ(2u, results.size());

  const DownloadRow& retrieved_download_A =
      results[0].id == 1 ? results[0] : results[1];
  const DownloadRow& retrieved_download_B =
      results[0].id == 1 ? results[1] : results[0];

  EXPECT_EQ(download_A, retrieved_download_A);
  EXPECT_EQ(download_B, retrieved_download_B);
}

TEST_F(HistoryBackendDBTest, DownloadCreateAndUpdate_VolatileFields) {
  CreateBackendAndDatabase();

  std::vector<GURL> url_chain;
  url_chain.push_back(GURL("http://example.com/a"));
  url_chain.push_back(GURL("http://example.com/b"));
  url_chain.push_back(GURL("http://example.com/c"));

  base::Time start_time(base::Time::Now());
  base::Time end_time(start_time + base::Hours(1));
  base::Time last_access_time(start_time + base::Hours(5));

  DownloadRow download;
  download.current_path = base::FilePath(FILE_PATH_LITERAL("/path/1"));
  download.target_path = base::FilePath(FILE_PATH_LITERAL("/path/2"));
  download.url_chain = url_chain;
  download.referrer_url = GURL("http://example.com/referrer");
  download.site_url = GURL("http://example.com");
  download.embedder_download_data = "embedder_download_data";
  download.tab_url = GURL("http://example.com/tab-url");
  download.tab_referrer_url = GURL("http://example.com/tab-referrer");
  download.http_method = "GET";
  download.mime_type = "mime/type";
  download.original_mime_type = "original/mime-type";
  download.start_time = start_time;
  download.end_time = end_time;
  download.etag = "etag1";
  download.last_modified = "last_modified_1";
  download.received_bytes = 100;
  download.total_bytes = 1000;
  download.state = DownloadState::INTERRUPTED;
  download.danger_type = DownloadDangerType::NOT_DANGEROUS;
  download.interrupt_reason = 3;
  download.hash = "some-hash-value";
  download.id = 1;
  download.guid = "FE672168-26EF-4275-A149-FEC25F6A75F9";
  download.opened = false;
  download.last_access_time = last_access_time;
  download.transient = false;
  download.by_ext_id = "extension-id";
  download.by_ext_name = "extension-name";
  download.by_web_app_id = "web-app-id";
  db_->CreateDownload(download);

  download.current_path =
      base::FilePath(FILE_PATH_LITERAL("/new/current_path"));
  download.target_path = base::FilePath(FILE_PATH_LITERAL("/new/target_path"));
  download.mime_type = "new/mime/type";
  download.original_mime_type = "new/original/mime/type";
  download.received_bytes += 1000;
  download.state = DownloadState::CANCELLED;
  download.danger_type = DownloadDangerType::USER_VALIDATED;
  download.interrupt_reason = 4;
  download.end_time += base::Hours(1);
  download.total_bytes += 1;
  download.hash = "some-other-hash";
  download.opened = !download.opened;
  download.transient = !download.transient;
  download.by_ext_id = "by-new-extension-id";
  download.by_ext_name = "by-new-extension-name";
  download.by_web_app_id = "by-new-web-app-id";
  download.etag = "new-etag";
  download.last_modified = "new-last-modified";

  ASSERT_TRUE(db_->UpdateDownload(download));

  std::vector<DownloadRow> results;
  db_->QueryDownloads(&results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(download, results[0]);
}

TEST_F(HistoryBackendDBTest, ConfirmDownloadRowCreateAndDelete) {
  // Create the DB.
  CreateBackendAndDatabase();

  base::Time now(base::Time::Now());

  // Add some downloads.
  uint32_t id1 = 1, id2 = 2, id3 = 3;
  AddDownload(id1,
              "05AF6C8E-E4E0-45D7-B5CE-BC99F7019918",
              DownloadState::COMPLETE,
              now);
  std::vector<DownloadRow> results;
  db_->QueryDownloads(&results);
  ASSERT_EQ(1u, results.size());
  // Add a download slice and update the DB
  results[0].download_slice_info.push_back(
      DownloadSliceInfo(id1, 500, 100, false));
  ASSERT_TRUE(db_->UpdateDownload(results[0]));

  AddDownload(id2, "05AF6C8E-E4E0-45D7-B5CE-BC99F7019919",
              DownloadState::COMPLETE, now + base::Days(2));
  AddDownload(id3, "05AF6C8E-E4E0-45D7-B5CE-BC99F701991A",
              DownloadState::COMPLETE, now - base::Days(2));

  // Confirm that resulted in the correct number of rows in the DB.
  DeleteBackend();
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    ASSERT_TRUE(statement.Step());
    EXPECT_EQ(3, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select Count(*) from downloads_url_chains"));
    ASSERT_TRUE(statement1.Step());
    EXPECT_EQ(3, statement1.ColumnInt(0));

    sql::Statement statement2(db.GetUniqueStatement(
        "Select Count(*) from downloads_slices"));
    ASSERT_TRUE(statement2.Step());
    EXPECT_EQ(1, statement2.ColumnInt(0));
  }

  // Delete some rows and make sure the results are still correct.
  CreateBackendAndDatabase();
  db_->RemoveDownload(id1);
  db_->RemoveDownload(id2);
  DeleteBackend();
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    ASSERT_TRUE(statement.Step());
    EXPECT_EQ(1, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select Count(*) from downloads_url_chains"));
    ASSERT_TRUE(statement1.Step());
    EXPECT_EQ(1, statement1.ColumnInt(0));

    sql::Statement statement2(db.GetUniqueStatement(
        "Select Count(*) from downloads_slices"));
    ASSERT_TRUE(statement2.Step());
    EXPECT_EQ(0, statement2.ColumnInt(0));
  }
}

TEST_F(HistoryBackendDBTest, DownloadNukeRecordsMissingURLs) {
  CreateBackendAndDatabase();
  base::Time now(base::Time::Now());

  DownloadRow download;
  download.current_path = base::FilePath(FILE_PATH_LITERAL("foo-path"));
  download.target_path = base::FilePath(FILE_PATH_LITERAL("foo-path"));
  download.mime_type = "application/octet-stream";
  download.original_mime_type = "application/octet-stream";
  download.start_time = now;
  download.end_time = now;
  download.received_bytes = 0;
  download.total_bytes = 512;
  download.state = DownloadState::COMPLETE;
  download.danger_type = DownloadDangerType::NOT_DANGEROUS;
  download.interrupt_reason = kTestDownloadInterruptReasonNone;
  download.id = 1;
  download.guid = "05AF6C8E-E4E0-45D7-B5CE-BC99F7019918";
  download.opened = false;
  download.last_access_time = now;
  download.transient = false;
  download.by_ext_id = "by_ext_id";
  download.by_ext_name = "by_ext_name";
  download.by_web_app_id = "by_web_app_id";

  // Creating records without any urls should fail.
  EXPECT_FALSE(db_->CreateDownload(download));

  download.url_chain.push_back(GURL("foo-url"));
  EXPECT_TRUE(db_->CreateDownload(download));

  // Pretend that the URLs were dropped.
  DeleteBackend();
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "DELETE FROM downloads_url_chains WHERE id=1"));
    ASSERT_TRUE(statement.Run());
  }
  CreateBackendAndDatabase();
  std::vector<DownloadRow> downloads;
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());

  // QueryDownloads should have nuked the corrupt record.
  DeleteBackend();
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement statement(db.GetUniqueStatement(
            "SELECT count(*) from downloads"));
      ASSERT_TRUE(statement.Step());
      EXPECT_EQ(0, statement.ColumnInt(0));
    }
  }
}

TEST_F(HistoryBackendDBTest, ConfirmDownloadInProgressCleanup) {
  // Create the DB.
  CreateBackendAndDatabase();

  base::Time now(base::Time::Now());

  // Put an IN_PROGRESS download in the DB.
  DownloadId id = 1;
  AddDownload(id,
              "05AF6C8E-E4E0-45D7-B5CE-BC99F7019918",
              DownloadState::IN_PROGRESS,
              now);
  std::vector<DownloadRow> results;
  db_->QueryDownloads(&results);
  ASSERT_EQ(1u, results.size());
  // Add a download slice and update the DB
  results[0].download_slice_info.push_back(
      DownloadSliceInfo(id, 500, 100, true));
  ASSERT_TRUE(db_->UpdateDownload(results[0]));

  // Confirm that they made it into the DB unchanged.
  DeleteBackend();
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(1, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select state, interrupt_reason from downloads"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(DownloadStateToInt(DownloadState::IN_PROGRESS),
              statement1.ColumnInt(0));
    EXPECT_EQ(DownloadInterruptReasonToInt(kTestDownloadInterruptReasonNone),
              statement1.ColumnInt(1));
    EXPECT_FALSE(statement1.Step());
  }

  // Read in the DB through query downloads, then test that the
  // right transformation was returned.
  CreateBackendAndDatabase();
  db_->QueryDownloads(&results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(DownloadState::INTERRUPTED, results[0].state);
  EXPECT_EQ(kTestDownloadInterruptReasonCrash, results[0].interrupt_reason);

  // Allow the update to propagate, shut down the DB, and confirm that
  // the query updated the on disk database as well.
  base::RunLoop().RunUntilIdle();
  DeleteBackend();
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(1, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select state, interrupt_reason from downloads"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(DownloadStateToInt(DownloadState::INTERRUPTED),
              statement1.ColumnInt(0));
    EXPECT_EQ(DownloadInterruptReasonToInt(kTestDownloadInterruptReasonCrash),
              statement1.ColumnInt(1));
    EXPECT_FALSE(statement1.Step());
  }
}

TEST_F(HistoryBackendDBTest, CreateAndUpdateDownloadingSlice) {
  CreateBackendAndDatabase();

  DownloadRow download;
  download.current_path = base::FilePath(FILE_PATH_LITERAL("/path/1"));
  download.target_path = base::FilePath(FILE_PATH_LITERAL("/path/2"));
  download.url_chain.push_back(GURL("http://example.com/a"));
  download.referrer_url = GURL("http://example.com/referrer");
  download.site_url = GURL("http://example.com");
  download.embedder_download_data = "embedder_download_data";
  download.tab_url = GURL("http://example.com/tab-url");
  download.tab_referrer_url = GURL("http://example.com/tab-referrer");
  download.http_method = "GET";
  download.mime_type = "mime/type";
  download.original_mime_type = "original/mime-type";
  download.start_time = base::Time::Now();
  download.end_time = download.start_time + base::Hours(1);
  download.etag = "etag1";
  download.last_modified = "last_modified_1";
  download.received_bytes = 10;
  download.total_bytes = 1500;
  download.state = DownloadState::INTERRUPTED;
  download.danger_type = DownloadDangerType::NOT_DANGEROUS;
  download.interrupt_reason = kTestDownloadInterruptReasonCrash;
  download.hash = "hash-value1";
  download.id = 1;
  download.guid = "FE672168-26EF-4275-A149-FEC25F6A75F9";
  download.opened = false;
  download.last_access_time = download.start_time + base::Hours(5);
  download.transient = false;
  download.by_ext_id = "extension-id";
  download.by_ext_name = "extension-name";
  download.by_web_app_id = "web-app-id";
  download.download_slice_info.push_back(
      DownloadSliceInfo(download.id, 500, download.received_bytes, true));

  ASSERT_TRUE(db_->CreateDownload(download));
  std::vector<DownloadRow> results;
  db_->QueryDownloads(&results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(download, results[0]);

  download.received_bytes += 10;
  download.download_slice_info[0].received_bytes = download.received_bytes;
  ASSERT_TRUE(db_->UpdateDownload(download));
  db_->QueryDownloads(&results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(download, results[0]);
}

// Test calling UpdateDownload with a new download slice.
TEST_F(HistoryBackendDBTest, UpdateDownloadWithNewSlice) {
  CreateBackendAndDatabase();

  DownloadRow download;
  download.current_path = base::FilePath(FILE_PATH_LITERAL("/path/1"));
  download.target_path = base::FilePath(FILE_PATH_LITERAL("/path/2"));
  download.url_chain.push_back(GURL("http://example.com/a"));
  download.referrer_url = GURL("http://example.com/referrer");
  download.site_url = GURL("http://example.com");
  download.embedder_download_data = "embedder_download_data";
  download.tab_url = GURL("http://example.com/tab-url");
  download.tab_referrer_url = GURL("http://example.com/tab-referrer");
  download.http_method = "GET";
  download.mime_type = "mime/type";
  download.original_mime_type = "original/mime-type";
  download.start_time = base::Time::Now();
  download.end_time = download.start_time + base::Hours(1);
  download.etag = "etag1";
  download.last_modified = "last_modified_1";
  download.received_bytes = 0;
  download.total_bytes = 1500;
  download.state = DownloadState::INTERRUPTED;
  download.danger_type = DownloadDangerType::NOT_DANGEROUS;
  download.interrupt_reason = kTestDownloadInterruptReasonCrash;
  download.hash = "hash-value1";
  download.id = 1;
  download.guid = "FE672168-26EF-4275-A149-FEC25F6A75F9";
  download.opened = false;
  download.last_access_time = download.start_time + base::Hours(5);
  download.transient = true;
  download.by_ext_id = "extension-id";
  download.by_ext_name = "extension-name";
  download.by_web_app_id = "web-app-id";

  ASSERT_TRUE(db_->CreateDownload(download));

  // Add a new slice and call UpdateDownload().
  download.download_slice_info.push_back(
      DownloadSliceInfo(download.id, 500, 100, true));
  ASSERT_TRUE(db_->UpdateDownload(download));
  std::vector<DownloadRow> results;
  db_->QueryDownloads(&results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(download.download_slice_info[0], results[0].download_slice_info[0]);
}

TEST_F(HistoryBackendDBTest, DownloadSliceDeletedIfEmpty) {
  CreateBackendAndDatabase();

  DownloadRow download;
  download.current_path = base::FilePath(FILE_PATH_LITERAL("/path/1"));
  download.target_path = base::FilePath(FILE_PATH_LITERAL("/path/2"));
  download.url_chain.push_back(GURL("http://example.com/a"));
  download.referrer_url = GURL("http://example.com/referrer");
  download.site_url = GURL("http://example.com");
  download.embedder_download_data = "embedder_download_data";
  download.tab_url = GURL("http://example.com/tab-url");
  download.tab_referrer_url = GURL("http://example.com/tab-referrer");
  download.http_method = "GET";
  download.mime_type = "mime/type";
  download.original_mime_type = "original/mime-type";
  download.start_time = base::Time::Now();
  download.end_time = download.start_time + base::Hours(1);
  download.etag = "etag1";
  download.last_modified = "last_modified_1";
  download.received_bytes = 10;
  download.total_bytes = 1500;
  download.state = DownloadState::INTERRUPTED;
  download.danger_type = DownloadDangerType::NOT_DANGEROUS;
  download.interrupt_reason = kTestDownloadInterruptReasonCrash;
  download.hash = "hash-value1";
  download.id = 1;
  download.guid = "FE672168-26EF-4275-A149-FEC25F6A75F9";
  download.opened = false;
  download.last_access_time = download.start_time + base::Hours(5);
  download.transient = true;
  download.by_ext_id = "extension-id";
  download.by_ext_name = "extension-name";
  download.by_web_app_id = "web-app-id";
  download.download_slice_info.push_back(
      DownloadSliceInfo(download.id, 0, download.received_bytes, false));
  download.download_slice_info.push_back(
      DownloadSliceInfo(download.id, 500, download.received_bytes, false));
  download.download_slice_info.push_back(
      DownloadSliceInfo(download.id, 100, download.received_bytes, false));
  // The empty slice will not be inserted.
  download.download_slice_info.push_back(
      DownloadSliceInfo(download.id, 1500, 0, true));

  ASSERT_TRUE(db_->CreateDownload(download));
  std::vector<DownloadRow> results;
  db_->QueryDownloads(&results);
  ASSERT_EQ(1u, results.size());
  // Only 3 slices are inserted.
  EXPECT_EQ(3u, results[0].download_slice_info.size());

  // If slice info vector is empty, all slice entries will be removed.
  download.download_slice_info.clear();
  ASSERT_TRUE(db_->UpdateDownload(download));
  db_->QueryDownloads(&results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(0u, results[0].download_slice_info.size());
}

// Test that the web app responsible for a download is recorded.
TEST_F(HistoryBackendDBTest, UpdateDownloadByWebApp) {
  CreateBackendAndDatabase();

  DownloadRow download;
  download.current_path = base::FilePath(FILE_PATH_LITERAL("/path/1"));
  download.target_path = base::FilePath(FILE_PATH_LITERAL("/path/2"));
  download.url_chain.push_back(GURL("http://example.com/a"));
  download.referrer_url = GURL("http://example.com/referrer");
  download.site_url = GURL("http://example.com");
  download.embedder_download_data = "embedder_download_data";
  download.tab_url = GURL("http://example.com/tab-url");
  download.tab_referrer_url = GURL("http://example.com/tab-referrer");
  download.http_method = "GET";
  download.mime_type = "mime/type";
  download.original_mime_type = "original/mime-type";
  download.start_time = base::Time::Now();
  download.end_time = download.start_time + base::Hours(1);
  download.etag = "etag1";
  download.last_modified = "last_modified_1";
  download.received_bytes = 0;
  download.total_bytes = 1500;
  download.state = DownloadState::INTERRUPTED;
  download.danger_type = DownloadDangerType::NOT_DANGEROUS;
  download.interrupt_reason = kTestDownloadInterruptReasonCrash;
  download.hash = "hash-value1";
  download.id = 1;
  download.guid = "FE672168-26EF-4275-A149-FEC25F6A75F9";
  download.opened = false;
  download.last_access_time = download.start_time + base::Hours(5);
  download.transient = true;
  download.by_ext_id = "extension-id";
  download.by_ext_name = "extension-name";
  download.by_web_app_id = "web-app-id";

  ASSERT_TRUE(db_->CreateDownload(download));

  // Add a new web app id and call UpdateDownload().
  download.by_web_app_id = "new_web_app_id";
  ASSERT_TRUE(db_->UpdateDownload(download));
  std::vector<DownloadRow> results;
  db_->QueryDownloads(&results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(download.by_web_app_id, results[0].by_web_app_id);
}

TEST_F(HistoryBackendDBTest, MigratePresentations) {
  // Create the db we want. Use 22 since segments didn't change in that time
  // frame.
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));

  const SegmentID segment_id = 2;
  const URLID url_id = 3;
  const GURL url("http://www.foo.com");
  const std::string url_name(VisitSegmentDatabase::ComputeSegmentName(url));
  const std::u16string title(u"Title1");
  const base::Time segment_time(base::Time::Now());

  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Add an entry to urls.
    {
      sql::Statement s(db.GetUniqueStatement(
                           "INSERT INTO urls "
                           "(id, url, title, last_visit_time) VALUES "
                           "(?, ?, ?, ?)"));
      s.BindInt64(0, url_id);
      s.BindString(1, url.spec());
      s.BindString16(2, title);
      s.BindTime(3, segment_time);
      ASSERT_TRUE(s.Run());
    }

    // Add an entry to segments.
    {
      sql::Statement s(db.GetUniqueStatement(
                           "INSERT INTO segments "
                           "(id, name, url_id, pres_index) VALUES "
                           "(?, ?, ?, ?)"));
      s.BindInt64(0, segment_id);
      s.BindString(1, url_name);
      s.BindInt64(2, url_id);
      s.BindInt(3, 4);  // pres_index
      ASSERT_TRUE(s.Run());
    }

    // And one to segment_usage.
    {
      sql::Statement s(db.GetUniqueStatement(
                           "INSERT INTO segment_usage "
                           "(id, segment_id, time_slot, visit_count) VALUES "
                           "(?, ?, ?, ?)"));
      s.BindInt64(0, 4);  // id.
      s.BindInt64(1, segment_id);
      s.BindTime(2, segment_time);
      s.BindInt(3, 5);  // visit count.
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  std::vector<std::unique_ptr<PageUsageData>> results =
      db_->QuerySegmentUsage(/*max_result_count=*/10, base::NullCallback());
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(url, results[0]->GetURL());
  EXPECT_EQ(segment_id, results[0]->GetID());
  EXPECT_EQ(title, results[0]->GetTitle());
}

TEST_F(HistoryBackendDBTest, CheckLastCompatibleVersion) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(28));
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      // Manually set last compatible version to one higher
      // than current version.
      sql::MetaTable meta;
      ASSERT_TRUE(meta.Init(&db, 1, 1));
      ASSERT_TRUE(meta.SetCompatibleVersionNumber(
          HistoryDatabase::GetCurrentVersion() + 1));
    }
  }
  // Try to create and init backend for non compatible db.
  // Allow failure in backend creation.
  CreateBackendAndDatabaseAllowFail();
  DeleteBackend();

  // Check that error delegate was called with correct init error status.
  EXPECT_EQ(sql::INIT_TOO_NEW, last_profile_error_);
  {
    // Re-open the db to check that it was not migrated.
    // Non compatible DB must be ignored.
    // Check that DB version in file remains the same.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::MetaTable meta;
      ASSERT_TRUE(meta.Init(&db, 1, 1));
      // Current browser version must be already higher than 28.
      ASSERT_LT(28, HistoryDatabase::GetCurrentVersion());
      // Expect that version in DB remains the same.
      EXPECT_EQ(28, meta.GetVersionNumber());
    }
  }
}

// Tests that visit segment names are recomputed and segments merged when
// migrating to version 37.
TEST_F(HistoryBackendDBTest, MigrateVisitSegmentNames) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(32));

  const SegmentID segment_id1 = 7;
  const SegmentID segment_id2 = 8;
  const URLID url_id1 = 3;
  const URLID url_id2 = 4;
  const GURL url1("http://www.foo.com");
  const GURL url2("http://m.foo.com");
  const std::string legacy_segment_name1("http://foo.com/");
  const std::string legacy_segment_name2("http://m.foo.com/");
  const std::u16string title1(u"Title1");
  const std::u16string title2(u"Title2");
  const base::Time segment_time(base::Time::Now());

  {
    // Open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Add first entry to urls.
    {
      sql::Statement s(
          db.GetUniqueStatement("INSERT INTO urls "
                                "(id, url, title, last_visit_time) VALUES "
                                "(?, ?, ?, ?)"));
      s.BindInt64(0, url_id1);
      s.BindString(1, url1.spec());
      s.BindString16(2, title1);
      s.BindTime(3, segment_time);
      ASSERT_TRUE(s.Run());
    }

    // Add first entry to segments.
    {
      sql::Statement s(
          db.GetUniqueStatement("INSERT INTO segments "
                                "(id, name, url_id) VALUES "
                                "(?, ?, ?)"));
      s.BindInt64(0, segment_id1);
      s.BindString(1, legacy_segment_name1);
      s.BindInt64(2, url_id1);
      ASSERT_TRUE(s.Run());
    }

    // And first to segment_usage.
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO segment_usage "
          "(id, segment_id, time_slot, visit_count) VALUES "
          "(?, ?, ?, ?)"));
      s.BindInt64(0, 4);  // id.
      s.BindInt64(1, segment_id1);
      s.BindTime(2, segment_time);
      s.BindInt(3, 11);  // visit count.
      ASSERT_TRUE(s.Run());
    }

    // Add second entry to urls.
    {
      sql::Statement s(
          db.GetUniqueStatement("INSERT INTO urls "
                                "(id, url, title, last_visit_time) VALUES "
                                "(?, ?, ?, ?)"));
      s.BindInt64(0, url_id2);
      s.BindString(1, url2.spec());
      s.BindString16(2, title2);
      s.BindTime(3, segment_time);
      ASSERT_TRUE(s.Run());
    }

    // Add second entry to segments.
    {
      sql::Statement s(
          db.GetUniqueStatement("INSERT INTO segments "
                                "(id, name, url_id) VALUES "
                                "(?, ?, ?)"));
      s.BindInt64(0, segment_id2);
      s.BindString(1, legacy_segment_name2);
      s.BindInt64(2, url_id2);
      ASSERT_TRUE(s.Run());
    }

    // And second to segment_usage.
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO segment_usage "
          "(id, segment_id, time_slot, visit_count) VALUES "
          "(?, ?, ?, ?)"));
      s.BindInt64(0, 5);  // id.
      s.BindInt64(1, segment_id2);
      s.BindTime(2, segment_time);
      s.BindInt(3, 13);  // visit count.
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  std::vector<std::unique_ptr<PageUsageData>> results = db_->QuerySegmentUsage(
      /*max_result_count=*/10, base::NullCallback());
  ASSERT_EQ(1u, results.size());
  EXPECT_THAT(results[0]->GetURL(), testing::AnyOf(url1, url2));
  EXPECT_THAT(results[0]->GetTitle(), testing::AnyOf(title1, title2));
  EXPECT_EQ(segment_id1, db_->GetSegmentNamed(legacy_segment_name1));
  EXPECT_EQ(0u, db_->GetSegmentNamed(legacy_segment_name2));
}

// Test to verify the finished column will be correctly added to download slices
// table during migration to version 39.
TEST_F(HistoryBackendDBTest, MigrateDownloadSliceFinished) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(38));
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
  }
  CreateBackendAndDatabase();
  DeleteBackend();

  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(38, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      // The downloads_slices table should have the finished column.
      sql::Statement s1(
          db.GetUniqueStatement("SELECT COUNT(*) from downloads_slices"));
      EXPECT_TRUE(s1.Step());
      EXPECT_EQ(0, s1.ColumnInt(0));
      const char kInsertStatement[] =
          "INSERT INTO downloads_slices "
          "(download_id, offset, received_bytes, finished) VALUES (1, 0, 100, "
          "1)";
      ASSERT_TRUE(db.Execute(kInsertStatement));
    }
  }
}

// Test to verify the incremented_omnibox_typed_score column will be correctly
// added to visits table during migration to version 40.
TEST_F(HistoryBackendDBTest, MigrateVisitsWithoutIncrementedOmniboxTypedScore) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(39));

  const VisitID visit_id1 = 1;
  const VisitID visit_id2 = 2;
  const URLID url_id1 = 3;
  const URLID url_id2 = 4;
  const base::Time visit_time1(base::Time::Now());
  const base::Time visit_time2(base::Time::Now());
  const VisitID referring_visit1 = 0;
  const VisitID referring_visit2 = 0;
  const ui::PageTransition transition1 = ui::PAGE_TRANSITION_LINK;
  const ui::PageTransition transition2 = ui::PAGE_TRANSITION_TYPED;
  const SegmentID segment_id1 = 7;
  const SegmentID segment_id2 = 8;
  const base::TimeDelta visit_duration1(base::Seconds(30));
  const base::TimeDelta visit_duration2(base::Seconds(45));

  const char kInsertStatement[] =
      "INSERT INTO visits "
      "(id, url, visit_time, from_visit, transition, segment_id, "
      "visit_duration) VALUES (?, ?, ?, ?, ?, ?, ?)";

  {
    // Open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Add entries to visits.
    {
      sql::Statement s(db.GetUniqueStatement(kInsertStatement));
      s.BindInt64(0, visit_id1);
      s.BindInt64(1, url_id1);
      s.BindInt64(2, visit_time1.ToDeltaSinceWindowsEpoch().InMicroseconds());
      s.BindInt64(3, referring_visit1);
      s.BindInt64(4, transition1);
      s.BindInt64(5, segment_id1);
      s.BindInt64(6, visit_duration1.InMicroseconds());
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(kInsertStatement));
      s.BindInt64(0, visit_id2);
      s.BindInt64(1, url_id2);
      s.BindInt64(2, visit_time2.ToDeltaSinceWindowsEpoch().InMicroseconds());
      s.BindInt64(3, referring_visit2);
      s.BindInt64(4, transition2);
      s.BindInt64(5, segment_id2);
      s.BindInt64(6, visit_duration2.InMicroseconds());
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  VisitRow visit_row1;
  db_->GetRowForVisit(visit_id1, &visit_row1);
  EXPECT_FALSE(visit_row1.incremented_omnibox_typed_score);

  VisitRow visit_row2;
  db_->GetRowForVisit(visit_id2, &visit_row2);
  EXPECT_TRUE(visit_row2.incremented_omnibox_typed_score);
}

// Tests that the migration code correctly handles rows in the visit database
// that may be in an invalid state where visit_id == referring_visit. Regression
// test for https://crbug.com/847246.
TEST_F(HistoryBackendDBTest,
       MigrateVisitsWithoutIncrementedOmniboxTypedScore_BadRow) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(39));

  const VisitID visit_id = 1;
  const URLID url_id = 2;
  const base::Time visit_time(base::Time::Now());
  // visit_id == referring_visit will trigger DCHECK_NE in UpdateVisitRow.
  const VisitID referring_visit = 1;
  const ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  const SegmentID segment_id = 8;
  const base::TimeDelta visit_duration(base::Seconds(45));

  const char kInsertStatement[] =
      "INSERT INTO visits "
      "(id, url, visit_time, from_visit, transition, segment_id, "
      "visit_duration) VALUES (?, ?, ?, ?, ?, ?, ?)";

  {
    // Open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Add entry to visits.
    sql::Statement s(db.GetUniqueStatement(kInsertStatement));
    s.BindInt64(0, visit_id);
    s.BindInt64(1, url_id);
    s.BindInt64(2, visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
    s.BindInt64(3, referring_visit);
    s.BindInt64(4, transition);
    s.BindInt64(5, segment_id);
    s.BindInt64(6, visit_duration.InMicroseconds());
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // Field should be false since the migration won't update it from the default
  // due to the invalid state of the row.
  VisitRow visit_row;
  db_->GetRowForVisit(visit_id, &visit_row);
  EXPECT_FALSE(visit_row.incremented_omnibox_typed_score);
}

TEST_F(HistoryBackendDBTest, MigrateVisitsWithoutPubliclyRoutableColumn) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(42));

  // Define common uninteresting data for visits.
  const VisitID referring_visit = 0;
  const ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  const base::Time visit_time(base::Time::Now());
  const base::TimeDelta visit_duration(base::Seconds(30));

  // The first visit has both a DB entry and a metadata entry.
  const VisitID visit_id1 = 1;
  const URLID url_id1 = 10;
  const SegmentID segment_id1 = 20;
  const std::string metadata_value1 = "BLOB1";

  // Open the db for manual manipulation.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    const char kInsertVisitStatement[] =
        "INSERT INTO visits "
        "(id, url, visit_time, from_visit, transition, segment_id, "
        "visit_duration) VALUES (?, ?, ?, ?, ?, ?, ?)";

    // Add an entry to "visits" table.
    sql::Statement s(db.GetUniqueStatement(kInsertVisitStatement));
    s.BindInt64(0, visit_id1);
    s.BindInt64(1, url_id1);
    s.BindInt64(2, visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
    s.BindInt64(3, referring_visit);
    s.BindInt64(4, transition);
    s.BindInt64(5, segment_id1);
    s.BindInt64(6, visit_duration.InMicroseconds());
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();
  DeleteBackend();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 44);

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // content_annotations should exist.
    EXPECT_TRUE(db.DoesTableExist("content_annotations"));

    // Confirm that content_annotations table has a annotation_flags column,
    // but has 0 entry in it because the publicly_routable field in the entry in
    // the visits table is "false" so is not migrated to the content_annotations
    // table.
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT annotation_flags FROM content_annotations"));
      EXPECT_FALSE(s.Step());
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateFlocAllowedToAnnotationsTable) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(43));

  // Define common uninteresting data for visits.
  const base::Time visit_time(base::Time::Now());

  // The first visit is publicly routable;
  const VisitID visit_id1 = 1;
  const URLID url_id1 = 10;
  const bool publicly_routable1 = true;

  // The second visit is not publicly routable;
  const VisitID visit_id2 = 2;
  const URLID url_id2 = 20;
  const bool publicly_routable2 = false;

  // The third visit is publicly routable;
  const VisitID visit_id3 = 3;
  const URLID url_id3 = 30;
  const bool publicly_routable3 = true;

  // Open the db for manual manipulation.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    const char kInsertVisitStatement[] =
        "INSERT INTO visits "
        "(id, url, visit_time, publicly_routable) VALUES (?, ?, ?, ?)";

    const char kInsertAnnotationsStatement[] =
        "INSERT INTO content_annotations "
        "(visit_id, floc_protected_score, categories, "
        "page_topics_model_version) "
        "VALUES (?, ?, ?, ?)";

    // Add the three entries to "visits" table.
    {
      sql::Statement s(db.GetUniqueStatement(kInsertVisitStatement));
      s.BindInt64(0, visit_id1);
      s.BindInt64(1, url_id1);
      s.BindInt64(2, visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
      s.BindBool(3, publicly_routable1);
      ASSERT_TRUE(s.Run());
    }

    {
      sql::Statement s(db.GetUniqueStatement(kInsertVisitStatement));
      s.BindInt64(0, visit_id2);
      s.BindInt64(1, url_id2);
      s.BindInt64(2, visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
      s.BindBool(3, publicly_routable2);
      ASSERT_TRUE(s.Run());
    }

    {
      sql::Statement s(db.GetUniqueStatement(kInsertVisitStatement));
      s.BindInt64(0, visit_id3);
      s.BindInt64(1, url_id3);
      s.BindInt64(2, visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
      s.BindBool(3, publicly_routable3);
      ASSERT_TRUE(s.Run());
    }

    // Add the two entries to "content_annotations" table
    {
      sql::Statement s(db.GetUniqueStatement(kInsertAnnotationsStatement));
      s.BindInt64(0, visit_id1);
      s.BindDouble(1, -1);
      s.BindString(2, "");
      s.BindInt64(3, -1);
      ASSERT_TRUE(s.Run());
    }

    {
      sql::Statement s(db.GetUniqueStatement(kInsertAnnotationsStatement));
      s.BindInt64(0, visit_id2);
      s.BindDouble(1, 0.5f);
      s.BindString(2, "1:1");
      s.BindInt64(3, 123);
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();
  DeleteBackend();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 44);

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Check the entries in the content_annotations table.
    sql::Statement s(db.GetUniqueStatement(
        "SELECT visit_id,visibility_score,"
        "categories,page_topics_model_version,annotation_flags "
        "FROM content_annotations "
        "ORDER BY visit_id"));

    EXPECT_TRUE(s.Step());
    EXPECT_EQ(visit_id1, s.ColumnInt64(0));
    EXPECT_EQ(-1, s.ColumnDouble(1));
    EXPECT_EQ("", s.ColumnString(2));
    EXPECT_EQ(-1, s.ColumnInt64(3));
    EXPECT_EQ(VisitContentAnnotationFlag::kDeprecatedFlocEligibleRelaxed,
              static_cast<uint64_t>(s.ColumnInt64(4)));

    EXPECT_TRUE(s.Step());
    EXPECT_EQ(visit_id2, s.ColumnInt64(0));
    EXPECT_EQ(-1, s.ColumnDouble(1));
    EXPECT_EQ("1:1", s.ColumnString(2));
    EXPECT_EQ(123, s.ColumnInt64(3));
    EXPECT_EQ(VisitContentAnnotationFlag::kNone,
              static_cast<uint64_t>(s.ColumnInt64(4)));

    EXPECT_TRUE(s.Step());
    EXPECT_EQ(visit_id3, s.ColumnInt64(0));
    EXPECT_EQ(-1, s.ColumnDouble(1));
    EXPECT_EQ("", s.ColumnString(2));
    EXPECT_EQ(-1, s.ColumnInt64(3));
    EXPECT_EQ(VisitContentAnnotationFlag::kDeprecatedFlocEligibleRelaxed,
              static_cast<uint64_t>(s.ColumnInt64(4)));

    EXPECT_FALSE(s.Step());
  }
}

TEST_F(HistoryBackendDBTest, MigrateReplaceClusterVisitsTable) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(44));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    const char kInsertVisitStatement[] =
        "INSERT INTO visits "
        "(id, url, visit_time) VALUES (?, ?, ?)";

    const char kInsertAnnotationsStatement[] =
        "INSERT INTO cluster_visits "
        "(cluster_visit_id, url_id, visit_id, "
        "cluster_visit_context_signal_bitmask, duration_since_last_visit, "
        "page_end_reason) "
        "VALUES (?, ?, ?, ?, ?, ?)";

    // Add a row to `visits` table.
    {
      sql::Statement s(db.GetUniqueStatement(kInsertVisitStatement));
      s.BindInt64(0, 1);
      s.BindInt64(1, 1);
      s.BindTime(2, base::Time::Now());
      ASSERT_TRUE(s.Run());
    }

    // Add a row to the `cluster_visits` table.
    {
      sql::Statement s(db.GetUniqueStatement(kInsertAnnotationsStatement));
      s.BindInt64(0, 1);
      s.BindInt64(1, 1);
      s.BindInt64(2, 1);
      s.BindInt64(3, 0);
      s.BindInt64(4, 0);
      s.BindInt(5, 0);
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();
  DeleteBackend();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 45);

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Confirm the old `cluster_visits` table no longer exists.
    ASSERT_FALSE(db.DoesTableExist("cluster_visits"));

    // Confirm the new `context_annotations` exists.
    ASSERT_TRUE(db.DoesTableExist("context_annotations"));

    // Check `context_annotations` is empty.
    {
      sql::Statement s(
          db.GetUniqueStatement("SELECT COUNT(*) FROM content_annotations"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(s.ColumnInt64(0), 0u);
      EXPECT_FALSE(s.Step());
    }
  }
}

// Tests that the migration code correctly replaces the lower_term column in the
// keyword search terms table which normalized_term which contains the
// normalized search term during migration to version 42.
TEST_F(HistoryBackendDBTest, MigrateKeywordSearchTerms) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(41));

  const KeywordID keyword_id = 12;
  const URLID url_id = 34;
  const std::u16string term = u"WEEKLY  NEWS  ";
  const std::u16string lower_term = base::i18n::ToLower(term);
  const std::u16string normalized_term =
      base::CollapseWhitespace(lower_term, false);

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement insert_statement(
        db.GetUniqueStatement("INSERT INTO keyword_search_terms (keyword_id, "
                              "url_id, lower_term, term) VALUES (?,?,?,?)"));
    insert_statement.BindInt64(0, keyword_id);
    insert_statement.BindInt64(1, url_id);
    insert_statement.BindString16(2, lower_term);
    insert_statement.BindString16(3, term);
    ASSERT_TRUE(insert_statement.Run());
  }
  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 42);

  history::KeywordSearchTermRow keyword_search_term_row;
  ASSERT_TRUE(db_->GetKeywordSearchTermRow(url_id, &keyword_search_term_row));
  EXPECT_EQ(keyword_id, keyword_search_term_row.keyword_id);
  EXPECT_EQ(url_id, keyword_search_term_row.url_id);
  EXPECT_EQ(term, keyword_search_term_row.term);
  EXPECT_EQ(normalized_term, keyword_search_term_row.normalized_term);
}

TEST_F(HistoryBackendDBTest, MigrateContentAnnotationsWithoutEntitiesColumn) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(46));

  const VisitID visit_id1 = 1;

  // Open the db for manual manipulation.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    const char kInsertContentAnnotationsStatement[] =
        "INSERT INTO content_annotations "
        "(visit_id, floc_protected_score, categories, "
        "page_topics_model_version, "
        "annotation_flags) "
        "VALUES (?, ?, ?, ?, ?)";

    // Add an entry to "content_annotations" table.
    sql::Statement s(db.GetUniqueStatement(kInsertContentAnnotationsStatement));
    s.BindInt64(0, visit_id1);
    s.BindDouble(1, -1);
    s.BindString(2, "");
    s.BindInt64(3, -1);
    s.BindInt64(4, 0);
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 47);

  // After the migration, the entities should be empty.
  {
    VisitContentAnnotations visit_content_annotations;
    db_->GetContentAnnotationsForVisit(visit_id1, &visit_content_annotations);
    EXPECT_TRUE(visit_content_annotations.model_annotations.entities.empty());
  }
}

TEST_F(HistoryBackendDBTest,
       MigrateContentAnnotationsAddRelatedSearchesColumn) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(47));

  const VisitID visit_id1 = 1;

  // Open the db for manual manipulation.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    const char kInsertContentAnnotationsStatement[] =
        "INSERT INTO content_annotations "
        "(visit_id, floc_protected_score, categories, "
        "page_topics_model_version, "
        "annotation_flags, entities) "
        "VALUES (?, ?, ?, ?, ?, ?)";

    // Add an entry to "content_annotations" table.
    {
      sql::Statement s(
          db.GetUniqueStatement(kInsertContentAnnotationsStatement));
      s.BindInt64(0, visit_id1);
      s.BindDouble(1, -1);
      s.BindString(2, "");
      s.BindInt64(3, -1);
      s.BindInt64(4, 0);
      s.BindString(5, "");
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 48);

  // After the migration, the related searches should be empty.
  {
    VisitContentAnnotations visit_content_annotations;
    db_->GetContentAnnotationsForVisit(visit_id1, &visit_content_annotations);
    EXPECT_TRUE(visit_content_annotations.related_searches.empty());
  }
}

TEST_F(HistoryBackendDBTest,
       MigrateVisitsWithoutOpenerVisitColumnAndDropPubliclyRoutableColumn) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(48));

  const VisitID visit_id1 = 1;

  // Open the db for manual manipulation.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    const char kInsertVisitStatement[] =
        "INSERT INTO visits "
        "(id, url, visit_time) VALUES (?, ?, ?)";

    // Add a row to `visits` table.
    sql::Statement s(db.GetUniqueStatement(kInsertVisitStatement));
    s.BindInt64(0, 1);
    s.BindInt64(1, 1);
    s.BindTime(2, base::Time::Now());
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 49);

  // After the migration, the opener visit should be 0.
  {
    VisitRow visit;
    db_->GetRowForVisit(visit_id1, &visit);
    EXPECT_EQ(visit.opener_visit, 0);
  }
}

TEST_F(HistoryBackendDBTest,
       MigrateContextAnnotationsAddTotalForegroundDurationColumn) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(50));

  const VisitID visit_id = 1;

  // Open the db for manual manipulation.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    const char kInsertContextAnnotationsStatement[] =
        "INSERT INTO context_annotations "
        "(visit_id,context_annotation_flags,duration_since_last_visit,"
        "page_end_reason) "
        "VALUES (?, ?, ?, ?)";

    // Add an entry to "context_annotations" table.
    sql::Statement s(db.GetUniqueStatement(kInsertContextAnnotationsStatement));
    s.BindInt64(0, visit_id);
    s.BindInt64(1, 1);
    s.BindInt64(2, 3);
    s.BindInt(3, 0);
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 51);

  // After the migration, the total foreground duration should have a default of
  // -1.
  {
    VisitContextAnnotations visit_context_annotations;
    db_->GetContextAnnotationsForVisit(visit_id, &visit_context_annotations);
    EXPECT_EQ(visit_context_annotations.total_foreground_duration,
              base::Seconds(-1));
  }
}

TEST_F(HistoryBackendDBTest,
       MigrateContentAnnotationsAddSearchMetadataColumns) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(52));

  const VisitID visit_id1 = 1;

  // Open the db for manual manipulation.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    const char kInsertContentAnnotationsStatement[] =
        "INSERT INTO content_annotations "
        "(visit_id, floc_protected_score, categories, "
        "page_topics_model_version, "
        "annotation_flags, entities, related_searches) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)";

    // Add an entry to "content_annotations" table.
    sql::Statement s(db.GetUniqueStatement(kInsertContentAnnotationsStatement));
    s.BindInt64(0, visit_id1);
    s.BindDouble(1, -1);
    s.BindString(2, "");
    s.BindInt64(3, -1);
    s.BindInt64(4, 0);
    s.BindString(5, "");
    s.BindString(6, "");
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 53);

  // After the migration, the search metadata should be empty.
  {
    VisitContentAnnotations visit_content_annotations;
    db_->GetContentAnnotationsForVisit(visit_id1, &visit_content_annotations);
    EXPECT_TRUE(visit_content_annotations.search_normalized_url.is_empty());
    EXPECT_TRUE(visit_content_annotations.search_terms.empty());
  }
}

TEST_F(HistoryBackendDBTest, MigrateContentAnnotationsAddPageMetadataColumns) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(53));

  const VisitID visit_id1 = 1;

  // Open the db for manual manipulation.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    const char kInsertContentAnnotationsStatement[] =
        "INSERT INTO content_annotations "
        "(visit_id, floc_protected_score, categories, "
        "page_topics_model_version, "
        "annotation_flags, entities, related_searches, search_normalized_url, "
        "search_terms) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";

    // Add an entry to "content_annotations" table.
    sql::Statement s(db.GetUniqueStatement(kInsertContentAnnotationsStatement));
    s.BindInt64(0, visit_id1);
    s.BindDouble(1, -1);
    s.BindString(2, "");
    s.BindInt64(3, -1);
    s.BindInt64(4, 0);
    s.BindString(5, "");
    s.BindString(6, "");
    s.BindString(7, "");
    s.BindString(8, "");
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 54);

  // After the migration, the page metadata should be empty.
  {
    VisitContentAnnotations visit_content_annotations;
    db_->GetContentAnnotationsForVisit(visit_id1, &visit_content_annotations);
    EXPECT_TRUE(visit_content_annotations.alternative_title.empty());
  }
}

TEST_F(HistoryBackendDBTest,
       MigrateVisitsAutoincrementIdAndAddOriginatorColumns) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(54));

  constexpr VisitID visit_id1 = 1;

  // Open the db for manual manipulation.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    const char kInsertVisitStatement[] =
        "INSERT INTO visits "
        "(id, url, visit_time) VALUES (?, ?, ?)";

    // Add a row to `visits` table.
    sql::Statement s(db.GetUniqueStatement(kInsertVisitStatement));
    s.BindInt64(0, 1);
    s.BindInt64(1, 1);
    s.BindTime(2, base::Time::Now());
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // After the migration, the originator columns should return default values.
  {
    VisitRow visit;
    db_->GetRowForVisit(visit_id1, &visit);
    EXPECT_EQ(visit.originator_cache_guid, "");
    EXPECT_EQ(visit.originator_visit_id, 0);
  }
}

TEST_F(HistoryBackendDBTest,
       MigrateVisitsAddOriginatorFromVisitAndOpenerVisitColumns) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(55));

  constexpr VisitID visit_id = 1;
  constexpr URLID url_id = 2;
  const base::Time visit_time = base::Time::Now();

  // Open the db for manual manipulation.
  {
    sql::Database sql_db;
    ASSERT_TRUE(sql_db.Open(history_dir_.Append(kHistoryFilename)));

    ASSERT_FALSE(sql_db.DoesColumnExist("visits", "originator_from_visit"));
    ASSERT_FALSE(sql_db.DoesColumnExist("visits", "originator_opener_visit"));

    const char kInsertVisitStatement[] =
        "INSERT INTO visits "
        "(id, url, visit_time) VALUES (?, ?, ?)";

    // Add a row to `visits` table.
    sql::Statement s(sql_db.GetUniqueStatement(kInsertVisitStatement));
    s.BindInt64(0, visit_id);
    s.BindInt64(1, url_id);
    s.BindTime(2, visit_time);
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The previously-added visit should still exist, with the new columns being
  // empty (equal to 0).
  {
    VisitRow visit;
    db_->GetRowForVisit(visit_id, &visit);
    EXPECT_EQ(visit.url_id, url_id);
    EXPECT_EQ(visit.visit_time, visit_time);
    EXPECT_EQ(visit.originator_referring_visit, 0);
    EXPECT_EQ(visit.originator_opener_visit, 0);
  }

  DeleteBackend();

  // Open the db manually again and make sure the new columns exist.
  {
    sql::Database sql_db;
    ASSERT_TRUE(sql_db.Open(history_dir_.Append(kHistoryFilename)));

    EXPECT_TRUE(sql_db.DoesColumnExist("visits", "originator_from_visit"));
    EXPECT_TRUE(sql_db.DoesColumnExist("visits", "originator_opener_visit"));
  }
}

TEST_F(HistoryBackendDBTest, MigrateClustersAddColumns) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(56));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Confirm the old 'clusters' columns exist.
    ASSERT_TRUE(db.DoesColumnExist("clusters", "cluster_id"));
    ASSERT_TRUE(db.DoesColumnExist("clusters", "score"));

    // Confirm the new 'clusters' columns don't exist.
    ASSERT_FALSE(
        db.DoesColumnExist("clusters", "should_show_on_prominent_ui_surfaces"));
    ASSERT_FALSE(db.DoesColumnExist("clusters", "label"));
    ASSERT_FALSE(db.DoesColumnExist("clusters", "raw_label"));

    // Confirm the old 'clusters_and_visits' columns exist.
    ASSERT_TRUE(db.DoesColumnExist("clusters_and_visits", "cluster_id"));
    ASSERT_TRUE(db.DoesColumnExist("clusters_and_visits", "visit_id"));
    ASSERT_TRUE(db.DoesColumnExist("clusters_and_visits", "score"));

    // Confirm the new 'clusters_and_visits' columns don't exist.
    ASSERT_FALSE(db.DoesColumnExist("clusters_and_visits", "engagement_score"));
    ASSERT_FALSE(db.DoesColumnExist("clusters_and_visits", "url_for_deduping"));
    ASSERT_FALSE(db.DoesColumnExist("clusters_and_visits", "normalized_url"));
    ASSERT_FALSE(db.DoesColumnExist("clusters_and_visits", "url_for_display"));
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();
  DeleteBackend();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 57);

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Confirm the tables still exist.
    ASSERT_TRUE(db.DoesTableExist("clusters"));
    ASSERT_TRUE(db.DoesTableExist("clusters_and_visits"));

    // Confirm the new 'clusters' columns exist.
    ASSERT_TRUE(db.DoesColumnExist("clusters", "cluster_id"));
    ASSERT_TRUE(
        db.DoesColumnExist("clusters", "should_show_on_prominent_ui_surfaces"));
    ASSERT_TRUE(db.DoesColumnExist("clusters", "label"));
    ASSERT_TRUE(db.DoesColumnExist("clusters", "raw_label"));

    // Confirm 'score' column was removed from 'clusters'.
    ASSERT_FALSE(db.DoesColumnExist("clusters", "score"));

    // Confirm the new 'clusters_and_visits' columns exist.
    ASSERT_TRUE(db.DoesColumnExist("clusters_and_visits", "cluster_id"));
    ASSERT_TRUE(db.DoesColumnExist("clusters_and_visits", "visit_id"));
    ASSERT_TRUE(db.DoesColumnExist("clusters_and_visits", "score"));
    ASSERT_TRUE(db.DoesColumnExist("clusters_and_visits", "engagement_score"));
    ASSERT_TRUE(db.DoesColumnExist("clusters_and_visits", "url_for_deduping"));
    ASSERT_TRUE(db.DoesColumnExist("clusters_and_visits", "normalized_url"));
    ASSERT_TRUE(db.DoesColumnExist("clusters_and_visits", "url_for_display"));
  }
}

TEST_F(HistoryBackendDBTest, MigrateAnnotationsAddColumnsForSync) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(57));

  // Precondition: Open the old version of the DB and make sure the new columns
  // don't exist yet.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    ASSERT_FALSE(db.DoesColumnExist("context_annotations", "browser_type"));
    ASSERT_FALSE(db.DoesColumnExist("context_annotations", "window_id"));
    ASSERT_FALSE(db.DoesColumnExist("context_annotations", "tab_id"));
    ASSERT_FALSE(db.DoesColumnExist("context_annotations", "task_id"));
    ASSERT_FALSE(db.DoesColumnExist("context_annotations", "root_task_id"));
    ASSERT_FALSE(db.DoesColumnExist("context_annotations", "parent_task_id"));
    ASSERT_FALSE(db.DoesColumnExist("context_annotations", "response_code"));

    ASSERT_FALSE(db.DoesColumnExist("content_annotations", "page_language"));
    ASSERT_FALSE(db.DoesColumnExist("content_annotations", "password_state"));
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 58);

  DeleteBackend();

  // Open the db manually again and make sure the new columns exist.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Confirm that the new columns exist now.
    EXPECT_TRUE(db.DoesColumnExist("context_annotations", "browser_type"));
    EXPECT_TRUE(db.DoesColumnExist("context_annotations", "window_id"));
    EXPECT_TRUE(db.DoesColumnExist("context_annotations", "tab_id"));
    EXPECT_TRUE(db.DoesColumnExist("context_annotations", "task_id"));
    EXPECT_TRUE(db.DoesColumnExist("context_annotations", "root_task_id"));
    EXPECT_TRUE(db.DoesColumnExist("context_annotations", "parent_task_id"));
    EXPECT_TRUE(db.DoesColumnExist("context_annotations", "response_code"));

    EXPECT_TRUE(db.DoesColumnExist("content_annotations", "page_language"));
    EXPECT_TRUE(db.DoesColumnExist("content_annotations", "password_state"));
  }
}

TEST_F(HistoryBackendDBTest, MigrateVisitsAddIsKnownToSyncColumn) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(58));

  // Open the old version of the DB and make sure the new columns don't exist
  // yet. Also add some visits marked as from SYNC in the old style.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    ASSERT_FALSE(db.DoesColumnExist("visits", "is_known_to_sync"));
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 59);

  DeleteBackend();

  // Open the db manually again and make sure the new columns exist.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    EXPECT_TRUE(db.DoesColumnExist("visits", "is_known_to_sync"));
  }
}

TEST_F(HistoryBackendDBTest, MigrateClustersAddTriggerabilityCalculatedColumn) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(59));

  int64_t cluster_id = 1;

  // Open the old version of the DB and make sure the new columns don't exist
  // yet.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    ASSERT_FALSE(db.DoesColumnExist("clusters", "triggerability_calculated"));

    const char kInsertClustersStatement[] =
        "INSERT INTO clusters"
        "(cluster_id,should_show_on_prominent_ui_surfaces,label,raw_label)"
        "VALUES(?,?,?,?)";

    // Add a row to `clusters` table.
    {
      sql::Statement s(db.GetUniqueStatement(kInsertClustersStatement));
      s.BindInt64(0, cluster_id);
      s.BindBool(1, true);
      s.BindString16(2, u"");
      s.BindString16(3, u"");
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 60);

  // Open the db manually again and make sure the new columns exist.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    EXPECT_TRUE(db.DoesColumnExist("clusters", "triggerability_calculated"));
  }

  // Check contents.
  Cluster cluster = db_->GetCluster(cluster_id);
  EXPECT_TRUE(cluster.triggerability_calculated);
}

TEST_F(HistoryBackendDBTest,
       MigrateClustersAutoincrementIdAndAddOriginatorColumns) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(60));

  int64_t cluster_id = 1;

  // Open the db for manual manipulation.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    const char kInsertClustersStatement[] =
        "INSERT INTO clusters"
        "(cluster_id,should_show_on_prominent_ui_surfaces,label,raw_label,"
        "triggerability_calculated)"
        "VALUES(?,?,?,?,?)";

    // Add a row to `clusters` table.
    sql::Statement s(db.GetUniqueStatement(kInsertClustersStatement));
    s.BindInt64(0, cluster_id);
    s.BindBool(1, true);
    s.BindString16(2, u"");
    s.BindString16(3, u"");
    s.BindBool(4, true);
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // After the migration, the originator columns should return default values.
  {
    // Check contents.
    Cluster cluster = db_->GetCluster(cluster_id);
    EXPECT_EQ(cluster.originator_cache_guid, "");
    EXPECT_EQ(cluster.originator_cluster_id, 0);
  }
}

TEST_F(HistoryBackendDBTest, MigrateContentAnnotationsAddHasUrlKeyedImage) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(61));

  const VisitID visit_id = 1;

  // Open the db for manual manipulation.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    const char kInsertContentAnnotationsStatement[] =
        "INSERT INTO "
        "content_annotations(visit_id,visibility_score,categories,"
        "page_topics_model_version,annotation_flags,entities,related_searches,"
        "search_normalized_url,search_terms,alternative_title,page_language,"
        "password_state)"
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?)";

    // Add an entry to "content_annotations" table.
    sql::Statement s(db.GetUniqueStatement(kInsertContentAnnotationsStatement));
    s.BindInt64(0, visit_id);
    s.BindDouble(1, -1);
    s.BindString(2, "");
    s.BindInt64(3, -1);
    s.BindInt64(4, 0);
    s.BindString(5, "");
    s.BindString(6, "");
    s.BindString(7, "");
    s.BindString16(8, u"");
    s.BindString(9, "");
    s.BindString(10, "");
    s.BindInt(11, 0);
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 62);

  // After the migration, has_url_keyed_image should be false.
  {
    VisitContentAnnotations visit_content_annotations;
    db_->GetContentAnnotationsForVisit(visit_id, &visit_content_annotations);
    EXPECT_FALSE(visit_content_annotations.has_url_keyed_image);
  }
}

TEST_F(HistoryBackendDBTest,
       MigrateVisitsAddConsiderForNewTabPageMostVisitedColumn) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(62));

  const VisitID visit_id = 1;
  const URLID url_id = 2;
  const base::Time visit_time(base::Time::Now());
  // visit_id == referring_visit will trigger DCHECK_NE in UpdateVisitRow.
  const VisitID referring_visit = 1;
  const ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  const SegmentID segment_id = 8;
  const base::TimeDelta visit_duration(base::Seconds(45));

  const char kInsertStatement[] =
      "INSERT INTO visits "
      "(id, url, visit_time, from_visit, transition, segment_id, "
      "visit_duration) VALUES (?, ?, ?, ?, ?, ?, ?)";

  // Open the old version of the DB and make sure the new columns don't exist
  // yet.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    ASSERT_FALSE(db.DoesColumnExist("visits", "consider_for_ntp_most_visited"));

    // Add entry to visits.
    sql::Statement s(db.GetUniqueStatement(kInsertStatement));
    s.BindInt64(0, visit_id);
    s.BindInt64(1, url_id);
    s.BindInt64(2, visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
    s.BindInt64(3, referring_visit);
    s.BindInt64(4, transition);
    s.BindInt64(5, segment_id);
    s.BindInt64(6, visit_duration.InMicroseconds());

    ASSERT_TRUE(s.Run());
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  // The version should have been updated.
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 63);

  VisitRow visit_row;
  db_->GetRowForVisit(visit_id, &visit_row);
  EXPECT_FALSE(visit_row.consider_for_ntp_most_visited);

  DeleteBackend();

  // Open the db manually again and make sure the new columns exist.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    EXPECT_TRUE(db.DoesColumnExist("visits", "consider_for_ntp_most_visited"));
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadByWebApp) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(63));

  // Precondition: Open the old version of the DB and make sure the new column
  // doesn't exist yet.
  {
    sql::Database db;
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
    sql::Database db;
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
    sql::Database db;
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
    sql::Database db;
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
    sql::Database db;
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
    sql::Database db;
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
    sql::Database db;
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
    sql::Database db;
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
    sql::Database db;
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
    sql::Database db;
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
    sql::Database db;
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
  ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 69);

  VisitRow visit_row;
  db_->GetRowForVisit(visit_id, &visit_row);
  EXPECT_FALSE(visit_row.app_id);

  DeleteBackend();

  // Open the db manually again and make sure the new columns exist.
  {
    sql::Database db;
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

}  // namespace
}  // namespace history
