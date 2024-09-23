// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_DATA_TYPE_MANAGER_H_
#define COMPONENTS_SYNC_SERVICE_DATA_TYPE_MANAGER_H_

#include <set>
#include <string>

#include "components/sync/base/data_type.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/data_type_status_table.h"

namespace syncer {

struct ConfigureContext;
class DataTypeConfigurer;

// This interface is for managing the start up and shut down life cycle
// of many different syncable data types.
// Lives on the UI thread.
class DataTypeManager {
 public:
  enum State {
    STOPPED,      // No data types are currently running.
    CONFIGURING,  // Data types are being started.
    RETRYING,     // Retrying a pending reconfiguration.

    CONFIGURED,  // All enabled data types are running.
    STOPPING     // Data types are being stopped.
  };

  enum ConfigureStatus {
    OK,       // Configuration finished some or all types.
    ABORTED,  // Configuration was aborted by calling Stop() before
              // all types were started.
  };

  struct ConfigureResult {
    ConfigureStatus status = ABORTED;
    DataTypeSet requested_types;
    DataTypeStatusTable data_type_status_table;
  };

  virtual ~DataTypeManager() = default;

  // Converts a ConfigureStatus to string for debug purposes.
  static std::string ConfigureStatusToString(ConfigureStatus status);

  // Clears metadata for all registered datatypes except for `types`. This
  // allows clearing metadata for types disabled in previous run early-on during
  // initialization. Must only be called while the state is STOPPED.
  virtual void ClearMetadataWhileStoppedExceptFor(DataTypeSet types) = 0;

  // Sets or clears the configurer (aka the SyncEngine) to use for
  // connecting/disconnecting and configuring the data types. Must only be
  // called while the state is STOPPED.
  virtual void SetConfigurer(DataTypeConfigurer* configurer) = 0;

  // Begins asynchronous configuration of data types. Any currently running data
  // types that are not in the `preferred_types` set will be stopped. Any
  // stopped data types that are in the `preferred_types` set will be started.
  // All other data types are left in their current state.
  //
  // Note that you may call Configure() while configuration is in progress.
  // Configuration will be complete only when the `preferred_types` supplied in
  // the last call to Configure() are achieved.
  //
  // SetConfigurer() must be called before this (with a non-null configurer).
  virtual void Configure(DataTypeSet preferred_types,
                         const ConfigureContext& context) = 0;

  // Informs the data type manager that the ready-for-start status of a
  // controller has changed. If the controller is not ready any more, it will
  // stop |type|. Otherwise, it will trigger reconfiguration so that |type| gets
  // started again. No-op if the type's state didn't actually change.
  virtual void DataTypePreconditionChanged(DataType type) = 0;

  // Resets all data type error state.
  virtual void ResetDataTypeErrors() = 0;

  virtual void PurgeForMigration(DataTypeSet undesired_types) = 0;

  // Synchronously stops all registered data types. If called after Configure()
  // is called but before it finishes, it will abort the configure and any data
  // types that have been started will be stopped. If called with metadata fate
  // |CLEAR_METADATA|, clears sync data for all datatypes.
  virtual void Stop(SyncStopMetadataFate metadata_fate) = 0;

  // Returns the set of data types that are supported in principle, possibly
  // influenced by command-line options.
  virtual DataTypeSet GetRegisteredDataTypes() const = 0;

  // Returns the DataTypes allowed in transport-only mode (i.e. those that are
  // not tied to sync-the-feature).
  virtual DataTypeSet GetDataTypesForTransportOnlyMode() const = 0;

  // Get the set of current active data types (those chosen or configured by the
  // user which have not also encountered a runtime error). Note that during
  // configuration, this will the the empty set. Once the configuration
  // completes the set will be updated.
  virtual DataTypeSet GetActiveDataTypes() const = 0;

  // Returns the datatypes that are stopped that are known to have cleared their
  // local sync metadata.
  virtual DataTypeSet GetPurgedDataTypes() const = 0;

  // Returns the datatypes that are configured but not connected to the sync
  // engine. Note that during configuration, this will be empty.
  virtual DataTypeSet GetActiveProxyDataTypes() const = 0;

  // Returns the datatypes that are about to become active, but are currently
  // in the process of downloading the initial data from the server (either
  // actively ongoing or queued).
  virtual DataTypeSet GetTypesWithPendingDownloadForInitialSync() const = 0;

  // Returns the datatypes with datatype errors (e.g. errors while loading from
  // the disk).
  virtual DataTypeSet GetDataTypesWithPermanentErrors() const = 0;

  // The current state of the data type manager.
  virtual State state() const = 0;

  // Exposes direct access to underlying controllers. Avoid using if possible,
  // as DataTypeManager usually offers higher-level APIs.
  // TODO(crbug.com/40901755): Remove this getter.
  virtual const DataTypeController::TypeMap& GetControllerMap() const = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_DATA_TYPE_MANAGER_H_
