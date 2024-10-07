// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/commit_processor.h"

#include <map>
#include <memory>
#include <utility>

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/sync/engine/commit_contribution.h"
#include "components/sync/engine/commit_contributor.h"

namespace syncer {

using TypeToIndexMap = std::map<DataType, size_t>;

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
    size_t max_entries) {
  DCHECK_GT(max_entries, 0u);
  if (phase_ == GatheringPhase::kDone) {
    return Commit::ContributionMap();
  }

  Commit::ContributionMap contributions;

  // NIGORI contributions are always gathered to make sure that no encrypted
  // data gets committed before the corresponding NIGORI commit, which can
  // otherwise lead to data loss if the commit fails partially.
  if (GatherCommitContributionsForType(NIGORI, max_entries, &contributions) >
      0) {
    // Encryptable entities cannot get combined in the same commit with NIGORI.
    // NIGORI commits are rare so to keep it simple and to play it safe, the
    // processor does not combine any other entities with NIGORI.
    return contributions;
  }

  size_t num_entries = 0;
  do {
    num_entries += GatherCommitContributionsForTypes(
        GetUserTypesForCurrentCommitPhase(), max_entries - num_entries,
        &contributions);
    DCHECK_LE(num_entries, max_entries);
    if (num_entries < max_entries) {
      // Move to the next phase because there are no further commit
      // contributions for this phase at this moment (as there's still capacity
      // left). Even if new contributions for this phase appear while this
      // commit is in flight, they will get ignored until the next nudge. This
      // prevents infinite commit cycles.
      phase_ = IncrementGatheringPhase(phase_);
    }
  } while (phase_ != GatheringPhase::kDone && num_entries < max_entries);

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
      NOTREACHED_IN_MIGRATION();
      return GatheringPhase::kDone;
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
      NOTREACHED_IN_MIGRATION();
      return DataTypeSet();
  }
}

size_t CommitProcessor::GatherCommitContributionsForType(
    DataType type,
    size_t max_entries,
    Commit::ContributionMap* contributions) {
  // Use base::debug::Alias() to ensure that crash dumps in reports include
  // DataType.
  base::debug::Alias(&type);

  if (max_entries == 0) {
    return 0;
  }
  auto cm_it = commit_contributor_map_->find(type);
  if (cm_it == commit_contributor_map_->end()) {
    DLOG(ERROR) << "Could not find requested type "
                << DataTypeToDebugString(type) << " in contributor map.";
    return 0;
  }

  std::unique_ptr<CommitContribution> contribution =
      cm_it->second->GetContribution(max_entries);
  if (!contribution) {
    return 0;
  }

  size_t num_entries = contribution->GetNumEntries();
  DCHECK_LE(num_entries, max_entries);
  contributions->emplace(type, std::move(contribution));

  return num_entries;
}

size_t CommitProcessor::GatherCommitContributionsForTypes(
    DataTypeSet types,
    size_t max_entries,
    Commit::ContributionMap* contributions) {
  size_t num_entries = 0;
  for (DataType type : types) {
    num_entries += GatherCommitContributionsForType(
        type, max_entries - num_entries, contributions);
    if (num_entries >= max_entries) {
      DCHECK_EQ(num_entries, max_entries)
          << "Number of commit entries exceeds maximum";
      break;
    }
  }
  return num_entries;
}

}  // namespace syncer
