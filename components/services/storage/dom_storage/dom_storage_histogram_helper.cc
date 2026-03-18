// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/dom_storage_histogram_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"

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
    bool started_in_memory)
    : reason(reason), started_in_memory(started_in_memory) {}

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

  if (state.started_in_memory) {
    // No Destroy() calls should occur if DB started in-memory.
    CHECK(state.destroy_results.empty());
    base::UmaHistogramBoolean(base::StrCat({histogram_name, ".InMemory"}),
                              has_database);
    return;
  }

  // On-disk recovery: log full outcome enum.
  base::UmaHistogramEnumeration(
      histogram_name,
      GetOnDiskDBRecoveryOutcome(state, has_database, is_in_memory));
}

void RecordCommitErrorCountAtReset(std::string_view storage_type_prefix,
                                   int commit_error_count) {
  if (commit_error_count > 0) {
    base::UmaHistogramExactLinear(base::StrCat({"Storage.", storage_type_prefix,
                                                ".CommitErrorCountAtReset"}),
                                  commit_error_count,
                                  kCommitErrorCountHistogramMax);
  }
}

}  // namespace storage
