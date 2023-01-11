// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_PROXY_MODEL_TYPE_CONTROLLER_DELEGATE_H_
#define COMPONENTS_SYNC_MODEL_PROXY_MODEL_TYPE_CONTROLLER_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace syncer {

// Implementation of ModelTypeControllerDelegate that simply delegates the work
// further to |other|, which lives in a difference thread/sequence. This means
// all methods are implemented via posting tasks to the destination sequence, as
// provided in the constructor via |task_runner|.
class ProxyModelTypeControllerDelegate : public ModelTypeControllerDelegate {
 public:
  using DelegateProvider =
      base::RepeatingCallback<base::WeakPtr<ModelTypeControllerDelegate>()>;
  // |delegate_provider| will be run lazily *AND* in |task_runner|.
  ProxyModelTypeControllerDelegate(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const DelegateProvider& delegate_provider);

  ProxyModelTypeControllerDelegate(const ProxyModelTypeControllerDelegate&) =
      delete;
  ProxyModelTypeControllerDelegate& operator=(
      const ProxyModelTypeControllerDelegate&) = delete;

  ~ProxyModelTypeControllerDelegate() override;

  // ModelTypeControllerDelegate implementation.
  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override;
  void OnSyncStopping(SyncStopMetadataFate metadata_fate) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void GetTypeEntitiesCountForDebugging(
      base::OnceCallback<void(const TypeEntitiesCount&)> callback)
      const override;
  void RecordMemoryUsageAndCountsHistograms() override;

 private:
  // Post the given task (that requires the destination delegate to run) to
  // |task_runner_|.
  void PostTask(
      const base::Location& location,
      base::OnceCallback<void(base::WeakPtr<ModelTypeControllerDelegate>)> task)
      const;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const DelegateProvider delegate_provider_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_PROXY_MODEL_TYPE_CONTROLLER_DELEGATE_H_
