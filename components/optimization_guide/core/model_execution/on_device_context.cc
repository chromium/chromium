// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_context.h"

#include "base/metrics/histogram_functions.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

namespace optimization_guide {

OnDeviceOptions::Client::~Client() = default;

OnDeviceOptions::OnDeviceOptions() = default;
OnDeviceOptions::OnDeviceOptions(OnDeviceOptions&&) = default;
OnDeviceOptions::~OnDeviceOptions() = default;

OnDeviceOptions::OnDeviceOptions(const OnDeviceOptions& orig)
    : model_client(orig.model_client->Clone()),
      model_versions(orig.model_versions),
      adapter(orig.adapter),
      safety_checker(std::make_unique<SafetyChecker>(*orig.safety_checker)),
      token_limits(orig.token_limits),
      logger(orig.logger),
      log_uploader(orig.log_uploader) {}

bool OnDeviceOptions::ShouldUse() const {
  return model_client->ShouldUse();
}

OnDeviceContext::OnDeviceContext(OnDeviceOptions opts,
                                 ModelBasedCapabilityKey feature)
    : opts_(std::move(opts)), feature_(feature) {}
OnDeviceContext::~OnDeviceContext() = default;

bool OnDeviceContext::SetInput(const google::protobuf::MessageLite& request) {
  auto input =
      opts_.adapter->ConstructInputString(request, /*want_input_context=*/true);
  if (!input) {
    return false;
  }
  session_.reset();
  client_.reset();
  input_ = std::move(input->input);
  GetOrCreateSession();  // Start processing
  return true;
}

mojo::Remote<on_device_model::mojom::Session>&
OnDeviceContext::GetOrCreateSession() {
  DCHECK(opts_.ShouldUse());
  if (session_) {
    return session_;
  }
  opts_.model_client->GetModelRemote()->StartSession(
      session_.BindNewPipeAndPassReceiver());
  session_.reset_on_disconnect();
  progress_ = Progress{};
  if (input_) {
    AddContext(features::GetOnDeviceModelMinTokensForContext());
  }
  return session_;
}

void OnDeviceContext::CloneSession(
    mojo::PendingReceiver<on_device_model::mojom::Session> clone,
    proto::OnDeviceModelServiceRequest* logged_request,
    bool ignore_context) {
  auto& session = GetOrCreateSession();
  CancelOptionalContext();
  if (input_) {
    base::UmaHistogramCounts10000(
        base::StrCat({"OptimizationGuide.ModelExecution."
                      "OnDeviceContextTokensProcessed.",
                      GetStringNameForModelExecutionFeature(feature_)}),
        progress_.tokens_processed_);
    base::UmaHistogramBoolean(
        base::StrCat({"OptimizationGuide.ModelExecution."
                      "OnDeviceContextFinishedProcessing.",
                      GetStringNameForModelExecutionFeature(feature_)}),
        progress_.finished_processing_);
    logged_request->set_input_context_num_tokens_processed(
        progress_.tokens_processed_);
    logged_request
        ->set_time_from_input_context_processed_to_request_initiated_millis(
            (progress_.cancelled_ - progress_.start_).InMilliseconds());
    if (!ignore_context) {
      logged_request->set_input_context_string(OnDeviceInputToString(*input_));
    }
  }
  session->Clone(std::move(clone));
}

void OnDeviceContext::CancelOptionalContext() {
  if (!progress_.cancelled_.is_null()) {
    // Already cancelled.
    return;
  }
  progress_.cancelled_ = base::Time::Now();
  if (progress_.can_cancel_) {
    client_.reset();
  }
}

void OnDeviceContext::AddContext(uint32_t num_tokens) {
  progress_.expected_tokens_ = num_tokens;
  if (num_tokens == 0) {
    // This only happens if MinTokens is 0, and we just consider the required
    // chunk to be trivially complete, and move on to the next chunk.
    OnComplete(0);
    return;
  }
  auto options = on_device_model::mojom::InputOptions::New();
  options->input = input_.Clone();
  options->max_tokens = num_tokens;
  options->token_offset = progress_.tokens_processed_;
  session_->AddContext(std::move(options), client_.BindNewPipeAndPassRemote());
}

void OnDeviceContext::OnComplete(uint32_t tokens_processed) {
  client_.reset();
  progress_.tokens_processed_ += tokens_processed;

  if (!progress_.cancelled_.is_null()) {
    return;
  }

  // This means input has been fully processed.
  if (tokens_processed < progress_.expected_tokens_) {
    progress_.finished_processing_ = true;
    return;
  }

  // Once the initial context is complete, we can cancel future context
  // processing.
  progress_.can_cancel_ = true;
  if (progress_.tokens_processed_ < opts_.token_limits.max_context_tokens) {
    AddContext(features::GetOnDeviceModelContextTokenChunkSize());
  }
}

}  // namespace optimization_guide
