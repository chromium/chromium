// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/forwarding_data_type_controller_delegate.h"

#include <utility>

namespace syncer {

ForwardingDataTypeControllerDelegate::ForwardingDataTypeControllerDelegate(
    DataTypeControllerDelegate* other)
    : other_(other) {
  // TODO(crbug.com/41420679): Put "DCHECK(other_);"" back once
  // FakeUserEventService provides a proper non-null test double.
}

ForwardingDataTypeControllerDelegate::~ForwardingDataTypeControllerDelegate() =
    default;

void ForwardingDataTypeControllerDelegate::OnSyncStarting(
    const DataTypeActivationRequest& request,
    StartCallback callback) {
  other_->OnSyncStarting(request, std::move(callback));
}

void ForwardingDataTypeControllerDelegate::OnSyncStopping(
    SyncStopMetadataFate metadata_fate) {
  other_->OnSyncStopping(metadata_fate);
}

void ForwardingDataTypeControllerDelegate::HasUnsyncedData(
    base::OnceCallback<void(bool)> callback) {
  other_->HasUnsyncedData(std::move(callback));
}

void ForwardingDataTypeControllerDelegate::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  other_->GetAllNodesForDebugging(std::move(callback));
}

void ForwardingDataTypeControllerDelegate::GetTypeEntitiesCountForDebugging(
    base::OnceCallback<void(const TypeEntitiesCount&)> callback) const {
  other_->GetTypeEntitiesCountForDebugging(std::move(callback));
}

void ForwardingDataTypeControllerDelegate::
    RecordMemoryUsageAndCountsHistograms() {
  other_->RecordMemoryUsageAndCountsHistograms();
}

void ForwardingDataTypeControllerDelegate::ClearMetadataIfStopped() {
  // `other_` can be null during testing.
  // TODO(crbug.com/40894683): Remove test-only code-path.
  if (other_) {
    other_->ClearMetadataIfStopped();
  }
}

void ForwardingDataTypeControllerDelegate::ReportBridgeErrorForTest() {
  other_->ReportBridgeErrorForTest();  // IN-TEST
}

}  // namespace syncer
