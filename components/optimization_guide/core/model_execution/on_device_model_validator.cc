// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_validator.h"

namespace optimization_guide {

OnDeviceModelValidator::OnDeviceModelValidator(
    const proto::OnDeviceModelValidationConfig& validation_config,
    FinishCallback callback,
    mojo::Remote<on_device_model::mojom::Session> session)
    : validation_config_(validation_config),
      session_(std::move(session)),
      finish_callback_(std::move(callback)) {
  // base::Unretained is safe since `this` owns the session.
  session_.set_disconnect_handler(base::BindOnce(
      &OnDeviceModelValidator::FinishValidation, base::Unretained(this),
      OnDeviceModelValidationResult::kInterrupted));
  ValidateNextPrompt();
}

OnDeviceModelValidator::~OnDeviceModelValidator() = default;

void OnDeviceModelValidator::ValidateNextPrompt() {
  if (index_ >= validation_config_.validation_prompts().size()) {
    FinishValidation(OnDeviceModelValidationResult::kSuccess);
    return;
  }

  receiver_.reset();
  current_response_ = "";
  auto options = on_device_model::mojom::InputOptions::New();
  options->input = on_device_model::mojom::Input::New();
  options->input->pieces.push_back(
      validation_config_.validation_prompts(index_).prompt());
  // Avoid bad responses spamming output and taking too long.
  options->max_output_tokens = 64;
  session_->Execute(std::move(options), receiver_.BindNewPipeAndPassRemote());
}

void OnDeviceModelValidator::OnResponse(
    on_device_model::mojom::ResponseChunkPtr chunk) {
  current_response_ += chunk->text;
}

void OnDeviceModelValidator::OnComplete(
    on_device_model::mojom::ResponseSummaryPtr summary) {
  std::string expected = base::ToLowerASCII(
      validation_config_.validation_prompts(index_).expected_output());
  if (base::ToLowerASCII(current_response_).find(expected) ==
      std::string::npos) {
    FinishValidation(OnDeviceModelValidationResult::kNonMatchingOutput);
    return;
  }

  index_++;
  ValidateNextPrompt();
}

void OnDeviceModelValidator::FinishValidation(
    OnDeviceModelValidationResult result) {
  std::move(finish_callback_).Run(result);
}

}  // namespace optimization_guide
