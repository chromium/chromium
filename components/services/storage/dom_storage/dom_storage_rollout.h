// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_ROLLOUT_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_ROLLOUT_H_

#include "base/files/file_path.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/dom_storage_histogram_helper.h"
#include "components/services/storage/dom_storage/features.h"

namespace storage {

enum class StorageType;

// On-disk state of the LevelDB database directory.
enum class LevelDbOnDiskState {
  // No LevelDB directory exists.
  kNone,
  // LevelDB directory exists, no experimental tag file.
  kExists,
  // LevelDB directory exists, with an experimental tag file.
  kExistsWithExpTag,
};

// Checks the on-disk state of the LevelDB database directory. Must be called
// on a thread that allows blocking I/O.
LevelDbOnDiskState CheckOnDiskLevelDbState(
    StorageType storage_type,
    const base::FilePath& storage_partition_dir);

// Returns the metrics type for the given rollout stage and disk state.
DatabaseMetricsType GetMetricsType(DomStorageSqliteRolloutStage stage,
                                   LevelDbOnDiskState leveldb_state);

// Returns the path to the experimental tag file inside the LevelDB directory.
base::FilePath GetLevelDbExperimentalTagPath(
    const base::FilePath& leveldb_path);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_ROLLOUT_H_
