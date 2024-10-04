// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_MODEL_LOAD_MANAGER_H_
#define COMPONENTS_SYNC_SERVICE_MODEL_LOAD_MANAGER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/service/data_type_controller.h"

namespace base {
class ElapsedTimer;
}  // namespace base

namespace syncer {

class ModelError;
class SyncError;

// Timeout duration for loading data types in ModelLoadManager. Exposed for
// testing.
extern const base::TimeDelta kSyncLoadModelsTimeoutDuration;

// Interface for ModelLoadManager to pass the results of async operations
// back to DataTypeManager.
class ModelLoadManagerDelegate {
 public:
  // Called when all desired types are loaded, i.e. are ready to be configured
  // with DataTypeConfigurer. A data type is ready when its progress marker is
  // available, which is the case once the local model has been loaded.
  // This function is called at most once after each call to
  // ModelLoadManager::Configure().
  virtual void OnAllDataTypesReadyForConfigure() = 0;

  // Called when the ModelLoadManager has decided it must stop `type`, likely
  // because it is no longer a desired data type, sync is shutting down, or some
  // error occurred during loading. Can be called for types that are not
  // connected or have already failed but should not be called for the same
  // error multiple times.
  virtual void OnSingleDataTypeWillStop(
      DataType type,
      const std::optional<SyncError>& error) = 0;

  virtual ~ModelLoadManagerDelegate() = default;
};

// `ModelLoadManager` instructs DataTypeControllers to load models and to stop
// (DataTypeManager is responsible for activating/deactivating data types).
// Since the operations are async it uses an interface to inform DataTypeManager
// of the results of the operations.
// This class is owned by DataTypeManager, and lives on the UI thread.
class ModelLoadManager {
 public:
  ModelLoadManager(const DataTypeController::TypeMap* controllers,
                   ModelLoadManagerDelegate* delegate);

  ModelLoadManager(const ModelLoadManager&) = delete;
  ModelLoadManager& operator=(const ModelLoadManager&) = delete;

  ~ModelLoadManager();

  // (Re)configures the ModelLoadManager with a new set of data types.
  // Stops any data types that are *not* in `preferred_types_without_errors`,
  // then kicks off loading of all `preferred_types_without_errors`.
  // `preferred_types_without_errors` must be a subset of `preferred_types`.
  // `preferred_types` contains all types selected by the user.
  void Configure(DataTypeSet preferred_types_without_errors,
                 DataTypeSet preferred_types,
                 const ConfigureContext& context);

  // Can be called at any time. Synchronously stops all datatypes.
  void Stop(SyncStopMetadataFate metadata_fate);

  // Stops an individual datatype `type`.
  void StopDatatype(DataType type,
                    SyncStopMetadataFate metadata_fate,
                    SyncError error);

 private:
  // Start loading non-running types that are in
  // `preferred_types_without_errors_`.
  void LoadDesiredTypes();

  // Callback that will be invoked when the model for `type` finishes loading.
  // This callback is passed to the controller's `LoadModels` method.
  void ModelLoadCallback(DataType type, const std::optional<ModelError>& error);

  // A helper to stop an individual datatype.
  void StopDatatypeImpl(const std::optional<SyncError>& error,
                        SyncStopMetadataFate metadata_fate,
                        DataTypeController* dtc,
                        DataTypeController::StopCallback callback);

  // Calls delegate's OnAllDataTypesReadyForConfigure() if all datatypes from
  // `preferred_types_without_errors_` are loaded. Ensures that
  // OnAllDataTypesReadyForConfigure() is called at most once for every call to
  // Configure().
  void NotifyDelegateIfReadyForConfigure();

  // Called by `load_models_timeout_timer_`. Issues stop signal (with
  // error) to controllers for all types which have not started till now.
  void OnLoadModelsTimeout();

  // Loads model for a type using `dtc`. Ensures that LoadModels is only
  // called for types which are not in a FAILED state.
  void LoadModelsForType(DataTypeController* dtc);

  // Set of all registered controllers.
  const raw_ptr<const DataTypeController::TypeMap> controllers_;

  // The delegate in charge of handling model load results.
  const raw_ptr<ModelLoadManagerDelegate> delegate_;

  std::optional<ConfigureContext> configure_context_;

  // Data types that are enabled.
  DataTypeSet preferred_types_without_errors_;

  // Timer to track LoadDesiredTypes() timeout. All types not loaded by now are
  // treated as having errors.
  base::OneShotTimer load_models_timeout_timer_;

  // Timer to measure time by which all types have finished loading (or timed
  // out).
  std::unique_ptr<base::ElapsedTimer> load_models_elapsed_timer_;

  bool delegate_waiting_for_ready_for_configure_ = false;

  base::WeakPtrFactory<ModelLoadManager> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_MODEL_LOAD_MANAGER_H_
