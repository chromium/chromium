// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/syncable_service_based_model_type_controller.h"

#include <utility>

#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/syncable_service_based_bridge.h"

namespace syncer {

namespace {

// Similar to ForwardingModelTypeControllerDelegate, but allows evaluating the
// reference to SyncableService in a lazy way, which is convenient for tests.
class ControllerDelegate : public ModelTypeControllerDelegate {
 public:
  using SyncableServiceProvider =
      SyncableServiceBasedModelTypeController::SyncableServiceProvider;

  ControllerDelegate(ModelType type,
                     OnceModelTypeStoreFactory store_factory,
                     SyncableServiceProvider syncable_service_provider,
                     const base::RepeatingClosure& dump_stack)
      : type_(type),
        store_factory_(std::move(store_factory)),
        syncable_service_provider_(std::move(syncable_service_provider)),
        dump_stack_(dump_stack) {
    DCHECK(store_factory_);
    DCHECK(syncable_service_provider_);
  }

  ~ControllerDelegate() override {}

  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override {
    BuildOrGetBridgeDelegate()->OnSyncStarting(request, std::move(callback));
  }

  void OnSyncStopping(SyncStopMetadataFate metadata_fate) override {
    BuildOrGetBridgeDelegate()->OnSyncStopping(metadata_fate);
  }

  void GetAllNodesForDebugging(AllNodesCallback callback) override {
    BuildOrGetBridgeDelegate()->GetAllNodesForDebugging(std::move(callback));
  }

  void GetStatusCountersForDebugging(StatusCountersCallback callback) override {
    BuildOrGetBridgeDelegate()->GetStatusCountersForDebugging(
        std::move(callback));
  }

  void RecordMemoryUsageAndCountsHistograms() override {
    BuildOrGetBridgeDelegate()->RecordMemoryUsageAndCountsHistograms();
  }

 private:
  ModelTypeControllerDelegate* BuildOrGetBridgeDelegate() {
    if (!bridge_) {
      base::WeakPtr<SyncableService> syncable_service =
          std::move(syncable_service_provider_).Run();
      DCHECK(syncable_service);
      bridge_ = std::make_unique<SyncableServiceBasedBridge>(
          type_, std::move(store_factory_),
          std::make_unique<ClientTagBasedModelTypeProcessor>(type_,
                                                             dump_stack_),
          syncable_service.get());
    }
    return bridge_->change_processor()->GetControllerDelegate().get();
  }

  const ModelType type_;
  OnceModelTypeStoreFactory store_factory_;
  SyncableServiceProvider syncable_service_provider_;
  const base::RepeatingClosure dump_stack_;
  std::unique_ptr<ModelTypeSyncBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(ControllerDelegate);
};

}  // namespace

SyncableServiceBasedModelTypeController::
    SyncableServiceBasedModelTypeController(
        ModelType type,
        OnceModelTypeStoreFactory store_factory,
        SyncableServiceProvider syncable_service_provider,
        const base::RepeatingClosure& dump_stack)
    : ModelTypeController(type,
                          std::make_unique<ControllerDelegate>(
                              type,
                              std::move(store_factory),
                              std::move(syncable_service_provider),
                              dump_stack)) {}

SyncableServiceBasedModelTypeController::
    ~SyncableServiceBasedModelTypeController() {}

}  // namespace syncer
