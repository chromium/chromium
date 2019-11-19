// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_MODEL_TYPE_PROCESSOR_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/nigori/nigori_local_change_processor.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {

class NigoriSyncBridge;
class ProcessorEntity;

class NigoriModelTypeProcessor : public ModelTypeProcessor,
                                 public ModelTypeControllerDelegate,
                                 public NigoriLocalChangeProcessor {
 public:
  NigoriModelTypeProcessor();
  ~NigoriModelTypeProcessor() override;

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       GetLocalChangesCallback callback) override;
  void OnCommitCompleted(const sync_pb::ModelTypeState& type_state,
                         const CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        UpdateResponseDataList updates) override;

  // ModelTypeControllerDelegate implementation.
  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override;
  void OnSyncStopping(SyncStopMetadataFate metadata_fate) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void GetStatusCountersForDebugging(StatusCountersCallback callback) override;
  void RecordMemoryUsageAndCountsHistograms() override;

  // NigoriLocalChangeProcessor implementation.
  void ModelReadyToSync(NigoriSyncBridge* bridge,
                        NigoriMetadataBatch nigori_metadata) override;
  void Put(std::unique_ptr<EntityData> entity_data) override;
  bool IsEntityUnsynced() override;
  NigoriMetadataBatch GetMetadata() override;
  void ReportError(const ModelError& error) override;
  base::WeakPtr<ModelTypeControllerDelegate> GetControllerDelegate() override;
  bool IsTrackingMetadata() override;

  bool IsConnectedForTest() const;

 private:
  // Returns true if the handshake with sync thread is complete.
  bool IsConnected() const;

  // If preconditions are met, informs sync that we are ready to connect.
  void ConnectIfReady();

  // Nudges worker if there are any local changes to be committed.
  void NudgeForCommitIfNeeded() const;

  // The bridge owns this processor instance so the pointer should never become
  // invalid.
  NigoriSyncBridge* bridge_;

  // The model type metadata (progress marker, initial sync done, etc).
  sync_pb::ModelTypeState model_type_state_;

  // Whether the model has initialized its internal state for sync (and provided
  // metadata).
  bool model_ready_to_sync_ = false;

  // Stores the start callback in between OnSyncStarting() and ReadyToConnect().
  StartCallback start_callback_;

  // The request context passed in as part of OnSyncStarting().
  DataTypeActivationRequest activation_request_;

  // The first model error that occurred, if any. Stored to track model state
  // and so it can be passed to sync if it happened prior to sync being ready.
  base::Optional<ModelError> model_error_;

  std::unique_ptr<ProcessorEntity> entity_;

  // Reference to the CommitQueue.
  //
  // The interface hides the posting of tasks across threads as well as the
  // CommitQueue's implementation.  Both of these features are useful in tests.
  std::unique_ptr<CommitQueue> worker_;

  SEQUENCE_CHECKER(sequence_checker_);

  // WeakPtrFactory for this processor for ModelTypeController (only gets
  // invalidated during destruction).
  base::WeakPtrFactory<ModelTypeControllerDelegate>
      weak_ptr_factory_for_controller_{this};

  DISALLOW_COPY_AND_ASSIGN(NigoriModelTypeProcessor);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_MODEL_TYPE_PROCESSOR_H_
