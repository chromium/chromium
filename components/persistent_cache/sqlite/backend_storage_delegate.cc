// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/backend_storage_delegate.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/types/expected_macros.h"
#include "components/persistent_cache/sqlite/constants.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"

namespace persistent_cache::sqlite {

std::unique_ptr<Backend> BackendStorageDelegate::MakeBackend(
    const base::FilePath& directory,
    const base::FilePath& base_name) {
  ASSIGN_OR_RETURN(
      auto file_set,
      SqliteVfsFileSet::Create(
          directory.Append(base_name).AddExtension(kDbFileExtension),
          directory.Append(base_name).AddExtension(kJournalFileExtension)),
      []() -> std::unique_ptr<Backend> { return nullptr; });

  return std::make_unique<SqliteBackendImpl>(std::move(file_set));
}

base::FilePath BackendStorageDelegate::GetBaseName(const base::FilePath& file) {
  return file.MatchesFinalExtension(kDbFileExtension)
             ? file.BaseName().RemoveFinalExtension()
             : base::FilePath();
}

int64_t BackendStorageDelegate::DeleteFiles(const base::FilePath& directory,
                                            const base::FilePath& base_name) {
  auto file_path = directory.Append(base_name).AddExtension(kDbFileExtension);
  int64_t bytes_recovered = base::GetFileSize(file_path).value_or(0);
  bool delete_success = base::DeleteFile(file_path);
  base::UmaHistogramBoolean("PersistentCache.ParamsManager.DbFile.DeleteSucess",
                            delete_success);
  if (!delete_success) {
    return 0;
  }

  file_path = directory.Append(base_name).AddExtension(kJournalFileExtension);
  auto file_size = base::GetFileSize(file_path).value_or(0);
  delete_success = base::DeleteFile(file_path);
  base::UmaHistogramBoolean(
      "PersistentCache.ParamsManager.JournalFile.DeleteSucess", delete_success);
  if (delete_success) {
    bytes_recovered = base::ClampAdd(bytes_recovered, file_size);
  }

  // TODO (https://crbug.com/377475540): Cleanup when deletion of journal
  // failed.
  return bytes_recovered;
}

}  // namespace persistent_cache::sqlite
