// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_MIGRATION_VFS_SQLITE_MIGRATION_METRICS_H_
#define COMPONENTS_SQLITE_MIGRATION_VFS_SQLITE_MIGRATION_METRICS_H_

#include <stddef.h>

#include <string>
#include <string_view>

#include "base/time/time.h"

namespace sqlite_migration_vfs {

// Identifies the target database undergoing SQLite migration.
//
// LINT.IfChange(SqlMigrationTarget)
enum class MigrationTarget {
  kTest = 0,
  kMaxValue = kTest,
};
// LINT.ThenChange(//components/sqlite_migration_vfs/sqlite_migration_metrics.cc:SqlMigrationTarget)

// Result of migrating an individual 64-page database block.
//
// LINT.IfChange(BlockMigrationResult)
enum class BlockMigrationResult {
  kSuccess = 0,
  kReadError = 1,
  kWriteError = 2,
  kEncryptionError = 3,
  kMaxValue = kEncryptionError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/sql/enums.xml:SqlMigrationBlockResult)

// Lifecycle status of SQLite database migration.
//
// LINT.IfChange(MigrationStatus)
enum class MigrationStatus {
  kStarted = 0,
  kCompleted = 1,
  kMaxValue = kCompleted,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/sql/enums.xml:SqlMigrationStatus)

std::string_view MigrationTargetToString(MigrationTarget target);

std::string GetStatusHistogramName(MigrationTarget target);
std::string GetBlockResultHistogramName(MigrationTarget target);
std::string GetBlockTimeHistogramName(MigrationTarget target);
std::string GetTotalTimeHistogramName(MigrationTarget target);
std::string GetTotalBlocksHistogramName(MigrationTarget target);

void RecordMigrationStatus(MigrationTarget target, MigrationStatus status);
void RecordBlockMigrationResult(MigrationTarget target,
                                BlockMigrationResult result);
void RecordBlockMigrationTime(MigrationTarget target, base::TimeDelta duration);
void RecordTotalMigrationTime(MigrationTarget target, base::TimeDelta duration);
void RecordTotalBlocksIntendedToMigrate(MigrationTarget target,
                                        size_t total_blocks);

}  // namespace sqlite_migration_vfs

#endif  // COMPONENTS_SQLITE_MIGRATION_VFS_SQLITE_MIGRATION_METRICS_H_
