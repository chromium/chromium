// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/backend_params_manager.h"

#include <cstdint>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"

namespace {

struct FilePathWithInfo {
  base::FilePath file_path;
  base::File::Info info;
};

const base::FilePath::CharType kDbFile[] = FILE_PATH_LITERAL(".db_file");
const base::FilePath::CharType kJournalFile[] =
    FILE_PATH_LITERAL(".journal_file");

constexpr size_t kLruCacheCapacity = 100;

}  // namespace

namespace persistent_cache {

BackendParamsManager::BackendParamsManager(base::FilePath top_directory)
    : backend_params_map_(kLruCacheCapacity),
      top_directory_(std::move(top_directory)) {
  if (!base::PathExists(top_directory_)) {
    base::CreateDirectory(top_directory_);
  }
}
BackendParamsManager::~BackendParamsManager() = default;

void BackendParamsManager::GetParamsSyncOrCreateAsync(
    BackendType backend_type,
    const std::string& key,
    AccessRights access_rights,
    CompletedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = backend_params_map_.Get(
      BackendParamsKey{.backend_type = backend_type, .key = key});
  if (it != backend_params_map_.end()) {
    std::move(callback).Run(it->second);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&BackendParamsManager::CreateParamsSync, top_directory_,
                     backend_type, key, access_rights),
      base::BindOnce(&BackendParamsManager::SaveParams,
                     weak_factory_.GetWeakPtr(), key, std::move(callback)));
}

BackendParams BackendParamsManager::GetOrCreateParamsSync(
    BackendType backend_type,
    const std::string& key,
    AccessRights access_rights) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = backend_params_map_.Get(
      BackendParamsKey{.backend_type = backend_type, .key = key});
  if (it != backend_params_map_.end()) {
    return it->second.Copy();
  }

  BackendParams new_params =
      CreateParamsSync(top_directory_, backend_type, key, access_rights);
  SaveParams(key, CompletedCallback(), new_params.Copy());

  return new_params;
}

void BackendParamsManager::DeleteAllFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear params cache so they don't hold on to files or prevent their
  // deletion. BackendParam instances that were vended by this class and
  // retained somewhere else can still create problems and need to be handled
  // appropriately.
  backend_params_map_.Clear();

  base::DeletePathRecursively(top_directory_);

  // Recreate the directory since the objective was to delete files only.
  base::CreateDirectory(top_directory_);
}

int64_t BackendParamsManager::BringDownTotalFootprintOfFiles(
    int64_t target_footprint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear params cache so they don't hold on to files or prevent their
  // deletion. BackendParam instances that were vended by this class and
  // retained somewhere else can still create problems and need to be handled
  // appropriately.
  backend_params_map_.Clear();

  int64_t total_footprint = 0;
  std::vector<FilePathWithInfo> filepaths_with_info;
  base::FileEnumerator file_enumerator(top_directory_, /*recursive=*/false,
                                       base::FileEnumerator::FILES);

  file_enumerator.ForEach([&total_footprint, &filepaths_with_info](
                              const base::FilePath& file_path) {
    base::File::Info info;
    base::GetFileInfo(file_path, &info);

    // Only target database files for deletion.
    if (file_path.MatchesFinalExtension(kDbFile)) {
      filepaths_with_info.emplace_back(file_path, info);
    }

    // All files count towards measured footprint.
    total_footprint += info.size;
  });

  // Nothing to do.
  if (total_footprint <= target_footprint) {
    return 0;
  }

  // Order files from least to most recently modified to prioritize deleting
  // older staler files.
  std::sort(filepaths_with_info.begin(), filepaths_with_info.end(),
            [](const FilePathWithInfo& left, const FilePathWithInfo& right) {
              return left.info.last_modified < right.info.last_modified;
            });

  int64_t size_of_necessary_deletes = total_footprint - target_footprint;
  int64_t deleted_size = 0;

  for (const FilePathWithInfo& file_path_with_info : filepaths_with_info) {
    if (size_of_necessary_deletes <= deleted_size) {
      break;
    }

    bool db_file_delete_success =
        base::DeleteFile(file_path_with_info.file_path);
    base::UmaHistogramBoolean(
        "PersistentCache.ParamsManager.DbFile.DeleteSucess",
        db_file_delete_success);

    if (db_file_delete_success) {
      deleted_size += file_path_with_info.info.size;

      base::FilePath journal_file_path =
          file_path_with_info.file_path.ReplaceExtension(kJournalFile);
      base::File::Info journal_file_info;
      base::GetFileInfo(journal_file_path, &journal_file_info);

      // TODO (https://crbug.com/377475540): Cleanup when deletion of journal
      // failed.
      bool journal_file_delete_success = base::DeleteFile(journal_file_path);
      base::UmaHistogramBoolean(
          "PersistentCache.ParamsManager.JournalFile.DeleteSucess",
          journal_file_delete_success);

      if (journal_file_delete_success) {
        deleted_size += journal_file_info.size;
      }
    };
  }

  return deleted_size;
}

// static
BackendParams BackendParamsManager::CreateParamsSync(
    base::FilePath directory,
    BackendType backend_type,
    const std::string& key,
    AccessRights access_rights) {
  BackendParams params;
  params.type = backend_type;

  const bool writes_supported = (access_rights == AccessRights::kReadWrite);
  uint32_t flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ;

  if (writes_supported) {
    flags |= base::File::FLAG_WRITE;
  }

#if BUILDFLAG(IS_WIN)
  // PersistentCache backing files are not executables.
  flags |= base::File::FLAG_WIN_NO_EXECUTE;

  // String conversion to wstring necessary on Windows.
  std::wstring key_part = base::UTF8ToWide(key);
  base::FilePath db_file_name =
      base::FilePath(base::StrCat({key_part, kDbFile}));
  base::FilePath journal_file_name =
      base::FilePath(base::StrCat({key_part, kJournalFile}));
#else
  base::FilePath db_file_name = base::FilePath(base::StrCat({key, kDbFile}));
  base::FilePath journal_file_name =
      base::FilePath(base::StrCat({key, kJournalFile}));
#endif

  params.db_file = base::File(directory.Append(db_file_name), flags);
  params.db_file_is_writable = writes_supported;

  params.journal_file = base::File(directory.Append(journal_file_name), flags);
  params.journal_file_is_writable = writes_supported;

  return params;
}

void BackendParamsManager::SaveParams(const std::string& key,
                                      CompletedCallback callback,
                                      BackendParams backend_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (callback) {
    std::move(callback).Run(backend_params);
  }

  // Avoid saving invalid files.
  if (backend_params.db_file.IsValid() &&
      backend_params.journal_file.IsValid()) {
    backend_params_map_.Put(
        BackendParamsKey{.backend_type = backend_params.type, .key = key},
        std::move(backend_params));
  }
}

}  // namespace persistent_cache
