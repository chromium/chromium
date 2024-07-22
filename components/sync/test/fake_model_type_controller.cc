// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_model_type_controller.h"

#include <memory>
#include <utility>

#include "components/sync/engine/data_type_activation_response.h"

namespace syncer {

FakeModelTypeController::FakeModelTypeController(ModelType type)
    : FakeModelTypeController(type, /*enable_transport_mode=*/false) {}

FakeModelTypeController::FakeModelTypeController(ModelType type,
                                                 bool enable_transport_mode)
    : ModelTypeController(
          type,
          /*delegate_for_full_sync_mode=*/
          std::make_unique<FakeModelTypeControllerDelegate>(type),
          /*delegate_for_transport_mode=*/
          enable_transport_mode
              ? std::make_unique<FakeModelTypeControllerDelegate>(type)
              : nullptr) {}

FakeModelTypeController::~FakeModelTypeController() = default;

void FakeModelTypeController::SetPreconditionState(PreconditionState state) {
  precondition_state_ = state;
}

FakeModelTypeControllerDelegate* FakeModelTypeController::model(
    SyncMode sync_mode) {
  return static_cast<FakeModelTypeControllerDelegate*>(
      GetDelegateForTesting(sync_mode));
}

void FakeModelTypeController::SetLocalDataBatchUploader(
    std::unique_ptr<ModelTypeLocalDataBatchUploader> uploader) {
  uploader_ = std::move(uploader);
}

ModelTypeController::PreconditionState
FakeModelTypeController::GetPreconditionState() const {
  return precondition_state_;
}

std::unique_ptr<DataTypeActivationResponse> FakeModelTypeController::Connect() {
  ++activate_call_count_;
  return ModelTypeController::Connect();
}

ModelTypeLocalDataBatchUploader*
FakeModelTypeController::GetModelTypeLocalDataBatchUploader() {
  return uploader_.get();
}

}  // namespace syncer
