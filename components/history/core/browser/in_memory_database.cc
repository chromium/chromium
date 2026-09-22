// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/in_memory_database.h"

#include <memory>
#include <tuple>

#include "base/notreached.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "sql/database.h"
#include "sql/transaction.h"

namespace history {

InMemoryDatabase::InMemoryDatabase() : db_(/*tag=*/"HistoryInMemoryDB") {}

InMemoryDatabase::~InMemoryDatabase() = default;

bool InMemoryDatabase::InitDB() {
  if (!db_.OpenInMemory()) {
    NOTREACHED() << "Cannot open databse " << GetDB().GetErrorMessage();
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
    NOTREACHED() << "Unable to create keyword search terms";
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

bool InMemoryDatabase::InitFromUrlDatabase(URLDatabase& history_db) {
  if (!InitDB()) {
    return false;
  }

  // Populating is best effort. An empty or incomplete cache is still useful
  // because `InMemoryHistoryBackend` keeps it up to date from here on, and read
  // errors on `history_db` are reported through its own error callback.
  PopulateFrom(history_db);

  // Index the table, this is faster than creating the index first and then
  // inserting into it.
  CreateMainURLIndex();

  // After this point, the database may be accessed from another sequence.
  db_.DetachFromSequence();

  return true;
}

void InMemoryDatabase::PopulateFrom(URLDatabase& history_db) {
  // The transaction holds a reference to `db_` and must be gone before
  // `InitFromUrlDatabase()` detaches the database from this sequence.
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return;
  }

  URLEnumerator url_enumerator;
  if (!history_db.InitURLEnumeratorForTypedOrSearched(&url_enumerator)) {
    return;
  }
  for (URLRow row; url_enumerator.GetNextURL(&row);) {
    if (!InsertOrUpdateURLRowByID(row)) {
      return;
    }
  }

  std::unique_ptr<KeywordSearchTermRowEnumerator> term_enumerator =
      history_db.CreateKeywordSearchTermRowEnumerator();
  if (!term_enumerator) {
    return;
  }
  while (std::unique_ptr<KeywordSearchTermRow> row =
             term_enumerator->GetNextRow()) {
    if (!InsertKeywordSearchTermRow(*row)) {
      return;
    }
  }

  std::ignore = transaction.Commit();
}

sql::Database& InMemoryDatabase::GetDB() {
  return db_;
}

}  // namespace history
