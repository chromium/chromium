// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_MODEL_ASSOCIATION_MANAGER_H__
#define COMPONENTS_SYNC_DRIVER_MODEL_ASSOCIATION_MANAGER_H__

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/engine/shutdown_reason.h"

namespace syncer {

struct ConfigureContext;

// Interface for ModelAssociationManager to pass the results of async operations
// back to DataTypeManager.
class ModelAssociationManagerDelegate {
 public:
  // Called when all desired types are loaded, i.e. are ready to be configured
  // with ModelTypeConfigurer. A data type is ready when its progress marker is
  // available, which is the case once the local model has been loaded.
  // This function is called at most once after each call to
  // ModelAssociationManager::Initialize().
  virtual void OnAllDataTypesReadyForConfigure() = 0;

  // Called when model association (MergeDataAndStartSyncing) has completed
  // for |type|, regardless of success or failure.
  virtual void OnSingleDataTypeAssociationDone(ModelType type) = 0;

  // Called when the ModelAssociationManager has decided it must stop |type|,
  // likely because it is no longer a desired data type, sync is shutting down,
  // or some error occurred during loading.
  virtual void OnSingleDataTypeWillStop(ModelType type,
                                        const SyncError& error) = 0;

  // Called when the ModelAssociationManager has tried to perform model
  // association for all desired types and has nothing left to do.
  virtual void OnModelAssociationDone(const ModelTypeSet& types) = 0;

  virtual ~ModelAssociationManagerDelegate() = default;
};

// |ModelAssociationManager| instructs DataTypeControllers to load models and
// to stop (DataTypeManager is responsible for activating/deactivating data
// types). Since the operations are async it uses an interface to inform
// DataTypeManager of the results of the operations.
// This class is owned by DataTypeManager.
// TODO(crbug.com/1102837): Association was a Directory concept, this class
// should disappear or be refactored.
class ModelAssociationManager {
 public:
  ModelAssociationManager(const DataTypeController::TypeMap* controllers,
                          ModelAssociationManagerDelegate* delegate);
  virtual ~ModelAssociationManager();

  // Stops any data types that are *not* in |desired_types|, then kicks off
  // loading of all |desired_types|. A subsequent Initialize() call is only
  // allowed after the ModelAssociationManager has invoked
  // OnModelAssociationDone() on the delegate. After this call, there should be
  // several calls to Associate() to associate subsets of |desired_types|, which
  // itself must be a subset of |preferred_types|.
  // |preferred_types| contains all types selected by the user.
  void Initialize(ModelTypeSet desired_types,
                  ModelTypeSet preferred_types,
                  const ConfigureContext& context);

  // Can be called at any time. Synchronously stops all datatypes.
  void Stop(ShutdownReason shutdown_reason);

  // Must only be called after all data type models have been loaded, i.e. after
  // OnAllDataTypesReadyForConfigure() has been called on the delegate.
  // |types_to_associate| should be subset of |desired_types| in Initialize().
  // Synchronously invokes |OnModelAssociationDone| on the delegate.
  void Associate(const ModelTypeSet& types_to_associate);

  // Stops an individual datatype |type| for |shutdown_reason|. |error| must be
  // an actual error (i.e. not UNSET).
  void StopDatatype(ModelType type,
                    ShutdownReason shutdown_reason,
                    SyncError error);

 private:
  enum State {
    // No configuration is in progress.
    IDLE,
    // The model association manager has been initialized with a set of desired
    // types.
    INITIALIZED,
  };

  // Start loading non-running types that are in |desired_types_|.
  void LoadDesiredTypes();

  // Callback that will be invoked when the model for |type| finishes loading.
  // This callback is passed to |LoadModels| function.
  void ModelLoadCallback(ModelType type, const SyncError& error);

  // A helper to stop an individual datatype.
  void StopDatatypeImpl(const SyncError& error,
                        ShutdownReason shutdown_reason,
                        DataTypeController* dtc,
                        DataTypeController::StopCallback callback);

  // Calls delegate's OnAllDataTypesReadyForConfigure if all datatypes from
  // |desired_types_| are loaded. Ensures that OnAllDataTypesReadyForConfigure
  // is called at most once for every call to Initialize().
  void NotifyDelegateIfReadyForConfigure();

  // Set of all registered controllers.
  const DataTypeController::TypeMap* const controllers_;

  // The delegate in charge of handling model association results.
  ModelAssociationManagerDelegate* const delegate_;

  State state_;

  ConfigureContext configure_context_;

  // Data types that are enabled.
  ModelTypeSet desired_types_;

  // Data types that are loaded, i.e. ready to associate.
  ModelTypeSet loaded_types_;

  // Data types that are associated, i.e. no more action needed during
  // reconfiguration if not disabled.
  ModelTypeSet associated_types_;

  bool notified_about_ready_for_configure_;

  base::WeakPtrFactory<ModelAssociationManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ModelAssociationManager);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_MODEL_ASSOCIATION_MANAGER_H__
