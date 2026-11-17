// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_encrypted_vfs/encrypted_database.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/pass_key.h"
#include "components/sqlite_encrypted_vfs/database_key_manager.h"
#include "components/sqlite_encrypted_vfs/sqlite_encrypted_vfs.h"
#include "components/sqlite_encrypted_vfs/sqlite_encrypted_vfs_metrics.h"

namespace sqlite_encrypted_vfs {

EncryptedDatabase::EncryptedDatabase(sql::DatabaseOptions options,
                                     sql::Database::Tag tag)
    : options_(std::move(options)), tag_(tag) {}

EncryptedDatabase::~EncryptedDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Explicit, because the VFS must outlive the connection.
  db_.reset();
  vfs_.reset();
}

void EncryptedDatabase::set_error_callback(
    sql::Database::ErrorCallback error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  error_callback_ = std::move(error_callback);
  if (db_) {
    db_->set_error_callback(error_callback_);
  }
}

void EncryptedDatabase::reset_error_callback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  error_callback_.Reset();
  if (db_) {
    db_->reset_error_callback();
  }
}

EncryptedDatabase::OpenResult EncryptedDatabase::Open(
    const base::FilePath& db_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Checked up front, before any state is touched: even a failed `Open()` may
  // have resolved `db_path_` (kept so `Delete()` can recover an unusable
  // database), so a retry on the same object must not reuse it.
  CHECK(!open_called_) << "Open() may only be called once";
  CHECK(!deleted_) << "Open() called after Delete()";
  open_called_ = true;

  base::ElapsedTimer open_timer;
  OpenResult result = OpenImpl(db_path);
  RecordDatabaseAction(DatabaseAction::kOpen, result == OpenResult::kOk,
                       open_timer.Elapsed());
  return result;
}

EncryptedDatabase::OpenResult EncryptedDatabase::OpenImpl(
    const base::FilePath& db_path) {
  base::FilePath normalized_path = NormalizeDatabasePath(db_path);
  if (normalized_path.empty()) {
    return OpenResult::kPermanentError;
  }
  // Another `EncryptedDatabase` already has this database open. Reject before
  // any `-key` file I/O (which could re-seal and rewrite the key file), and
  // leave `db_path_` unset so a subsequent `Delete()` cannot remove the other
  // connection's live database.
  if (IsEncryptedVfsRegisteredForPath(normalized_path)) {
    return OpenResult::kPermanentError;
  }
  db_path_ = std::move(normalized_path);

  DatabaseKeyManager::KeyResult key_result =
      DatabaseKeyManager::Get(base::PassKey<EncryptedDatabase>())
          .GetKeyForDatabase(db_path_);
  if (!key_result.has_value()) {
    // Fail closed: no key, no database, and no unencrypted fallback.
    switch (key_result.error()) {
      case DatabaseKeyManager::KeyError::kTransientError:
        return OpenResult::kTransientError;
      case DatabaseKeyManager::KeyError::kKeyUnrecoverable:
        return OpenResult::kKeyUnrecoverable;
      case DatabaseKeyManager::KeyError::kPermanentError:
        return OpenResult::kPermanentError;
    }
    NOTREACHED();
  }

  std::unique_ptr<SqliteEncryptedVfs> vfs =
      SqliteEncryptedVfs::Create(db_path_, *key_result);
  if (!vfs) {
    return OpenResult::kPermanentError;
  }

  // These two are not the caller's to choose. Memory-mapped I/O bypasses the
  // VFS read path entirely, and the VFS name is what routes this connection
  // through the encryption layer at all.
  vfs_ = std::move(vfs);
  sql::DatabaseOptions options = options_;
  options.set_vfs_name_discouraged(vfs_->vfs_name()).set_mmap_enabled(false);
  db_ = std::make_unique<sql::Database>(std::move(options), tag_);
  if (error_callback_) {
    db_->set_error_callback(error_callback_);
  }
  if (!db_->Open(db_path_)) {
    db_.reset();
    vfs_.reset();
    return OpenResult::kPermanentError;
  }

  return OpenResult::kOk;
}

bool EncryptedDatabase::is_open() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_ && db_->is_open();
}

sql::Database& EncryptedDatabase::DB() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(db_) << "DB() called when database is not open";
  return *db_;
}

bool EncryptedDatabase::Delete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  deleted_ = true;

  base::ElapsedTimer delete_timer;

  // `sql::Database::Delete()` must not run against an open connection, and the
  // VFS must outlive the connection it serves.
  db_.reset();
  vfs_.reset();

  // No database path was resolved (`Open()` was never called, the path was
  // invalid, or another connection owns the database), so there is nothing
  // this object may delete.
  if (db_path_.empty()) {
    RecordDatabaseAction(DatabaseAction::kDelete, false,
                         delete_timer.Elapsed());
    return false;
  }

  const bool files_deleted = sql::Database::Delete(db_path_);

  // Key file last, and only once the database is gone: deleting the key while
  // the database file remains would leave an encrypted database that nothing
  // can ever open, whereas a leftover key file is merely orphaned.
  const bool key_deleted =
      files_deleted &&
      DatabaseKeyManager::Get(base::PassKey<EncryptedDatabase>())
          .DeleteKeyForDatabase(db_path_);
  const bool success = files_deleted && key_deleted;
  RecordDatabaseAction(DatabaseAction::kDelete, success,
                       delete_timer.Elapsed());
  return success;
}

}  // namespace sqlite_encrypted_vfs
