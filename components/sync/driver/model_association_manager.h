// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_MODEL_ASSOCIATION_MANAGER_H__
#define COMPONENTS_SYNC_DRIVER_MODEL_ASSOCIATION_MANAGER_H__

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

#include "components/sync/base/weak_handle.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/data_type_manager.h"
#include "components/sync/engine/data_type_association_stats.h"
#include "components/sync/engine/shutdown_reason.h"

namespace syncer {

struct ConfigureContext;

// |ModelAssociationManager| does the heavy lifting for doing the actual model
// association. It instructs DataTypeControllers to load models, start
// associating and stopping. Since the operations are async it uses an
// interface to inform DataTypeManager the results of the operations.
// This class is owned by DataTypeManager.
// |ModelAssociationManager| association functions are async. The results of
// those operations are passed back via this interface.
class ModelAssociationManagerDelegate {
 public:
  // Called right before ModelAssociationManager calls LoadModels. Allows
  // directory types to register with sync engine before download starts.
  virtual void OnSingleDataTypeWillStart(ModelType type) = 0;

  // Called when all desired types are ready to be configured with
  // ModelTypeConfigurer. Data type is ready when its progress marker is
  // available to configurer. Directory data types are always ready, their
  // progress markers are read from directory. USS data type controllers need to
  // load model and read data type context first.
  // This function is called at most once after each call to
  // ModelAssociationManager::Initialize().
  virtual void OnAllDataTypesReadyForConfigure() = 0;

  // Called when model association (MergeDataAndStartSyncing) has completed
  // for |type|, regardless of success or failure.
  virtual void OnSingleDataTypeAssociationDone(
      ModelType type,
      const DataTypeAssociationStats& association_stats) = 0;

  // Called when the ModelAssociationManager has decided it must stop |type|,
  // likely because it is no longer a desired data type or sync is shutting
  // down.
  virtual void OnSingleDataTypeWillStop(ModelType type,
                                        const SyncError& error) = 0;

  // Called when the ModelAssociationManager has tried to perform model
  // association for all desired types and has nothing left to do.
  virtual void OnModelAssociationDone(
      const DataTypeManager::ConfigureResult& result) = 0;
  virtual ~ModelAssociationManagerDelegate() {}
};

// The class that is responsible for model association.
class ModelAssociationManager {
 public:
  enum State {
    // No configuration is in progress.
    IDLE,
    // The model association manager has been initialized with a set of desired
    // types, but is not actively associating any.
    INITIALIZED,
    // One or more types from |desired_types_| are in the process of
    // associating.
    ASSOCIATING,
  };

  ModelAssociationManager(const DataTypeController::TypeMap* controllers,
                          ModelAssociationManagerDelegate* delegate);
  virtual ~ModelAssociationManager();

  // Initializes the state to do the model association in future. This
  // should be called before communicating with sync server. A subsequent call
  // of Initialize is only allowed if the ModelAssociationManager has invoked
  // |OnModelAssociationDone| on the |ModelAssociationManagerDelegate|. After
  // this call, there should be several calls to StartAssociationAsync()
  // to associate subset of |desired_types| which must be a subset of
  // |preferred_types|.
  // |preferred_types| contains types selected by user.
  void Initialize(ModelTypeSet desired_types,
                  ModelTypeSet preferred_types,
                  const ConfigureContext& context);

  // Can be called at any time. Synchronously stops all datatypes.
  void Stop(ShutdownReason shutdown_reason);

  // Should only be called after Initialize to start the actual association.
  // |types_to_associate| should be subset of |desired_types| in Initialize().
  // When this is completed, |OnModelAssociationDone| will be invoked.
  void StartAssociationAsync(const ModelTypeSet& types_to_associate);

  // Stops an individual datatype |type| for |shutdown_reason|. |error| must be
  // an actual error (i.e. not UNSET).
  void StopDatatype(ModelType type,
                    ShutdownReason shutdown_reason,
                    SyncError error);

  // This is used for TESTING PURPOSE ONLY. The test case can inspect
  // and modify the timer.
  // TODO(sync) : This would go away if we made this class be able to do
  // Dependency injection. crbug.com/129212.
  base::OneShotTimer* GetTimerForTesting();

  State state() const { return state_; }

 private:
  // Start loading non-running types that are in |desired_types_|.
  void LoadEnabledTypes();

  // Callback passed to each data type controller on starting association. This
  // callback will be invoked when the model association is done.
  void TypeStartCallback(ModelType type,
                         base::TimeTicks type_start_time,
                         DataTypeController::ConfigureResult start_result,
                         const SyncMergeResult& local_merge_result,
                         const SyncMergeResult& syncer_merge_result);

  // Callback that will be invoked when the models finish loading. This callback
  // will be passed to |LoadModels| function.
  void ModelLoadCallback(ModelType type, const SyncError& error);

  // Called when all requested types are associated or association times out.
  // Will clean up any unfinished types, and update |state_| to be |new_state|
  // Finally, it will notify |delegate_| of the configuration result.
  void ModelAssociationDone(State new_state);

  // A helper to stop an individual datatype.
  void StopDatatypeImpl(const SyncError& error,
                        ShutdownReason shutdown_reason,
                        DataTypeController* dtc,
                        DataTypeController::StopCallback callback);

  // Calls delegate's OnAllDataTypesReadyForConfigure when all datatypes from
  // desired_types_ are ready for configure. Ensures that for every call to
  // Initialize callback is called at most once.
  // Datatype is ready if either it doesn't require LoadModels before configure
  // or LoadModels successfully finished.
  void NotifyDelegateIfReadyForConfigure();

  State state_;

  ConfigureContext configure_context_;

  // Data types that are enabled.
  ModelTypeSet desired_types_;

  // Data types that are requested to associate.
  ModelTypeSet requested_types_;

  // Data types currently being associated, including types waiting for model
  // load.
  ModelTypeSet associating_types_;

  // Data types that are loaded, i.e. ready to associate.
  ModelTypeSet loaded_types_;

  // Data types that are associated, i.e. no more action needed during
  // reconfiguration if not disabled.
  ModelTypeSet associated_types_;

  // Time when StartAssociationAsync() is called to associate for a set of data
  // types.
  base::TimeTicks association_start_time_;

  // Set of all registered controllers.
  const DataTypeController::TypeMap* controllers_;

  // The processor in charge of handling model association results.
  ModelAssociationManagerDelegate* delegate_;

  // Timer to track and limit how long a datatype takes to model associate.
  base::OneShotTimer timer_;

  DataTypeManager::ConfigureStatus configure_status_;

  bool notified_about_ready_for_configure_;

  base::WeakPtrFactory<ModelAssociationManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ModelAssociationManager);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_MODEL_ASSOCIATION_MANAGER_H__
