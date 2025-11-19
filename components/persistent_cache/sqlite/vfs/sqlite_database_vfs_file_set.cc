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
  // It makes no sense to have one file writeable and not the other.
  CHECK_EQ(db_file_->access_rights(), journal_file_->access_rights());
}

SqliteVfsFileSet::SqliteVfsFileSet(SqliteVfsFileSet&& other) = default;
SqliteVfsFileSet& SqliteVfsFileSet::operator=(SqliteVfsFileSet&& other) =
    default;
SqliteVfsFileSet::~SqliteVfsFileSet() = default;

std::array<std::pair<base::FilePath, raw_ptr<SandboxedFile>>, 2>
SqliteVfsFileSet::GetFiles() const {
  return {std::make_pair(GetDbVirtualFilePath(), db_file_.get()),
          std::make_pair(GetJournalVirtualFilePath(), journal_file_.get())};
}

base::FilePath SqliteVfsFileSet::GetDbVirtualFilePath() const {
  constexpr const char kDbFileName[] = "data.db";
  return base::FilePath::FromASCII(
      base::StrCat({virtual_fs_path_, kPathSeperator, kDbFileName}));
}

const base::File& SqliteVfsFileSet::GetDbFile() const {
  return db_file_->GetFile();
}

const base::File& SqliteVfsFileSet::GetJournalFile() const {
  return journal_file_->GetFile();
}

LockState SqliteVfsFileSet::Abandon() {
  return db_file_->Abandon();
}

base::FilePath SqliteVfsFileSet::GetJournalVirtualFilePath() const {
  constexpr const char kJournalFileName[] = "data.db-journal";
  return base::FilePath::FromASCII(
      base::StrCat({virtual_fs_path_, kPathSeperator, kJournalFileName}));
}

}  // namespace persistent_cache
