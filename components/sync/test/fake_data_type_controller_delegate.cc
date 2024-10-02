// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_data_type_controller_delegate.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/protocol/data_type_state.pb.h"

namespace syncer {

FakeDataTypeControllerDelegate::FakeDataTypeControllerDelegate(DataType type)
    : type_(type) {}

FakeDataTypeControllerDelegate::~FakeDataTypeControllerDelegate() = default;

void FakeDataTypeControllerDelegate::SetDataTypeStateForActivationResponse(
    const sync_pb::DataTypeState& data_type_state) {
  activation_response_.data_type_state = data_type_state;
}

void FakeDataTypeControllerDelegate::
    EnableSkipEngineConnectionForActivationResponse() {
  activation_response_.skip_engine_connection = true;
}

void FakeDataTypeControllerDelegate::EnableManualModelStart() {
  manual_model_start_enabled_ = true;
}

void FakeDataTypeControllerDelegate::SimulateModelStartFinished() {
  DCHECK(manual_model_start_enabled_);
  if (start_callback_) {
    std::move(start_callback_).Run(MakeActivationResponse());
  }
}

void FakeDataTypeControllerDelegate::SimulateModelError(
    const ModelError& error) {
  model_error_ = error;
  start_callback_.Reset();

  if (!error_handler_) {
    return;
  }

  error_handler_.Run(error);
  // DataTypeController's implementation uses task-posting for errors. To
  // process the error, the posted task needs processing in a RunLoop.
  base::RunLoop().RunUntilIdle();
}

int FakeDataTypeControllerDelegate::clear_metadata_count() const {
  return clear_metadata_count_;
}

void FakeDataTypeControllerDelegate::OnSyncStarting(
    const DataTypeActivationRequest& request,
    StartCallback callback) {
  sync_started_ = true;

  error_handler_ = request.error_handler;

  // If the model has already experienced the error, report it immediately.
  if (model_error_) {
    error_handler_.Run(*model_error_);
    // DataTypeController's implementation uses task-posting for errors. To
    // process the error, the posted task needs processing in a RunLoop.
    base::RunLoop().RunUntilIdle();
    return;
  }

  if (manual_model_start_enabled_) {
    // Completion will be triggered from SimulateModelStartFinished().
    start_callback_ = std::move(callback);
  } else {
    // Trigger completion immediately.
    std::move(callback).Run(MakeActivationResponse());
  }
}

void FakeDataTypeControllerDelegate::OnSyncStopping(
    SyncStopMetadataFate metadata_fate) {
  if (metadata_fate == CLEAR_METADATA) {
    ++clear_metadata_count_;
  }
  sync_started_ = false;
}

void FakeDataTypeControllerDelegate::HasUnsyncedData(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
}

void FakeDataTypeControllerDelegate::GetAllNodesForDebugging(
    DataTypeControllerDelegate::AllNodesCallback callback) {
  std::move(callback).Run(base::Value::List());
}

void FakeDataTypeControllerDelegate::RecordMemoryUsageAndCountsHistograms() {}

void FakeDataTypeControllerDelegate::GetTypeEntitiesCountForDebugging(
    base::OnceCallback<void(const TypeEntitiesCount&)> callback) const {
  std::move(callback).Run(TypeEntitiesCount(type_));
}

base::WeakPtr<DataTypeControllerDelegate>
FakeDataTypeControllerDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<DataTypeActivationResponse>
FakeDataTypeControllerDelegate::MakeActivationResponse() const {
  auto response = std::make_unique<DataTypeActivationResponse>();
  response->data_type_state = activation_response_.data_type_state;
  response->skip_engine_connection =
      activation_response_.skip_engine_connection;
  return response;
}

void FakeDataTypeControllerDelegate::ClearMetadataIfStopped() {
  // If Sync is not actually stopped, ignore this call. This mirrors logic in
  // ClientTagBasedDataTypeProcessor and BookmarkDataTypeProcessor.
  if (sync_started_) {
    return;
  }
  ++clear_metadata_count_;
}

void FakeDataTypeControllerDelegate::ReportBridgeErrorForTest() {
  SimulateModelError(ModelError(FROM_HERE, "Report error for test"));
}

}  // namespace syncer
