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
#include "base/memory/ptr_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/persistent_cache/backend_type.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_sandboxed_vfs.h"
#include "components/persistent_cache/transaction_error.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace {

std::string GetFullHistogramName(std::string_view name, bool read_write) {
  return base::StrCat({"PersistentCache.", name, ".SQLite",
                       (read_write ? ".ReadWrite" : ".ReadOnly")});
}

}  // namespace

namespace persistent_cache {

// static
std::optional<SqliteVfsFileSet> SqliteBackendImpl::BindToFileSet(
    PendingBackend pending_backend) {
  // Write-ahead logging requires single connection.
  CHECK(!pending_backend.sqlite_data.wal_file.IsValid() ||
        !pending_backend.sqlite_data.shared_lock.IsValid());
  // Write-ahead logging requires read-write access.
  CHECK(!pending_backend.sqlite_data.wal_file.IsValid() ||
        pending_backend.read_write);

  base::WritableSharedMemoryMapping mapped_shared_lock;
  if (pending_backend.sqlite_data.shared_lock.IsValid()) {
    mapped_shared_lock = pending_backend.sqlite_data.shared_lock.Map();
    if (!mapped_shared_lock.IsValid()) {
      return std::nullopt;  // Failed to map the shared lock.
    }
  }

  const auto access_rights = pending_backend.read_write
                                 ? SandboxedFile::AccessRights::kReadWrite
                                 : SandboxedFile::AccessRights::kReadOnly;

  auto db_file = std::make_unique<SandboxedFile>(
      std::move(pending_backend.sqlite_data.db_file), access_rights,
      std::move(mapped_shared_lock));
  auto journal_file = std::make_unique<SandboxedFile>(
      std::move(pending_backend.sqlite_data.journal_file), access_rights);
  std::unique_ptr<SandboxedFile> wal_file;
  if (pending_backend.sqlite_data.wal_file.IsValid()) {
    wal_file = std::make_unique<SandboxedFile>(
        std::move(pending_backend.sqlite_data.wal_file), access_rights);
  }

  return SqliteVfsFileSet(std::move(db_file), std::move(journal_file),
                          std::move(wal_file),
                          std::move(pending_backend.sqlite_data.shared_lock));
}

// static
std::unique_ptr<Backend> SqliteBackendImpl::Bind(
    PendingBackend pending_backend) {
  const auto access_rights = pending_backend.read_write
                                 ? SandboxedFile::AccessRights::kReadWrite
                                 : SandboxedFile::AccessRights::kReadOnly;

  auto file_set = BindToFileSet(std::move(pending_backend));
  if (!file_set.has_value()) {
    return nullptr;
  }

  auto instance = base::WrapUnique(new SqliteBackendImpl(*std::move(file_set)));

  base::ElapsedTimer timer;
  if (!instance->Initialize()) {
    return nullptr;
  }

  base::UmaHistogramMicrosecondsTimes(
      GetFullHistogramName(
          "BackendInitialize",
          access_rights == SandboxedFile::AccessRights::kReadWrite),
      timer.Elapsed());
  return instance;
}

SqliteBackendImpl::SqliteBackendImpl(SqliteVfsFileSet vfs_file_set)
    : database_path_(vfs_file_set.GetDbVirtualFilePath()),
      vfs_file_set_(std::move(vfs_file_set)),
      unregister_runner_(
          SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
              vfs_file_set_)),
      db_(std::in_place,
          sql::DatabaseOptions()
              .set_read_only(vfs_file_set_.read_only())
              // Set the database's locking_mode to EXCLUSIVE if the file set
              // supports only a single connection to the database.
              .set_exclusive_locking(vfs_file_set_.is_single_connection())
              // Enable write-ahead logging if such a file is provided.
              .set_wal_mode(vfs_file_set_.wal_journal_mode())
              .set_vfs_name_discouraged(
                  SqliteSandboxedVfsDelegate::kSqliteVfsName)
              // Prevent SQLite from trying to use mmap, as SandboxedVfs does
              // not currently support this.
              .set_mmap_enabled(false),
          sql::Database::Tag("PersistentCache")) {}

SqliteBackendImpl::~SqliteBackendImpl() {
  base::AutoLock lock(lock_, base::subtle::LockTracking::kEnabled);
  db_.reset();
}

bool SqliteBackendImpl::Initialize() {
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

  return true;
}

base::expected<std::optional<EntryMetadata>, TransactionError>
SqliteBackendImpl::Find(std::string_view key, BufferProvider buffer_provider) {
  base::AutoLock lock(lock_, base::subtle::LockTracking::kEnabled);
  CHECK_GT(key.length(), 0ull);
  TRACE_EVENT0("persistent_cache", "Find");

  ASSIGN_OR_RETURN(auto metadata, FindImpl(key, buffer_provider),
                   [](int error_code) {
                     TRACE_EVENT_INSTANT1("persistent_cache", "find_failed",
                                          TRACE_EVENT_SCOPE_THREAD,
                                          "error_code", error_code);
                     return TranslateError(error_code);
                   });
  return metadata;
}

base::expected<void, TransactionError> SqliteBackendImpl::Insert(
    std::string_view key,
    base::span<const uint8_t> content,
    EntryMetadata metadata) {
  base::AutoLock lock(lock_, base::subtle::LockTracking::kEnabled);

  CHECK_GT(key.length(), 0ull);
  TRACE_EVENT0("persistent_cache", "insert");

  CHECK_EQ(metadata.write_timestamp, 0)
      << "Write timestamp is generated by SQLite so it should not be specified "
         "manually";

  RETURN_IF_ERROR(InsertImpl(key, content, std::move(metadata)),
                  [](int error_code) {
                    TRACE_EVENT_INSTANT1("persistent_cache", "insert_failed",
                                         TRACE_EVENT_SCOPE_THREAD, "error_code",
                                         error_code);
                    return TranslateError(error_code);
                  });

  return base::ok();
}

base::expected<std::optional<EntryMetadata>, int> SqliteBackendImpl::FindImpl(
    std::string_view key,
    BufferProvider buffer_provider) {
  // Begin an explicit read transaction under which multiple statements will be
  // used to read from the database.
  sql::Transaction transaction(&*db_);
  if (!transaction.Begin()) {
    return base::unexpected(db_->GetErrorCode());
  }

  // Read the rowid and metadata.
  sql::Statement stm = sql::Statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT rowid, input_signature, write_timestamp "
                              "FROM entries WHERE key = ?"));
  DCHECK(stm.is_valid());

  stm.BindString(0, key);

  if (!stm.Step()) {
    if (stm.Succeeded()) {
      // Cache miss. Do not run `buffer_provider`, return no value.
      return std::nullopt;
    }
    // Error stepping.
    return base::unexpected(db_->GetErrorCode());
  }

  // Open a handle to get the size of the content.
  if (auto blob =
          db_->GetStreamingBlob("entries", "content", stm.ColumnInt64(0),
                                /*readonly=*/true);
      blob.has_value()) {
    bool succeeded = true;
    size_t content_size = base::checked_cast<size_t>(blob->GetSize());
    // Get a buffer from the caller.
    if (base::span<uint8_t> content_buffer = buffer_provider(content_size);
        !content_buffer.empty()) {
      CHECK_EQ(content_buffer.size(), content_size);
      // Copy the content from the database directly into the caller's buffer.
      succeeded = blob->Read(/*offset=*/0, content_buffer);
    }
    if (succeeded) {
      return EntryMetadata{.input_signature = stm.ColumnInt64(1),
                           .write_timestamp = stm.ColumnInt64(2)};
    }
  }

  return base::unexpected(db_->GetErrorCode());
}

base::expected<void, int> SqliteBackendImpl::InsertImpl(
    std::string_view key,
    base::span<const uint8_t> content,
    EntryMetadata metadata) {
  // Use a transaction for insertions so that the creation of the row and the
  // writing of the data are a single atomic operation.
  sql::Transaction transaction(&*db_);
  if (!transaction.Begin()) {
    return base::unexpected(db_->GetErrorCode());
  }

  sql::Statement stm(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "REPLACE INTO entries (key, content, input_signature, write_timestamp) "
      "VALUES (?, ?, ?, strftime(\'%s\', \'now\'))"));

  stm.BindString(0, key);
  stm.BindBlobForStreaming(1, content.size());
  stm.BindInt64(2, metadata.input_signature);

  DCHECK(stm.is_valid());
  if (!stm.Run()) {
    return base::unexpected(db_->GetErrorCode());
  }

  const auto row_id = db_->GetLastInsertRowId();
  if (auto blob_handle = db_->GetStreamingBlob("entries", "content", row_id,
                                               /*readonly=*/false);
      !blob_handle.has_value() || !blob_handle->Write(0, content)) {
    return base::unexpected(db_->GetErrorCode());
  }

  if (!transaction.Commit()) {
    return base::unexpected(db_->GetErrorCode());
  }

  return base::ok();
}

// static
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

LockState SqliteBackendImpl::Abandon() {
  // Read only instances do not have the privilege of abandoning an instance.
  CHECK(!IsReadOnly());
  return vfs_file_set_.Abandon();
}

}  // namespace persistent_cache
