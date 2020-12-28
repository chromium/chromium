// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/legacy_dom_storage_database.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/sqlite/sqlite3.h"

namespace storage {

LegacyDomStorageDatabase::LegacyDomStorageDatabase(
    const base::FilePath& file_path,
    std::unique_ptr<FilesystemProxy> filesystem_proxy)
    : file_path_(file_path), filesystem_proxy_(std::move(filesystem_proxy)) {
  DCHECK(!file_path_.empty());
  Init();
}

LegacyDomStorageDatabase::LegacyDomStorageDatabase(
    std::unique_ptr<FilesystemProxy> filesystem_proxy)
    : filesystem_proxy_(std::move(filesystem_proxy)) {
  Init();
}

void LegacyDomStorageDatabase::Init() {
  failed_to_open_ = false;
  tried_to_recreate_ = false;
  known_to_be_empty_ = false;
}

LegacyDomStorageDatabase::~LegacyDomStorageDatabase() {
  if (known_to_be_empty_ && !file_path_.empty()) {
    // Delete the db and any lingering journal file from disk.
    Close();
    sql::Database::Delete(file_path_);
  }
}

void LegacyDomStorageDatabase::ReadAllValues(
    LegacyDomStorageValuesMap* result) {
  if (!LazyOpen(false))
    return;

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, "SELECT * from ItemTable"));
  DCHECK(statement.is_valid());

  while (statement.Step()) {
    base::string16 key = statement.ColumnString16(0);
    base::string16 value;
    statement.ColumnBlobAsString16(1, &value);
    (*result)[key] = base::NullableString16(value, false);
  }
  known_to_be_empty_ = result->empty();

  // Drop SQLite's caches.
  db_->TrimMemory();
}

bool LegacyDomStorageDatabase::CommitChanges(
    bool clear_all_first,
    const LegacyDomStorageValuesMap& changes) {
  if (!LazyOpen(!changes.empty())) {
    // If we're being asked to commit changes that will result in an
    // empty database, we return true if the database file doesn't exist.
    return clear_all_first && changes.empty() &&
           !filesystem_proxy_->PathExists(file_path_);
  }

  bool old_known_to_be_empty = known_to_be_empty_;
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  if (clear_all_first) {
    if (!db_->Execute("DELETE FROM ItemTable"))
      return false;
    known_to_be_empty_ = true;
  }

  bool did_delete = false;
  bool did_insert = false;
  auto it = changes.begin();
  for (; it != changes.end(); ++it) {
    sql::Statement statement;
    base::string16 key = it->first;
    base::NullableString16 value = it->second;
    if (value.is_null()) {
      statement.Assign(db_->GetCachedStatement(
          SQL_FROM_HERE, "DELETE FROM ItemTable WHERE key=?"));
      statement.BindString16(0, key);
      did_delete = true;
    } else {
      statement.Assign(db_->GetCachedStatement(
          SQL_FROM_HERE, "INSERT INTO ItemTable VALUES (?,?)"));
      statement.BindString16(0, key);
      statement.BindBlob(1, value.string().data(),
                         value.string().length() * sizeof(base::char16));
      known_to_be_empty_ = false;
      did_insert = true;
    }
    DCHECK(statement.is_valid());
    statement.Run();
  }

  if (!known_to_be_empty_ && did_delete && !did_insert) {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE, "SELECT count(key) from ItemTable"));
    if (statement.Step())
      known_to_be_empty_ = statement.ColumnInt(0) == 0;
  }

  bool success = transaction.Commit();
  if (!success)
    known_to_be_empty_ = old_known_to_be_empty;

  // Drop SQLite's caches.
  db_->TrimMemory();

  return success;
}

void LegacyDomStorageDatabase::ReportMemoryUsage(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& name) {
  if (IsOpen())
    db_->ReportMemoryUsage(pmd, name);
}

bool LegacyDomStorageDatabase::LazyOpen(bool create_if_needed) {
  if (failed_to_open_) {
    // Don't try to open a database that we know has failed
    // already.
    return false;
  }

  if (IsOpen())
    return true;

  bool database_exists = filesystem_proxy_->PathExists(file_path_);

  if (!database_exists && !create_if_needed) {
    // If the file doesn't exist already and we haven't been asked to create
    // a file on disk, then we don't bother opening the database. This means
    // we wait until we absolutely need to put something onto disk before we
    // do so.
    return false;
  }

  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions{
      // This database should only be accessed from the process hosting the
      // storage service, so exclusive locking is appropriate.
      .exclusive_locking = true,
      .page_size = 4096,
      .cache_size = 500});
  db_->set_histogram_tag("DOMStorageDatabase");

  // This database is only opened to migrate DOMStorage data to a new backend.
  // Given the use case, mmap()'s performance improvements are not worth the
  // (tiny amount of) problems that mmap() may cause.
  db_->set_mmap_disabled();

  if (!db_->Open(file_path_)) {
    LOG(ERROR) << "Unable to open DOM storage database at "
               << file_path_.value() << " error: " << db_->GetErrorMessage();
    if (database_exists && !tried_to_recreate_)
      return DeleteFileAndRecreate();
    failed_to_open_ = true;
    return false;
  }

  // sql::Database uses UTF-8 encoding, but WebCore style databases use
  // UTF-16, so ensure we match.
  ignore_result(db_->Execute("PRAGMA encoding=\"UTF-16\""));

  if (!database_exists) {
    // This is a new database, create the table and we're done!
    if (CreateTableV2())
      return true;
  } else {
    // The database exists already - check if we need to upgrade
    // and whether it's usable (i.e. not corrupted).
    SchemaVersion current_version = DetectSchemaVersion();

    if (current_version == V2)
      return true;
  }

  // This is the exceptional case - to try and recover we'll attempt
  // to delete the file and start again.
  Close();
  return DeleteFileAndRecreate();
}

LegacyDomStorageDatabase::SchemaVersion
LegacyDomStorageDatabase::DetectSchemaVersion() {
  DCHECK(IsOpen());

  // Connection::Open() may succeed even if the file we try and open is not a
  // database, however in the case that the database is corrupted to the point
  // that SQLite doesn't actually think it's a database,
  // sql::Database::GetCachedStatement will DCHECK when we later try and
  // run statements. So we run a query here that will not DCHECK but fail
  // on an invalid database to verify that what we've opened is usable.
  if (db_->ExecuteAndReturnErrorCode("PRAGMA auto_vacuum") != SQLITE_OK)
    return INVALID;

  // Look at the current schema - if it doesn't look right, assume corrupt.
  if (!db_->DoesTableExist("ItemTable") ||
      !db_->DoesColumnExist("ItemTable", "key") ||
      !db_->DoesColumnExist("ItemTable", "value"))
    return INVALID;

  return V2;
}

bool LegacyDomStorageDatabase::CreateTableV2() {
  DCHECK(IsOpen());

  return db_->Execute(
      "CREATE TABLE ItemTable ("
      "key TEXT UNIQUE ON CONFLICT REPLACE, "
      "value BLOB NOT NULL ON CONFLICT FAIL)");
}

bool LegacyDomStorageDatabase::DeleteFileAndRecreate() {
  DCHECK(!IsOpen());
  DCHECK(filesystem_proxy_->PathExists(file_path_));

  // We should only try and do this once.
  if (tried_to_recreate_)
    return false;

  tried_to_recreate_ = true;

  base::Optional<base::File::Info> info =
      filesystem_proxy_->GetFileInfo(file_path_);
  // If it's not a directory and we can delete the file, try and open it again.
  if (info && !info->is_directory && sql::Database::Delete(file_path_)) {
    return LazyOpen(true);
  }

  failed_to_open_ = true;
  return false;
}

void LegacyDomStorageDatabase::Close() {
  db_.reset(nullptr);
}

}  // namespace storage
