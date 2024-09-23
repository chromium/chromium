// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_VALIDATOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_VALIDATOR_H_

#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

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
  mojo::Receiver<on_device_model::mojom::StreamingResponder> receiver_{this};
  FinishCallback finish_callback_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_VALIDATOR_H_
