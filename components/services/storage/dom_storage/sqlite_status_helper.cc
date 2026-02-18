// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite_status_helper.h"

#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/sqlite_result_code_values.h"

namespace storage {

DbStatus FromSqliteCode(const sql::Database& database) {
  const char* sql_error_message = database.GetErrorMessage();
  int database_error_code = database.GetErrorCode();

  if (sql::IsErrorCatastrophic(database_error_code)) {
    return DbStatus::Corruption(sql_error_message);
  }

  sql::SqliteResultCode sql_result_code =
      sql::ToSqliteResultCode(database.GetErrorCode());

  if (sql_result_code == sql::SqliteResultCode::kNotFound) {
    return DbStatus::NotFound(sql_error_message);
  }

  if (sql_result_code == sql::SqliteResultCode::kIo ||
      sql_result_code == sql::SqliteResultCode::kError) {
    return DbStatus::IOError(sql_error_message);
  }
  return DbStatus::DatabaseEngineCode(database_error_code, sql_error_message);
}

}  // namespace storage
