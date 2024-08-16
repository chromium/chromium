// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_COMMIT_CONTRIBUTION_IMPL_H_
#define COMPONENTS_SYNC_ENGINE_COMMIT_CONTRIBUTION_IMPL_H_

#include <stddef.h>

#include "base/functional/callback.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/commit_contribution.h"

namespace sync_pb {
class ClientToServerMessage;
class SyncEntity;
class ClientToServerResponse;
}  // namespace sync_pb

namespace syncer {

// A sync type's contribution to an outgoing commit message.
//
// Helps build a commit message and process its response.  It collaborates
// closely with the DataTypeWorker.
class CommitContributionImpl : public CommitContribution {
 public:
  // Only one of |on_commit_response_callback| and
  // |on_full_commit_failure_callback| will be called.
  // TODO(rushans): there is still possible rare case when both of these
  // callbacks are never called, i.e. if get updates from the server fails.
  CommitContributionImpl(
      DataType type,
      const sync_pb::DataTypeContext& context,
      CommitRequestDataList commit_requests,
      base::OnceCallback<void(const CommitResponseDataList&,
                              const FailedCommitResponseDataList&)>
          on_commit_response_callback,
      base::OnceCallback<void(SyncCommitError)> on_full_commit_failure_callback,
      PassphraseType passphrase_type);

  CommitContributionImpl(const CommitContributionImpl&) = delete;
  CommitContributionImpl& operator=(const CommitContributionImpl&) = delete;

  ~CommitContributionImpl() override;

  // Implementation of CommitContribution
  void AddToCommitMessage(sync_pb::ClientToServerMessage* msg) override;
  SyncerError ProcessCommitResponse(
      const sync_pb::ClientToServerResponse& response,
      StatusController* status) override;
  void ProcessCommitFailure(SyncCommitError commit_error) override;
  size_t GetNumEntries() const override;

  // Public for testing.
  // Copies data to be committed from CommitRequestData into SyncEntity proto.
  static void PopulateCommitProto(DataType type,
                                  const CommitRequestData& commit_entity,
                                  sync_pb::SyncEntity* commit_proto);

 private:
  // Generates id for new entities and encrypts entity if needed.
  void AdjustCommitProto(sync_pb::SyncEntity* commit_proto);

  const DataType type_;

  // A callback to inform the object that created this contribution about commit
  // result.
  base::OnceCallback<void(const CommitResponseDataList&,
                          const FailedCommitResponseDataList&)>
      on_commit_response_callback_;

  // A callback to inform the object that created this contribution about commit
  // failure. This callback differs from |on_commit_response_callback_| and will
  // be called when the server respond with any error code or do not respond at
  // all (i.e. there is no internet connection).
  base::OnceCallback<void(SyncCommitError)> on_full_commit_failure_callback_;

  const PassphraseType passphrase_type_;

  // The type-global context information.
  const sync_pb::DataTypeContext context_;

  // The list of entities to be committed.
  CommitRequestDataList commit_requests_;

  // The index in the commit message where this contribution's entities are
  // added.  Used to correlate per-item requests with per-item responses.
  size_t entries_start_index_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_COMMIT_CONTRIBUTION_IMPL_H_
