// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_model_type_controller_delegate.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/type_entities_count.h"

namespace syncer {

FakeModelTypeControllerDelegate::FakeModelTypeControllerDelegate(ModelType type)
    : type_(type) {}

FakeModelTypeControllerDelegate::~FakeModelTypeControllerDelegate() = default;

void FakeModelTypeControllerDelegate::SetModelTypeStateForActivationResponse(
    const sync_pb::ModelTypeState& model_type_state) {
  activation_response_.model_type_state = model_type_state;
}

void FakeModelTypeControllerDelegate::
    EnableSkipEngineConnectionForActivationResponse() {
  activation_response_.skip_engine_connection = true;
}

void FakeModelTypeControllerDelegate::EnableManualModelStart() {
  manual_model_start_enabled_ = true;
}

void FakeModelTypeControllerDelegate::SimulateModelStartFinished() {
  DCHECK(manual_model_start_enabled_);
  if (start_callback_) {
    std::move(start_callback_).Run(MakeActivationResponse());
  }
}

void FakeModelTypeControllerDelegate::SimulateModelError(
    const ModelError& error) {
  model_error_ = error;
  start_callback_.Reset();

  if (!error_handler_) {
    return;
  }

  error_handler_.Run(error);
  // ModelTypeController's implementation uses task-posting for errors. To
  // process the error, the posted task needs processing in a RunLoop.
  base::RunLoop().RunUntilIdle();
}

int FakeModelTypeControllerDelegate::clear_metadata_call_count() const {
  return clear_metadata_call_count_;
}

void FakeModelTypeControllerDelegate::OnSyncStarting(
    const DataTypeActivationRequest& request,
    StartCallback callback) {
  error_handler_ = request.error_handler;

  // If the model has already experienced the error, report it immediately.
  if (model_error_) {
    error_handler_.Run(*model_error_);
    // ModelTypeController's implementation uses task-posting for errors. To
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

void FakeModelTypeControllerDelegate::OnSyncStopping(
    SyncStopMetadataFate metadata_fate) {
  if (metadata_fate == CLEAR_METADATA) {
    ++clear_metadata_call_count_;
  }
}

void FakeModelTypeControllerDelegate::GetAllNodesForDebugging(
    ModelTypeControllerDelegate::AllNodesCallback callback) {
  std::move(callback).Run(type_, base::Value::List());
}

void FakeModelTypeControllerDelegate::RecordMemoryUsageAndCountsHistograms() {}

void FakeModelTypeControllerDelegate::GetTypeEntitiesCountForDebugging(
    base::OnceCallback<void(const TypeEntitiesCount&)> callback) const {
  std::move(callback).Run(TypeEntitiesCount(type_));
}

base::WeakPtr<ModelTypeControllerDelegate>
FakeModelTypeControllerDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<DataTypeActivationResponse>
FakeModelTypeControllerDelegate::MakeActivationResponse() const {
  auto response = std::make_unique<DataTypeActivationResponse>();
  response->model_type_state = activation_response_.model_type_state;
  response->skip_engine_connection =
      activation_response_.skip_engine_connection;
  return response;
}

void FakeModelTypeControllerDelegate::ClearMetadataWhileStopped() {
  ++clear_metadata_call_count_;
}

}  // namespace syncer
