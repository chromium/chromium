// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_DATA_TYPE_MANAGER_IMPL_H_
#define COMPONENTS_SYNC_SERVICE_DATA_TYPE_MANAGER_IMPL_H_

#include <map>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/service/data_type_manager.h"
#include "components/sync/service/model_load_manager.h"

namespace syncer {

class DataTypeController;
class DataTypeEncryptionHandler;
class DataTypeManagerObserver;

class DataTypeManagerImpl : public DataTypeManager,
                            public ModelLoadManagerDelegate {
 public:
  DataTypeManagerImpl(const DataTypeController::TypeMap* controllers,
                      const DataTypeEncryptionHandler* encryption_handler,
                      ModelTypeConfigurer* configurer,
                      DataTypeManagerObserver* observer);

  DataTypeManagerImpl(const DataTypeManagerImpl&) = delete;
  DataTypeManagerImpl& operator=(const DataTypeManagerImpl&) = delete;

  ~DataTypeManagerImpl() override;

  // DataTypeManager interface.
  void Configure(ModelTypeSet preferred_types,
                 const ConfigureContext& context) override;
  void DataTypePreconditionChanged(ModelType type) override;
  void ResetDataTypeErrors() override;

  // Needed only for backend migration.
  void PurgeForMigration(ModelTypeSet undesired_types) override;

  void Stop(SyncStopMetadataFate metadata_fate) override;
  ModelTypeSet GetActiveDataTypes() const override;
  ModelTypeSet GetPurgedDataTypes() const override;
  ModelTypeSet GetActiveProxyDataTypes() const override;
  ModelTypeSet GetTypesWithPendingDownloadForInitialSync() const override;
  ModelTypeSet GetDataTypesWithPermanentErrors() const override;
  State state() const override;

  // `ModelLoadManagerDelegate` implementation.
  void OnAllDataTypesReadyForConfigure() override;
  // No-op if the type is not connected or has already failed.
  void OnSingleDataTypeWillStop(ModelType type,
                                const SyncError& error) override;

  bool needs_reconfigure_for_test() const { return needs_reconfigure_; }
  ConfigureReason last_configure_reason_for_test() {
    return last_requested_context_.reason;
  }

 private:
  enum DataTypeConfigState {
    CONFIGURE_ACTIVE,    // Actively being configured. Data of such types
                         // will be downloaded if not present locally.
    CONFIGURE_INACTIVE,  // Already configured or to be configured in future.
                         // Data of such types is left as it is, no
                         // downloading or purging.
    DISABLED,            // Not syncing. Disabled by user.
    FATAL,               // Not syncing due to unrecoverable error.
    CRYPTO,              // Not syncing due to a cryptographer error.
    UNREADY,             // Not syncing due to transient error.
  };
  using DataTypeConfigStateMap = std::map<ModelType, DataTypeConfigState>;

  // Return model types in `state_map` that match `state`.
  static ModelTypeSet GetDataTypesInState(
      DataTypeConfigState state,
      const DataTypeConfigStateMap& state_map);

  // Set state of `types` in `state_map` to `state`.
  static void SetDataTypesState(DataTypeConfigState state,
                                ModelTypeSet types,
                                DataTypeConfigStateMap* state_map);

  // Prepare the parameters for the configurer's configuration.
  ModelTypeConfigurer::ConfigureParams PrepareConfigureParams();

  // Update precondition state of types in `data_type_status_table_` to match
  // value of DataTypeController::GetPreconditionState().
  void UpdatePreconditionErrors();

  // Update precondition state for `type`, such that `data_type_status_table_`
  // matches DataTypeController::GetPreconditionState(). Returns true if there
  // was an actual change.
  bool UpdatePreconditionError(ModelType type);

  // Starts a reconfiguration if it's required and no configuration is running.
  void ProcessReconfigure();

  // Programmatically force reconfiguration of all data types (if needed).
  void ForceReconfiguration();

  void Restart();

  void NotifyStart();
  void NotifyDone(const ConfigureResult& result);

  void ConfigureImpl(ModelTypeSet preferred_types,
                     const ConfigureContext& context);

  // Calls data type controllers of requested types to connect.
  void ConnectDataTypes();

  DataTypeConfigStateMap BuildDataTypeConfigStateMap(
      const ModelTypeSet& types_being_configured) const;

  // Start configuration of next set of types in `configuration_types_queue_`
  // (if any exist, does nothing otherwise).
  void StartNextConfiguration();
  void ConfigurationCompleted(ModelTypeSet succeeded_configuration_types,
                              ModelTypeSet failed_configuration_types);

  ModelTypeSet GetEnabledTypes() const;

  const raw_ptr<ModelTypeConfigurer> configurer_;

  // Map of all data type controllers that are available for sync.
  // This list is determined at startup by various command line flags.
  const raw_ptr<const DataTypeController::TypeMap> controllers_;

  State state_ = DataTypeManager::STOPPED;

  // The set of types whose initial download of sync data has completed.
  // Note: This class mostly doesn't handle control types (i.e. NIGORI) -
  // `controllers_` doesn't contain an entry for NIGORI, and by the time this
  // class gets instantiated, NIGORI is already up and running. It still has to
  // be maintained as part of `downloaded_types_`, however, since in some edge
  // cases (notably PurgeForMigration()), this class might have to trigger a
  // re-download of NIGORI data.
  // TODO(crbug.com/1422901): Consider removing this; see bug for details.
  ModelTypeSet downloaded_types_ = ControlTypes();

  // Types that requested in current configuration cycle.
  ModelTypeSet preferred_types_;

  // Context information (e.g. the reason) for the last reconfigure attempt.
  ConfigureContext last_requested_context_;

  // A set of types that were enabled at the time of Restart().
  ModelTypeSet preferred_types_without_errors_;

  // A set of types that have been configured but haven't been
  // connected/activated.
  ModelTypeSet configured_proxy_types_;

  // A set of types that should be redownloaded even if initial sync is
  // completed for them.
  ModelTypeSet force_redownload_types_;

  // Whether an attempt to reconfigure was made while we were busy configuring.
  // The `preferred_types_` will reflect the newest set of requested types.
  bool needs_reconfigure_ = false;

  // The last time Restart() was called.
  base::Time last_restart_time_;

  // The manager that loads the local models of the data types.
  ModelLoadManager model_load_manager_;

  // DataTypeManager must have only one observer -- the SyncServiceImpl that
  // created it and manages its lifetime.
  const raw_ptr<DataTypeManagerObserver> observer_;

  // For querying failed data types (having unrecoverable error) when
  // configuring backend.
  DataTypeStatusTable data_type_status_table_;

  // Types waiting to be configured, prioritized (highest priority first).
  base::queue<ModelTypeSet> configuration_types_queue_;

  // The encryption handler lets the DataTypeManager know the state of sync
  // datatype encryption.
  const raw_ptr<const DataTypeEncryptionHandler> encryption_handler_;

  base::WeakPtrFactory<DataTypeManagerImpl> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_DATA_TYPE_MANAGER_IMPL_H_
