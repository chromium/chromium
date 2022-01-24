// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_FAKE_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_SYNC_DRIVER_FAKE_DATA_TYPE_CONTROLLER_H__

#include <memory>

#include "components/sync/base/sync_mode.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/test/model/fake_model_type_controller_delegate.h"

namespace syncer {

// Fake DataTypeController implementation based on ModelTypeController that
// simulates the state machine of a typical asynchronous data type.
class FakeDataTypeController : public ModelTypeController {
 public:
  explicit FakeDataTypeController(ModelType type);
  FakeDataTypeController(ModelType type, bool enable_transport_only_model);
  ~FakeDataTypeController() override;

  void SetPreconditionState(PreconditionState state);

  // Access to the fake underlying model. |kTransportOnly] only exists if
  // |enable_transport_only_model| is set upon construction.
  FakeModelTypeControllerDelegate* model(SyncMode sync_mode = SyncMode::kFull);

  int activate_call_count() const { return activate_call_count_; }

  // ModelTypeController overrides.
  PreconditionState GetPreconditionState() const override;
  std::unique_ptr<DataTypeActivationResponse> Connect() override;

 private:
  PreconditionState precondition_state_ = PreconditionState::kPreconditionsMet;
  int activate_call_count_ = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_FAKE_DATA_TYPE_CONTROLLER_H__
