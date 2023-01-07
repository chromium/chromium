// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_api_component_factory.h"

#include "base/test/bind.h"
#include "components/sync/driver/data_type_manager_impl.h"
#include "components/sync/test/fake_sync_engine.h"

namespace syncer {

namespace {

// Subclass of DataTypeManagerImpl to support weak pointers.
class TestDataTypeManagerImpl
    : public DataTypeManagerImpl,
      public base::SupportsWeakPtr<TestDataTypeManagerImpl> {
 public:
  using DataTypeManagerImpl::DataTypeManagerImpl;
};

}  // namespace

FakeSyncApiComponentFactory::FakeSyncApiComponentFactory() = default;

FakeSyncApiComponentFactory::~FakeSyncApiComponentFactory() = default;

void FakeSyncApiComponentFactory::AllowFakeEngineInitCompletion(bool allow) {
  allow_fake_engine_init_completion_ = allow;
}

std::unique_ptr<DataTypeManager>
FakeSyncApiComponentFactory::CreateDataTypeManager(
    const DataTypeController::TypeMap* controllers,
    const DataTypeEncryptionHandler* encryption_handler,
    ModelTypeConfigurer* configurer,
    DataTypeManagerObserver* observer) {
  auto data_type_manager = std::make_unique<TestDataTypeManagerImpl>(
      controllers, encryption_handler, configurer, observer);
  last_created_data_type_manager_ = data_type_manager->AsWeakPtr();
  return data_type_manager;
}

std::unique_ptr<SyncEngine> FakeSyncApiComponentFactory::CreateSyncEngine(
    const std::string& name,
    invalidation::InvalidationService* invalidator,
    syncer::SyncInvalidationsService* sync_invalidations_service) {
  auto engine = std::make_unique<FakeSyncEngine>(
      allow_fake_engine_init_completion_,
      /*is_first_time_sync_configure=*/!is_first_time_sync_configure_done_,
      /*sync_transport_data_cleared_cb=*/base::BindLambdaForTesting([this]() {
        ++clear_transport_data_call_count_;
      }));
  last_created_engine_ = engine->AsWeakPtr();
  return engine;
}

void FakeSyncApiComponentFactory::ClearAllTransportData() {
  ++clear_transport_data_call_count_;
}

}  // namespace syncer
