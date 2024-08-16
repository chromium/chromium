// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/proxy_data_type_controller_delegate.h"

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
    DataTypeControllerDelegate::StartCallback callback_bound_to_ui_thread,
    base::WeakPtr<DataTypeControllerDelegate> delegate) {
  DCHECK(delegate);
  delegate->OnSyncStarting(request, std::move(callback_bound_to_ui_thread));
}

void HasUnsyncedDataHelperOnModelThread(
    base::OnceCallback<void(bool)> callback_bound_to_ui_thread,
    base::WeakPtr<DataTypeControllerDelegate> delegate) {
  DCHECK(delegate);
  delegate->HasUnsyncedData(std::move(callback_bound_to_ui_thread));
}

void GetAllNodesForDebuggingHelperOnModelThread(
    ProxyDataTypeControllerDelegate::AllNodesCallback
        callback_bound_to_ui_thread,
    base::WeakPtr<DataTypeControllerDelegate> delegate) {
  DCHECK(delegate);
  delegate->GetAllNodesForDebugging(std::move(callback_bound_to_ui_thread));
}

void GetTypeEntitiesCountForDebuggingHelperOnModelThread(
    base::OnceCallback<void(const TypeEntitiesCount&)>
        callback_bound_to_ui_thread,
    base::WeakPtr<DataTypeControllerDelegate> delegate) {
  DCHECK(delegate);
  delegate->GetTypeEntitiesCountForDebugging(
      std::move(callback_bound_to_ui_thread));
}

void StopSyncHelperOnModelThread(
    SyncStopMetadataFate metadata_fate,
    base::WeakPtr<DataTypeControllerDelegate> delegate) {
  DCHECK(delegate);
  delegate->OnSyncStopping(metadata_fate);
}

void RecordMemoryUsageAndCountsHistogramsHelperOnModelThread(
    base::WeakPtr<DataTypeControllerDelegate> delegate) {
  DCHECK(delegate);
  delegate->RecordMemoryUsageAndCountsHistograms();
}

void ClearMetadataIfStoppedHelperOnModelThread(
    base::WeakPtr<DataTypeControllerDelegate> delegate) {
  DCHECK(delegate);
  delegate->ClearMetadataIfStopped();
}

void ReportBridgeErrorOnModelThreadForTest(  // IN-TEST
    base::WeakPtr<DataTypeControllerDelegate> delegate) {
  CHECK(delegate);
  delegate->ReportBridgeErrorForTest();  // IN-TEST
}

// Rurns some task on the destination task runner (backend sequence), first
// exercising |delegate_provider| *also* in the backend sequence.
void RunModelTask(
    const ProxyDataTypeControllerDelegate::DelegateProvider& delegate_provider,
    base::OnceCallback<void(base::WeakPtr<DataTypeControllerDelegate>)> task) {
  base::WeakPtr<DataTypeControllerDelegate> delegate = delegate_provider.Run();
  // TODO(mastiz): Migrate away from weak pointers, since there is no actual
  // need, provided that KeyedServices have proper dependencies.
  // TODO(crbug.com/41496574): switch to CHECK once all data types provide
  // non-null delegates.
  if (delegate) {
    std::move(task).Run(delegate);
  }
}

}  // namespace

ProxyDataTypeControllerDelegate::ProxyDataTypeControllerDelegate(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const DelegateProvider& delegate_provider)
    : task_runner_(task_runner), delegate_provider_(delegate_provider) {
  DCHECK(task_runner_);
}

ProxyDataTypeControllerDelegate::~ProxyDataTypeControllerDelegate() = default;

void ProxyDataTypeControllerDelegate::OnSyncStarting(
    const DataTypeActivationRequest& request,
    StartCallback callback) {
  PostTask(
      FROM_HERE,
      base::BindOnce(&OnSyncStartingHelperOnModelThread, request,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void ProxyDataTypeControllerDelegate::OnSyncStopping(
    SyncStopMetadataFate metadata_fate) {
  PostTask(FROM_HERE,
           base::BindOnce(&StopSyncHelperOnModelThread, metadata_fate));
}

void ProxyDataTypeControllerDelegate::HasUnsyncedData(
    base::OnceCallback<void(bool)> callback) {
  PostTask(
      FROM_HERE,
      base::BindOnce(&HasUnsyncedDataHelperOnModelThread,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void ProxyDataTypeControllerDelegate::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  PostTask(
      FROM_HERE,
      base::BindOnce(&GetAllNodesForDebuggingHelperOnModelThread,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void ProxyDataTypeControllerDelegate::GetTypeEntitiesCountForDebugging(
    base::OnceCallback<void(const TypeEntitiesCount&)> callback) const {
  PostTask(
      FROM_HERE,
      base::BindOnce(&GetTypeEntitiesCountForDebuggingHelperOnModelThread,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void ProxyDataTypeControllerDelegate::RecordMemoryUsageAndCountsHistograms() {
  PostTask(
      FROM_HERE,
      base::BindOnce(&RecordMemoryUsageAndCountsHistogramsHelperOnModelThread));
}

void ProxyDataTypeControllerDelegate::ClearMetadataIfStopped() {
  PostTask(FROM_HERE,
           base::BindOnce(&ClearMetadataIfStoppedHelperOnModelThread));
}

void ProxyDataTypeControllerDelegate::ReportBridgeErrorForTest() {
  PostTask(FROM_HERE,
           base::BindOnce(&ReportBridgeErrorOnModelThreadForTest));  // IN-TEST
}

void ProxyDataTypeControllerDelegate::PostTask(
    const base::Location& location,
    base::OnceCallback<void(base::WeakPtr<DataTypeControllerDelegate>)> task)
    const {
  task_runner_->PostTask(
      location,
      base::BindOnce(&RunModelTask, delegate_provider_, std::move(task)));
}

}  // namespace syncer
