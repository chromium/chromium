// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include <string>
#include <unordered_set>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database.h"
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
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(5));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(6));

      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(2, statement.ColumnInt64(0));
      EXPECT_EQ(DownloadInterruptReasonToInt(kTestDownloadInterruptReasonNone),
                statement.ColumnInt(1));
      EXPECT_EQ("/path/to/some/file", statement.ColumnString(2));
      EXPECT_EQ("/path/to/some/file", statement.ColumnString(3));
      EXPECT_EQ(0, statement.ColumnInt(4));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(5));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(6));

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
  // base::IsValidGUID() doesn't restrict its validation to version (or subtype)
  // 4 GUIDs as described in RFC 4122. So we check if base::IsValidGUID() thinks
  // it's a valid GUID first, and then check the additional constraints.
  //
  // * Bits 4-7 of time_hi_and_version should be set to 0b0100 == 4
  //   => guid[14] == '4'
  //
  // * Bits 6-7 of clk_seq_hi_res should be set to 0b10
  //   => guid[19] in {'8','9','A','B','a','b'}
  //
  // * All other bits should be random or pseudo random.
  //   => http://dilbert.com/strip/2001-10-25
  return base::IsValidGUID(guid) && guid[14] == '4' &&
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
    ASSERT_TRUE(db.Execute(download_insert_query.c_str()));
    ASSERT_TRUE(db.Execute(url_insert_query.c_str()));
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
      EXPECT_EQ(base::Time(), base::Time::FromInternalValue(s.ColumnInt64(0)));
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
  base::Time end_time(start_time + base::TimeDelta::FromHours(1));
  base::Time last_access_time;

  DownloadRow download_A;
  download_A.current_path = base::FilePath(FILE_PATH_LITERAL("/path/1"));
  download_A.target_path = base::FilePath(FILE_PATH_LITERAL("/path/2"));
  download_A.url_chain = url_chain;
  download_A.referrer_url = GURL("http://example.com/referrer");
  download_A.site_url = GURL("http://example.com");
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

  ASSERT_TRUE(db_->CreateDownload(download_A));

  url_chain.push_back(GURL("http://example.com/d"));

  base::Time start_time2(start_time + base::TimeDelta::FromHours(10));
  base::Time end_time2(end_time + base::TimeDelta::FromHours(10));
  base::Time last_access_time2(start_time2 + base::TimeDelta::FromHours(5));

  DownloadRow download_B;
  download_B.current_path = base::FilePath(FILE_PATH_LITERAL("/path/3"));
  download_B.target_path = base::FilePath(FILE_PATH_LITERAL("/path/4"));
  download_B.url_chain = url_chain;
  download_B.referrer_url = GURL("http://example.com/referrer2");
  download_B.site_url = GURL("http://2.example.com");
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
  base::Time end_time(start_time + base::TimeDelta::FromHours(1));
  base::Time last_access_time(start_time + base::TimeDelta::FromHours(5));

  DownloadRow download;
  download.current_path = base::FilePath(FILE_PATH_LITERAL("/path/1"));
  download.target_path = base::FilePath(FILE_PATH_LITERAL("/path/2"));
  download.url_chain = url_chain;
  download.referrer_url = GURL("http://example.com/referrer");
  download.site_url = GURL("http://example.com");
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
  download.end_time += base::TimeDelta::FromHours(1);
  download.total_bytes += 1;
  download.hash = "some-other-hash";
  download.opened = !download.opened;
  download.transient = !download.transient;
  download.by_ext_id = "by-new-extension-id";
  download.by_ext_name = "by-new-extension-name";
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

  AddDownload(id2,
              "05AF6C8E-E4E0-45D7-B5CE-BC99F7019919",
              DownloadState::COMPLETE,
              now + base::TimeDelta::FromDays(2));
  AddDownload(id3,
              "05AF6C8E-E4E0-45D7-B5CE-BC99F701991A",
              DownloadState::COMPLETE,
              now - base::TimeDelta::FromDays(2));

  // Confirm that resulted in the correct number of rows in the DB.
  DeleteBackend();
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(3, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select Count(*) from downloads_url_chains"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(3, statement1.ColumnInt(0));

    sql::Statement statement2(db.GetUniqueStatement(
        "Select Count(*) from downloads_slices"));
    EXPECT_TRUE(statement2.Step());
    EXPECT_EQ(1, statement2.ColumnInt(0));
    EXPECT_EQ(0, statement2.ColumnInt(3));
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
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(1, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select Count(*) from downloads_url_chains"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(1, statement1.ColumnInt(0));

    sql::Statement statement2(db.GetUniqueStatement(
        "Select Count(*) from downloads_slices"));
    EXPECT_TRUE(statement2.Step());
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
  download.opened = 0;
  download.last_access_time = now;
  download.transient = false;
  download.by_ext_id = "by_ext_id";
  download.by_ext_name = "by_ext_name";

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
  download.tab_url = GURL("http://example.com/tab-url");
  download.tab_referrer_url = GURL("http://example.com/tab-referrer");
  download.http_method = "GET";
  download.mime_type = "mime/type";
  download.original_mime_type = "original/mime-type";
  download.start_time = base::Time::Now();
  download.end_time = download.start_time + base::TimeDelta::FromHours(1);
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
  download.last_access_time =
      download.start_time + base::TimeDelta::FromHours(5);
  download.transient = false;
  download.by_ext_id = "extension-id";
  download.by_ext_name = "extension-name";
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
  download.tab_url = GURL("http://example.com/tab-url");
  download.tab_referrer_url = GURL("http://example.com/tab-referrer");
  download.http_method = "GET";
  download.mime_type = "mime/type";
  download.original_mime_type = "original/mime-type";
  download.start_time = base::Time::Now();
  download.end_time = download.start_time + base::TimeDelta::FromHours(1);
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
  download.last_access_time =
      download.start_time + base::TimeDelta::FromHours(5);
  download.transient = true;
  download.by_ext_id = "extension-id";
  download.by_ext_name = "extension-name";

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
  download.tab_url = GURL("http://example.com/tab-url");
  download.tab_referrer_url = GURL("http://example.com/tab-referrer");
  download.http_method = "GET";
  download.mime_type = "mime/type";
  download.original_mime_type = "original/mime-type";
  download.start_time = base::Time::Now();
  download.end_time = download.start_time + base::TimeDelta::FromHours(1);
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
  download.last_access_time =
      download.start_time + base::TimeDelta::FromHours(5);
  download.transient = true;
  download.by_ext_id = "extension-id";
  download.by_ext_name = "extension-name";
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

TEST_F(HistoryBackendDBTest, MigratePresentations) {
  // Create the db we want. Use 22 since segments didn't change in that time
  // frame.
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));

  const SegmentID segment_id = 2;
  const URLID url_id = 3;
  const GURL url("http://www.foo.com");
  const std::string url_name(VisitSegmentDatabase::ComputeSegmentName(url));
  const base::string16 title(base::ASCIIToUTF16("Title1"));
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
      s.BindInt64(3, segment_time.ToInternalValue());
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
      s.BindInt64(2, segment_time.ToInternalValue());
      s.BindInt(3, 5);  // visit count.
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  std::vector<std::unique_ptr<PageUsageData>> results = db_->QuerySegmentUsage(
      segment_time, 10, base::Callback<bool(const GURL&)>());
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
      meta.Init(&db, 1, 1);
      meta.SetCompatibleVersionNumber(HistoryDatabase::GetCurrentVersion() + 1);
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
      meta.Init(&db, 1, 1);
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
  const base::string16 title1(base::ASCIIToUTF16("Title1"));
  const base::string16 title2(base::ASCIIToUTF16("Title2"));
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
      s.BindInt64(3, segment_time.ToInternalValue());
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
      s.BindInt64(2, segment_time.ToInternalValue());
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
      s.BindInt64(3, segment_time.ToInternalValue());
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
      s.BindInt64(2, segment_time.ToInternalValue());
      s.BindInt(3, 13);  // visit count.
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  std::vector<std::unique_ptr<PageUsageData>> results =
      db_->QuerySegmentUsage(segment_time, /*max_result_count=*/10,
                             base::Callback<bool(const GURL&)>());
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
  const base::TimeDelta visit_duration1(base::TimeDelta::FromSeconds(30));
  const base::TimeDelta visit_duration2(base::TimeDelta::FromSeconds(45));

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
  const base::TimeDelta visit_duration(base::TimeDelta::FromSeconds(45));

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

// Tests that the migration code correctly replaces the lower_term column in the
// keyword search terms table which normalized_term which contains the
// normalized search term during migration to version 42.
TEST_F(HistoryBackendDBTest, MigrateKeywordSearchTerms) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(41));

  const KeywordID keyword_id = 12;
  const URLID url_id = 34;
  const base::string16 term = base::ASCIIToUTF16("WEEKLY  NEWS  ");
  const base::string16 lower_term = base::i18n::ToLower(term);
  const base::string16 normalized_term =
      base::CollapseWhitespace(lower_term, false);

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

// Test to verify the left-over typed_url sync metadata gets cleared correctly
// during migration to version 41.
TEST_F(HistoryBackendDBTest, MigrateTypedURLLeftoverMetadata) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(40));

  // Define common uninteresting data for visits.
  const VisitID referring_visit = 0;
  const ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  const base::Time visit_time(base::Time::Now());
  const base::TimeDelta visit_duration(base::TimeDelta::FromSeconds(30));

  // The first visit has both a DB entry and a metadata entry.
  const VisitID visit_id1 = 1;
  const URLID url_id1 = 10;
  const SegmentID segment_id1 = 20;
  const std::string metadata_value1 = "BLOB1";

  // The second one as well has both a DB entry and a metadata entry.
  const VisitID visit_id2 = 2;
  const URLID url_id2 = 11;
  const SegmentID segment_id2 = 21;
  const std::string metadata_value2 = "BLOB2";

  // The second visit has only a left-over metadata entry.
  const URLID url_id3 = 12;
  const std::string metadata_value3 = "BLOB3";

  {
    // Open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    const char kInsertVisitStatement[] =
        "INSERT INTO visits "
        "(id, url, visit_time, from_visit, transition, segment_id, "
        "visit_duration) VALUES (?, ?, ?, ?, ?, ?, ?)";
    {
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
    {
      sql::Statement s(db.GetUniqueStatement(kInsertVisitStatement));
      s.BindInt64(0, visit_id2);
      s.BindInt64(1, url_id2);
      s.BindInt64(2, visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
      s.BindInt64(3, referring_visit);
      s.BindInt64(4, transition);
      s.BindInt64(5, segment_id2);
      s.BindInt64(6, visit_duration.InMicroseconds());
      ASSERT_TRUE(s.Run());
    }

    const char kInsertMetadataStatement[] =
        "INSERT INTO typed_url_sync_metadata (storage_key, value) VALUES (?, "
        "?)";
    {
      sql::Statement s(db.GetUniqueStatement(kInsertMetadataStatement));
      s.BindInt64(0, url_id3);
      s.BindString(1, metadata_value3);
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(kInsertMetadataStatement));
      s.BindInt64(0, url_id2);
      s.BindString(1, metadata_value2);
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(kInsertMetadataStatement));
      s.BindInt64(0, url_id1);
      s.BindString(1, metadata_value1);
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Database db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      // The version should have been updated.
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      ASSERT_GE(HistoryDatabase::GetCurrentVersion(), 41);
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(HistoryDatabase::GetCurrentVersion(), s.ColumnInt(0));
    }
    {
      // Check that the left-over metadata entry is deleted.
      sql::Statement s(db.GetUniqueStatement(
          "SELECT storage_key FROM typed_url_sync_metadata"));
      std::set<URLID> remaining_metadata;
      while (s.Step()) {
        remaining_metadata.insert(s.ColumnInt64(0));
      }
      EXPECT_EQ(remaining_metadata.count(url_id3), 0u);
      EXPECT_EQ(remaining_metadata.count(url_id2), 1u);
      EXPECT_EQ(remaining_metadata.count(url_id1), 1u);
    }
  }
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
  const base::Time time(base::Time::Now());

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

  ASSERT_TRUE(db_->IncreaseSegmentVisitCount(segment_id1, time, visit_count1));
  ASSERT_TRUE(db_->IncreaseSegmentVisitCount(segment_id2, time, visit_count2));

  // Without a filter, the "file://" URL should win.
  std::vector<std::unique_ptr<PageUsageData>> results =
      db_->QuerySegmentUsage(time, 1, base::Callback<bool(const GURL&)>());
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(url1, results[0]->GetURL());
  EXPECT_EQ(segment_id1, results[0]->GetID());

  // With the filter, the "file://" URL should be filtered out, so the "http://"
  // URL should win instead.
  std::vector<std::unique_ptr<PageUsageData>> results2 =
      db_->QuerySegmentUsage(time, 1, base::Bind(&FilterURL));
  ASSERT_EQ(1u, results2.size());
  EXPECT_EQ(url2, results2[0]->GetURL());
  EXPECT_EQ(segment_id2, results2[0]->GetID());
}

}  // namespace
}  // namespace history
