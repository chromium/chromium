// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/non_ui_syncable_service_based_model_type_controller.h"

#include <utility>

#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/proxy_model_type_controller_delegate.h"
#include "components/sync/model_impl/syncable_service_based_bridge.h"

namespace syncer {

namespace {

// Helper object that allows constructing and destructing the
// SyncableServiceBasedBridge lazily and on the model thread.
class LazyBridgeBuilder {
 public:
  LazyBridgeBuilder(
      ModelType type,
      OnceModelTypeStoreFactory store_factory,
      NonUiSyncableServiceBasedModelTypeController::SyncableServiceProvider
          syncable_service_provider,
      const base::RepeatingClosure& dump_stack,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<SyncableServiceBasedBridge::ModelCryptographer>
          cryptographer)
      : type_(type),
        cryptographer_(std::move(cryptographer)),
        store_factory_(std::move(store_factory)),
        syncable_service_provider_(std::move(syncable_service_provider)),
        dump_stack_(dump_stack),
        bridge_(nullptr, base::OnTaskRunnerDeleter(std::move(task_runner))) {
    DCHECK(store_factory_);
    DCHECK(syncable_service_provider_);
  }

  base::WeakPtr<ModelTypeControllerDelegate> BuildOrGetBridgeDelegate() {
    if (!bridge_) {
      base::WeakPtr<SyncableService> syncable_service =
          std::move(syncable_service_provider_).Run();
      DCHECK(syncable_service);
      // std::make_unique() avoided here due to custom deleter.
      bridge_.reset(new SyncableServiceBasedBridge(
          type_, std::move(store_factory_),
          std::make_unique<ClientTagBasedModelTypeProcessor>(type_,
                                                             dump_stack_),
          syncable_service.get(), cryptographer_));
    }
    return bridge_->change_processor()->GetControllerDelegate();
  }

 private:
  const ModelType type_;
  const scoped_refptr<SyncableServiceBasedBridge::ModelCryptographer>
      cryptographer_;
  OnceModelTypeStoreFactory store_factory_;
  NonUiSyncableServiceBasedModelTypeController::SyncableServiceProvider
      syncable_service_provider_;
  const base::RepeatingClosure dump_stack_;
  std::unique_ptr<ModelTypeSyncBridge, base::OnTaskRunnerDeleter> bridge_;

  DISALLOW_COPY_AND_ASSIGN(LazyBridgeBuilder);
};

}  // namespace

NonUiSyncableServiceBasedModelTypeController::
    NonUiSyncableServiceBasedModelTypeController(
        ModelType type,
        OnceModelTypeStoreFactory store_factory,
        SyncableServiceProvider syncable_service_provider,
        const base::RepeatingClosure& dump_stack,
        scoped_refptr<base::SequencedTaskRunner> task_runner,
        scoped_refptr<SyncableServiceBasedBridge::ModelCryptographer>
            cryptographer)
    : ModelTypeController(
          type,
          std::make_unique<ProxyModelTypeControllerDelegate>(
              task_runner,
              base::BindRepeating(&LazyBridgeBuilder::BuildOrGetBridgeDelegate,
                                  std::make_unique<LazyBridgeBuilder>(
                                      type,
                                      std::move(store_factory),
                                      std::move(syncable_service_provider),
                                      dump_stack,
                                      task_runner,
                                      std::move(cryptographer))))) {}

NonUiSyncableServiceBasedModelTypeController::
    ~NonUiSyncableServiceBasedModelTypeController() {}

}  // namespace syncer
