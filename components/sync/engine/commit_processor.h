// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_COMMIT_PROCESSOR_H_
#define COMPONENTS_SYNC_ENGINE_COMMIT_PROCESSOR_H_

#include <stddef.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/commit.h"
#include "components/sync/engine/data_type_registry.h"

namespace syncer {

class CommitContributor;

// This class manages the set of per-type committer objects.
//
// It owns these types and hides the details of iterating over all of them.
// It is a logic error if the supplied set of types contains a type which was
// not previously registered.
class CommitProcessor {
 public:
  // `commit_types` must contain NIGORI. `commit_contributor_map` must be not
  // null and must outlive this object.
  CommitProcessor(DataTypeSet commit_types,
                  CommitContributorMap* commit_contributor_map);

  CommitProcessor(const CommitProcessor&) = delete;
  CommitProcessor& operator=(const CommitProcessor&) = delete;

  ~CommitProcessor();

  // Gathers a set of contributions to be used to populate a commit message.
  //
  // For each of the `commit_types` in this CommitProcessor's CommitContributor
  // map, gather any entries queued for commit into CommitContributions.  The
  // total number of entries in all the returned CommitContributions shall not
  // exceed `max_entries`, and the total `EntitySpecifics` byte size stops
  // growing once it reaches or exceeds `max_target_specifics_bytes`.
  // Returns no contribution if previous call collected them from all datatypes
  // and no datatype had remaining local changes.
  Commit::ContributionMap GatherCommitContributions(
      size_t max_entries,
      size_t max_target_specifics_bytes);

 private:
  // Gathering is split into phases for 2 reasons: 1) to provide prioritization,
  // and 2) to avoid infinite commit cycles when some data type generates
  // updates at very high speed.
  enum class GatheringPhase { kHighPriority, kRegular, kLowPriority, kDone };

  // Increments `phase` (in the order given above in GatheringPhase).
  static GatheringPhase IncrementGatheringPhase(GatheringPhase phase);

  // Returns user data types that should be gathered for committing in the
  // current phase.
  DataTypeSet GetUserTypesForCurrentCommitPhase() const;

  // Gathers commit contributions for an individual datatype, or returns null if
  // nothing is contributed.
  std::unique_ptr<CommitContribution> GatherCommitContributionsForType(
      DataType type,
      size_t max_entries,
      size_t max_target_specifics_bytes);

  const DataTypeSet commit_types_;

  // A map of 'commit contributors', one for each enabled type.
  const raw_ptr<CommitContributorMap> commit_contributor_map_;
  GatheringPhase phase_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_COMMIT_PROCESSOR_H_
