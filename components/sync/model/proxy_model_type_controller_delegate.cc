// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/proxy_model_type_controller_delegate.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/type_entities_count.h"

namespace syncer {
namespace {

void OnSyncStartingHelperOnModelThread(
    const DataTypeActivationRequest& request,
    ModelTypeControllerDelegate::StartCallback callback_bound_to_ui_thread,
    base::WeakPtr<ModelTypeControllerDelegate> delegate) {
  DCHECK(delegate);
  delegate->OnSyncStarting(request, std::move(callback_bound_to_ui_thread));
}

void GetAllNodesForDebuggingHelperOnModelThread(
    ProxyModelTypeControllerDelegate::AllNodesCallback
        callback_bound_to_ui_thread,
    base::WeakPtr<ModelTypeControllerDelegate> delegate) {
  DCHECK(delegate);
  delegate->GetAllNodesForDebugging(std::move(callback_bound_to_ui_thread));
}

void GetTypeEntitiesCountForDebuggingHelperOnModelThread(
    base::OnceCallback<void(const TypeEntitiesCount&)>
        callback_bound_to_ui_thread,
    base::WeakPtr<ModelTypeControllerDelegate> delegate) {
  DCHECK(delegate);
  delegate->GetTypeEntitiesCountForDebugging(
      std::move(callback_bound_to_ui_thread));
}

void StopSyncHelperOnModelThread(
    SyncStopMetadataFate metadata_fate,
    base::WeakPtr<ModelTypeControllerDelegate> delegate) {
  DCHECK(delegate);
  delegate->OnSyncStopping(metadata_fate);
}

void RecordMemoryUsageAndCountsHistogramsHelperOnModelThread(
    base::WeakPtr<ModelTypeControllerDelegate> delegate) {
  DCHECK(delegate);
  delegate->RecordMemoryUsageAndCountsHistograms();
}

void ClearMetadataIfStoppedHelperOnModelThread(
    base::WeakPtr<ModelTypeControllerDelegate> delegate) {
  DCHECK(delegate);
  delegate->ClearMetadataIfStopped();
}

// Rurns some task on the destination task runner (backend sequence), first
// exercising |delegate_provider| *also* in the backend sequence.
void RunModelTask(
    const ProxyModelTypeControllerDelegate::DelegateProvider& delegate_provider,
    base::OnceCallback<void(base::WeakPtr<ModelTypeControllerDelegate>)> task) {
  base::WeakPtr<ModelTypeControllerDelegate> delegate = delegate_provider.Run();
  // TODO(mastiz): Migrate away from weak pointers, since there is no actual
  // need, provided that KeyedServices have proper dependencies.
  DCHECK(delegate);
  std::move(task).Run(delegate);
}

}  // namespace

ProxyModelTypeControllerDelegate::ProxyModelTypeControllerDelegate(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const DelegateProvider& delegate_provider)
    : task_runner_(task_runner), delegate_provider_(delegate_provider) {
  DCHECK(task_runner_);
}

ProxyModelTypeControllerDelegate::~ProxyModelTypeControllerDelegate() = default;

void ProxyModelTypeControllerDelegate::OnSyncStarting(
    const DataTypeActivationRequest& request,
    StartCallback callback) {
  PostTask(
      FROM_HERE,
      base::BindOnce(&OnSyncStartingHelperOnModelThread, request,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void ProxyModelTypeControllerDelegate::OnSyncStopping(
    SyncStopMetadataFate metadata_fate) {
  PostTask(FROM_HERE,
           base::BindOnce(&StopSyncHelperOnModelThread, metadata_fate));
}

void ProxyModelTypeControllerDelegate::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  PostTask(
      FROM_HERE,
      base::BindOnce(&GetAllNodesForDebuggingHelperOnModelThread,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void ProxyModelTypeControllerDelegate::GetTypeEntitiesCountForDebugging(
    base::OnceCallback<void(const TypeEntitiesCount&)> callback) const {
  PostTask(
      FROM_HERE,
      base::BindOnce(&GetTypeEntitiesCountForDebuggingHelperOnModelThread,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void ProxyModelTypeControllerDelegate::RecordMemoryUsageAndCountsHistograms() {
  PostTask(
      FROM_HERE,
      base::BindOnce(&RecordMemoryUsageAndCountsHistogramsHelperOnModelThread));
}

void ProxyModelTypeControllerDelegate::ClearMetadataIfStopped() {
  PostTask(FROM_HERE,
           base::BindOnce(&ClearMetadataIfStoppedHelperOnModelThread));
}

void ProxyModelTypeControllerDelegate::PostTask(
    const base::Location& location,
    base::OnceCallback<void(base::WeakPtr<ModelTypeControllerDelegate>)> task)
    const {
  task_runner_->PostTask(
      location,
      base::BindOnce(&RunModelTask, delegate_provider_, std::move(task)));
}

}  // namespace syncer
