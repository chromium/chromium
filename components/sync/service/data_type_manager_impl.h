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
#include "components/sync/engine/data_type_configurer.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/data_type_manager.h"
#include "components/sync/service/data_type_status_table.h"
#include "components/sync/service/model_load_manager.h"

namespace syncer {

class DataTypeEncryptionHandler;
class DataTypeManagerObserver;

class DataTypeManagerImpl : public DataTypeManager,
                            public ModelLoadManagerDelegate {
 public:
  DataTypeManagerImpl(DataTypeController::TypeVector controllers,
                      const DataTypeEncryptionHandler* encryption_handler,
                      DataTypeManagerObserver* observer);

  DataTypeManagerImpl(const DataTypeManagerImpl&) = delete;
  DataTypeManagerImpl& operator=(const DataTypeManagerImpl&) = delete;

  ~DataTypeManagerImpl() override;

  // DataTypeManager interface.
  void ClearMetadataWhileStoppedExceptFor(DataTypeSet types) override;
  void SetConfigurer(DataTypeConfigurer* configurer) override;
  void Configure(DataTypeSet preferred_types,
                 const ConfigureContext& context) override;
  void DataTypePreconditionChanged(DataType type) override;
  void ResetDataTypeErrors() override;

  // Needed only for backend migration.
  void PurgeForMigration(DataTypeSet undesired_types) override;

  void Stop(SyncStopMetadataFate metadata_fate) override;

  DataTypeSet GetRegisteredDataTypes() const override;
  DataTypeSet GetDataTypesForTransportOnlyMode() const override;
  DataTypeSet GetActiveDataTypes() const override;
  DataTypeSet GetStoppedDataTypesExcludingNigori() const override;
  DataTypeSet GetActiveProxyDataTypes() const override;
  DataTypeSet GetTypesWithPendingDownloadForInitialSync() const override;
  DataTypeSet GetDataTypesWithPermanentErrors() const override;
  void GetTypesWithUnsyncedData(
      DataTypeSet requested_types,
      base::OnceCallback<void(DataTypeSet)> callback) const override;
  void GetLocalDataDescriptions(
      DataTypeSet types,
      base::OnceCallback<void(std::map<DataType, LocalDataDescription>)>
          callback) override;
  void TriggerLocalDataMigration(DataTypeSet types) override;
  State state() const override;
  TypeStatusMapForDebugging GetTypeStatusMapForDebugging(
      DataTypeSet throttled_types,
      DataTypeSet backed_off_types) const override;
  void GetAllNodesForDebugging(
      base::OnceCallback<void(base::Value::List)> callback) const override;
  void GetEntityCountsForDebugging(
      base::RepeatingCallback<void(const TypeEntitiesCount&)> callback)
      const override;
  DataTypeController* GetControllerForTest(DataType type) override;

  // `ModelLoadManagerDelegate` implementation.
  void OnAllDataTypesReadyForConfigure() override;
  void OnSingleDataTypeWillStop(DataType type,
                                const std::optional<SyncError>& error) override;

  bool needs_reconfigure_for_test() const { return needs_reconfigure_; }

 private:
  // Prepare the parameters for the configurer's configuration.
  DataTypeConfigurer::ConfigureParams PrepareConfigureParams();

  // Update precondition state of types in `data_type_status_table_` to match
  // value of DataTypeController::GetPreconditionState().
  void UpdatePreconditionErrors();

  // Update precondition state for `type`, such that `data_type_status_table_`
  // matches DataTypeController::GetPreconditionState(). Returns true if there
  // was an actual change.
  bool UpdatePreconditionError(DataType type);

  // Starts a reconfiguration if it's required and no configuration is running.
  void ProcessReconfigure();

  // Programmatically force reconfiguration of all data types (if needed).
  void ForceReconfiguration();

  void Restart();

  void NotifyStart();
  void NotifyDone(ConfigureStatus status);

  void ConfigureImpl(DataTypeSet preferred_types,
                     const ConfigureContext& context);

  // Calls data type controllers of requested types to connect.
  void ConnectDataTypes();

  // Start configuration of next set of types in `configuration_types_queue_`
  // (if any exist, does nothing otherwise).
  void StartNextConfiguration();
  void ConfigurationCompleted(DataTypeSet succeeded_configuration_types,
                              DataTypeSet failed_configuration_types);

  DataTypeSet GetEnabledTypes() const;

  // Returns the types that have a non-null DataTypeLocalDataBatchUploader.
  DataTypeSet GetDataTypesWithLocalDataBatchUploader() const;

  // Records per type histograms for estimated memory usage and number of
  // entities.
  void RecordMemoryUsageAndCountsHistograms();

  // Map of all data type controllers that are available for sync.
  // This list is determined at startup by various command line flags.
  const DataTypeController::TypeMap controllers_;

  // DataTypeManager must have only one observer -- the SyncServiceImpl that
  // created it and manages its lifetime.
  const raw_ptr<DataTypeManagerObserver> observer_;

  // The encryption handler lets the DataTypeManager know the state of sync
  // datatype encryption.
  const raw_ptr<const DataTypeEncryptionHandler> encryption_handler_;

  // The manager that loads the local models of the data types.
  ModelLoadManager model_load_manager_;

  raw_ptr<DataTypeConfigurer> configurer_ = nullptr;

  State state_ = DataTypeManager::STOPPED;

  // Types that were requested in the current configuration cycle.
  DataTypeSet preferred_types_;

  // Context information (e.g. the reason) for the last reconfigure attempt.
  ConfigureContext last_requested_context_;

  // A set of types that were enabled at the time of Restart().
  DataTypeSet preferred_types_without_errors_;

  // A set of types that have been configured but haven't been
  // connected/activated.
  DataTypeSet configured_proxy_types_;

  // The set of types whose initial download of sync data has completed.
  // Note: This class mostly doesn't handle control types (i.e. NIGORI) -
  // `controllers_` doesn't contain an entry for NIGORI. It still has to be
  // maintained as part of `downloaded_types_`, however, since in some edge
  // cases (notably PurgeForMigration()), this class might have to trigger a
  // re-download of NIGORI data.
  // TODO(crbug.com/40897183): Consider removing this; see bug for details.
  DataTypeSet downloaded_types_ = ControlTypes();

  // A set of types that should be redownloaded even if initial sync is
  // completed for them. Set when a type's precondition status changes from
  // not-met to met.
  DataTypeSet force_redownload_types_;

  // Whether a (re)configure was requested while a configuration was ongoing.
  // The `preferred_types_` will reflect the newest set of requested types.
  bool needs_reconfigure_ = false;

  // The last time Restart() was called.
  base::Time last_restart_time_;

  // For querying failed data types (having unrecoverable error) when
  // configuring backend.
  DataTypeStatusTable data_type_status_table_;

  // Types waiting to be configured, prioritized (highest priority first).
  base::queue<DataTypeSet> configuration_types_queue_;

  base::WeakPtrFactory<DataTypeManagerImpl> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_DATA_TYPE_MANAGER_IMPL_H_
