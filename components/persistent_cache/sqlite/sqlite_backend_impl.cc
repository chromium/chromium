// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "components/persistent_cache/sqlite/sqlite_entry_impl.h"
#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_sandboxed_vfs.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace {

constexpr const char kSqliteHistogramTag[] = "PersistentCache";

}  // namespace

namespace persistent_cache {

// static
SqliteVfsFileSet SqliteBackendImpl::GetVfsFileSetFromParams(
    BackendParams backend_params) {
  CHECK_EQ(backend_params.type, BackendType::kSqlite);

  using AccessRights = SandboxedFile::AccessRights;
  SandboxedFile db_file = SandboxedFile(std::move(backend_params.db_file),
                                        backend_params.db_file_is_writable
                                            ? AccessRights::kReadWrite
                                            : AccessRights::kReadOnly);
  SandboxedFile journal_file = SandboxedFile(
      std::move(backend_params.journal_file),
      backend_params.journal_file_is_writable ? AccessRights::kReadWrite
                                              : AccessRights::kReadOnly);

  return SqliteVfsFileSet(std::move(db_file), std::move(journal_file));
}

SqliteBackendImpl::SqliteBackendImpl(BackendParams backend_params)
    : SqliteBackendImpl(GetVfsFileSetFromParams(std::move(backend_params))) {}

SqliteBackendImpl::SqliteBackendImpl(SqliteVfsFileSet vfs_file_set)
    : database_path_(vfs_file_set.GetDbVirtualFilePath()),
      db_(sql::DatabaseOptions()
              .set_vfs_name_discouraged(
                  SqliteSandboxedVfsDelegate::kSqliteVfsName)
              // Prevent SQLite from trying to use mmap, as SandboxedVfs does
              // not currently support this.
              .set_mmap_enabled(false),
          kSqliteHistogramTag),
      unregister_runner_(
          SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
              std::move(vfs_file_set))) {}

SqliteBackendImpl::~SqliteBackendImpl() = default;

bool SqliteBackendImpl::Initialize() {
  CHECK(!initialized_);
  TRACE_EVENT0("persistent_cache", "initialize");

  if (!db_.Open(database_path_)) {
    TRACE_EVENT_INSTANT1("persistent_cache", "open_failed",
                         TRACE_EVENT_SCOPE_THREAD, "error_code",
                         db_.GetErrorCode());
    return false;
  }

  if (!db_.Execute(
          "CREATE TABLE IF NOT EXISTS entries(key TEXT PRIMARY KEY UNIQUE NOT "
          "NULL, content BLOB NOT NULL)")) {
    TRACE_EVENT_INSTANT1("persistent_cache", "create_failed",
                         TRACE_EVENT_SCOPE_THREAD, "error_code",
                         db_.GetErrorCode());
    return false;
  }

  initialized_ = true;
  return true;
}

std::unique_ptr<Entry> SqliteBackendImpl::Find(std::string_view key) {
  CHECK(initialized_);
  CHECK_GT(key.length(), 0ull);
  TRACE_EVENT0("persistent_cache", "Find");

  sql::Statement stm = sql::Statement(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT content FROM entries WHERE key = ?"));
  stm.BindString(0, key);

  DCHECK(stm.is_valid());
  if (!stm.Step()) {
    const int error_code = db_.GetErrorCode();
    // If the last error code is SQLITE_DONE then `Step()` failed because the
    // row was not found which is not a reportable error.
    if (error_code != SQLITE_DONE) {
      TRACE_EVENT_INSTANT1("persistent_cache", "find_failed",
                           TRACE_EVENT_SCOPE_THREAD, "error_code", error_code);
    }
    return nullptr;
  }

  return SqliteEntryImpl::MakeUnique(Passkey(), stm.ColumnString(0));
}

void SqliteBackendImpl::Insert(std::string_view key,
                               base::span<const uint8_t> content) {
  CHECK(initialized_);
  CHECK_GT(key.length(), 0ull);
  TRACE_EVENT0("persistent_cache", "insert");

  sql::Statement stm(db_.GetCachedStatement(
      SQL_FROM_HERE, "REPLACE INTO entries (key, content) VALUES (?, ?)"));

  stm.BindString(0, key);
  stm.BindString(1, base::as_string_view(content));

  DCHECK(stm.is_valid());
  if (!stm.Run()) {
    TRACE_EVENT_INSTANT1("persistent_cache", "insert_failed",
                         TRACE_EVENT_SCOPE_THREAD, "error_code",
                         db_.GetErrorCode());
  }
}

}  // namespace persistent_cache
