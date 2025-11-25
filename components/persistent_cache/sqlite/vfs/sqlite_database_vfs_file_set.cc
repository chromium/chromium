// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"

#include <atomic>
#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"
#include "sql/database.h"

namespace {

std::atomic<uint64_t> g_file_set_id_generator(0);

}  // namespace

namespace persistent_cache {

SqliteVfsFileSet::SqliteVfsFileSet(
    std::unique_ptr<SandboxedFile> db_file,
    std::unique_ptr<SandboxedFile> journal_file,
    std::unique_ptr<SandboxedFile> wal_journal_file,
    base::UnsafeSharedMemoryRegion shared_lock)
    : shared_lock_(std::move(shared_lock)),
      db_file_(std::move(db_file)),
      journal_file_(std::move(journal_file)),
      wal_journal_file_(std::move(wal_journal_file)),
      virtual_fs_path_(base::FilePath::FromASCII(
          base::NumberToString(g_file_set_id_generator.fetch_add(1)))),
      read_only_(db_file_->access_rights() ==
                 SandboxedFile::AccessRights::kReadOnly) {
  // It makes no sense to have one file writeable and not the other(s).
  CHECK_EQ(db_file_->access_rights(), journal_file_->access_rights());
  if (wal_journal_file_) {
    CHECK_EQ(db_file_->access_rights(), wal_journal_file_->access_rights());
  }
  // Write-ahead logging requires single connection.
  CHECK(!wal_journal_file_ || !shared_lock_.IsValid());
  // Write-ahead logging requires read-write access.
  CHECK(!wal_journal_file_ ||
        db_file_->access_rights() == SandboxedFile::AccessRights::kReadWrite);
}

SqliteVfsFileSet::SqliteVfsFileSet(SqliteVfsFileSet&& other) = default;
SqliteVfsFileSet& SqliteVfsFileSet::operator=(SqliteVfsFileSet&& other) =
    default;
SqliteVfsFileSet::~SqliteVfsFileSet() = default;

base::FilePath SqliteVfsFileSet::GetDbVirtualFilePath() const {
  static constexpr base::FilePath::StringViewType kDbFileName =
      FILE_PATH_LITERAL("data");

  return virtual_fs_path_.Append(kDbFileName);
}

base::FilePath SqliteVfsFileSet::GetJournalVirtualFilePath() const {
  return sql::Database::JournalPath(GetDbVirtualFilePath());
}

base::FilePath SqliteVfsFileSet::GetWalJournalVirtualFilePath() const {
  return sql::Database::WriteAheadLogPath(GetDbVirtualFilePath());
}

const base::File& SqliteVfsFileSet::GetDbFile() const {
  return db_file_->GetFile();
}

const base::File& SqliteVfsFileSet::GetJournalFile() const {
  return journal_file_->GetFile();
}

const base::File& SqliteVfsFileSet::GetWalJournalFile() const {
  CHECK(wal_journal_mode());
  return wal_journal_file_->GetFile();
}

LockState SqliteVfsFileSet::Abandon() {
  return db_file_->Abandon();
}

}  // namespace persistent_cache
