// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/update_applicator.h"

#include "base/logging.h"
#include "components/sync/engine_impl/syncer_util.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/syncable_write_transaction.h"

using std::vector;

namespace syncer {

using syncable::ID;

UpdateApplicator::UpdateApplicator(const Cryptographer* cryptographer)
    : cryptographer_(cryptographer),
      updates_applied_(0),
      encryption_conflicts_(0),
      hierarchy_conflicts_(0) {}

UpdateApplicator::~UpdateApplicator() {}

// Attempt to apply all updates, using multiple passes if necessary.
//
// Some updates must be applied in order.  For example, children must be created
// after their parent folder is created.  This function runs an O(n^2) algorithm
// that will keep trying until there is nothing left to apply, or it stops
// making progress, which would indicate that the hierarchy is invalid.
//
// The update applicator also has to deal with simple conflicts, which occur
// when an item is modified on both the server and the local model.  We remember
// their IDs so they can be passed to the conflict resolver after all the other
// applications are complete.
//
// Finally, there are encryption conflicts, which can occur when we don't have
// access to all the Nigori keys.  There's nothing we can do about them here.
void UpdateApplicator::AttemptApplications(
    syncable::WriteTransaction* trans,
    const std::vector<int64_t>& handles) {
  std::vector<int64_t> to_apply = handles;

  DVLOG(1) << "UpdateApplicator running over " << to_apply.size() << " items.";
  while (!to_apply.empty()) {
    std::vector<int64_t> to_reapply;

    for (auto i = to_apply.begin(); i != to_apply.end(); ++i) {
      syncable::MutableEntry entry(trans, syncable::GET_BY_HANDLE, *i);
      UpdateAttemptResponse result =
          AttemptToUpdateEntry(trans, &entry, cryptographer_);

      switch (result) {
        case SUCCESS:
          updates_applied_++;
          break;
        case CONFLICT_SIMPLE:
          simple_conflict_ids_.insert(entry.GetId());
          break;
        case CONFLICT_ENCRYPTION:
          encryption_conflicts_++;
          break;
        case CONFLICT_HIERARCHY:
          // The decision to classify these as hierarchy conflcits is tentative.
          // If we make any progress this round, we'll clear the hierarchy
          // conflict count and attempt to reapply these updates.
          to_reapply.push_back(*i);
          break;
        default:
          NOTREACHED();
          break;
      }
    }

    if (to_reapply.size() == to_apply.size()) {
      // We made no progress.  Must be stubborn hierarchy conflicts.
      hierarchy_conflicts_ = to_apply.size();
      break;
    }

    // We made some progress, so prepare for what might be another iteration.
    // If everything went well, to_reapply will be empty and we'll break out on
    // the while condition.
    to_apply.swap(to_reapply);
    to_reapply.clear();
  }
}

}  // namespace syncer
