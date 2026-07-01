// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/dom_storage_rollout.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/files/file_error_or.h"
#include "base/notreached.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"

namespace storage {

LevelDbOnDiskState CheckOnDiskLevelDbState(
    StorageType storage_type,
    const base::FilePath& storage_partition_dir) {
  CHECK(!storage_partition_dir.empty());
  base::FilePath leveldb_path =
      DomStorageDatabase::GetLevelDbPath(storage_type, storage_partition_dir);
  // The storage service runs in a sandbox, so filesystem access must be
  // brokered through a `FilesystemProxy`. Raw `base::` calls are blocked by
  // the sandbox and fail silently, which would misclassify a pre-existing
  // database as new.
  std::unique_ptr<FilesystemProxy> filesystem = CreateFilesystemProxy();
  base::FileErrorOr<std::vector<base::FilePath>> entries =
      filesystem->GetDirectoryEntries(
          leveldb_path,
          FilesystemProxy::DirectoryEntryType::kFilesAndDirectories);
  // A missing directory yields an error and an empty directory yields an
  // empty list. Both mean there is no database on disk.
  if (!entries.has_value() || entries.value().empty()) {
    return LevelDbOnDiskState::kNone;
  }
  if (filesystem->PathExists(GetLevelDbExperimentalTagPath(leveldb_path))) {
    return LevelDbOnDiskState::kExistsWithExpTag;
  }
  return LevelDbOnDiskState::kExists;
}

DatabaseMetricsType GetMetricsType(DomStorageSqliteRolloutStage stage,
                                   LevelDbOnDiskState leveldb_state) {
  switch (stage) {
    case DomStorageSqliteRolloutStage::kUseLevelDbOnly:
    case DomStorageSqliteRolloutStage::kUseSqliteOnly:
      return DatabaseMetricsType::kOnDisk;
    case DomStorageSqliteRolloutStage::kUseLevelDbAsControl:
      return (leveldb_state == LevelDbOnDiskState::kNone ||
              leveldb_state == LevelDbOnDiskState::kExistsWithExpTag)
                 ? DatabaseMetricsType::kOnDiskExperimental
                 : DatabaseMetricsType::kOnDisk;
    case DomStorageSqliteRolloutStage::kUseSqliteForNewDatabases:
      return (leveldb_state == LevelDbOnDiskState::kNone)
                 ? DatabaseMetricsType::kOnDiskExperimental
                 : DatabaseMetricsType::kOnDisk;
  }
  NOTREACHED();
}

base::FilePath GetLevelDbExperimentalTagPath(
    const base::FilePath& leveldb_path) {
  CHECK(!leveldb_path.empty());
  CHECK(leveldb_path.IsAbsolute());
  return leveldb_path.AppendASCII("exp-v1");
}

}  // namespace storage
