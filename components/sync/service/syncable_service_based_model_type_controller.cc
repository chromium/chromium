// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/syncable_service_based_model_type_controller.h"

#include <memory>
#include <utility>

#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/forwarding_model_type_controller_delegate.h"
#include "components/sync/model/syncable_service_based_bridge.h"

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
      : type_(type), dump_stack_(dump_stack) {
    DCHECK(store_factory);

    // The |syncable_service| can be null in some cases (e.g. when the
    // underlying service failed to initialize), and in tests.
    if (syncable_service) {
      bridge_ = std::make_unique<SyncableServiceBasedBridge>(
          type_, std::move(store_factory),
          std::make_unique<ClientTagBasedModelTypeProcessor>(type_,
                                                             dump_stack_),
          syncable_service.get());
    }
  }

  ControllerDelegate(const ControllerDelegate&) = delete;
  ControllerDelegate& operator=(const ControllerDelegate&) = delete;

  ~ControllerDelegate() override = default;

  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override {
    if (!bridge_) {
      // TODO(crbug.com/1394815): Consider running `callback` here to avoid
      // blocking the Sync machinery.
      return;
    }
    GetBridgeDelegate()->OnSyncStarting(request, std::move(callback));
  }

  void OnSyncStopping(SyncStopMetadataFate metadata_fate) override {
    if (!bridge_) {
      return;
    }
    GetBridgeDelegate()->OnSyncStopping(metadata_fate);
  }

  void GetAllNodesForDebugging(AllNodesCallback callback) override {
    if (!bridge_) {
      // TODO(crbug.com/1394815): Consider running `callback` here.
      return;
    }
    GetBridgeDelegate()->GetAllNodesForDebugging(std::move(callback));
  }

  void GetTypeEntitiesCountForDebugging(
      base::OnceCallback<void(const TypeEntitiesCount&)> callback)
      const override {
    if (!bridge_) {
      // TODO(crbug.com/1394815): Consider running `callback` here.
      return;
    }
    GetBridgeDelegate()->GetTypeEntitiesCountForDebugging(std::move(callback));
  }

  void RecordMemoryUsageAndCountsHistograms() override {
    if (!bridge_) {
      return;
    }
    GetBridgeDelegate()->RecordMemoryUsageAndCountsHistograms();
  }

  void ClearMetadataIfStopped() override {
    if (!bridge_) {
      return;
    }
    GetBridgeDelegate()->ClearMetadataIfStopped();
  }

 private:
  ModelTypeControllerDelegate* GetBridgeDelegate() const {
    DCHECK(bridge_);
    return bridge_->change_processor()->GetControllerDelegate().get();
  }

  const ModelType type_;
  const base::RepeatingClosure dump_stack_;
  std::unique_ptr<ModelTypeSyncBridge> bridge_;
};

}  // namespace

SyncableServiceBasedModelTypeController::
    SyncableServiceBasedModelTypeController(
        ModelType type,
        OnceModelTypeStoreFactory store_factory,
        base::WeakPtr<SyncableService> syncable_service,
        const base::RepeatingClosure& dump_stack,
        DelegateMode delegate_mode)
    : ModelTypeController(type),
      delegate_(std::make_unique<ControllerDelegate>(type,
                                                     std::move(store_factory),
                                                     syncable_service,
                                                     dump_stack)) {
  // Delegate for full sync is always created.
  auto full_sync_delegate =
      std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
          delegate_.get());

  // Delegate for transport is only created if requested and left null
  // otherwise.
  std::unique_ptr<syncer::ForwardingModelTypeControllerDelegate>
      transport_delegate;
  if (delegate_mode == DelegateMode::kTransportModeWithSingleModel) {
    transport_delegate =
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate_.get());
  }

  InitModelTypeController(std::move(full_sync_delegate),
                          std::move(transport_delegate));
}

SyncableServiceBasedModelTypeController::
    ~SyncableServiceBasedModelTypeController() = default;

}  // namespace syncer
