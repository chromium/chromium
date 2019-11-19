// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/commit_processor.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "components/sync/engine_impl/commit_contribution.h"
#include "components/sync/engine_impl/commit_contributor.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

using TypeToIndexMap = std::map<ModelType, size_t>;

CommitProcessor::CommitProcessor(CommitContributorMap* commit_contributor_map)
    : commit_contributor_map_(commit_contributor_map) {}

CommitProcessor::~CommitProcessor() {}

Commit::ContributionMap CommitProcessor::GatherCommitContributions(
    ModelTypeSet commit_types,
    size_t max_entries,
    bool cookie_jar_mismatch,
    bool cookie_jar_empty) {
  Commit::ContributionMap contributions;
  size_t num_entries = 0;

  // NIGORI should be committed before any other datatype.
  DCHECK(commit_types.Has(NIGORI));

  commit_types.Remove(NIGORI);
  num_entries +=
      GatherCommitContributionsForType(NIGORI, max_entries, cookie_jar_mismatch,
                                       cookie_jar_empty, &contributions);
  if (num_entries != 0) {
    // If the outgoing commit has a NIGORI update, there are some risks if
    // changes from other datatypes are bundled together in the same commit, as
    // long as the datatype is encryptable. Hence, restrict to
    // PriorityUserTypes() which are never encrypted.
    commit_types.RetainAll(PriorityUserTypes());
  }

  for (ModelType type : commit_types) {
    num_entries += GatherCommitContributionsForType(
        type, max_entries - num_entries, cookie_jar_mismatch, cookie_jar_empty,
        &contributions);
    if (num_entries >= max_entries) {
      DCHECK_EQ(num_entries, max_entries)
          << "Number of commit entries exceeds maximum";
      break;
    }
  }

  return contributions;
}

int CommitProcessor::GatherCommitContributionsForType(
    ModelType type,
    size_t max_entries,
    bool cookie_jar_mismatch,
    bool cookie_jar_empty,
    Commit::ContributionMap* contributions) {
  auto cm_it = commit_contributor_map_->find(type);
  if (cm_it == commit_contributor_map_->end()) {
    DLOG(ERROR) << "Could not find requested type " << ModelTypeToString(type)
                << " in contributor map.";
    return 0;
  }

  std::unique_ptr<CommitContribution> contribution =
      cm_it->second->GetContribution(max_entries);
  if (!contribution) {
    return 0;
  }

  int num_entries = contribution->GetNumEntries();
  DCHECK_LE(num_entries, static_cast<int>(max_entries));
  contributions->emplace(type, std::move(contribution));

  if (type == SESSIONS) {
    UMA_HISTOGRAM_BOOLEAN("Sync.CookieJarMatchOnNavigation",
                          !cookie_jar_mismatch);
    if (cookie_jar_mismatch) {
      UMA_HISTOGRAM_BOOLEAN("Sync.CookieJarEmptyOnMismatch", cookie_jar_empty);
    }
  }

  return num_entries;
}

}  // namespace syncer
