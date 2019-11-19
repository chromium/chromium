// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_IMPL_H__
#define COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_IMPL_H__

#include "components/sync/driver/data_type_manager.h"

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/model_association_manager.h"
#include "components/sync/engine/model_type_configurer.h"

namespace syncer {

class DataTypeController;
class DataTypeDebugInfoListener;
class DataTypeEncryptionHandler;
class DataTypeManagerObserver;
struct DataTypeConfigurationStats;

// List of data types grouped by priority and ordered from high priority to
// low priority.
using TypeSetPriorityList = base::queue<ModelTypeSet>;

class DataTypeManagerImpl : public DataTypeManager,
                            public ModelAssociationManagerDelegate {
 public:
  DataTypeManagerImpl(
      ModelTypeSet initial_types,
      const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
      const DataTypeController::TypeMap* controllers,
      const DataTypeEncryptionHandler* encryption_handler,
      ModelTypeConfigurer* configurer,
      DataTypeManagerObserver* observer);
  ~DataTypeManagerImpl() override;

  // DataTypeManager interface.
  void Configure(ModelTypeSet desired_types,
                 const ConfigureContext& context) override;
  void DataTypePreconditionChanged(ModelType type) override;
  void ResetDataTypeErrors() override;

  // Needed only for backend migration.
  void PurgeForMigration(ModelTypeSet undesired_types) override;

  void Stop(ShutdownReason reason) override;
  ModelTypeSet GetActiveDataTypes() const override;
  bool IsNigoriEnabled() const override;
  State state() const override;

  // |ModelAssociationManagerDelegate| implementation.
  void OnSingleDataTypeWillStart(ModelType type) override;
  void OnAllDataTypesReadyForConfigure() override;
  void OnSingleDataTypeAssociationDone(
      ModelType type,
      const DataTypeAssociationStats& association_stats) override;
  void OnModelAssociationDone(
      const DataTypeManager::ConfigureResult& result) override;
  void OnSingleDataTypeWillStop(ModelType type,
                                const SyncError& error) override;

  // Used by unit tests. TODO(sync) : This would go away if we made
  // this class be able to do Dependency injection. crbug.com/129212.
  ModelAssociationManager* GetModelAssociationManagerForTesting() {
    return &model_association_manager_;
  }

 protected:
  // Returns the priority types (control + priority user types).
  // Virtual for overriding during tests.
  virtual ModelTypeSet GetPriorityTypes() const;

  // The set of types whose initial download of sync data has completed.
  ModelTypeSet downloaded_types_;

 private:
  enum DataTypeConfigState {
    CONFIGURE_ACTIVE,    // Actively being configured. Data of such types
                         // will be downloaded if not present locally.
    CONFIGURE_INACTIVE,  // Already configured or to be configured in future.
                         // Data of such types is left as it is, no
                         // downloading or purging.
    CONFIGURE_CLEAN,     // Actively being configured but requiring unapply
                         // and GetUpdates first (e.g. for persistence errors).
    DISABLED,            // Not syncing. Disabled by user.
    FATAL,               // Not syncing due to unrecoverable error.
    CRYPTO,              // Not syncing due to a cryptographer error.
    UNREADY,             // Not syncing due to transient error.
  };
  using DataTypeConfigStateMap = std::map<ModelType, DataTypeConfigState>;

  // Helper enum for identifying which types within a priority group to
  // associate.
  enum AssociationGroup {
    // Those types that were already downloaded and didn't have an error at
    // configuration time. Corresponds with AssociationTypesInfo's
    // |ready_types|. These types can start associating as soon as the
    // ModelAssociationManager is not busy.
    READY_AT_CONFIG,
    // All other types, including first time sync types and those that have
    // encountered an error. These types must wait until the syncer has done
    // any db changes and/or downloads before associating.
    UNREADY_AT_CONFIG,
  };

  // Return model types in |state_map| that match |state|.
  static ModelTypeSet GetDataTypesInState(
      DataTypeConfigState state,
      const DataTypeConfigStateMap& state_map);

  // Set state of |types| in |state_map| to |state|.
  static void SetDataTypesState(DataTypeConfigState state,
                                ModelTypeSet types,
                                DataTypeConfigStateMap* state_map);

  // Prepare the parameters for the configurer's configuration. Returns the set
  // of types that are already ready for association.
  ModelTypeSet PrepareConfigureParams(
      ModelTypeConfigurer::ConfigureParams* params);

  // Abort configuration and stop all data types due to configuration errors.
  void Abort(ConfigureStatus status);

  // Divide |types| into sets by their priorities and return the sets from
  // high priority to low priority.
  TypeSetPriorityList PrioritizeTypes(const ModelTypeSet& types);

  // Update precondition state of types in data_type_status_table_ to match
  // value of DataTypeController::GetPreconditionState().
  void UpdatePreconditionErrors(const ModelTypeSet& desired_types);

  // Update precondition state for |type|, such that data_type_status_table_
  // matches DataTypeController::GetPreconditionState(). Returns true if there
  // was an actual change.
  bool UpdatePreconditionError(ModelType type);

  // Post a task to reconfigure when no downloading or association are running.
  void ProcessReconfigure();

  // Programmatically force reconfiguration of data type (if needed).
  void ForceReconfiguration();

  void Restart();
  void DownloadReady(ModelTypeSet types_to_download,
                     ModelTypeSet first_sync_types,
                     ModelTypeSet failed_configuration_types);

  void NotifyStart();
  void NotifyDone(const ConfigureResult& result);

  void ConfigureImpl(ModelTypeSet desired_types,
                     const ConfigureContext& context);

  // Calls data type controllers of requested types to register with backend.
  void RegisterTypesWithBackend();

  DataTypeConfigStateMap BuildDataTypeConfigStateMap(
      const ModelTypeSet& types_being_configured) const;

  // Start download of next set of types in |download_types_queue_| (if
  // any exist, does nothing otherwise).
  // Will kick off association of any new ready types.
  void StartNextDownload(ModelTypeSet high_priority_types_before);

  // Start association of next batch of data types after association of
  // previous batch finishes. |group| controls which set of types within
  // an AssociationTypesInfo to associate. Does nothing if model associator
  // is busy performing association.
  void StartNextAssociation(AssociationGroup group);

  void StopImpl(ShutdownReason reason);

  // Returns the currently enabled types.
  ModelTypeSet GetEnabledTypes() const;

  ModelTypeConfigurer* configurer_;

  // Map of all data type controllers that are available for sync.
  // This list is determined at startup by various command line flags.
  const DataTypeController::TypeMap* controllers_;
  State state_;

  // Types that requested in current configuration cycle.
  ModelTypeSet last_requested_types_;

  // Context information (e.g. the reason) for the last reconfigure attempt.
  // Note: this will be set to a valid value only when |needs_reconfigure_| is
  // set.
  ConfigureContext last_requested_context_;

  // A set of types that were enabled at the time initialization with the
  // |model_association_manager_| was last attempted.
  ModelTypeSet last_enabled_types_;

  // A set of types that should be redownloaded even if initial sync is
  // completed for them.
  // TODO(crbug.com/967677): Once all datatypes are in USS, we should redesign
  // this class and for example compute |downloaded_types_|'s initial value
  // only after all datatypes have loaded for the first time.
  ModelTypeSet force_redownload_types_;

  // Whether an attempt to reconfigure was made while we were busy configuring.
  // The |last_requested_types_| will reflect the newest set of requested types.
  bool needs_reconfigure_;

  // The last time Restart() was called.
  base::Time last_restart_time_;

  // Sync's datatype debug info listener, which we pass model association
  // statistics to.
  const WeakHandle<DataTypeDebugInfoListener> debug_info_listener_;

  // The manager that handles the model association of the individual types.
  ModelAssociationManager model_association_manager_;

  // DataTypeManager must have only one observer -- the ProfileSyncService that
  // created it and manages its lifetime.
  DataTypeManagerObserver* const observer_;

  // For querying failed data types (having unrecoverable error) when
  // configuring backend.
  DataTypeStatusTable data_type_status_table_;

  // Types waiting to be downloaded.
  TypeSetPriorityList download_types_queue_;

  // Types waiting for association and related time tracking info.
  struct AssociationTypesInfo {
    AssociationTypesInfo();
    AssociationTypesInfo(const AssociationTypesInfo& other);
    ~AssociationTypesInfo();

    // Types to associate.
    ModelTypeSet types;
    // Types that have just been downloaded and are being associated for the
    // first time. This includes types that had previously encountered an error
    // and had to be purged/unapplied from the sync db.
    // This is a subset of |types|.
    ModelTypeSet first_sync_types;
    // Types that were already ready for association at configuration time.
    ModelTypeSet ready_types;
    // Time at which |types| began downloading.
    base::Time download_start_time;
    // Time at which |types| finished downloading.
    base::Time download_ready_time;
    // Time at which the association for |read_types| began.
    base::Time ready_association_request_time;
    // Time at which the association for |types| began (not relevant to
    // |ready_types|.
    base::Time full_association_request_time;
    // The set of types that are higher priority (and were therefore blocking)
    // the association of |types|.
    ModelTypeSet high_priority_types_before;
    // The subset of |types| that were successfully configured.
    ModelTypeSet configured_types;
  };
  base::queue<AssociationTypesInfo> association_types_queue_;

  // The encryption handler lets the DataTypeManager know the state of sync
  // datatype encryption.
  const DataTypeEncryptionHandler* encryption_handler_;

  // Association and time stats of data type configuration.
  std::vector<DataTypeConfigurationStats> configuration_stats_;

  // Configuration process is started when ModelAssociationManager notifies
  // DataTypeManager that all types are ready for configure.
  // This flag ensures that this process is started only once.
  bool download_started_;

  base::WeakPtrFactory<DataTypeManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DataTypeManagerImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_IMPL_H__
