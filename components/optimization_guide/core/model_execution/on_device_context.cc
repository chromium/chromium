// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_context.h"

#include "base/metrics/histogram_functions.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
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

bool OnDeviceContext::SetInput(MultimodalMessageReadView request) {
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
  opts_.model_client->StartSession(session_.BindNewPipeAndPassReceiver());
  session_.reset_on_disconnect();
  if (input_ && input_->pieces.size() > 0) {
    AddContext();
  }
  return session_;
}

void OnDeviceContext::CloneSession(
    mojo::PendingReceiver<on_device_model::mojom::Session> clone,
    proto::OnDeviceModelServiceRequest* logged_request,
    bool ignore_context) {
  auto& session = GetOrCreateSession();
  if (input_ && !ignore_context && logged_request) {
    logged_request->set_input_context_string(OnDeviceInputToString(*input_));
  }
  session->Clone(std::move(clone));
}

std::unique_ptr<OnDeviceContext> OnDeviceContext::Clone() {
  auto context = std::make_unique<OnDeviceContext>(opts_, feature_);
  context->input_ = input_.Clone();
  CloneSession(context->session_.BindNewPipeAndPassReceiver(),
               /*logged_request=*/nullptr, /*ignore_context=*/false);
  context->session_.reset_on_disconnect();
  return context;
}

void OnDeviceContext::AddContext() {
  auto options = on_device_model::mojom::AppendOptions::New();
  options->input = input_.Clone();
  options->max_tokens = opts_.token_limits.max_context_tokens;
  options->token_offset = 0;
  session_->Append(std::move(options), client_.BindNewPipeAndPassRemote());
}

void OnDeviceContext::OnComplete(uint32_t tokens_processed) {
  client_.reset();
  base::UmaHistogramCounts10000(
      base::StrCat({"OptimizationGuide.ModelExecution."
                    "OnDeviceContextTokensProcessed.",
                    GetStringNameForModelExecutionFeature(feature_)}),
      tokens_processed);
}

}  // namespace optimization_guide
