// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_TEST_FAKE_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "components/sync/base/sync_mode.h"
#include "components/sync/service/model_type_controller.h"
#include "components/sync/test/fake_model_type_controller_delegate.h"

namespace syncer {

// Fake ModelTypeController implementation that simulates the state machine of a
// typical asynchronous data type.
class FakeModelTypeController : public ModelTypeController {
 public:
  explicit FakeModelTypeController(ModelType type);
  FakeModelTypeController(ModelType type, bool enable_transport_mode);
  ~FakeModelTypeController() override;

  void SetPreconditionState(PreconditionState state);

  // Access to the fake underlying model. |kTransportOnly] only exists if
  // |enable_transport_mode| is set upon construction.
  FakeModelTypeControllerDelegate* model(SyncMode sync_mode = SyncMode::kFull);

  int activate_call_count() const { return activate_call_count_; }

  void SetLocalDataBatchUploader(
      std::unique_ptr<ModelTypeLocalDataBatchUploader> uploader);

  // ModelTypeController overrides.
  PreconditionState GetPreconditionState() const override;
  std::unique_ptr<DataTypeActivationResponse> Connect() override;
  ModelTypeLocalDataBatchUploader* GetModelTypeLocalDataBatchUploader()
      override;

 private:
  PreconditionState precondition_state_ = PreconditionState::kPreconditionsMet;
  int activate_call_count_ = 0;

  std::unique_ptr<ModelTypeLocalDataBatchUploader> uploader_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_MODEL_TYPE_CONTROLLER_H_
