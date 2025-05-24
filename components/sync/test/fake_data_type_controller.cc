// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_data_type_controller.h"

#include <memory>
#include <utility>

#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/model_error.h"
#include "components/sync/service/sync_error.h"

namespace syncer {

FakeDataTypeController::FakeDataTypeController(
    DataType type,
    bool enable_transport_mode,
    std::unique_ptr<DataTypeLocalDataBatchUploader> uploader)
    : DataTypeController(
          type,
          /*delegate_for_full_sync_mode=*/
          std::make_unique<FakeDataTypeControllerDelegate>(type),
          /*delegate_for_transport_mode=*/
          enable_transport_mode
              ? std::make_unique<FakeDataTypeControllerDelegate>(type)
              : nullptr,
          std::move(uploader)) {}

FakeDataTypeController::~FakeDataTypeController() = default;

void FakeDataTypeController::SetPreconditionState(PreconditionState state) {
  precondition_state_ = state;
}

FakeDataTypeControllerDelegate* FakeDataTypeController::model(
    SyncMode sync_mode) {
  return static_cast<FakeDataTypeControllerDelegate*>(
      GetDelegateForTesting(sync_mode));
}

void FakeDataTypeController::SimulateControllerError(
    const base::Location& location) {
  ReportModelError(ModelError(location, "Test error"));
}

DataTypeController::PreconditionState
FakeDataTypeController::GetPreconditionState() const {
  return precondition_state_;
}

std::unique_ptr<DataTypeActivationResponse> FakeDataTypeController::Connect() {
  ++activate_call_count_;
  return DataTypeController::Connect();
}

}  // namespace syncer
