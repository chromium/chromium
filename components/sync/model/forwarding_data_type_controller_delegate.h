// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_FORWARDING_DATA_TYPE_CONTROLLER_DELEGATE_H_
#define COMPONENTS_SYNC_MODEL_FORWARDING_DATA_TYPE_CONTROLLER_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "components/sync/model/data_type_controller_delegate.h"

namespace syncer {

// Trivial implementation of DataTypeControllerDelegate that simply forwards
// call to another delegate (no task posting involved). This is useful when an
// API requires transferring ownership, but the calling site also wants to keep
// ownership of the actual implementation, and can guarantee the lifetime
// constraints.
class ForwardingDataTypeControllerDelegate : public DataTypeControllerDelegate {
 public:
  // Except for tests, `other` must not be null. Usually `other` will outlive
  // this object, but there are some cases where `other` may be destroyed just
  // before `this` is destroyed, which would cause a dangling pointer. To fix
  // this, `other_` is a weak pointer.
  explicit ForwardingDataTypeControllerDelegate(
      DataTypeControllerDelegate* other);

  ForwardingDataTypeControllerDelegate(
      const ForwardingDataTypeControllerDelegate&) = delete;
  ForwardingDataTypeControllerDelegate& operator=(
      const ForwardingDataTypeControllerDelegate&) = delete;

  ~ForwardingDataTypeControllerDelegate() override;

  // DataTypeControllerDelegate implementation.
  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override;
  void OnSyncStopping(SyncStopMetadataFate metadata_fate) override;
  void GetUnsyncedDataCount(base::OnceCallback<void(size_t)> callback) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void GetTypeEntitiesCountForDebugging(
      base::OnceCallback<void(const TypeEntitiesCount&)> callback)
      const override;
  void RecordMemoryUsageAndCountsHistograms() override;
  void ClearMetadataIfStopped() override;
  void ReportBridgeErrorForTest() override;

 private:
  // The delegate where calls are forwarded to. Should outlive this object
  // though it may be destroyed just before `this` is destroyed.
  const base::WeakPtr<DataTypeControllerDelegate> other_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_FORWARDING_DATA_TYPE_CONTROLLER_DELEGATE_H_
