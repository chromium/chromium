// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"

#include <atomic>
#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"

namespace {

std::atomic<uint64_t> g_file_set_id_generator(0);
constexpr const char kPathSeperator[] = "_";

}  // namespace

namespace persistent_cache {

// static
std::optional<SqliteVfsFileSet> SqliteVfsFileSet::Create(
    base::FilePath db_file_path,
    base::FilePath journal_file_path) {
  uint32_t create_flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                          base::File::FLAG_WRITE |
                          base::File::FLAG_WIN_SHARE_DELETE |
                          base::File::FLAG_CAN_DELETE_ON_CLOSE;

  // Make sure handles to these files are safe to pass to untrusted processes.
  create_flags = base::File::AddFlagsForPassingToUntrustedProcess(create_flags);

  base::File db_file(db_file_path, create_flags);
  if (!db_file.IsValid()) {
    return std::nullopt;
  }

  base::File journal_file(journal_file_path, create_flags);
  if (!journal_file.IsValid()) {
    return std::nullopt;
  }

  auto shared_lock = base::UnsafeSharedMemoryRegion::Create(sizeof(LockState));
  if (!shared_lock.IsValid()) {
    return std::nullopt;
  }

  auto mapped_shared_lock = shared_lock.Map();
  if (!mapped_shared_lock.IsValid()) {
    return std::nullopt;
  }

  return SqliteVfsFileSet(
      std::make_unique<SandboxedFile>(std::move(db_file),
                                      std::move(db_file_path),
                                      SandboxedFile::AccessRights::kReadWrite,
                                      std::move(mapped_shared_lock)),
      std::make_unique<SandboxedFile>(std::move(journal_file),
                                      std::move(journal_file_path),
                                      SandboxedFile::AccessRights::kReadWrite),
      std::move(shared_lock));
}

SqliteVfsFileSet::SqliteVfsFileSet(std::unique_ptr<SandboxedFile> db_file,
                                   std::unique_ptr<SandboxedFile> journal_file,
                                   base::UnsafeSharedMemoryRegion shared_lock)
    : shared_lock_(std::move(shared_lock)),
      db_file_(std::move(db_file)),
      journal_file_(std::move(journal_file)),
      virtual_fs_path_(
          base::NumberToString(g_file_set_id_generator.fetch_add(1))),
      read_only_(db_file_->access_rights() ==
                 SandboxedFile::AccessRights::kReadOnly) {
  // It makes no sense to have a file writeable and not the other.
  CHECK_EQ(db_file_->access_rights(), journal_file_->access_rights());
}

SqliteVfsFileSet::SqliteVfsFileSet(SqliteVfsFileSet&& other) = default;
SqliteVfsFileSet& SqliteVfsFileSet::operator=(SqliteVfsFileSet&& other) =
    default;
SqliteVfsFileSet::~SqliteVfsFileSet() = default;

base::FilePath SqliteVfsFileSet::GetDbVirtualFilePath() const {
  constexpr const char kDbFileName[] = "data.db";
  return base::FilePath::FromASCII(
      base::StrCat({virtual_fs_path_, kPathSeperator, kDbFileName}));
}

std::array<base::File, 2> SqliteVfsFileSet::DuplicateFiles(
    bool read_write) const {
  // Can't upgrade from read-only to read-write.
  CHECK(!read_write || !read_only_);
  const auto access_rights = read_write
                                 ? SandboxedFile::AccessRights::kReadWrite
                                 : SandboxedFile::AccessRights::kReadOnly;
  return {db_file_->DuplicateFile(access_rights),
          journal_file_->DuplicateFile(access_rights)};
}

base::UnsafeSharedMemoryRegion SqliteVfsFileSet::DuplicateLock() const {
  return shared_lock_.Duplicate();
}

base::FilePath SqliteVfsFileSet::GetJournalVirtualFilePath() const {
  constexpr const char kJournalFileName[] = "data.db-journal";
  return base::FilePath::FromASCII(
      base::StrCat({virtual_fs_path_, kPathSeperator, kJournalFileName}));
}

std::array<std::pair<base::FilePath, raw_ptr<SandboxedFile>>, 2>
SqliteVfsFileSet::GetFiles() const {
  return {std::make_pair(GetDbVirtualFilePath(), db_file_.get()),
          std::make_pair(GetJournalVirtualFilePath(), journal_file_.get())};
}

}  // namespace persistent_cache
