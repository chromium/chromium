// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_validator.h"

#include "base/strings/string_util.h"

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
      OnDeviceModelValidationResult::kServiceCrash));
  ValidateNextPrompt();
}

OnDeviceModelValidator::~OnDeviceModelValidator() = default;

void OnDeviceModelValidator::ValidateNextPrompt() {
  if (index_ >= validation_config_.validation_prompts().size()) {
    FinishValidation(OnDeviceModelValidationResult::kSuccess);
    return;
  }

  receiver_.reset();
  active_session_.reset();
  session_->Clone(active_session_.BindNewPipeAndPassReceiver());
  // base::Unretained is safe since `this` owns the session.
  active_session_.set_disconnect_handler(base::BindOnce(
      &OnDeviceModelValidator::FinishValidation, base::Unretained(this),
      OnDeviceModelValidationResult::kServiceCrash));

  current_response_ = "";
  auto append_options = on_device_model::mojom::AppendOptions::New();
  append_options->input = on_device_model::mojom::Input::New();
  append_options->input->pieces.push_back(
      validation_config_.validation_prompts(index_).prompt());
  active_session_->Append(std::move(append_options), {});

  auto generate_options = on_device_model::mojom::GenerateOptions::New();
  // Avoid bad responses spamming output and taking too long.
  generate_options->max_output_tokens = 64;
  active_session_->Generate(std::move(generate_options),
                            receiver_.BindNewPipeAndPassRemote());
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
  // Reset sessions to avoid further callbacks.
  active_session_.reset();
  session_.reset();
  std::move(finish_callback_).Run(result);
}

}  // namespace optimization_guide
