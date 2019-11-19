// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_ENGINE_MOCK_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_TEST_ENGINE_MOCK_MODEL_TYPE_PROCESSOR_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {

// Mocks the ModelTypeProcessor.
//
// This mock is made simpler by not using any threads.  It does still have the
// ability to defer execution if we need to test race conditions, though.
//
// It maintains some state to try to make its behavior more realistic.  It
// updates this state as it creates commit requests or receives update and
// commit responses.
//
// It keeps a log of all received messages so tests can make assertions based
// on their value.
class MockModelTypeProcessor : public ModelTypeProcessor {
 public:
  using DisconnectCallback = base::Callback<void()>;

  MockModelTypeProcessor();
  ~MockModelTypeProcessor() override;

  // Implementation of ModelTypeProcessor.
  void ConnectSync(std::unique_ptr<CommitQueue> commit_queue) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       GetLocalChangesCallback callback) override;
  void OnCommitCompleted(const sync_pb::ModelTypeState& type_state,
                         const CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        UpdateResponseDataList response_list) override;

  // By default, this object behaves as if all messages are processed
  // immediately.  Sometimes it is useful to defer work until later, as might
  // happen in the real world if the model thread's task queue gets backed up.
  void SetSynchronousExecution(bool is_synchronous);

  // Runs any work that was deferred while this class was in asynchronous mode.
  //
  // This function is not useful unless this object is set to synchronous
  // execution mode, which is the default.
  void RunQueuedTasks();

  // Generate commit or deletion requests to be sent to the server.
  // These functions update local state to keep sequence numbers consistent.
  //
  // A real ModelTypeProcessor would forward these kinds of messages
  // directly to its attached CommitQueue.  These methods
  // return the value to the caller so the test framework can handle them as it
  // sees fit.
  std::unique_ptr<CommitRequestData> CommitRequest(
      const ClientTagHash& tag_hash,
      const sync_pb::EntitySpecifics& specifics);
  std::unique_ptr<CommitRequestData> DeleteRequest(
      const ClientTagHash& tag_hash);

  // Getters to access the log of received update responses.
  //
  // Does not includes responses that are in pending tasks.
  size_t GetNumUpdateResponses() const;
  std::vector<const UpdateResponseData*> GetNthUpdateResponse(size_t n) const;
  sync_pb::ModelTypeState GetNthUpdateState(size_t n) const;

  // Getters to access the log of received commit responses.
  //
  // Does not includes responses that are in pending tasks.
  size_t GetNumCommitResponses() const;
  CommitResponseDataList GetNthCommitResponse(size_t n) const;
  sync_pb::ModelTypeState GetNthCommitState(size_t n) const;

  // Getters to access the lastest update response for a given tag_hash.
  bool HasUpdateResponse(const ClientTagHash& tag_hash) const;
  const UpdateResponseData& GetUpdateResponse(
      const ClientTagHash& tag_hash) const;

  // Getters to access the lastest commit response for a given tag_hash.
  bool HasCommitResponse(const ClientTagHash& tag_hash) const;
  CommitResponseData GetCommitResponse(const ClientTagHash& tag_hash) const;

  void SetDisconnectCallback(const DisconnectCallback& callback);

  // Sets commit request that will be returned by GetLocalChanges().
  void SetCommitRequest(CommitRequestDataList commit_request);

  int GetLocalChangesCallCount() const;

 private:
  // Process a received commit response.
  //
  // Implemented as an Impl method so we can defer its execution in some cases.
  void OnCommitCompletedImpl(const sync_pb::ModelTypeState& type_state,
                             const CommitResponseDataList& response_list);

  // Process a received update response.
  //
  // Implemented as an Impl method so we can defer its execution in some cases.
  void OnUpdateReceivedImpl(const sync_pb::ModelTypeState& type_state,
                            UpdateResponseDataList response_list);

  // Getter and setter for per-item sequence number tracking.
  int64_t GetCurrentSequenceNumber(const ClientTagHash& tag_hash) const;
  int64_t GetNextSequenceNumber(const ClientTagHash& tag_hash);

  // Getter and setter for per-item base version tracking.
  int64_t GetBaseVersion(const ClientTagHash& tag_hash) const;
  void SetBaseVersion(const ClientTagHash& tag_hash, int64_t version);

  // Getters and setter for server-assigned ID values.
  bool HasServerAssignedId(const ClientTagHash& tag_hash) const;
  const std::string& GetServerAssignedId(const ClientTagHash& tag_hash) const;
  void SetServerAssignedId(const ClientTagHash& tag_hash,
                           const std::string& id);

  // State related to the implementation of deferred work.
  // See SetSynchronousExecution() for details.
  bool is_synchronous_;
  std::vector<base::OnceClosure> pending_tasks_;
  std::unique_ptr<CommitQueue> commit_queue_;

  // A log of messages received by this object.
  std::vector<CommitResponseDataList> received_commit_responses_;
  std::vector<UpdateResponseDataList> received_update_responses_;
  std::vector<sync_pb::ModelTypeState> type_states_received_on_update_;
  std::vector<sync_pb::ModelTypeState> type_states_received_on_commit_;

  // Latest responses received, indexed by tag_hash.
  std::map<ClientTagHash, CommitResponseData> commit_response_items_;
  std::map<ClientTagHash, const UpdateResponseData*> update_response_items_;

  // The per-item state maps.
  std::map<ClientTagHash, int64_t> sequence_numbers_;
  std::map<ClientTagHash, int64_t> base_versions_;
  std::map<ClientTagHash, std::string> assigned_ids_;

  // Set of tag hashes which were deleted with DeleteRequest but haven't yet
  // been confirmed by the server with OnCommitCompleted.
  std::set<ClientTagHash> pending_deleted_hashes_;

  // Callback which will be call during disconnection
  DisconnectCallback disconnect_callback_;

  // Commit request to return from GetLocalChanges().
  CommitRequestDataList commit_request_;

  int get_local_changes_call_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MockModelTypeProcessor);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_ENGINE_MOCK_MODEL_TYPE_PROCESSOR_H_
