// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_migration_vfs/sqlite_migration_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"

namespace sqlite_migration_vfs {

// LINT.IfChange(SqlMigrationTarget)
std::string_view MigrationTargetToString(MigrationTarget target) {
  switch (target) {
    case MigrationTarget::kTest:
      return "Test";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/sql/histograms.xml:SqlMigrationTarget)

std::string GetStatusHistogramName(MigrationTarget target) {
  return base::StrCat(
      {"Sql.Migration.", MigrationTargetToString(target), ".Status"});
}

std::string GetBlockResultHistogramName(MigrationTarget target) {
  return base::StrCat(
      {"Sql.Migration.", MigrationTargetToString(target), ".BlockResult"});
}

std::string GetBlockTimeHistogramName(MigrationTarget target) {
  return base::StrCat(
      {"Sql.Migration.", MigrationTargetToString(target), ".BlockTime"});
}

std::string GetTotalTimeHistogramName(MigrationTarget target) {
  return base::StrCat(
      {"Sql.Migration.", MigrationTargetToString(target), ".TotalTime"});
}

std::string GetTotalBlocksHistogramName(MigrationTarget target) {
  return base::StrCat(
      {"Sql.Migration.", MigrationTargetToString(target), ".TotalBlocks"});
}

void RecordMigrationStatus(MigrationTarget target, MigrationStatus status) {
  base::UmaHistogramEnumeration(GetStatusHistogramName(target), status);
}

void RecordBlockMigrationResult(MigrationTarget target,
                                BlockMigrationResult result) {
  base::UmaHistogramEnumeration(GetBlockResultHistogramName(target), result);
}

void RecordBlockMigrationTime(MigrationTarget target,
                              base::TimeDelta duration) {
  base::UmaHistogramTimes(GetBlockTimeHistogramName(target), duration);
}

void RecordTotalMigrationTime(MigrationTarget target,
                              base::TimeDelta duration) {
  base::UmaHistogramMediumTimes(GetTotalTimeHistogramName(target), duration);
}

void RecordTotalBlocksIntendedToMigrate(MigrationTarget target,
                                        size_t total_blocks) {
  base::UmaHistogramCounts10000(GetTotalBlocksHistogramName(target),
                                base::saturated_cast<int>(total_blocks));
}

}  // namespace sqlite_migration_vfs
