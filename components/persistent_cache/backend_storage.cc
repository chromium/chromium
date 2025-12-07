// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/backend_storage.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/numerics/clamped_math.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/backend_type.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/persistent_cache.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "components/persistent_cache/sqlite/backend_storage_delegate.h"
#endif

namespace persistent_cache {

namespace {

std::unique_ptr<BackendStorage::Delegate> MakeDelegateOfType(
    BackendType backend_type) {
#if BUILDFLAG(IS_FUCHSIA)
  return nullptr;
#else
  switch (backend_type) {
    case BackendType::kSqlite:
      return std::make_unique<sqlite::BackendStorageDelegate>();
  }
#endif
}

// Deletes the contents of `directory` without deleting `directory` itself.
void DeleteDirectoryContents(const base::FilePath& directory) {
  base::FileEnumerator(directory, /*recursive=*/false,
                       base::FileEnumerator::NAMES_ONLY)
      .ForEach([](const base::FilePath& path) {
        base::DeletePathRecursively(path);
      });
}

}  // namespace

BackendStorage::BackendStorage(BackendType backend_type,
                               base::FilePath directory)
    : BackendStorage(MakeDelegateOfType(backend_type), std::move(directory)) {}

BackendStorage::BackendStorage(std::unique_ptr<Delegate> delegate,
                               base::FilePath directory)
    : delegate_(std::move(delegate)), directory_(std::move(directory)) {
  CHECK(!directory_.empty());
  is_valid_ = delegate_ && base::CreateDirectory(directory_);
}

BackendStorage::~BackendStorage() = default;

std::optional<PendingBackend> BackendStorage::MakePendingBackend(
    const base::FilePath& base_name,
    bool single_connection,
    bool journal_mode_wal) {
  return is_valid_
             ? delegate_->MakePendingBackend(
                   directory_, base_name, single_connection, journal_mode_wal)
             : std::nullopt;
}

std::unique_ptr<Backend> BackendStorage::MakeBackend(
    const base::FilePath& base_name,
    bool single_connection,
    bool journal_mode_wal) {
  return is_valid_ ? delegate_->MakeBackend(directory_, base_name,
                                            single_connection, journal_mode_wal)
                   : nullptr;
}

std::optional<PendingBackend> BackendStorage::ShareReadOnlyConnection(
    const base::FilePath& base_name,
    const PersistentCache& cache) {
  return is_valid_ ? delegate_->ShareReadOnlyConnection(directory_, base_name,
                                                        cache.backend())
                   : std::nullopt;
}

std::optional<PendingBackend> BackendStorage::ShareReadWriteConnection(
    const base::FilePath& base_name,
    const PersistentCache& cache) {
  return is_valid_ ? delegate_->ShareReadWriteConnection(directory_, base_name,
                                                         cache.backend())
                   : std::nullopt;
}

void BackendStorage::DeleteAllFiles() {
  if (is_valid_) {
    // All files are opened with FLAG_WIN_SHARE_DELETE, so it is possible to
    // delete them even if any are still open. The parent directory will not
    // be deleted in this case, but that's okay.

    // TODO(https://crbug.com/377475540): On Windows, a file cannot be marked
    // for deletion while it is mapped into a process's address space. If WAL
    // mode is used, we will need to investigate if the wal-index ("-shm") file
    // can always be opened with `FLAG_DELETE_ON_CLOSE` so that it is
    // unconditionally deleted when the DB is closed.
    // https://sqlite.org/walformat.html#shm indicates that this should be safe.
    DeleteDirectoryContents(directory_);
  }
}

void BackendStorage::DeleteFiles(const base::FilePath& base_name) {
  if (is_valid_) {
    delegate_->DeleteFiles(directory_, base_name);
  }
}

BackendStorage::FootprintReductionResult
BackendStorage::BringDownTotalFootprintOfFiles(int64_t target_footprint) {
  if (!is_valid_) {
    return {0, 0};
  }

  // Measure the size of the directory while collecting the basenames and
  // last modified times of each of the backend's files.
  int64_t total_footprint = 0;
  std::vector<std::pair<base::FilePath, base::Time>> base_names;
  base::FileEnumerator(directory_, /*recursive=*/false,
                       base::FileEnumerator::FILES)
      .ForEach([&total_footprint, &base_names,
                &delegate = *delegate_](const base::FilePath& file_path) {
        base::File::Info info;
        // Ignore errors -- info.last_modified will be zero, so this will look
        // like the oldest file; first to be deleted.
        base::GetFileInfo(file_path, &info);

        // Only target files managed by the backend for deletion.
        if (auto base_name = delegate.GetBaseName(file_path);
            !base_name.empty()) {
          base_names.emplace_back(std::move(base_name), info.last_modified);
        }

        // All files count towards measured footprint.
        total_footprint = base::ClampAdd(total_footprint, info.size);
      });

  // Nothing to do.
  if (total_footprint <= target_footprint) {
    return FootprintReductionResult{.current_footprint = total_footprint};
  }

  // Sort the collected base names oldest to newest.
  std::ranges::sort(base_names, std::ranges::less(),
                    &std::pair<base::FilePath, base::Time>::second);

  // Delete files until reaching the desired target.
  const int64_t size_of_necessary_deletes = total_footprint - target_footprint;
  int64_t deleted_size = 0;

  for (const auto& [base_name, last_modified_time] : base_names) {
    deleted_size = base::ClampAdd(
        deleted_size, delegate_->DeleteFiles(directory_, base_name));
    if (deleted_size >= size_of_necessary_deletes) {
      break;
    }
  }

  return FootprintReductionResult{
      .current_footprint = total_footprint - deleted_size,
      .number_of_bytes_deleted = deleted_size};
}

}  // namespace persistent_cache
