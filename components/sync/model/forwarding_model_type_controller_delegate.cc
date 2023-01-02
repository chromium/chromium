// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/forwarding_model_type_controller_delegate.h"

#include <utility>

namespace syncer {

ForwardingModelTypeControllerDelegate::ForwardingModelTypeControllerDelegate(
    ModelTypeControllerDelegate* other)
    : other_(other) {
  // TODO(crbug.com/895340): Put "DCHECK(other_);"" back once
  // FakeUserEventService provides a proper non-null test double.
}

ForwardingModelTypeControllerDelegate::
    ~ForwardingModelTypeControllerDelegate() = default;

void ForwardingModelTypeControllerDelegate::OnSyncStarting(
    const DataTypeActivationRequest& request,
    StartCallback callback) {
  other_->OnSyncStarting(request, std::move(callback));
}

void ForwardingModelTypeControllerDelegate::OnSyncStopping(
    SyncStopMetadataFate metadata_fate) {
  other_->OnSyncStopping(metadata_fate);
}

void ForwardingModelTypeControllerDelegate::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  other_->GetAllNodesForDebugging(std::move(callback));
}

void ForwardingModelTypeControllerDelegate::GetTypeEntitiesCountForDebugging(
    base::OnceCallback<void(const TypeEntitiesCount&)> callback) const {
  other_->GetTypeEntitiesCountForDebugging(std::move(callback));
}

void ForwardingModelTypeControllerDelegate::
    RecordMemoryUsageAndCountsHistograms() {
  other_->RecordMemoryUsageAndCountsHistograms();
}

void ForwardingModelTypeControllerDelegate::ClearMetadataWhileStopped() {
  other_->ClearMetadataWhileStopped();
}

}  // namespace syncer
