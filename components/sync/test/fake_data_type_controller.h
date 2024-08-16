// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "components/sync/base/sync_mode.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"

namespace base {
class Location;
}  // namespace base

namespace syncer {

// Fake DataTypeController implementation that simulates the state machine of a
// typical asynchronous data type.
class FakeDataTypeController : public DataTypeController {
 public:
  explicit FakeDataTypeController(
      DataType type,
      bool enable_transport_mode = false,
      std::unique_ptr<DataTypeLocalDataBatchUploader> uploader = nullptr);
  ~FakeDataTypeController() override;

  void SetPreconditionState(PreconditionState state);

  // Access to the fake underlying model. |kTransportOnly] only exists if
  // |enable_transport_mode| is set upon construction.
  FakeDataTypeControllerDelegate* model(SyncMode sync_mode = SyncMode::kFull);

  int activate_call_count() const { return activate_call_count_; }

  // Mimics the advanced/hypothetical scenario where a custom controller
  // (subclass of DataTypeController) issues an error. Prefer using
  // model()->SimulateModelError() unless you know what you are doing, as the
  // most common source for errors is the actual model itself (e.g. I/O error).
  void SimulateControllerError(const base::Location& location);

  // DataTypeController overrides.
  PreconditionState GetPreconditionState() const override;
  std::unique_ptr<DataTypeActivationResponse> Connect() override;

 private:
  PreconditionState precondition_state_ = PreconditionState::kPreconditionsMet;
  int activate_call_count_ = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_CONTROLLER_H_
