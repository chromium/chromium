// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"

#include <stdint.h>

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
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/persistent_cache/backend_type.h"
#include "components/persistent_cache/client.h"
#include "components/persistent_cache/metrics_util.h"
#include "components/persistent_cache/sqlite/vfs_util.h"
#include "components/persistent_cache/transaction_error.h"
#include "components/sqlite_vfs/client.h"
#include "components/sqlite_vfs/lock_state.h"
#include "components/sqlite_vfs/pending_file_set.h"
#include "components/sqlite_vfs/sandboxed_file.h"
#include "components/sqlite_vfs/sqlite_sandboxed_vfs.h"
#include "components/sqlite_vfs/vfs_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace persistent_cache {

namespace {

sql::Database::Tag TagFromClient(Client client) {
  switch (client) {
    case Client::kCodeCache:
      return sql::Database::Tag("CodeCache");
    case Client::kShaderCache:
      return sql::Database::Tag("ShaderCache");
    case Client::kTest:
      return sql::Database::Tag("Test");
  }
}

}  // namespace

// static
std::unique_ptr<Backend> SqliteBackendImpl::Bind(PendingBackend pending_backend,
                                                 Client client) {
  const auto access_rights =
      pending_backend.pending_file_set.read_write
          ? sqlite_vfs::SandboxedFile::AccessRights::kReadWrite
          : sqlite_vfs::SandboxedFile::AccessRights::kReadOnly;

  auto file_set = sqlite_vfs::SqliteVfsFileSet::Bind(
      VfsClientFromClient(client), std::move(pending_backend.pending_file_set));
  if (!file_set.has_value()) {
    return nullptr;
  }
  auto instance =
      base::WrapUnique(new SqliteBackendImpl(*std::move(file_set), client));

  base::ElapsedTimer timer;
  if (!instance->Initialize()) {
    return nullptr;
  }
  base::UmaHistogramMicrosecondsTimes(
      GetHistogramName(
          client, "BackendInitialize",
          access_rights == sqlite_vfs::SandboxedFile::AccessRights::kReadWrite),
      timer.Elapsed());
  return instance;
}

SqliteBackendImpl::SqliteBackendImpl(sqlite_vfs::SqliteVfsFileSet vfs_file_set,
                                     Client client)
    : database_path_(vfs_file_set.GetDbVirtualFilePath()),
      vfs_file_set_(std::move(vfs_file_set)),
      unregister_runner_(sqlite_vfs::SqliteSandboxedVfsDelegate::GetInstance()
                             ->RegisterSandboxedFiles(vfs_file_set_)),
      db_(std::in_place,
          sql::DatabaseOptions()
              .set_read_only(vfs_file_set_.read_only())
              // Set the database's locking_mode to EXCLUSIVE if the file set
              // supports only a single connection to the database.
              .set_exclusive_locking(vfs_file_set_.is_single_connection())
              // Enable write-ahead logging if such a file is provided.
              .set_wal_mode(vfs_file_set_.wal_journal_mode())
              .set_vfs_name_discouraged(
                  sqlite_vfs::SqliteSandboxedVfsDelegate::kSqliteVfsName)
              // Prevent SQLite from trying to use mmap, as SandboxedVfs does
              // not currently support this.
              .set_mmap_enabled(false),
          TagFromClient(client)) {}

SqliteBackendImpl::~SqliteBackendImpl() {
  base::AutoLock lock(lock_, base::subtle::LockTracking::kEnabled);
  db_.reset();
}

bool SqliteBackendImpl::Initialize() {
  TRACE_EVENT("persistent_cache", "Initialize");

  // Open  `db_` under `lock_` with lock tracking enabled. This allows this
  // class to be usable from multiple threads even though `sql::Database` is
  // sequence bound.
  base::AutoLock lock(lock_, base::subtle::LockTracking::kEnabled);

  if (!db_->Open(database_path_)) {
    return false;
  }

  // Check the user-version (https://sqlite.org/pragma.html#pragma_user_version)
  // to see if there has been a schema change since the last time this database
  // was modified.
  int detected_user_version;
  if (sql::Statement get_user_version_stm(
          db_->GetUniqueStatement("PRAGMA user_version"));
      get_user_version_stm.is_valid() && get_user_version_stm.Step()) {
    detected_user_version = get_user_version_stm.ColumnInt(0);
  } else {
    return false;
  }

  if (detected_user_version == kCurrentUserVersion) {
    return true;
  }

  // A read only connection cannot do anything to recover from a mismatched
  // user version.
  if (IsReadOnly()) {
    return false;
  }

  // This is either a new database (user-version has never been set) or was last
  // written with an old schema. Recreate the table with the current schema and
  // update the user-version.

  // Begin an explicit transaction so that creating the table and
  // setting the associated user version is done atomically.
  sql::Transaction transaction(&*db_);
  if (!transaction.Begin()) {
    return false;
  }

  if (!db_->Execute("DROP TABLE IF EXISTS entries")) {
    return false;
  }

  // IMPORTANT: Revise the DROP TABLE statement above if more than the one
  // "entries" table is created here.
  if (!db_->Execute("CREATE TABLE entries(key BLOB PRIMARY KEY "
                    "UNIQUE NOT NULL, content BLOB NOT NULL,"
                    " input_signature INTEGER, write_timestamp INTEGER)")) {
    return false;
  }

  if (!db_->Execute(
          base::StrCat({"PRAGMA user_version=",
                        base::NumberToString(kCurrentUserVersion)}))) {
    return false;
  }

  return transaction.Commit();
}

base::expected<std::optional<EntryMetadata>, TransactionError>
SqliteBackendImpl::Find(base::span<const uint8_t> key,
                        BufferProvider buffer_provider) {
  base::AutoLock lock(lock_, base::subtle::LockTracking::kEnabled);
  CHECK_GT(key.size(), 0ull);
  TRACE_EVENT("persistent_cache", "Find");

  ASSIGN_OR_RETURN(auto metadata, FindImpl(key, buffer_provider),
                   [](int error_code) {
                     return TranslateError(error_code);
                   });
  return metadata;
}

base::expected<void, TransactionError> SqliteBackendImpl::Insert(
    base::span<const uint8_t> key,
    base::span<const uint8_t> content,
    EntryMetadata metadata) {
  base::AutoLock lock(lock_, base::subtle::LockTracking::kEnabled);

  CHECK_GT(key.size(), 0ull);
  TRACE_EVENT("persistent_cache", "Insert");

  CHECK_EQ(metadata.write_timestamp, 0)
      << "Write timestamp is generated by SQLite so it should not be specified "
         "manually";

  RETURN_IF_ERROR(InsertImpl(key, content, std::move(metadata)),
                  [](int error_code) {
                    return TranslateError(error_code);
                  });

  return base::ok();
}

base::expected<void, int> SqliteBackendImpl::ExecuteStatementForTesting(
    base::cstring_view statement) {
  base::AutoLock lock(lock_, base::subtle::LockTracking::kEnabled);

  if (!db_->Execute(statement)) {
    return base::unexpected(db_->GetErrorCode());
  }

  return base::ok();
}

base::expected<std::optional<EntryMetadata>, int> SqliteBackendImpl::FindImpl(
    base::span<const uint8_t> key,
    BufferProvider buffer_provider) {
  // Begin an explicit read transaction under which multiple statements will be
  // used to read from the database if the database may have multiple
  // connections. A transaction is not necessary if the database is opened for a
  // single connection, as it is not possible for another connection to modify
  // the database between the statements below.
  std::optional<sql::Transaction> transaction;
  if (!vfs_file_set_.is_single_connection() &&
      !transaction.emplace(&*db_).Begin()) {
    return base::unexpected(db_->GetErrorCode());
  }

  // Read the rowid and metadata.
  sql::Statement stm(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT rowid, input_signature, write_timestamp "
                              "FROM entries WHERE key = ?"));
  if (!stm.is_valid()) {
    return base::unexpected(db_->GetErrorCode());
  }

  stm.BindBlob(0, key);

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
    base::span<const uint8_t> key,
    base::span<const uint8_t> content,
    EntryMetadata metadata) {
  // Performance tests show that unconditional use of a transaction is faster
  // even for single connections where contention with another reader/writer
  // isn't a concern.
  sql::Transaction transaction(&*db_);
  if (!transaction.Begin()) {
    return base::unexpected(db_->GetErrorCode());
  }

  if (sql::Statement stm(db_->GetCachedStatement(
          SQL_FROM_HERE,
          "REPLACE INTO entries (key, content, input_signature, "
          "write_timestamp) "
          "VALUES (?, ?, ?, strftime(\'%s\', \'now\'))"));
      stm.is_valid()) {
    stm.BindBlob(0, key);
    stm.BindBlobForStreaming(1, content.size());
    stm.BindInt64(2, metadata.input_signature);
    if (!stm.Run()) {
      return base::unexpected(db_->GetErrorCode());
    }
  } else {
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
  switch (vfs_file_set_.Abandon()) {
    case sqlite_vfs::LockState::kNotHeld:
      return LockState::kNotHeld;
    case sqlite_vfs::LockState::kReading:
      return LockState::kReading;
    case sqlite_vfs::LockState::kWriting:
      return LockState::kWriting;
  }
}

}  // namespace persistent_cache
