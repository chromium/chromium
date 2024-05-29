// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/file_info_table.h"

#include "sql/statement.h"

namespace ash::file_manager {

namespace {

#define FILE_INFO_TABLE "file_info_table"
#define URL_ID "url_id"
#define LAST_MODIFIED "last_modified"
#define SIZE "size"
#define REMOTE_ID "remote_id"

// The statement used to create the file_info table.
static constexpr char kCreateFileInfoTableQuery[] =
    // clang-format off
    "CREATE TABLE IF NOT EXISTS " FILE_INFO_TABLE "("
      URL_ID " INTEGER PRIMARY KEY NOT NULL REFERENCES url_table(url_id),"
      LAST_MODIFIED " INTEGER NOT NULL,"
      SIZE " INTEGER NOT NULL,"
      REMOTE_ID " TEXT)";
// clang-format on

// The statement used to insert a new term into the table.
static constexpr char kInsertFileInfoQuery[] =
    // clang-format off
    "INSERT OR REPLACE INTO " FILE_INFO_TABLE "(" URL_ID ", " LAST_MODIFIED ", "
    SIZE ", " REMOTE_ID ") VALUES (?, ?, ?, ?)";
// clang-format on

// The statement used to delete a FileInfo from the database by URL ID.
static constexpr char kDeleteFileInfoQuery[] =
    // clang-format off
    "DELETE FROM " FILE_INFO_TABLE " WHERE " URL_ID " = ?";
// clang-format on

// The statement used fetch the file info by the URL ID.
static constexpr char kGetFileInfoQuery[] =
    // clang-format off
    "SELECT " LAST_MODIFIED ", " SIZE ", " REMOTE_ID " FROM "
    FILE_INFO_TABLE " WHERE " URL_ID " = ?";
// clang-format on

}  // namespace

FileInfoTable::FileInfoTable(sql::Database* db) : db_(db) {}
FileInfoTable::~FileInfoTable() = default;

bool FileInfoTable::Init() {
  if (!db_->is_open()) {
    LOG(WARNING) << "Faield to initialize " FILE_INFO_TABLE " "
                 << "due to closed database";
    return false;
  }
  sql::Statement create_table(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreateFileInfoTableQuery));
  DCHECK(create_table.is_valid()) << "Invalid create the table statement: \""
                                  << create_table.GetSQLStatement() << "\"";
  if (!create_table.Run()) {
    LOG(ERROR) << "Failed to create the table";
    return false;
  }
  return true;
}

std::optional<FileInfo> FileInfoTable::GetFileInfo(int64_t url_id) const {
  sql::Statement get_file_info(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetFileInfoQuery));
  DCHECK(get_file_info.is_valid()) << "Invalid get file info statement: \""
                                   << get_file_info.GetSQLStatement() << "\"";
  get_file_info.BindInt64(0, url_id);
  if (!get_file_info.Step()) {
    return std::nullopt;
  }
  base::Time last_modified = get_file_info.ColumnTime(0);
  int64_t size = get_file_info.ColumnInt64(1);
  FileInfo file_info(GURL(), size, last_modified);

  if (get_file_info.GetColumnType(2) != sql::ColumnType::kNull) {
    file_info.remote_id = get_file_info.ColumnString(2);
  }
  return file_info;
}

int64_t FileInfoTable::DeleteFileInfo(int64_t url_id) {
  sql::Statement delete_file_info(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteFileInfoQuery));
  DCHECK(delete_file_info.is_valid())
      << "Invalid get file info statement: \""
      << delete_file_info.GetSQLStatement() << "\"";
  delete_file_info.BindInt64(0, url_id);
  if (!delete_file_info.Run()) {
    return -1;
  }
  return url_id;
}

int64_t FileInfoTable::PutFileInfo(int64_t url_id, const FileInfo& info) {
  sql::Statement insert_file_info(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertFileInfoQuery));
  DCHECK(insert_file_info.is_valid())
      << "Invalid create the table statement: \""
      << insert_file_info.GetSQLStatement() << "\"";
  insert_file_info.BindInt64(0, url_id);
  insert_file_info.BindTime(1, info.last_modified);
  insert_file_info.BindInt64(2, info.size);
  if (info.remote_id.has_value()) {
    insert_file_info.BindString(3, info.remote_id.value());
  } else {
    insert_file_info.BindNull(3);
  }
  if (!insert_file_info.Run()) {
    LOG(ERROR) << "Failed to insert file_info";
    return -1;
  }
  return url_id;
}

}  // namespace ash::file_manager
