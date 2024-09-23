// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/in_memory_database.h"

#include <tuple>

#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"

namespace history {

InMemoryDatabase::InMemoryDatabase()
    : db_({.page_size = 4096, .cache_size = 500}) {}

InMemoryDatabase::~InMemoryDatabase() = default;

bool InMemoryDatabase::InitDB() {
  if (!db_.OpenInMemory()) {
    NOTREACHED_IN_MIGRATION()
        << "Cannot open databse " << GetDB().GetErrorMessage();
    return false;
  }

  // No reason to leave data behind in memory when rows are removed.
  std::ignore = db_.Execute("PRAGMA auto_vacuum=1");

  // Create the URL table, but leave it empty for now.
  if (!CreateURLTable(false)) {
    DUMP_WILL_BE_NOTREACHED() << "Unable to create table";
    db_.Close();
    return false;
  }

  // Create the keyword search terms table.
  if (!InitKeywordSearchTermsTable()) {
    NOTREACHED_IN_MIGRATION() << "Unable to create keyword search terms";
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

  // Attach to the history database on disk.
  if (!db_.AttachDatabase(history_name, "history")) {
    return false;
  }

  // Copy URL data to memory.

  // Need to explicitly specify the column names here since databases on disk
  // may or may not have a favicon_id column, but the in-memory one will never
  // have it. Therefore, the columns aren't guaranteed to match.
  //
  // TODO(crbug.com/40527222) Once we can guarantee that the favicon_id
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
  UMA_HISTOGRAM_COUNTS_1M("History.InMemoryDBItemCount",
                          db_.GetLastChangeCount());

  // Insert keyword search related URLs.
  if (!db_.Execute("INSERT OR IGNORE INTO urls SELECT u.id, u.url, u.title, "
                   "u.visit_count, u.typed_count, u.last_visit_time, u.hidden "
                   "FROM history.urls u JOIN history.keyword_search_terms kst "
                   "WHERE u.typed_count = 0 AND u.id = kst.url_id")) {
    // Unable to get data from the history database. This is OK, the file may
    // just not exist yet.
  }

  // Copy search terms to memory.
  if (!db_.Execute(
      "INSERT INTO keyword_search_terms SELECT * FROM "
      "history.keyword_search_terms")) {
    // Unable to get data from the history database. This is OK, the file may
    // just not exist yet.
  }

  // Detach from the history database on disk.
  if (!db_.DetachDatabase("history")) {
    NOTREACHED_IN_MIGRATION() << "Unable to detach from history database.";
    return false;
  }

  // Index the table, this is faster than creating the index first and then
  // inserting into it.
  CreateMainURLIndex();

  // After this point, the database may be accessed from another sequence.
  db_.DetachFromSequence();

  return true;
}

sql::Database& InMemoryDatabase::GetDB() {
  return db_;
}

}  // namespace history
