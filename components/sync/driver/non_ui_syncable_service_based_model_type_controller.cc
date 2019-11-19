// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/non_ui_syncable_service_based_model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/proxy_model_type_controller_delegate.h"
#include "components/sync/model_impl/syncable_service_based_bridge.h"

namespace syncer {

namespace {

// Helper object that allows constructing and destructing the
// SyncableServiceBasedBridge on the model thread. Gets constructed on the UI
// thread, but all other operations including destruction happen on the model
// thread.
class BridgeBuilder {
 public:
  BridgeBuilder(
      ModelType type,
      OnceModelTypeStoreFactory store_factory,
      NonUiSyncableServiceBasedModelTypeController::SyncableServiceProvider
          syncable_service_provider,
      const base::RepeatingClosure& dump_stack,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : task_runner_(task_runner) {
    DCHECK(store_factory);
    DCHECK(syncable_service_provider);

    // Unretained is safe because destruction also happens on |task_runner_| and
    // can't overtake this task.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&BridgeBuilder::InitOnModelThread,
                       base::Unretained(this), type, std::move(store_factory),
                       std::move(syncable_service_provider), dump_stack));
  }

  ~BridgeBuilder() { DCHECK(task_runner_->RunsTasksInCurrentSequence()); }

  base::WeakPtr<ModelTypeControllerDelegate> GetBridgeDelegate() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(bridge_);
    return bridge_->change_processor()->GetControllerDelegate();
  }

 private:
  void InitOnModelThread(
      ModelType type,
      OnceModelTypeStoreFactory store_factory,
      NonUiSyncableServiceBasedModelTypeController::SyncableServiceProvider
          syncable_service_provider,
      const base::RepeatingClosure& dump_stack) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(!bridge_);

    base::WeakPtr<SyncableService> syncable_service =
        std::move(syncable_service_provider).Run();
    // |syncable_service| can be null in tests.
    if (syncable_service) {
      bridge_ = std::make_unique<SyncableServiceBasedBridge>(
          type, std::move(store_factory),
          std::make_unique<ClientTagBasedModelTypeProcessor>(type, dump_stack),
          syncable_service.get());
    }
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<ModelTypeSyncBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(BridgeBuilder);
};

// This is a slightly adapted version of base::OnTaskRunnerDeleter: The one
// difference is that if the destruction request already happens on the target
// sequence, then this avoids posting a task, and instead deletes the given
// object immediately. See https://crbug.com/970354#c19.
struct CustomOnTaskRunnerDeleter {
  explicit CustomOnTaskRunnerDeleter(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}
  ~CustomOnTaskRunnerDeleter() = default;

  CustomOnTaskRunnerDeleter(CustomOnTaskRunnerDeleter&&) = default;
  CustomOnTaskRunnerDeleter& operator=(CustomOnTaskRunnerDeleter&&) = default;

  // For compatibility with std:: deleters.
  template <typename T>
  void operator()(const T* ptr) {
    if (!ptr)
      return;

    if (task_runner_->RunsTasksInCurrentSequence()) {
      delete ptr;
    } else {
      task_runner_->DeleteSoon(FROM_HERE, ptr);
    }
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

ProxyModelTypeControllerDelegate::DelegateProvider BuildDelegateProvider(
    ModelType type,
    OnceModelTypeStoreFactory store_factory,
    NonUiSyncableServiceBasedModelTypeController::SyncableServiceProvider
        syncable_service_provider,
    const base::RepeatingClosure& dump_stack,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  // Can't use std::make_unique or base::WrapUnique because of custom deleter.
  auto bridge_builder =
      std::unique_ptr<BridgeBuilder, CustomOnTaskRunnerDeleter>(
          new BridgeBuilder(type, std::move(store_factory),
                            std::move(syncable_service_provider), dump_stack,
                            task_runner),
          CustomOnTaskRunnerDeleter(task_runner));
  return base::BindRepeating(&BridgeBuilder::GetBridgeDelegate,
                             std::move(bridge_builder));
}

}  // namespace

NonUiSyncableServiceBasedModelTypeController::
    NonUiSyncableServiceBasedModelTypeController(
        ModelType type,
        OnceModelTypeStoreFactory store_factory,
        SyncableServiceProvider syncable_service_provider,
        const base::RepeatingClosure& dump_stack,
        scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ModelTypeController(
          type,
          std::make_unique<ProxyModelTypeControllerDelegate>(
              task_runner,
              BuildDelegateProvider(type,
                                    std::move(store_factory),
                                    std::move(syncable_service_provider),
                                    dump_stack,
                                    task_runner))) {}

NonUiSyncableServiceBasedModelTypeController::
    ~NonUiSyncableServiceBasedModelTypeController() {}

}  // namespace syncer
