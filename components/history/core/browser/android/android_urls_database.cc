// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/android/android_urls_database.h"

#include <stdint.h>

#include "base/logging.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace history {

AndroidURLsDatabase::AndroidURLsDatabase() {
}

AndroidURLsDatabase::~AndroidURLsDatabase() {
}

bool AndroidURLsDatabase::CreateAndroidURLsTable() {
  const char* name = "android_urls";
  if (!GetDB().DoesTableExist(name)) {
    std::string sql;
    sql.append("CREATE TABLE ");
    sql.append(name);
    sql.append("("
               "id INTEGER PRIMARY KEY,"
               "raw_url LONGVARCHAR,"              // Passed in raw url.
               "url_id INTEGER NOT NULL"           // url id in urls table.
               ")");
    if (!GetDB().Execute(sql)) {
      LOG(ERROR) << GetDB().GetErrorMessage();
      return false;
    }

    if (!GetDB().Execute("CREATE INDEX android_urls_raw_url_idx"
                         " ON android_urls(raw_url)")) {
      LOG(ERROR) << GetDB().GetErrorMessage();
      return false;
    }

    if (!GetDB().Execute("CREATE INDEX android_urls_url_id_idx"
                         " ON android_urls(url_id)")) {
      LOG(ERROR) << GetDB().GetErrorMessage();
      return false;
    }
  }
  return true;
}

AndroidURLID AndroidURLsDatabase::AddAndroidURLRow(const std::string& raw_url,
                                                   URLID url_id) {
  if (GetAndroidURLRow(url_id, NULL)) {
    LOG(ERROR) << "url_id already exist";
    return 0;
  }

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO android_urls (raw_url, url_id) VALUES (?, ?)"));

  statement.BindString(0, raw_url);
  statement.BindInt64(1, static_cast<int64_t>(url_id));

  if (!statement.Run()) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool AndroidURLsDatabase::GetAndroidURLRow(URLID url_id, AndroidURLRow* row) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, raw_url, url_id FROM android_urls WHERE url_id = ?"));

  statement.BindInt64(0, url_id);

  if (!statement.Step())
    return false;
  if (row) {
    row->id = statement.ColumnInt64(0);
    row->raw_url = statement.ColumnString(1);
    row->url_id = statement.ColumnInt64(2);
  }
  return true;
}

bool AndroidURLsDatabase::DeleteAndroidURLRows(
    const std::vector<URLID>& url_ids) {
  if (url_ids.empty())
    return true;

  std::string sql;
  sql.append("DELETE FROM android_urls ");
  std::ostringstream oss;
  bool has_id = false;
  for (std::vector<URLID>::const_iterator i = url_ids.begin();
       i != url_ids.end(); ++i) {
    if (has_id)
      oss << ", ";
    else
      has_id = true;
    oss << *i;
  }

  if (has_id) {
    sql.append(" WHERE url_id in ( ");
    sql.append(oss.str());
    sql.append(" )");
  }

  if (!GetDB().Execute(sql)) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return false;
  }
  return true;
}

bool AndroidURLsDatabase::DeleteUnusedAndroidURLs() {
  return GetDB().Execute("DELETE FROM android_urls WHERE url_id NOT IN ("
                         "SELECT id FROM urls)");
}

bool AndroidURLsDatabase::UpdateAndroidURLRow(AndroidURLID id,
                                              const std::string& raw_url,
                                              URLID url_id) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE android_urls SET raw_url = ?, url_id = ? WHERE id = ?"));

  statement.BindString(0, raw_url);
  statement.BindInt64(1, url_id);
  statement.BindInt64(2, id);

  if (!statement.is_valid()) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return false;
  }

  if (!statement.Run()) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return false;
  }

  return true;
}

bool AndroidURLsDatabase::ClearAndroidURLRows() {
  // The android_urls table might not exist if the Android content provider is
  // never used, especially in the unit tests. See http://b/6385692.
  if (GetDB().DoesTableExist("android_urls"))
    return GetDB().Execute("DELETE FROM android_urls");

  return true;
}

bool AndroidURLsDatabase::MigrateToVersion22() {
  if (!GetDB().DoesTableExist("android_urls"))
    return true;

  if (!GetDB().Execute("ALTER TABLE android_urls RENAME TO android_urls_tmp"))
    return false;

  if (!GetDB().Execute("DROP INDEX android_urls_raw_url_idx"))
    return false;

  if (!GetDB().Execute("DROP INDEX android_urls_url_id_idx"))
    return false;

  if (!CreateAndroidURLsTable())
    return false;

  if (!GetDB().Execute(
      "INSERT INTO android_urls (id, raw_url, url_id) "
      "SELECT id, raw_url, url_id FROM android_urls_tmp"))
    return false;

  if (!GetDB().Execute("DROP TABLE android_urls_tmp"))
    return false;

  return true;
}

}  // namespace history
