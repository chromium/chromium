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
  ControllerDelegate(ModelType type,
                     OnceModelTypeStoreFactory store_factory,
                     base::WeakPtr<SyncableService> syncable_service,
                     const base::RepeatingClosure& dump_stack)
      : type_(type),
        dump_stack_(dump_stack) {
    DCHECK(store_factory);

    // The |syncable_service| can be null in tests.
    if (syncable_service) {
      bridge_ = std::make_unique<SyncableServiceBasedBridge>(
          type_, std::move(store_factory),
          std::make_unique<ClientTagBasedModelTypeProcessor>(type_,
                                                             dump_stack_),
          syncable_service.get());
    }
  }

  ~ControllerDelegate() override {}

  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override {
    GetBridgeDelegate()->OnSyncStarting(request, std::move(callback));
  }

  void OnSyncStopping(SyncStopMetadataFate metadata_fate) override {
    GetBridgeDelegate()->OnSyncStopping(metadata_fate);
  }

  void GetAllNodesForDebugging(AllNodesCallback callback) override {
    GetBridgeDelegate()->GetAllNodesForDebugging(std::move(callback));
  }

  void GetStatusCountersForDebugging(StatusCountersCallback callback) override {
    GetBridgeDelegate()->GetStatusCountersForDebugging(std::move(callback));
  }

  void RecordMemoryUsageAndCountsHistograms() override {
    GetBridgeDelegate()->RecordMemoryUsageAndCountsHistograms();
  }

 private:
  ModelTypeControllerDelegate* GetBridgeDelegate() {
    DCHECK(bridge_);
    return bridge_->change_processor()->GetControllerDelegate().get();
  }

  const ModelType type_;
  const base::RepeatingClosure dump_stack_;
  std::unique_ptr<ModelTypeSyncBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(ControllerDelegate);
};

}  // namespace

SyncableServiceBasedModelTypeController::
    SyncableServiceBasedModelTypeController(
        ModelType type,
        OnceModelTypeStoreFactory store_factory,
        base::WeakPtr<SyncableService> syncable_service,
        const base::RepeatingClosure& dump_stack)
    : ModelTypeController(
          type,
          std::make_unique<ControllerDelegate>(type,
                                               std::move(store_factory),
                                               syncable_service,
                                               dump_stack)) {}

SyncableServiceBasedModelTypeController::
    ~SyncableServiceBasedModelTypeController() {}

}  // namespace syncer
