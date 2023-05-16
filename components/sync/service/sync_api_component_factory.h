// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_API_COMPONENT_FACTORY_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_API_COMPONENT_FACTORY_H_

#include <memory>
#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/service/data_type_controller.h"

namespace invalidation {
class InvalidationService;
}  // namespace invalidation

namespace syncer {

class DataTypeEncryptionHandler;
class DataTypeManager;
class DataTypeManagerObserver;
class ModelTypeConfigurer;
class SyncEngine;
class SyncInvalidationsService;

// This factory provides sync service code with the model type specific sync/api
// service (like SyncableService) implementations.
class SyncApiComponentFactory {
 public:
  virtual ~SyncApiComponentFactory() = default;

  virtual std::unique_ptr<DataTypeManager> CreateDataTypeManager(
      const DataTypeController::TypeMap* controllers,
      const DataTypeEncryptionHandler* encryption_handler,
      ModelTypeConfigurer* configurer,
      DataTypeManagerObserver* observer) = 0;

  // Creating this in the factory helps us mock it out in testing. |invalidator|
  // and |sync_invalidation_service| are different invalidations. SyncEngine
  // handles incoming invalidations from both of them (if provided).
  // |sync_invalidation_service| is a new sync-specific invalidations service
  // and must not be null. In future, there will be only one invalidation
  // service.
  virtual std::unique_ptr<SyncEngine> CreateSyncEngine(
      const std::string& name,
      invalidation::InvalidationService* invalidator,
      SyncInvalidationsService* sync_invalidation_service) = 0;

  // Clears all local transport data. Upon calling this, the deletion is
  // guaranteed to finish before a new engine returned by |CreateSyncEngine()|
  // can do any proper work.
  virtual void ClearAllTransportData() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_API_COMPONENT_FACTORY_H_
