// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"

#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/strings/string_view_util.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/sqlite/sqlite_entry_impl.h"
#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_sandboxed_vfs.h"
#include "components/persistent_cache/transaction_error.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace {

constexpr char kTag[] = "PersistentCache";

}  // namespace

namespace persistent_cache {

// static
SqliteVfsFileSet SqliteBackendImpl::GetVfsFileSetFromParams(
    BackendParams backend_params) {
  CHECK_EQ(backend_params.type, BackendType::kSqlite);

  base::UnsafeSharedMemoryRegion shared_lock =
      std::move(backend_params.shared_lock);

  base::WritableSharedMemoryMapping mapped_shared_lock = shared_lock.Map();

  using AccessRights = SandboxedFile::AccessRights;
  std::unique_ptr<SandboxedFile> db_file = std::make_unique<SandboxedFile>(
      std::move(backend_params.db_file), std::move(backend_params.db_file_path),
      backend_params.db_file_is_writable ? AccessRights::kReadWrite
                                         : AccessRights::kReadOnly,
      std::move(mapped_shared_lock));
  std::unique_ptr<SandboxedFile> journal_file = std::make_unique<SandboxedFile>(
      std::move(backend_params.journal_file),
      std::move(backend_params.journal_file_path),
      backend_params.journal_file_is_writable ? AccessRights::kReadWrite
                                              : AccessRights::kReadOnly);

  return SqliteVfsFileSet(std::move(db_file), std::move(journal_file),
                          std::move(shared_lock));
}

SqliteBackendImpl::SqliteBackendImpl(BackendParams backend_params)
    : SqliteBackendImpl(GetVfsFileSetFromParams(std::move(backend_params))) {}

SqliteBackendImpl::SqliteBackendImpl(SqliteVfsFileSet vfs_file_set)
    : database_path_(vfs_file_set.GetDbVirtualFilePath()),
      vfs_file_set_(std::move(vfs_file_set)),
      unregister_runner_(
          SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
              vfs_file_set_)),
      db_(std::in_place,
          sql::DatabaseOptions()
              .set_exclusive_locking(false)
              .set_vfs_name_discouraged(
                  SqliteSandboxedVfsDelegate::kSqliteVfsName)
              // Prevent SQLite from trying to use mmap, as SandboxedVfs does
              // not currently support this.
              .set_mmap_enabled(false),
          sql::Database::Tag(kTag)) {}

SqliteBackendImpl::~SqliteBackendImpl() {
  base::AutoLock lock(lock_, base::subtle::LockTracking::kEnabled);
  db_.reset();
}

bool SqliteBackendImpl::Initialize() {
  CHECK(!initialized_);
  TRACE_EVENT0("persistent_cache", "initialize");

  // Open  `db_` under `lock_` with lock tracking enabled. This allows this
  // class to be usable from multiple threads even though `sql::Database` is
  // sequence bound.
  base::AutoLock lock(lock_, base::subtle::LockTracking::kEnabled);

  if (!db_->Open(database_path_)) {
    TRACE_EVENT_INSTANT1("persistent_cache", "open_failed",
                         TRACE_EVENT_SCOPE_THREAD, "error_code",
                         db_->GetErrorCode());
    return false;
  }

  if (!db_->Execute(
          "CREATE TABLE IF NOT EXISTS entries(key TEXT PRIMARY KEY UNIQUE NOT "
          "NULL, content BLOB NOT NULL, input_signature INTEGER, "
          "write_timestamp INTEGER)")) {
    TRACE_EVENT_INSTANT1("persistent_cache", "create_failed",
                         TRACE_EVENT_SCOPE_THREAD, "error_code",
                         db_->GetErrorCode());
    return false;
  }

  initialized_ = true;
  return true;
}

base::expected<std::unique_ptr<Entry>, TransactionError>
SqliteBackendImpl::Find(std::string_view key) {
  base::AutoLock lock(lock_, base::subtle::LockTracking::kEnabled);
  CHECK(initialized_);
  CHECK_GT(key.length(), 0ull);
  TRACE_EVENT0("persistent_cache", "Find");

  sql::Statement stm = sql::Statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT content, input_signature, write_timestamp "
      "FROM entries WHERE key = ?"));
  stm.BindString(0, key);

  DCHECK(stm.is_valid());

  // Cache hit.
  if (stm.Step()) {
    return SqliteEntryImpl::MakeUnique(
        Passkey(), stm.ColumnString(0),
        EntryMetadata{.input_signature = stm.ColumnInt64(1),
                      .write_timestamp = stm.ColumnInt64(2)});
  }

  // Cache miss.
  if (stm.Succeeded()) {
    return base::ok(nullptr);
  }

  // Error handling.
  const int error_code = db_->GetErrorCode();
  TRACE_EVENT_INSTANT1("persistent_cache", "find_failed",
                       TRACE_EVENT_SCOPE_THREAD, "error_code", error_code);
  return base::unexpected(TranslateError(error_code));
}

base::expected<void, TransactionError> SqliteBackendImpl::Insert(
    std::string_view key,
    base::span<const uint8_t> content,
    EntryMetadata metadata) {
  base::AutoLock lock(lock_, base::subtle::LockTracking::kEnabled);

  CHECK(initialized_);
  CHECK_GT(key.length(), 0ull);
  TRACE_EVENT0("persistent_cache", "insert");

  CHECK_EQ(metadata.write_timestamp, 0)
      << "Write timestamp is generated by SQLite so it should not be specified "
         "manually";

  sql::Statement stm(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "REPLACE INTO entries (key, content, input_signature, write_timestamp) "
      "VALUES (?, ?, ?, strftime(\'%s\', \'now\'))"));

  stm.BindString(0, key);
  stm.BindBlob(1, content);
  stm.BindInt64(2, metadata.input_signature);

  DCHECK(stm.is_valid());
  if (!stm.Run()) {
    const int error_code = db_->GetErrorCode();
    TRACE_EVENT_INSTANT1("persistent_cache", "insert_failed",
                         TRACE_EVENT_SCOPE_THREAD, "error_code", error_code);
    return base::unexpected(TranslateError(error_code));
  }

  return base::ok();
}

TransactionError SqliteBackendImpl::TranslateError(int error_code) {
  switch (error_code) {
    case SQLITE_BUSY:
    case SQLITE_NOMEM:
      return TransactionError::kTransient;
    case SQLITE_CANTOPEN:
    case SQLITE_IOERR_LOCK:  // Lock abandonment.
      return TransactionError::kConnectionError;
    case SQLITE_ERROR:
    case SQLITE_CORRUPT:
    case SQLITE_FULL:
    case SQLITE_IOERR_FSTAT:
    case SQLITE_IOERR_FSYNC:
    case SQLITE_IOERR_READ:
    case SQLITE_IOERR_WRITE:
      return TransactionError::kPermanent;
  }

  // Remaining errors are treasted as transient.
  // `Sql.Database.Statement.Error.PersistentCache` should be monitored to
  // ensure that there are no surprising permanent errors wrongly handled here
  // as this will mean unusable databases that keep being used.
  return TransactionError::kTransient;
}

BackendType SqliteBackendImpl::GetType() const {
  return BackendType::kSqlite;
}

bool SqliteBackendImpl::IsReadOnly() const {
  return vfs_file_set_.read_only();
}

std::optional<BackendParams> SqliteBackendImpl::ExportReadOnlyParams() {
  return ExportParams(/*read_write=*/false);
}

std::optional<BackendParams> SqliteBackendImpl::ExportReadWriteParams() {
  return ExportParams(/*read_write=*/true);
}

void SqliteBackendImpl::Abandon() {
  // Read only instances do not have the privilege of abandoning an instance.
  CHECK(!IsReadOnly());
  vfs_file_set_.Abandon();
}

std::optional<BackendParams> SqliteBackendImpl::ExportParams(bool read_write) {
  BackendParams result;
  result.type = BackendType::kSqlite;
  std::tie(result.db_file, result.journal_file) =
      vfs_file_set_.DuplicateFiles(read_write);
  if (!result.db_file.IsValid() || !result.journal_file.IsValid()) {
    return std::nullopt;
  }
  result.db_file_is_writable = read_write;
  result.journal_file_is_writable = read_write;
  result.shared_lock = vfs_file_set_.DuplicateLock();
  if (!result.shared_lock.IsValid()) {
    return std::nullopt;
  }
  return result;
}

}  // namespace persistent_cache
