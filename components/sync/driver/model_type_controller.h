// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_MODEL_TYPE_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/sync_error.h"

namespace syncer {

struct DataTypeActivationResponse;

// DataTypeController implementation for Unified Sync and Storage model types.
class ModelTypeController : public DataTypeController {
 public:
  // For datatypes that do not run in transport-only mode.
  ModelTypeController(
      ModelType type,
      std::unique_ptr<ModelTypeControllerDelegate> delegate_for_full_sync_mode);
  // For datatypes that have support for STORAGE_IN_MEMORY.
  ModelTypeController(
      ModelType type,
      std::unique_ptr<ModelTypeControllerDelegate> delegate_for_full_sync_mode,
      std::unique_ptr<ModelTypeControllerDelegate> delegate_for_transport_mode);

  ModelTypeController(const ModelTypeController&) = delete;
  ModelTypeController& operator=(const ModelTypeController&) = delete;

  ~ModelTypeController() override;

  // DataTypeController implementation.
  void LoadModels(const ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  std::unique_ptr<DataTypeActivationResponse> Connect() override;
  void Stop(ShutdownReason reason, StopCallback callback) override;
  State state() const override;
  bool ShouldRunInTransportOnlyMode() const override;
  void GetAllNodes(AllNodesCallback callback) override;
  void GetTypeEntitiesCount(base::OnceCallback<void(const TypeEntitiesCount&)>
                                callback) const override;
  void RecordMemoryUsageAndCountsHistograms() override;

  ModelTypeControllerDelegate* GetDelegateForTesting(SyncMode sync_mode);

 protected:
  // Subclasses that use this constructor must call InitModelTypeController().
  explicit ModelTypeController(ModelType type);

  // |delegate_for_transport_mode| may be null if the type does not run in
  // transport mode.
  void InitModelTypeController(
      std::unique_ptr<ModelTypeControllerDelegate> delegate_for_full_sync_mode,
      std::unique_ptr<ModelTypeControllerDelegate> delegate_for_transport_mode);

  void ReportModelError(SyncError::ErrorType error_type,
                        const ModelError& error);

 private:
  void RecordStartFailure() const;
  void RecordRunFailure() const;
  void OnDelegateStarted(
      std::unique_ptr<DataTypeActivationResponse> activation_response);
  void TriggerCompletionCallbacks(const SyncError& error);
  void ClearMetadataWhileStopped();

  base::flat_map<SyncMode, std::unique_ptr<ModelTypeControllerDelegate>>
      delegate_map_;

  // State of this datatype controller.
  State state_ = NOT_RUNNING;

  // Owned by |delegate_map_|. Null while NOT_RUNNING.
  raw_ptr<ModelTypeControllerDelegate> delegate_ = nullptr;

  // Callback for use when starting the datatype (usually MODEL_STARTING, but
  // STOPPING if abort requested while starting).
  ModelLoadCallback model_load_callback_;

  // Callbacks for use when stopping the datatype (STOPPING), which also
  // includes aborting a start. This is important because STOPPING is a state
  // used to make sure we don't request two starts in parallel to the delegate,
  // which is hard to support, most notably in ClientTagBasedModelTypeProcessor.
  // We use a vector because it's allowed to call Stop() multiple times (i.e.
  // while STOPPING).
  std::vector<StopCallback> model_stop_callbacks_;
  SyncStopMetadataFate model_stop_metadata_fate_;

  // Controller receives |activation_response_| from
  // ClientTagBasedModelTypeProcessor callback and must temporarily own it until
  // Connect is called.
  std::unique_ptr<DataTypeActivationResponse> activation_response_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_MODEL_TYPE_CONTROLLER_H_
