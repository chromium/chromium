// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_VALIDATOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_VALIDATOR_H_

#include "base/feature_list.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

namespace features {

// TODO: crbug.com/428242339 - Clean this up once this is stabilized.
BASE_DECLARE_FEATURE(kOnDeviceModelValidation);

// Whether the on-device model will be validated when updated using a set of
// prompts with expected output.
bool IsOnDeviceModelValidationEnabled();

// Whether on-device sessions should be blocked on validation failures.
bool ShouldOnDeviceModelBlockOnValidationFailure();

// Whether the validation result for a model should be cleared if Chrome's
// version changes.
bool ShouldOnDeviceModelClearValidationOnVersionChange();

// The delay from when a new model is received (or startup if validation has not
// completed) until the validation is run.
base::TimeDelta GetOnDeviceModelValidationDelay();

// The maximum number of attempts model validation will be retried.
int GetOnDeviceModelValidationAttemptCount();

}  // namespace features

// The result of running validation prompts for the on-device model.
//
// Keep in sync with OnDeviceModelValidationResult in enums.xml.
enum class OnDeviceModelValidationResult {
  kUnknown = 0,
  // The validation is currently running or was interrupted.
  kPending = 1,
  // The validation test succeeded.
  kSuccess = 2,
  // The validation test produced non-matching output.
  kNonMatchingOutput = 3,
  // The service crashed while running the validation test.
  kServiceCrash = 4,
  // The validation test was interrupted by another session.
  kInterrupted = 5,

  // This must be kept in sync with OnDeviceModelValidationResult in
  // optimization/enums.xml.
  kMaxValue = kInterrupted,
};

// A class used for validating prompts when a new model is received.
class OnDeviceModelValidator
    : public on_device_model::mojom::StreamingResponder {
 public:
  using FinishCallback =
      base::OnceCallback<void(OnDeviceModelValidationResult)>;
  OnDeviceModelValidator(
      const proto::OnDeviceModelValidationConfig& validation_config,
      FinishCallback callback,
      mojo::Remote<on_device_model::mojom::Session> session);
  ~OnDeviceModelValidator() override;

 private:
  void ValidateNextPrompt();

  // on_device_model::mojom::StreamingResponder:
  void OnResponse(on_device_model::mojom::ResponseChunkPtr chunk) override;
  void OnComplete(on_device_model::mojom::ResponseSummaryPtr summary) override;

  void FinishValidation(OnDeviceModelValidationResult result);

  std::string current_response_;
  int index_ = 0;
  proto::OnDeviceModelValidationConfig validation_config_;
  mojo::Remote<on_device_model::mojom::Session> session_;
  mojo::Remote<on_device_model::mojom::Session> active_session_;
  mojo::Receiver<on_device_model::mojom::StreamingResponder> receiver_{this};
  FinishCallback finish_callback_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_VALIDATOR_H_
