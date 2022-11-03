// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/android/android_cache_database.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "components/history/core/browser/android/android_time.h"
#include "sql/database.h"
#include "sql/statement.h"

using base::Time;

namespace history {

AndroidCacheDatabase::AndroidCacheDatabase() {
}

AndroidCacheDatabase::~AndroidCacheDatabase() {
}

sql::InitStatus AndroidCacheDatabase::InitAndroidCacheDatabase(
    const base::FilePath& db_name) {
  if (!CreateDatabase(db_name))
    return sql::INIT_FAILURE;

  if (!Attach())
    return sql::INIT_FAILURE;

  if (!CreateBookmarkCacheTable())
    return sql::INIT_FAILURE;

  if (!CreateSearchTermsTable())
    return sql::INIT_FAILURE;

  return sql::INIT_OK;
}

bool AndroidCacheDatabase::AddBookmarkCacheRow(const Time& created_time,
                                               const Time& last_visit_time,
                                               URLID url_id) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO android_cache_db.bookmark_cache (created_time, "
      "last_visit_time, url_id) VALUES (?, ?, ?)"));

  statement.BindTime(0, created_time);
  statement.BindTime(1, last_visit_time);
  statement.BindInt64(2, url_id);

  if (!statement.Run()) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return false;
  }

  return true;
}

bool AndroidCacheDatabase::ClearAllBookmarkCache() {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM android_cache_db.bookmark_cache"));
  if (!statement.Run()) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return false;
  }
  return true;
}

bool AndroidCacheDatabase::MarkURLsAsBookmarked(
    const std::vector<URLID>& url_ids) {
  bool has_id = false;
  std::ostringstream oss;
  for (const auto& url_id : url_ids) {
    if (has_id)
      oss << ", ";
    else
      has_id = true;
    oss << url_id;
  }

  if (!has_id)
    return true;

  std::string sql("UPDATE android_cache_db.bookmark_cache "
                  "SET bookmark = 1 WHERE url_id in (");
  sql.append(oss.str());
  sql.append(")");
  if (!GetDB().Execute(sql.c_str())) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return false;
  }
  return true;
}

bool AndroidCacheDatabase::SetFaviconID(URLID url_id,
                                        favicon_base::FaviconID favicon_id) {
  sql::Statement update_statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE android_cache_db.bookmark_cache "
      "SET favicon_id = ? WHERE url_id = ? "));

  update_statement.BindInt64(0, favicon_id);
  update_statement.BindInt64(1, url_id);
  if (!update_statement.Run()) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return false;
  }
  return true;
}

SearchTermID AndroidCacheDatabase::AddSearchTerm(
    const std::u16string& term,
    const base::Time& last_visit_time) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO android_cache_db.search_terms (search, "
      "date) VALUES (?, ?)"));

  statement.BindString16(0, term);
  statement.BindTime(1, last_visit_time);

  if (!statement.Run()) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return 0;
  }

  return GetDB().GetLastInsertRowId();
}

bool AndroidCacheDatabase::UpdateSearchTerm(SearchTermID id,
                                            const SearchTermRow& row) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE android_cache_db.search_terms "
      "SET search = ?, date = ? "
      "WHERE _id = ?"
      ));
  statement.BindString16(0, row.term);
  statement.BindTime(1, row.last_visit_time);
  statement.BindInt64(2, id);

  return statement.Run();
}

SearchTermID AndroidCacheDatabase::GetSearchTerm(const std::u16string& term,
                                                 SearchTermRow* row) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT _id, search, date "
      "FROM android_cache_db.search_terms "
      "WHERE search = ?"
      ));
  if (!statement.is_valid()) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return 0;
  }
  statement.BindString16(0, term);
  if (!statement.Step())
    return 0;

  if (row) {
    row->id = statement.ColumnInt64(0);
    row->term = statement.ColumnString16(1);
    row->last_visit_time = statement.ColumnTime(2);
  }
  return statement.ColumnInt64(0);
}

bool AndroidCacheDatabase::DeleteUnusedSearchTerms() {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM android_cache_db.search_terms "
      "WHERE search NOT IN (SELECT DISTINCT term FROM keyword_search_terms)"
      ));
  if (!statement.is_valid())
    return false;
  return statement.Run();
}

bool AndroidCacheDatabase::CreateDatabase(const base::FilePath& db_name) {
  db_name_ = db_name;
  sql::Database::Delete(db_name_);

  // Using a new connection, otherwise we can not create the database.
  //
  // The db doesn't store too much data, so we don't need that big a page
  // size or cache.
  //
  // The database is open in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  sql::Database connection(
      {.exclusive_locking = true, .page_size = 2048, .cache_size = 32});

  if (!connection.Open(db_name_)) {
    LOG(ERROR) << connection.GetErrorMessage();
    return false;
  }
  connection.Close();
  return true;
}

bool AndroidCacheDatabase::CreateBookmarkCacheTable() {
  const char* name = "android_cache_db.bookmark_cache";
  DCHECK(!GetDB().DoesTableExist(name));

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append("("
             "id INTEGER PRIMARY KEY,"
             "created_time INTEGER NOT NULL,"     // Time in millisecond.
             "last_visit_time INTEGER NOT NULL,"  // Time in millisecond.
             "url_id INTEGER NOT NULL,"           // url id in urls table.
             "favicon_id INTEGER DEFAULT NULL,"   // favicon id.
             "bookmark INTEGER DEFAULT 0"         // whether is bookmark.
             ")");
  if (!GetDB().Execute(sql.c_str())) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return false;
  }

  sql.assign("CREATE INDEX ");
  sql.append("android_cache_db.bookmark_cache_url_id_idx ON "
             "bookmark_cache(url_id)");
  if (!GetDB().Execute(sql.c_str())) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return false;
  }
  return true;
}

bool AndroidCacheDatabase::CreateSearchTermsTable() {
  const char* name = "android_cache_db.search_terms";

  // The table's column name matchs Android's definition.
  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append("("
             "_id INTEGER PRIMARY KEY,"
             "date INTEGER NOT NULL,"   // last visit time in millisecond.
             "search LONGVARCHAR NOT NULL)");   // The actual search term.

  if (!GetDB().Execute(sql.c_str())) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return false;
  }

  sql.assign("CREATE INDEX "
             "android_cache_db.search_terms_term_idx ON "
             "search_terms(search)");
  if (!GetDB().Execute(sql.c_str())) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return false;
  }
  return true;
}

bool AndroidCacheDatabase::Attach() {
  // Commit all open transactions to make attach succeed.
  int transaction_nesting = GetDB().transaction_nesting();
  int count = transaction_nesting;
  while (count--)
    GetDB().CommitTransaction();

  bool result = DoAttach();

  // No matter whether the attach succeeded or not, we need to create the
  // transaction stack again.
  count = transaction_nesting;
  while (count--)
    GetDB().BeginTransaction();
  return result;
}

bool AndroidCacheDatabase::DoAttach() {
  std::string sql("ATTACH ? AS android_cache_db");
  sql::Statement attach(GetDB().GetUniqueStatement(sql.c_str()));
  if (!attach.is_valid())
    // Keep the transaction open, even though we failed.
    return false;

  attach.BindString(0, db_name_.value());
  if (!attach.Run()) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return false;
  }

  return true;
}

}  // namespace history
