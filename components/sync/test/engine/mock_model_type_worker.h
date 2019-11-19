// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_ENGINE_MOCK_MODEL_TYPE_WORKER_H_
#define COMPONENTS_SYNC_TEST_ENGINE_MOCK_MODEL_TYPE_WORKER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

// Receives and records commit requests sent through the ModelTypeWorker.
//
// This class also includes features intended to help mock out server behavior.
// It has some basic functionality to keep track of server state and generate
// plausible UpdateResponseData and CommitResponseData messages.
class MockModelTypeWorker : public CommitQueue {
 public:
  MockModelTypeWorker(const sync_pb::ModelTypeState& model_type_state,
                      ModelTypeProcessor* processor);
  ~MockModelTypeWorker() override;

  // Callback when local changes are received from the processor.
  void LocalChangesReceived(CommitRequestDataList&& commit_request);

  // Implementation of ModelTypeWorker.
  void NudgeForCommit() override;

  // Getters to inspect the requests sent to this object.
  size_t GetNumPendingCommits() const;
  std::vector<const CommitRequestData*> GetNthPendingCommit(size_t n) const;
  bool HasPendingCommitForHash(const ClientTagHash& tag_hash) const;
  const CommitRequestData* GetLatestPendingCommitForHash(
      const ClientTagHash& tag_hash) const;

  // Verify that the |n|th commit request list has the corresponding commit
  // requests for |tag_hashes| with |value| set.
  void VerifyNthPendingCommit(
      size_t n,
      const std::vector<ClientTagHash>& tag_hashes,
      const std::vector<sync_pb::EntitySpecifics>& specificsList);

  // Verify the pending commits each contain a list of CommitRequestData and
  // they have the same hashes in the same order as |tag_hashes|.
  void VerifyPendingCommits(
      const std::vector<std::vector<ClientTagHash>>& tag_hashes);

  // Updates the model type state to be used in all future updates from server.
  void UpdateModelTypeState(const sync_pb::ModelTypeState& model_type_state);

  // Trigger an update from the server. See GenerateUpdateData for parameter
  // descriptions. |version_offset| defaults to 1 and |ekn| defaults to the
  // current encryption key name the worker has.
  void UpdateFromServer();
  void UpdateFromServer(const ClientTagHash& tag_hash,
                        const sync_pb::EntitySpecifics& specifics);
  void UpdateFromServer(const ClientTagHash& tag_hash,
                        const sync_pb::EntitySpecifics& specifics,
                        int64_t version_offset);
  void UpdateFromServer(const ClientTagHash& tag_hash,
                        const sync_pb::EntitySpecifics& specifics,
                        int64_t version_offset,
                        const std::string& ekn);
  void UpdateFromServer(UpdateResponseDataList updates);

  // Returns an UpdateResponseData representing an update received from
  // the server. Updates server state accordingly.
  //
  // The |version_offset| field can be used to emulate stale data (ie. versions
  // going backwards), reflections and redeliveries (ie. several instances of
  // the same version) or new updates.
  //
  // |ekn| is the encryption key name this item will fake having.
  std::unique_ptr<syncer::UpdateResponseData> GenerateUpdateData(
      const ClientTagHash& tag_hash,
      const sync_pb::EntitySpecifics& specifics,
      int64_t version_offset,
      const std::string& ekn);

  // Mostly same as GenerateUpdateData above, but set 1 as |version_offset|, and
  // use model_type_state_.encryption_key_name() as |ekn|.
  std::unique_ptr<syncer::UpdateResponseData> GenerateUpdateData(
      const ClientTagHash& tag_hash,
      const sync_pb::EntitySpecifics& specifics);

  // Returns an UpdateResponseData representing an update received from
  // the server for a type root node.
  std::unique_ptr<syncer::UpdateResponseData> GenerateTypeRootUpdateData(
      const ModelType& model_type);

  // Triggers a server-side deletion of the entity with |tag_hash|; updates
  // server state accordingly.
  void TombstoneFromServer(const ClientTagHash& tag_hash);

  // Pops one pending commit from the front of the queue and send a commit
  // response to the processor for it.
  void AckOnePendingCommit();
  void AckOnePendingCommit(int64_t version_offset);

  // Pops one pending commit, but returns empty commit response list to indicate
  // that commit failed for requested entities.
  void FailOneCommit();

  // Set the encryption key to |ekn| and inform the processor with an update
  // containing the data in |update|, which defaults to an empty list.
  void UpdateWithEncryptionKey(const std::string& ekn);
  void UpdateWithEncryptionKey(const std::string& ekn,
                               UpdateResponseDataList update);

  void UpdateWithGarbageCollection(
      UpdateResponseDataList updates,
      const sync_pb::GarbageCollectionDirective& gcd);

  void UpdateWithGarbageCollection(
      const sync_pb::GarbageCollectionDirective& gcd);

 private:
  // Generate an ID string.
  static std::string GenerateId(const ClientTagHash& tag_hash);

  // Returns a commit response that indicates a successful commit of the
  // given |request_data|. Updates server state accordingly.
  CommitResponseData SuccessfulCommitResponse(
      const CommitRequestData& request_data,
      int64_t version_offset);

  // Retrieve or set the server version.
  int64_t GetServerVersion(const ClientTagHash& tag_hash);
  void SetServerVersion(const ClientTagHash& tag_hash, int64_t version);

  sync_pb::ModelTypeState model_type_state_;

  // A pointer to the processor for this mock worker.
  ModelTypeProcessor* processor_;

  // A record of past commits requests.
  base::circular_deque<CommitRequestDataList> pending_commits_;

  // Map of versions by client tag hash.
  // This is an essential part of the mocked server state.
  std::map<ClientTagHash, int64_t> server_versions_;

  // WeakPtrFactory for this worker which will be sent to sync thread.
  base::WeakPtrFactory<MockModelTypeWorker> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockModelTypeWorker);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_ENGINE_MOCK_MODEL_TYPE_WORKER_H_
