// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/dom_storage_histogram_helper.h"

#include "base/byte_size.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace storage {

using Outcome = DomStorageDatabaseRecoveryOutcome;

namespace {

// Exclusive max for the CommitErrorCountAtReset histogram. This gives exact
// buckets for [0, kCommitErrorThreshold + 1]. The `commit_error_count_` reaches
// kCommitErrorThreshold + 1 before triggering database recovery. So setting
// this max value gives us exact buckets for the expected range. Additionally,
// after an attempted recovery if errors continue, the counter can keep
// increasing past the expected range. Values above kCommitErrorThreshold + 1
// are captured in the kCommitErrorThreshold + 2 overflow bucket.
constexpr int kCommitErrorCountHistogramMax = kCommitErrorThreshold + 2;

uint32_t GetDatabaseProperty(sql::Database& database,
                             base::cstring_view pragma) {
  sql::Statement statement(database.GetReadonlyStatement(pragma));
  if (!statement.Step()) {
    return 0;
  }
  return base::checked_cast<uint32_t>(statement.ColumnInt(0));
}

// Maps the destroy results from a recovery cycle that started on-disk to the
// appropriate histogram outcome enum value based on the terminal state.
Outcome GetOnDiskDBRecoveryOutcome(const DomStorageRecoveryState& state,
                                   bool has_database,
                                   bool is_in_memory) {
  // On-disk recovery paths must call Destroy() at least once.
  CHECK(!state.destroy_results.empty());
  const bool first_ok = state.destroy_results[0];

  // Recovered to on-disk.
  if (has_database && !is_in_memory) {
    return first_ok ? Outcome::kRecoveredToDiskDestroySucceeded
                    : Outcome::kRecoveredToDiskDestroyFailed;
  }

  CHECK_EQ(state.destroy_results.size(), 2u);
  const bool second_ok = state.destroy_results[1];

  // Recovered to in-memory.
  if (has_database) {
    if (first_ok && second_ok) {
      return Outcome::kRecoveredToInMemoryBothDestroysSucceeded;
    }
    if (!first_ok && !second_ok) {
      return Outcome::kRecoveredToInMemoryBothDestroysFailed;
    }
    if (!first_ok) {
      return Outcome::kRecoveredToInMemoryFirstDestroyFailed;
    }
    return Outcome::kRecoveredToInMemorySecondDestroyFailed;
  }

  // Gave up.
  if (first_ok && second_ok) {
    return Outcome::kGaveUpBothDestroysSucceeded;
  }
  if (!first_ok && !second_ok) {
    return Outcome::kGaveUpBothDestroysFailed;
  }
  if (!first_ok) {
    return Outcome::kGaveUpFirstDestroyFailed;
  }
  return Outcome::kGaveUpSecondDestroyFailed;
}

// Returns the histogram suffix string for the given recovery reason.
const char* GetReasonSuffix(DomStorageRecoveryReason reason) {
  switch (reason) {
    case DomStorageRecoveryReason::kOpenFailure:
      return "OpenFailure";
    case DomStorageRecoveryReason::kMetadataReadFailure:
      return "MetadataReadFailure";
    case DomStorageRecoveryReason::kCommitErrorThresholdExceeded:
      return "CommitErrorThresholdExceeded";
  }
}

}  // namespace

DomStorageRecoveryState::DomStorageRecoveryState(
    DomStorageRecoveryReason reason,
    DatabaseMetricsType metrics_type)
    : reason(reason), metrics_type(metrics_type) {}

DomStorageRecoveryState::~DomStorageRecoveryState() = default;

DomStorageRecoveryState::DomStorageRecoveryState(DomStorageRecoveryState&&) =
    default;

DomStorageRecoveryState& DomStorageRecoveryState::operator=(
    DomStorageRecoveryState&&) = default;

void DomStorageRecoveryState::AddDestroyResult(bool succeeded) {
  // The recovery flow calls Destroy() at most twice per cycle: once before
  // the disk retry and once before the in-memory fallback.
  CHECK_LT(destroy_results.size(), 2u);
  destroy_results.push_back(succeeded);
}

void LogDomStorageRecoveryOutcome(std::string_view storage_type_prefix,
                                  const DomStorageRecoveryState& state,
                                  bool has_database,
                                  bool is_in_memory) {
  const std::string histogram_name =
      base::StrCat({"Storage.", storage_type_prefix, ".Recovery.",
                    GetReasonSuffix(state.reason)});

  if (state.metrics_type == DatabaseMetricsType::kInMemory) {
    // No Destroy() calls should occur if DB started in-memory.
    CHECK(state.destroy_results.empty());
    base::UmaHistogramBoolean(base::StrCat({histogram_name, ".InMemory"}),
                              has_database);
    return;
  }

  // On-disk recovery: log full outcome enum, suffixed for the experiment arm.
  base::UmaHistogramEnumeration(
      base::StrCat({histogram_name,
                    MaybeGetOnDiskExperimentalSuffix(state.metrics_type)}),
      GetOnDiskDBRecoveryOutcome(state, has_database, is_in_memory));
}

void RecordCommitErrorCountAtReset(std::string_view storage_type_prefix,
                                   int commit_error_count,
                                   DatabaseMetricsType metrics_type) {
  if (commit_error_count > 0) {
    base::UmaHistogramExactLinear(
        base::StrCat({"Storage.", storage_type_prefix,
                      ".CommitErrorCountAtReset",
                      MaybeGetOnDiskExperimentalSuffix(metrics_type)}),
        commit_error_count, kCommitErrorCountHistogramMax);
  }
}

void RecordOnDiskSqliteVacuumMetrics(std::string_view storage_type_prefix,
                                     sql::Database& database) {
  const std::string histogram_prefix =
      base::StrCat({"Storage.", storage_type_prefix, ".Sqlite"});

  uint32_t freelist_count =
      GetDatabaseProperty(database, "PRAGMA freelist_count");
  uint32_t page_size = GetDatabaseProperty(database, "PRAGMA page_size");
  uint32_t page_count = GetDatabaseProperty(database, "PRAGMA page_count");

  base::UmaHistogramMemoryKB(
      base::StrCat({histogram_prefix, ".FreelistBytes"}),
      base::ByteSize(uint64_t{freelist_count} * page_size));
  base::UmaHistogramPercentage(
      base::StrCat({histogram_prefix, ".FreelistPercentage"}),
      base::ClampDiv(freelist_count * 100, page_count));
}

std::string_view GetHistogramSuffix(DatabaseMetricsType type) {
  switch (type) {
    case DatabaseMetricsType::kInMemory:
      return ".InMemory";
    case DatabaseMetricsType::kOnDisk:
      return ".OnDisk";
    case DatabaseMetricsType::kOnDiskExperimental:
      return ".OnDiskExperimental";
  }
}

std::string_view MaybeGetOnDiskExperimentalSuffix(DatabaseMetricsType type) {
  return type == DatabaseMetricsType::kOnDiskExperimental
             ? ".OnDiskExperimental"
             : "";
}

}  // namespace storage
