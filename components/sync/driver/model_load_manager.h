// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_MODEL_LOAD_MANAGER_H_
#define COMPONENTS_SYNC_DRIVER_MODEL_LOAD_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/engine/shutdown_reason.h"

namespace syncer {

struct ConfigureContext;

// Interface for ModelLoadManager to pass the results of async operations
// back to DataTypeManager.
class ModelLoadManagerDelegate {
 public:
  // Called when all desired types are loaded, i.e. are ready to be configured
  // with ModelTypeConfigurer. A data type is ready when its progress marker is
  // available, which is the case once the local model has been loaded.
  // This function is called at most once after each call to
  // ModelLoadManager::Initialize().
  virtual void OnAllDataTypesReadyForConfigure() = 0;

  // Called when the ModelLoadManager has decided it must stop |type|, likely
  // because it is no longer a desired data type, sync is shutting down, or some
  // error occurred during loading. Can be called for types that are not
  // connected or have already failed.
  virtual void OnSingleDataTypeWillStop(ModelType type,
                                        const SyncError& error) = 0;

  virtual ~ModelLoadManagerDelegate() = default;
};

// |ModelLoadManager| instructs DataTypeControllers to load models and to stop
// (DataTypeManager is responsible for activating/deactivating data types).
// Since the operations are async it uses an interface to inform DataTypeManager
// of the results of the operations.
// This class is owned by DataTypeManager.
class ModelLoadManager {
 public:
  ModelLoadManager(const DataTypeController::TypeMap* controllers,
                   ModelLoadManagerDelegate* delegate);

  ModelLoadManager(const ModelLoadManager&) = delete;
  ModelLoadManager& operator=(const ModelLoadManager&) = delete;

  virtual ~ModelLoadManager();

  // Stops any data types that are *not* in |preferred_types_without_errors|,
  // then kicks off loading of all |preferred_types_without_errors|.
  // |preferred_types_without_errors| must be a subset of |preferred_types|.
  // |preferred_types| contains all types selected by the user.
  void Initialize(ModelTypeSet preferred_types_without_errors,
                  ModelTypeSet preferred_types,
                  const ConfigureContext& context);

  // Can be called at any time. Synchronously stops all datatypes.
  void Stop(ShutdownReason shutdown_reason);

  // Stops an individual datatype |type| for |shutdown_reason|. |error| must be
  // an actual error (i.e. not UNSET).
  void StopDatatype(ModelType type,
                    ShutdownReason shutdown_reason,
                    SyncError error);

 private:
  // Start loading non-running types that are in
  // |preferred_types_without_errors_|.
  void LoadDesiredTypes();

  // Callback that will be invoked when the model for |type| finishes loading.
  // This callback is passed to the controller's |LoadModels| method.
  void ModelLoadCallback(ModelType type, const SyncError& error);

  // A helper to stop an individual datatype.
  void StopDatatypeImpl(const SyncError& error,
                        ShutdownReason shutdown_reason,
                        DataTypeController* dtc,
                        DataTypeController::StopCallback callback);

  // Calls delegate's OnAllDataTypesReadyForConfigure if all datatypes from
  // |preferred_types_without_errors_| are loaded. Ensures that
  // OnAllDataTypesReadyForConfigure is called at most once for every call to
  // Initialize().
  void NotifyDelegateIfReadyForConfigure();

  // Set of all registered controllers.
  const raw_ptr<const DataTypeController::TypeMap> controllers_;

  // The delegate in charge of handling model load results.
  const raw_ptr<ModelLoadManagerDelegate> delegate_;

  ConfigureContext configure_context_;

  // Data types that are enabled.
  ModelTypeSet preferred_types_without_errors_;

  // Data types that are loaded.
  ModelTypeSet loaded_types_;

  bool notified_about_ready_for_configure_ = false;

  base::WeakPtrFactory<ModelLoadManager> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_MODEL_LOAD_MANAGER_H_
