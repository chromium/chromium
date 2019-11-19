// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_H__
#define COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_H__

#include <list>
#include <set>
#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/data_type_status_table.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/shutdown_reason.h"
#include "components/sync/model/sync_error.h"

namespace syncer {

struct ConfigureContext;

// This interface is for managing the start up and shut down life cycle
// of many different syncable data types.
class DataTypeManager {
 public:
  enum State {
    STOPPED,      // No data types are currently running.
    CONFIGURING,  // Data types are being started.
    RETRYING,     // Retrying a pending reconfiguration.

    CONFIGURED,  // All enabled data types are running.
    STOPPING     // Data types are being stopped.
  };

  // Update NotifyDone() in data_type_manager_impl.cc if you update
  // this.
  enum ConfigureStatus {
    UNKNOWN = -1,
    OK,                  // Configuration finished some or all types.
    ABORTED,             // Start was aborted by calling Stop() before
                         // all types were started.
    UNRECOVERABLE_ERROR  // We got an unrecoverable error during startup.
  };

  // Note: |errors| is only filled when status is not OK.
  struct ConfigureResult {
    ConfigureResult();
    ConfigureResult(ConfigureStatus status, ModelTypeSet requested_types);
    ConfigureResult(const ConfigureResult& other);
    ~ConfigureResult();

    ConfigureStatus status;
    ModelTypeSet requested_types;
    DataTypeStatusTable data_type_status_table;
  };

  virtual ~DataTypeManager() {}

  // Convert a ConfigureStatus to string for debug purposes.
  static std::string ConfigureStatusToString(ConfigureStatus status);

  // Begins asynchronous configuration of data types.  Any currently
  // running data types that are not in the desired_types set will be
  // stopped.  Any stopped data types that are in the desired_types
  // set will be started.  All other data types are left in their
  // current state.
  //
  // Note that you may call Configure() while configuration is in
  // progress.  Configuration will be complete only when the
  // desired_types supplied in the last call to Configure is achieved.
  virtual void Configure(ModelTypeSet desired_types,
                         const ConfigureContext& context) = 0;

  // Informs the data type manager that the ready-for-start status of a
  // controller has changed. If the controller is not ready any more, it will
  // stop |type|. Otherwise, it will trigger reconfiguration so that |type| gets
  // started again. No-op if the type's state didn't actually change.
  virtual void DataTypePreconditionChanged(ModelType type) = 0;

  // Resets all data type error state.
  virtual void ResetDataTypeErrors() = 0;

  virtual void PurgeForMigration(ModelTypeSet undesired_types) = 0;

  // Synchronously stops all registered data types. If called after
  // Configure() is called but before it finishes, it will abort the
  // configure and any data types that have been started will be
  // stopped.
  // If called with reason |DISABLE_SYNC|, purges sync data for all datatypes.
  virtual void Stop(ShutdownReason reason) = 0;

  // Get the set of current active data types (those chosen or configured by the
  // user which have not also encountered a runtime error). Note that during
  // configuration, this will the the empty set. Once the configuration
  // completes the set will be updated.
  virtual ModelTypeSet GetActiveDataTypes() const = 0;

  // Whether or not we are syncing encryption keys.
  virtual bool IsNigoriEnabled() const = 0;

  // The current state of the data type manager.
  virtual State state() const = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_H__
