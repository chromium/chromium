// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_FORWARDING_MODEL_TYPE_CONTROLLER_DELEGATE_H_
#define COMPONENTS_SYNC_MODEL_IMPL_FORWARDING_MODEL_TYPE_CONTROLLER_DELEGATE_H_

#include "base/macros.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace syncer {

// Trivial implementation of ModelTypeControllerDelegate that simply forwards
// call to another delegate (no task posting involved). This is useful when an
// API requires transferring ownership, but the calling site also wants to keep
// ownership of the actual implementation, and can guarantee the lifetime
// constraints.
class ForwardingModelTypeControllerDelegate
    : public ModelTypeControllerDelegate {
 public:
  // Except for tests, |other| must not be null and must outlive this object.
  explicit ForwardingModelTypeControllerDelegate(
      ModelTypeControllerDelegate* other);
  ~ForwardingModelTypeControllerDelegate() override;

  // ModelTypeControllerDelegate implementation.
  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override;
  void OnSyncStopping(SyncStopMetadataFate metadata_fate) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void GetStatusCountersForDebugging(StatusCountersCallback callback) override;
  void RecordMemoryUsageAndCountsHistograms() override;

 private:
  ModelTypeControllerDelegate* const other_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingModelTypeControllerDelegate);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_FORWARDING_MODEL_TYPE_CONTROLLER_DELEGATE_H_
