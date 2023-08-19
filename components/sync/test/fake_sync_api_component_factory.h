// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_API_COMPONENT_FACTORY_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_API_COMPONENT_FACTORY_H_

#include <memory>
#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/data_type_manager.h"
#include "components/sync/service/sync_api_component_factory.h"

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

  // Returns the number of times transport data was cleared, which includes
  // ClearAllTransportData() being invoked as well as SyncEngine::Shutdown()
  // being invoked with DISABLE_SYNC.
  int clear_transport_data_call_count() const {
    return clear_transport_data_call_count_;
  }

  // Determines whether future initialization of FakeSyncEngine will report
  // being an initial sync.
  void set_first_time_sync_configure_done(bool done) {
    is_first_time_sync_configure_done_ = done;
  }

  // SyncApiComponentFactory overrides.
  std::unique_ptr<DataTypeManager> CreateDataTypeManager(
      const DataTypeController::TypeMap* controllers,
      const DataTypeEncryptionHandler* encryption_handler,
      ModelTypeConfigurer* configurer,
      DataTypeManagerObserver* observer) override;
  std::unique_ptr<SyncEngine> CreateSyncEngine(
      const std::string& name,
      syncer::SyncInvalidationsService* sync_invalidations_service) override;
  void ClearAllTransportData() override;

 private:
  base::WeakPtr<DataTypeManagerImpl> last_created_data_type_manager_;
  base::WeakPtr<FakeSyncEngine> last_created_engine_;
  bool allow_fake_engine_init_completion_ = true;
  bool is_first_time_sync_configure_done_ = false;
  int clear_transport_data_call_count_ = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_API_COMPONENT_FACTORY_H_
