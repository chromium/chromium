// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/in_memory_database.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace history {

InMemoryDatabase::InMemoryDatabase() {}

InMemoryDatabase::~InMemoryDatabase() {
}

bool InMemoryDatabase::InitDB() {
  // Set the database page size to 4K for better performance.
  db_.set_page_size(4096);

  if (!db_.OpenInMemory()) {
    NOTREACHED() << "Cannot open databse " << GetDB().GetErrorMessage();
    return false;
  }

  // No reason to leave data behind in memory when rows are removed.
  ignore_result(db_.Execute("PRAGMA auto_vacuum=1"));

  // Create the URL table, but leave it empty for now.
  if (!CreateURLTable(false)) {
    NOTREACHED() << "Unable to create table";
    db_.Close();
    return false;
  }

  // Create the keyword search terms table.
  if (!InitKeywordSearchTermsTable()) {
    NOTREACHED() << "Unable to create keyword search terms";
    db_.Close();
    return false;
  }

  return true;
}

bool InMemoryDatabase::InitFromScratch() {
  if (!InitDB())
    return false;

  // InitDB doesn't create the index so in the disk-loading case, it can be
  // added afterwards.
  CreateMainURLIndex();
  return true;
}

bool InMemoryDatabase::InitFromDisk(const base::FilePath& history_name) {
  if (!InitDB())
    return false;

  // Attach to the history database on disk.  (We can't ATTACH in the middle of
  // a transaction.)
  sql::Statement attach(GetDB().GetUniqueStatement("ATTACH ? AS history"));
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  attach.BindString(0, history_name.value());
#else
  attach.BindString(0, base::WideToUTF8(history_name.value()));
#endif
  if (!attach.Run())
    return false;

  // Copy URL data to memory.
  base::TimeTicks begin_load = base::TimeTicks::Now();

  // Need to explicitly specify the column names here since databases on disk
  // may or may not have a favicon_id column, but the in-memory one will never
  // have it. Therefore, the columns aren't guaranteed to match.
  //
  // TODO(https://crbug.com/736136) Once we can guarantee that the favicon_id
  // column doesn't exist with migration code, this can be replaced with the
  // simpler:
  //   "INSERT INTO urls SELECT * FROM history.urls WHERE typed_count > 0"
  // which does not require us to keep the list of columns in sync. However,
  // we may still want to keep the explicit columns as a safety measure.
  if (!db_.Execute(
      "INSERT INTO urls "
      "(id, url, title, visit_count, typed_count, last_visit_time, hidden) "
      "SELECT "
      "id, url, title, visit_count, typed_count, last_visit_time, hidden "
      "FROM history.urls WHERE typed_count > 0")) {
    // Unable to get data from the history database. This is OK, the file may
    // just not exist yet.
  }
  base::TimeTicks end_load = base::TimeTicks::Now();
  UMA_HISTOGRAM_MEDIUM_TIMES("History.InMemoryDBPopulate",
                             end_load - begin_load);
  UMA_HISTOGRAM_COUNTS_1M("History.InMemoryDBItemCount",
                          db_.GetLastChangeCount());

  // Insert keyword search related URLs.
  begin_load = base::TimeTicks::Now();
  if (!db_.Execute("INSERT OR IGNORE INTO urls SELECT u.id, u.url, u.title, "
                   "u.visit_count, u.typed_count, u.last_visit_time, u.hidden "
                   "FROM history.urls u JOIN history.keyword_search_terms kst "
                   "WHERE u.typed_count = 0 AND u.id = kst.url_id")) {
    // Unable to get data from the history database. This is OK, the file may
    // just not exist yet.
  }
  end_load = base::TimeTicks::Now();
  UMA_HISTOGRAM_MEDIUM_TIMES("History.InMemoryDBKeywordURLPopulate",
                             end_load - begin_load);
  UMA_HISTOGRAM_COUNTS_1M("History.InMemoryDBKeywordURLItemCount",
                          db_.GetLastChangeCount());

  // Copy search terms to memory.
  begin_load = base::TimeTicks::Now();
  if (!db_.Execute(
      "INSERT INTO keyword_search_terms SELECT * FROM "
      "history.keyword_search_terms")) {
    // Unable to get data from the history database. This is OK, the file may
    // just not exist yet.
  }
  end_load = base::TimeTicks::Now();
  UMA_HISTOGRAM_MEDIUM_TIMES("History.InMemoryDBKeywordTermsPopulate",
                             end_load - begin_load);
  UMA_HISTOGRAM_COUNTS_1M("History.InMemoryDBKeywordTermsCount",
                          db_.GetLastChangeCount());

  // Detach from the history database on disk.
  if (!db_.Execute("DETACH history")) {
    NOTREACHED() << "Unable to detach from history database.";
    return false;
  }

  // Index the table, this is faster than creating the index first and then
  // inserting into it.
  CreateMainURLIndex();

  return true;
}

sql::Database& InMemoryDatabase::GetDB() {
  return db_;
}

}  // namespace history
