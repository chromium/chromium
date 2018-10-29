// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/model/data_type_error_handler.h"
#include "components/sync/model/syncable_service.h"

namespace base {
class FilePath;
}  // namespace base

namespace invalidation {
class InvalidationService;
}  // namespace invalidation

namespace syncer {

class AssociatorInterface;
class ChangeProcessor;
class DataTypeDebugInfoListener;
class DataTypeEncryptionHandler;
class DataTypeManager;
class DataTypeManagerObserver;
class LocalDeviceInfoProvider;
class SyncEngine;
class SyncPrefs;
class SyncableService;

// This factory provides sync driver code with the model type specific sync/api
// service (like SyncableService) implementations.
class SyncApiComponentFactory {
 public:
  virtual ~SyncApiComponentFactory() {}

  // The various factory methods for the data type model associators
  // and change processors all return this struct.  This is needed
  // because the change processors typically require a type-specific
  // model associator at construction time.
  //
  // Note: This interface is deprecated in favor of the SyncableService API.
  // New datatypes that do not live on the UI thread should directly return a
  // weak pointer to a SyncableService. All others continue to return
  // SyncComponents. It is safe to assume that the factory methods below are
  // called on the same thread in which the datatype resides.
  struct SyncComponents {
    SyncComponents();
    SyncComponents(SyncComponents&&);
    ~SyncComponents();

    std::unique_ptr<AssociatorInterface> model_associator;
    std::unique_ptr<ChangeProcessor> change_processor;
  };

  // Creates and returns enabled datatypes and their controllers.
  // |disabled_types| allows callers to prevent certain types from being
  // created (e.g. to honor command-line flags).
  // TODO(crbug.com/895455): Remove |local_device_info_provider| once the
  // migration to USS is completed.
  virtual DataTypeController::TypeVector CreateCommonDataTypeControllers(
      ModelTypeSet disabled_types,
      LocalDeviceInfoProvider* local_device_info_provider) = 0;

  virtual std::unique_ptr<DataTypeManager> CreateDataTypeManager(
      ModelTypeSet initial_types,
      const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
      const DataTypeController::TypeMap* controllers,
      const DataTypeEncryptionHandler* encryption_handler,
      ModelTypeConfigurer* configurer,
      DataTypeManagerObserver* observer) = 0;

  // Creating this in the factory helps us mock it out in testing.
  virtual std::unique_ptr<SyncEngine> CreateSyncEngine(
      const std::string& name,
      invalidation::InvalidationService* invalidator,
      const base::WeakPtr<SyncPrefs>& sync_prefs,
      const base::FilePath& sync_folder) = 0;

  // Creating this in the factory helps us mock it out in testing.
  virtual std::unique_ptr<LocalDeviceInfoProvider>
  CreateLocalDeviceInfoProvider() = 0;

  // Legacy datatypes that need to be converted to the SyncableService API.
  virtual SyncComponents CreateBookmarkSyncComponents(
      std::unique_ptr<DataTypeErrorHandler> error_handler) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_H_
