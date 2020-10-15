// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/driver/data_type_controller.h"

namespace invalidation {
class InvalidationService;
}  // namespace invalidation

namespace syncer {

class DataTypeDebugInfoListener;
class DataTypeEncryptionHandler;
class DataTypeManager;
class DataTypeManagerObserver;
class SyncEngine;
class SyncInvalidationsService;
class SyncPrefs;

// This factory provides sync driver code with the model type specific sync/api
// service (like SyncableService) implementations.
class SyncApiComponentFactory {
 public:
  virtual ~SyncApiComponentFactory() {}

  virtual std::unique_ptr<DataTypeManager> CreateDataTypeManager(
      ModelTypeSet initial_types,
      const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
      const DataTypeController::TypeMap* controllers,
      const DataTypeEncryptionHandler* encryption_handler,
      ModelTypeConfigurer* configurer,
      DataTypeManagerObserver* observer) = 0;

  // Creating this in the factory helps us mock it out in testing. |invalidator|
  // and |sync_invalidation_service| are different invalidations. SyncEngine
  // handles incoming invalidations from both of them (if provided).
  // |sync_invalidation_service| is a new sync-specific invalidations service
  // and it may be nullptr if it is disabled or not supported. In future, there
  // will leave only one invalidation service.
  virtual std::unique_ptr<SyncEngine> CreateSyncEngine(
      const std::string& name,
      invalidation::InvalidationService* invalidator,
      syncer::SyncInvalidationsService* sync_invalidation_service,
      const base::WeakPtr<SyncPrefs>& sync_prefs) = 0;

  // Deletes the directory database files from the sync data folder to cleanup
  // all files. The main purpose is to delete the legacy Directory files
  // (sqlite) but it also currently deletes the files corresponding to the
  // modern NigoriStorageImpl.
  // Upon calling this, the deletion is guaranteed to finish before a new engine
  // returned by |CreateSyncEngine()| can do any proper work.
  virtual void DeleteLegacyDirectoryFilesAndNigoriStorage() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_H_
