// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_FAKE_SYNC_API_COMPONENT_FACTORY_H_
#define COMPONENTS_SYNC_DRIVER_FAKE_SYNC_API_COMPONENT_FACTORY_H_

#include <memory>
#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/data_type_manager.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/engine/sync_engine.h"

namespace syncer {

class DataTypeEncryptionHandler;
class DataTypeManagerImpl;
class FakeSyncEngine;

class FakeSyncApiComponentFactory : public SyncApiComponentFactory {
 public:
  FakeSyncApiComponentFactory();
  ~FakeSyncApiComponentFactory() override;

  // Enables or disables FakeSyncEngine's synchronous completion of
  // Initialize(). Defaults to true.
  void AllowFakeEngineInitCompletion(bool allow);

  DataTypeManagerImpl* last_created_data_type_manager() {
    return last_created_data_type_manager_.get();
  }
  FakeSyncEngine* last_created_engine() { return last_created_engine_.get(); }

  // SyncApiComponentFactory overrides.
  std::unique_ptr<DataTypeManager> CreateDataTypeManager(
      const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
      const DataTypeController::TypeMap* controllers,
      const DataTypeEncryptionHandler* encryption_handler,
      ModelTypeConfigurer* configurer,
      DataTypeManagerObserver* observer) override;
  std::unique_ptr<SyncEngine> CreateSyncEngine(
      const std::string& name,
      invalidation::InvalidationService* invalidator,
      syncer::SyncInvalidationsService* sync_invalidations_service,
      const base::WeakPtr<SyncTransportDataPrefs>& sync_prefs) override;
  void DeleteLegacyDirectoryFilesAndNigoriStorage() override;

 private:
  base::WeakPtr<DataTypeManagerImpl> last_created_data_type_manager_;
  base::WeakPtr<FakeSyncEngine> last_created_engine_;
  bool allow_fake_engine_init_completion_ = true;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_FAKE_SYNC_API_COMPONENT_FACTORY_H_
