// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/syncable_service_based_data_type_controller.h"

#include <memory>
#include <utility>

#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/forwarding_data_type_controller_delegate.h"
#include "components/sync/model/syncable_service_based_bridge.h"

namespace syncer {

namespace {

// Similar to ForwardingDataTypeControllerDelegate, but allows evaluating the
// reference to SyncableService in a lazy way, which is convenient for tests.
class ControllerDelegate : public DataTypeControllerDelegate {
 public:
  ControllerDelegate(DataType type,
                     OnceDataTypeStoreFactory store_factory,
                     base::WeakPtr<SyncableService> syncable_service,
                     const base::RepeatingClosure& dump_stack)
      : type_(type), dump_stack_(dump_stack) {
    DCHECK(store_factory);

    // The |syncable_service| can be null in some cases (e.g. when the
    // underlying service failed to initialize), and in tests.
    if (syncable_service) {
      bridge_ = std::make_unique<SyncableServiceBasedBridge>(
          type_, std::move(store_factory),
          std::make_unique<ClientTagBasedDataTypeProcessor>(type_, dump_stack_),
          syncable_service.get());
    }
  }

  ControllerDelegate(const ControllerDelegate&) = delete;
  ControllerDelegate& operator=(const ControllerDelegate&) = delete;

  ~ControllerDelegate() override = default;

  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override {
    if (!bridge_) {
      // TODO(crbug.com/40248786): Consider running `callback` here to avoid
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

  void HasUnsyncedData(base::OnceCallback<void(bool)> callback) override {
    if (!bridge_) {
      std::move(callback).Run(false);
      return;
    }
    GetBridgeDelegate()->HasUnsyncedData(std::move(callback));
  }

  void GetAllNodesForDebugging(AllNodesCallback callback) override {
    if (!bridge_) {
      // TODO(crbug.com/40248786): Consider running `callback` here.
      return;
    }
    GetBridgeDelegate()->GetAllNodesForDebugging(std::move(callback));
  }

  void GetTypeEntitiesCountForDebugging(
      base::OnceCallback<void(const TypeEntitiesCount&)> callback)
      const override {
    if (!bridge_) {
      // TODO(crbug.com/40248786): Consider running `callback` here.
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

  void ReportBridgeErrorForTest() override {
    if (!bridge_) {
      return;
    }
    GetBridgeDelegate()->ReportBridgeErrorForTest();  // IN-TEST
  }

 private:
  DataTypeControllerDelegate* GetBridgeDelegate() const {
    DCHECK(bridge_);
    return bridge_->change_processor()->GetControllerDelegate().get();
  }

  const DataType type_;
  const base::RepeatingClosure dump_stack_;
  std::unique_ptr<DataTypeSyncBridge> bridge_;
};

}  // namespace

SyncableServiceBasedDataTypeController::SyncableServiceBasedDataTypeController(
    DataType type,
    OnceDataTypeStoreFactory store_factory,
    base::WeakPtr<SyncableService> syncable_service,
    const base::RepeatingClosure& dump_stack,
    DelegateMode delegate_mode)
    : DataTypeController(type),
      delegate_(std::make_unique<ControllerDelegate>(type,
                                                     std::move(store_factory),
                                                     syncable_service,
                                                     dump_stack)) {
  // Delegate for full sync is always created.
  auto full_sync_delegate =
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
          delegate_.get());

  // Delegate for transport is only created if requested and left null
  // otherwise.
  std::unique_ptr<syncer::ForwardingDataTypeControllerDelegate>
      transport_delegate;
  if (delegate_mode == DelegateMode::kTransportModeWithSingleModel) {
    transport_delegate =
        std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
            delegate_.get());
  }

  InitDataTypeController(std::move(full_sync_delegate),
                         std::move(transport_delegate));
}

SyncableServiceBasedDataTypeController::
    ~SyncableServiceBasedDataTypeController() {
  // The constructor passed a forwarding controller delegate referencing
  // `delegate_` to the base class. They are stored by the base class and need
  // to be destroyed before `delegate_` to avoid dangling pointers.
  ClearDelegateMap();
}

}  // namespace syncer
