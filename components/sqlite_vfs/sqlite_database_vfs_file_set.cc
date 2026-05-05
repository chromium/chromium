// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/sqlite_database_vfs_file_set.h"

#include <atomic>
#include <memory>
#include <optional>
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
#include "components/sqlite_vfs/file_type.h"
#include "components/sqlite_vfs/pending_file_set.h"
#include "components/sqlite_vfs/sandboxed_file.h"
#include "components/sqlite_vfs/shared_locks.h"
#include "sql/database.h"

namespace {

std::atomic<uint64_t> g_file_set_id_generator(0);

// The base name of the virtual database files served by a file set.
constexpr base::FilePath::StringViewType kDbFileName =
    FILE_PATH_LITERAL("data");

}  // namespace

namespace sqlite_vfs {

// static
std::optional<SqliteVfsFileSet> SqliteVfsFileSet::Bind(
    Client client,
    PendingFileSet pending_file_set) {
  // A WAL-index is required for only multi-connection WAL-mode.
  CHECK_EQ(pending_file_set.wal_index_file.IsValid(),
           pending_file_set.shared_lock.IsValid() &&
               pending_file_set.wal_file.IsValid());

  std::optional<SharedLocks> shared_locks;
  if (pending_file_set.shared_lock.IsValid()) {
    shared_locks =
        SharedLocks::Create(pending_file_set.shared_lock,
                            /*wal_mode=*/pending_file_set.wal_file.IsValid());
    if (!shared_locks) {
      return std::nullopt;  // Failed to map the shared lock.
    }
  }

  const auto access_rights = pending_file_set.read_write
                                 ? SandboxedFile::AccessRights::kReadWrite
                                 : SandboxedFile::AccessRights::kReadOnly;

  auto db_file = std::make_unique<SandboxedFile>(
      client, FileType::kMainDb, std::move(pending_file_set.db_file),
      access_rights, std::move(shared_locks),
      std::move(pending_file_set.wal_index_file));
  auto journal_file = std::make_unique<SandboxedFile>(
      client, FileType::kMainJournal, std::move(pending_file_set.journal_file),
      access_rights);
  std::unique_ptr<SandboxedFile> wal_file;
  if (pending_file_set.wal_file.IsValid()) {
    wal_file = std::make_unique<SandboxedFile>(
        client, FileType::kWal, std::move(pending_file_set.wal_file),
        access_rights);
  }
  return SqliteVfsFileSet(
      std::move(db_file), std::move(journal_file), std::move(wal_file),
#if !BUILDFLAG(IS_WIN)
      std::move(pending_file_set.wal_index_file_read_only),
#endif
      std::move(pending_file_set.shared_lock), pending_file_set.wal_mode);
}

SqliteVfsFileSet::SqliteVfsFileSet(
    std::unique_ptr<SandboxedFile> db_file,
    std::unique_ptr<SandboxedFile> journal_file,
    std::unique_ptr<SandboxedFile> wal_journal_file,
#if !BUILDFLAG(IS_WIN)
    base::File wal_index_file_read_only,
#endif
    base::UnsafeSharedMemoryRegion shared_lock,
    bool wal_mode)
    : shared_lock_(std::move(shared_lock)),
      db_file_(std::move(db_file)),
      journal_file_(std::move(journal_file)),
      wal_journal_file_(std::move(wal_journal_file)),
#if !BUILDFLAG(IS_WIN)
      wal_index_file_read_only_(std::move(wal_index_file_read_only)),
#endif
      virtual_fs_path_(base::FilePath::FromASCII(
          base::NumberToString(g_file_set_id_generator.fetch_add(1)))),
      read_only_(db_file_->access_rights() ==
                 SandboxedFile::AccessRights::kReadOnly),
      wal_mode_(wal_mode) {
  // WAL-mode requires a WAL file (but one might be provided when false to
  // migrate from WAL-mode to a rollback journal).
  CHECK(!wal_mode_ || wal_journal_file_);
  // It makes no sense to have one file writeable and not the other(s).
  CHECK_EQ(db_file_->access_rights(), journal_file_->access_rights());
  if (wal_journal_file_) {
    CHECK_EQ(db_file_->access_rights(), wal_journal_file_->access_rights());
  }
#if !BUILDFLAG(IS_WIN)
  // Only shareable read-write sets with a WAL file have a read-only handle to
  // the WAL-index.
  CHECK_EQ(shared_lock_.IsValid() && !read_only_ && wal_journal_file_,
           wal_index_file_read_only_.IsValid());
#endif
}

SqliteVfsFileSet::SqliteVfsFileSet(SqliteVfsFileSet&& other) = default;
SqliteVfsFileSet& SqliteVfsFileSet::operator=(SqliteVfsFileSet&& other) =
    default;
SqliteVfsFileSet::~SqliteVfsFileSet() = default;

base::FilePath SqliteVfsFileSet::GetDbVirtualFilePath() const {
  return virtual_fs_path_.Append(kDbFileName);
}

base::FilePath SqliteVfsFileSet::GetJournalVirtualFilePath() const {
  return sql::Database::JournalPath(GetDbVirtualFilePath());
}

base::FilePath SqliteVfsFileSet::GetWalJournalVirtualFilePath() const {
  return sql::Database::WriteAheadLogPath(GetDbVirtualFilePath());
}

// static
std::string_view SqliteVfsFileSet::GetVirtualFileHistogramVariant(
    const base::FilePath& virtual_file_path) {
  auto base_name = virtual_file_path.BaseName();
  if (base_name.value() == kDbFileName) {
    return "DbFile";
  }
  auto db_path = base::FilePath(kDbFileName);
  if (base_name == sql::Database::JournalPath(db_path)) {
    return "JournalFile";
  }
  if (base_name == sql::Database::WriteAheadLogPath(db_path)) {
    return "WalJournalFile";
  }
  NOTREACHED();
}

const base::File& SqliteVfsFileSet::GetDbFile() const {
  return db_file_->GetFile();
}

const base::File& SqliteVfsFileSet::GetJournalFile() const {
  return journal_file_->GetFile();
}

const base::File& SqliteVfsFileSet::GetWalJournalFile() const {
  CHECK(has_wal_file());
  return wal_journal_file_->GetFile();
}

LockState SqliteVfsFileSet::Abandon() {
  return db_file_->Abandon();
}

}  // namespace sqlite_vfs
