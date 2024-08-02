// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_PROXY_DATA_TYPE_CONTROLLER_DELEGATE_H_
#define COMPONENTS_SYNC_MODEL_PROXY_DATA_TYPE_CONTROLLER_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/model/data_type_controller_delegate.h"

namespace syncer {

// Implementation of DataTypeControllerDelegate that simply delegates the work
// further to |other|, which lives in a difference thread/sequence. This means
// all methods are implemented via posting tasks to the destination sequence, as
// provided in the constructor via |task_runner|.
// Instantiations of this typically live on the UI thread, for use by the
// DataTypeController, and forward calls to the real implementation on the
// model sequence.
class ProxyDataTypeControllerDelegate : public DataTypeControllerDelegate {
 public:
  using DelegateProvider =
      base::RepeatingCallback<base::WeakPtr<DataTypeControllerDelegate>()>;
  // |delegate_provider| will be run lazily *AND* in |task_runner|.
  ProxyDataTypeControllerDelegate(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const DelegateProvider& delegate_provider);

  ProxyDataTypeControllerDelegate(const ProxyDataTypeControllerDelegate&) =
      delete;
  ProxyDataTypeControllerDelegate& operator=(
      const ProxyDataTypeControllerDelegate&) = delete;

  ~ProxyDataTypeControllerDelegate() override;

  // DataTypeControllerDelegate implementation.
  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override;
  void OnSyncStopping(SyncStopMetadataFate metadata_fate) override;
  void HasUnsyncedData(base::OnceCallback<void(bool)> callback) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void GetTypeEntitiesCountForDebugging(
      base::OnceCallback<void(const TypeEntitiesCount&)> callback)
      const override;
  void RecordMemoryUsageAndCountsHistograms() override;
  void ClearMetadataIfStopped() override;
  void ReportBridgeErrorForTest() override;

 private:
  // Post the given task (that requires the destination delegate to run) to
  // |task_runner_|.
  void PostTask(
      const base::Location& location,
      base::OnceCallback<void(base::WeakPtr<DataTypeControllerDelegate>)> task)
      const;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const DelegateProvider delegate_provider_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_PROXY_DATA_TYPE_CONTROLLER_DELEGATE_H_
