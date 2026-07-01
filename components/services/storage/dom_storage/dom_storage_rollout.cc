// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/dom_storage_rollout.h"

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"

namespace storage {

namespace {

base::FilePath GetLevelDbExperimentalTagPath(
    const base::FilePath& leveldb_path) {
  CHECK(!leveldb_path.empty());
  CHECK(leveldb_path.IsAbsolute());
  return leveldb_path.AppendASCII("exp-v1");
}

}  // namespace

LevelDbOnDiskState CheckOnDiskLevelDbState(
    StorageType storage_type,
    const base::FilePath& storage_partition_dir) {
  CHECK(!storage_partition_dir.empty());
  base::FilePath leveldb_path =
      DomStorageDatabase::GetLevelDbPath(storage_type, storage_partition_dir);
  if (base::IsDirectoryEmpty(leveldb_path)) {
    return LevelDbOnDiskState::kNone;
  }
  if (base::PathExists(GetLevelDbExperimentalTagPath(leveldb_path))) {
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

DbStatus WriteLevelDbExperimentalTag(const base::FilePath& database_path) {
  if (!base::WriteFile(GetLevelDbExperimentalTagPath(database_path), "")) {
    return DbStatus::IOError("exp tag write failed");
  }
  return DbStatus::OK();
}

}  // namespace storage
