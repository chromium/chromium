// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_HISTOGRAM_HELPER_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_HISTOGRAM_HELPER_H_

#include <string_view>
#include <vector>

namespace sql {
class Database;
}  // namespace sql

namespace storage {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(DomStorageDatabaseRecoveryOutcome)
enum class DomStorageDatabaseRecoveryOutcome {
  // Recovered to disk. Only one destroy attempt occurs.
  kRecoveredToDiskDestroySucceeded = 0,
  kRecoveredToDiskDestroyFailed = 1,

  // Recovered to in-memory from on-disk. Two destroy attempts occur (first
  // before disk retry, second before in-memory fallback).
  kRecoveredToInMemoryBothDestroysSucceeded = 2,
  kRecoveredToInMemoryFirstDestroyFailed = 3,
  kRecoveredToInMemorySecondDestroyFailed = 4,
  kRecoveredToInMemoryBothDestroysFailed = 5,

  // Gave up (all open attempts failed). Two destroy attempts occur.
  kGaveUpBothDestroysSucceeded = 6,
  kGaveUpFirstDestroyFailed = 7,
  kGaveUpSecondDestroyFailed = 8,
  kGaveUpBothDestroysFailed = 9,

  // Despite attempting recovery, we continue to see commit errors beyond the
  // error threshold.
  kOngoingErrorsAfterAttemptedRecovery = 10,

  // After a previous recovery, some commit errors occurred but then we had a
  // successful commit.
  kTransientErrorsAfterAttemptedRecovery = 11,

  kMaxValue = kTransientErrorsAfterAttemptedRecovery,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:DomStorageDatabaseRecoveryOutcome)

enum class DomStorageRecoveryReason {
  kOpenFailure,
  kMetadataReadFailure,
  kCommitErrorThresholdExceeded,
};

// Classifies the histogram suffix for DomStorage metrics.
enum class DatabaseMetricsType {
  kInMemory,            // ".InMemory"
  kOnDisk,              // ".OnDisk"
  kOnDiskExperimental,  // ".OnDiskExperimental"
};

// Tracks the state of a single database recovery cycle, including what
// triggered it and the outcome of each Destroy() attempt.
struct DomStorageRecoveryState {
  DomStorageRecoveryState(DomStorageRecoveryReason reason,
                          DatabaseMetricsType metrics_type);
  ~DomStorageRecoveryState();
  DomStorageRecoveryState(DomStorageRecoveryState&&);
  DomStorageRecoveryState& operator=(DomStorageRecoveryState&&);

  DomStorageRecoveryReason reason;

  // The metrics type of the database when recovery started. Captured at the
  // start because a recovery cycle spans multiple destroys and reopens that may
  // change the database backend (experimentation) or lifetime (recovered to
  // in-memory DB).
  DatabaseMetricsType metrics_type;

  // Result of each Destroy() call in order. true = succeeded,
  // false = failed. Size indicates how many destroy attempts occurred
  // (0, 1, or 2).
  std::vector<bool> destroy_results;

  void AddDestroyResult(bool succeeded);
};

// Logs recovery outcome histograms:
// - On-disk: `Storage.{Local,Session}Storage.Recovery.{Reason}` (enum), with a
//   `.OnDiskExperimental` suffix for an experimental on-disk database.
// - In-memory: `Storage.{Local,Session}Storage.Recovery.{Reason}.InMemory`
//   (bool: true = recovered, false = gave up)
void LogDomStorageRecoveryOutcome(std::string_view storage_type_prefix,
                                  const DomStorageRecoveryState& state,
                                  bool has_database,
                                  bool is_in_memory);

// Records `Storage.{Local,Session}Storage.CommitErrorCountAtReset` (with a
// `.OnDiskExperimental` suffix for an experimental on-disk database) if
// `commit_error_count` > 0.
void RecordCommitErrorCountAtReset(std::string_view storage_type_prefix,
                                   int commit_error_count,
                                   DatabaseMetricsType metrics_type);

void RecordOnDiskSqliteVacuumMetrics(std::string_view storage_type_prefix,
                                     sql::Database& database);

// Returns the histogram suffix string for the given metrics type.
std::string_view GetHistogramSuffix(DatabaseMetricsType type);

// Returns ".OnDiskExperimental" for an experimental on-disk database, and ""
// otherwise.
std::string_view MaybeGetOnDiskExperimentalSuffix(DatabaseMetricsType type);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_HISTOGRAM_HELPER_H_
