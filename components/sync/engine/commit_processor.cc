// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/commit_processor.h"

#include <memory>
#include <utility>

#include "base/containers/map_util.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/sync/engine/commit_contribution.h"
#include "components/sync/engine/commit_contributor.h"

namespace syncer {

CommitProcessor::CommitProcessor(DataTypeSet commit_types,
                                 CommitContributorMap* commit_contributor_map)
    : commit_types_(commit_types),
      commit_contributor_map_(commit_contributor_map),
      phase_(GatheringPhase::kHighPriority) {
  // NIGORI contributions must be collected in every commit cycle.
  DCHECK(commit_types_.Has(NIGORI));
  DCHECK(commit_contributor_map);
}

CommitProcessor::~CommitProcessor() = default;

Commit::ContributionMap CommitProcessor::GatherCommitContributions(
    size_t max_entries,
    size_t max_target_specifics_bytes) {
  if (phase_ == GatheringPhase::kDone) {
    return Commit::ContributionMap();
  }

  Commit::ContributionMap contributions;

  // NIGORI contributions are always gathered to make sure that no encrypted
  // data gets committed before the corresponding NIGORI commit, which can
  // otherwise lead to data loss if the commit fails partially.
  if (std::unique_ptr<CommitContribution> nigori_contribution =
          GatherCommitContributionsForType(NIGORI, max_entries,
                                           max_target_specifics_bytes)) {
    // Encryptable entities cannot get combined in the same commit with NIGORI.
    // NIGORI commits are rare so to keep it simple and to play it safe, the
    // processor does not combine any other entities with NIGORI.
    contributions.emplace(NIGORI, std::move(nigori_contribution));
    return contributions;
  }

  size_t num_entries = 0;
  size_t specifics_bytes = 0;
  while (phase_ != GatheringPhase::kDone) {
    for (DataType type : GetUserTypesForCurrentCommitPhase()) {
      std::unique_ptr<CommitContribution> contribution =
          GatherCommitContributionsForType(
              type, max_entries - num_entries,
              max_target_specifics_bytes - specifics_bytes);
      if (!contribution) {
        continue;
      }
      num_entries += contribution->GetNumEntries();
      specifics_bytes += contribution->GetPostEncryptionSpecificsByteSize();
      const bool has_more_local_changes = contribution->HasMoreLocalChanges();
      contributions.emplace(type, std::move(contribution));
      if (has_more_local_changes || num_entries >= max_entries ||
          specifics_bytes >= max_target_specifics_bytes) {
        // Stop gathering and keep `phase_` unchanged for the next call if:
        // 1) `type` truncated its changes (`has_more_local_changes`), which can
        //    happen even when the post-encryption `specifics_bytes` is below
        //    `max_target_specifics_bytes` (because DataTypeWorker passes an
        //    estimated pre-encryption byte limit to the processor), or
        // 2) `type` exhausted its own changes (`has_more_local_changes` is
        //    false) but filled or overshot the batch budget, so subsequent
        //    types cannot be queried with positive remaining capacity.
        // Note: because progress is tracked per `phase_` rather than per
        // DataType, the next call will re-query all types in `phase_`.
        return contributions;
      }
    }
    // Move to the next phase because there are no further commit
    // contributions for this phase at this moment (as there's still capacity
    // left). Even if new contributions for this phase appear while this
    // commit is in flight, they will get ignored until the next nudge. This
    // prevents infinite commit cycles.
    phase_ = IncrementGatheringPhase(phase_);
  }

  return contributions;
}

// static
CommitProcessor::GatheringPhase CommitProcessor::IncrementGatheringPhase(
    GatheringPhase phase) {
  switch (phase) {
    case GatheringPhase::kHighPriority:
      return GatheringPhase::kRegular;
    case GatheringPhase::kRegular:
      return GatheringPhase::kLowPriority;
    case GatheringPhase::kLowPriority:
      return GatheringPhase::kDone;
    case GatheringPhase::kDone:
      NOTREACHED();
  }
}

DataTypeSet CommitProcessor::GetUserTypesForCurrentCommitPhase() const {
  switch (phase_) {
    case GatheringPhase::kHighPriority:
      return Intersection(commit_types_, HighPriorityUserTypes());
    case GatheringPhase::kRegular:
      return Difference(commit_types_, Union(Union(HighPriorityUserTypes(),
                                                   LowPriorityUserTypes()),
                                             {NIGORI}));
    case GatheringPhase::kLowPriority:
      return Intersection(commit_types_, LowPriorityUserTypes());
    case GatheringPhase::kDone:
      NOTREACHED();
  }
}

std::unique_ptr<CommitContribution>
CommitProcessor::GatherCommitContributionsForType(
    DataType type,
    size_t max_entries,
    size_t max_target_specifics_bytes) {
  // Use base::debug::Alias() to ensure that crash dumps in reports include
  // DataType.
  base::debug::Alias(&type);

  CHECK_GT(max_entries, 0u);
  CHECK_GT(max_target_specifics_bytes, 0u);
  CommitContributor* commit_contributor =
      base::FindPtrOrNull(*commit_contributor_map_, type);
  if (!commit_contributor) {
    DLOG(ERROR) << "Could not find requested type "
                << DataTypeToDebugString(type) << " in contributor map.";
    return nullptr;
  }

  std::unique_ptr<CommitContribution> contribution =
      commit_contributor->GetContribution(max_entries,
                                          max_target_specifics_bytes);
  if (contribution) {
    DCHECK_GT(contribution->GetNumEntries(), 0u);
    DCHECK_LE(contribution->GetNumEntries(), max_entries);
  }
  return contribution;
}

}  // namespace syncer
