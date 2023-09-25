// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_COMMIT_CONTRIBUTION_H_
#define COMPONENTS_SYNC_ENGINE_COMMIT_CONTRIBUTION_H_

#include <stddef.h>

#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/cycle/status_controller.h"
#include "components/sync/engine/syncer_error.h"

namespace sync_pb {
class ClientToServerMessage;
class ClientToServerResponse;
}  // namespace sync_pb

namespace syncer {

class StatusController;

// This class represents a set of items belonging to a particular data type that
// have been selected from a CommitContributor and prepared for commit.
//
// This class handles the bookkeeping related to the commit of these items.
class CommitContribution {
 public:
  CommitContribution() = default;
  virtual ~CommitContribution() = default;

  // Serialize this contribution's entries to the given commit request |msg|.
  //
  // This function is not const.  It may update some state in this contribution
  // that will be used when processing the associated commit response.  This
  // function should not be called more than once.
  virtual void AddToCommitMessage(sync_pb::ClientToServerMessage* msg) = 0;

  // Updates this contribution's contents in accordance with the provided
  // |response|.
  //
  // It is not valid to call this function unless AddToCommitMessage() was
  // called earlier.  This function should not be called more than once.
  virtual SyncerError ProcessCommitResponse(
      const sync_pb::ClientToServerResponse& response,
      StatusController* status) = 0;

  // This method is called when there is an error during commit and there is no
  // proper response to process (i.e. unparsable or unexpected server response,
  // network error). This method may be called only if ProcessCommitResponse
  // was never called.
  virtual void ProcessCommitFailure(SyncCommitError commit_error) = 0;

  // Returns the number of entries included in this contribution.
  virtual size_t GetNumEntries() const = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_COMMIT_CONTRIBUTION_H_
