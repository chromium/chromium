// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_data_type_controller.h"

#include <memory>

#include "components/sync/engine/data_type_activation_response.h"

namespace syncer {

FakeDataTypeController::FakeDataTypeController(ModelType type)
    : FakeDataTypeController(type, /*enable_transport_only_model=*/false) {}

FakeDataTypeController::FakeDataTypeController(ModelType type,
                                               bool enable_transport_only_model)
    : ModelTypeController(
          type,
          /*delegate_for_full_sync_mode=*/
          std::make_unique<FakeModelTypeControllerDelegate>(type),
          /*delegate_for_transport_mode=*/
          enable_transport_only_model
              ? std::make_unique<FakeModelTypeControllerDelegate>(type)
              : nullptr) {}

FakeDataTypeController::~FakeDataTypeController() = default;

void FakeDataTypeController::SetPreconditionState(PreconditionState state) {
  precondition_state_ = state;
}

FakeModelTypeControllerDelegate* FakeDataTypeController::model(
    SyncMode sync_mode) {
  return static_cast<FakeModelTypeControllerDelegate*>(
      GetDelegateForTesting(sync_mode));
}

DataTypeController::PreconditionState
FakeDataTypeController::GetPreconditionState() const {
  return precondition_state_;
}

std::unique_ptr<DataTypeActivationResponse> FakeDataTypeController::Connect() {
  ++activate_call_count_;
  return ModelTypeController::Connect();
}

}  // namespace syncer
