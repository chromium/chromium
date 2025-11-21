// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_context.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "services/on_device_model/ml/chrome_ml_audio_buffer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace optimization_guide {

namespace {

bool Match(const SkBitmap& l, const SkBitmap& r) {
  CHECK(!l.isNull());
  CHECK(!r.isNull());
  if (l.info() != r.info()) {
    return false;
  }
  if (l.rowBytes() != r.rowBytes()) {
    return false;
  }
  if (l.pixelRef() == r.pixelRef()) {
    // They share the same data, they must be the same.
    return true;
  }
  // Pessimistically assume the images are different.
  return false;
}

bool Match(const ml::AudioBuffer& l, const ml::AudioBuffer& r) {
  return l.num_channels == r.num_channels && l.num_frames == r.num_frames &&
         l.sample_rate_hz == r.sample_rate_hz && l.data == r.data;
}

// Check if two pieces seem to be the same input.
bool Match(const ::ml::InputPiece& l, const ::ml::InputPiece& r) {
  if (l.index() != r.index()) {
    return false;
  }
  if (std::holds_alternative<ml::Token>(l)) {
    return std::get<ml::Token>(l) == std::get<ml::Token>(r);
  }
  if (std::holds_alternative<std::string>(l)) {
    return std::get<std::string>(l) == std::get<std::string>(r);
  }
  if (std::holds_alternative<SkBitmap>(l)) {
    return Match(std::get<SkBitmap>(l), std::get<SkBitmap>(r));
  }
  if (std::holds_alternative<ml::AudioBuffer>(l)) {
    return Match(std::get<ml::AudioBuffer>(l), std::get<ml::AudioBuffer>(r));
  }
  return false;
}

// Returns true iff `next` is a just an extension of `curr`.
size_t IsPrefix(const on_device_model::mojom::Input& curr,
                const on_device_model::mojom::Input& next) {
  if (curr.pieces.size() > next.pieces.size()) {
    return false;
  }
  for (size_t i = 0; i < curr.pieces.size(); i++) {
    if (!Match(curr.pieces[i], next.pieces[i])) {
      return false;
    }
  }
  return true;
}

// Get an input with only the pieces of `original` after `begin_pos`.
on_device_model::mojom::InputPtr GetSuffix(
    const on_device_model::mojom::Input& original,
    size_t begin_pos) {
  auto result = on_device_model::mojom::Input::New();
  result->pieces.insert(result->pieces.end(),
                        original.pieces.begin() + begin_pos,
                        original.pieces.end());
  return result;
}

}  // namespace

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
      session_params(orig.session_params),
      logger(orig.logger) {}

bool OnDeviceOptions::ShouldUse() const {
  return model_client->ShouldUse();
}

OnDeviceContext::OnDeviceContext(OnDeviceOptions opts,
                                 mojom::OnDeviceFeature feature)
    : opts_(std::move(opts)), feature_(feature) {
  CHECK(opts_.session_params.sampling_params.has_value());
}
OnDeviceContext::~OnDeviceContext() = default;

bool OnDeviceContext::SetInput(MultimodalMessageReadView request,
                               OnDeviceSession::SetInputCallback callback) {
  callback_ = std::move(callback);
  auto input =
      opts_.adapter->ConstructInputString(request, /*want_input_context=*/true);
  if (!input) {
    if (callback_) {
      std::move(callback_).Run(base::unexpected(
          OptimizationGuideModelExecutionError::FromModelExecutionError(
              OptimizationGuideModelExecutionError::ModelExecutionError::
                  kInvalidRequest)));
    }
    return false;
  }
  if (session_ && IsPrefix(*input_, *input->input)) {
    // Update the existing session with just the new pieces.
    // We've already sent some of this input to the session, just update it
    // with the new pieces.
    size_t prefix_size = input_->pieces.size();
    input_ = std::move(input->input);
    if (prefix_size < input_->pieces.size()) {
      Append(GetSuffix(*input_, prefix_size));
    } else if (clients_.empty() && callback_) {
      std::move(callback_).Run(tokens_processed_);
    }
    return true;
  }
  // Keep the old session alive until the new session is ready. This prevents
  // the model from freeing resources that may be needed in the new session.
  auto old_session = std::move(session_);
  clients_.Clear();
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
  auto params = on_device_model::mojom::SessionParams::New();
  params->capabilities = opts_.session_params.capabilities;
  params->top_k = opts_.session_params.sampling_params->top_k;
  params->temperature = opts_.session_params.sampling_params->temperature;
  opts_.model_client->StartSession(session_.BindNewPipeAndPassReceiver(),
                                   std::move(params));
  session_.reset_on_disconnect();
  session_->SetPriority(priority_);
  tokens_processed_ = 0;
  if (input_->pieces.size() > 0) {
    Append(input_->Clone());
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
  // TODO(crbug.com/406585895): This does not account for tokens in outstanding
  // Append() calls.
  context->tokens_processed_ = tokens_processed_;
  CloneSession(context->session_.BindNewPipeAndPassReceiver(),
               /*logged_request=*/nullptr, /*ignore_context=*/false);
  context->SetPriority(priority_);
  context->session_.reset_on_disconnect();
  return context;
}

void OnDeviceContext::SetPriority(on_device_model::mojom::Priority priority) {
  priority_ = priority;
  if (session_) {
    session_->SetPriority(priority);
  }
}

void OnDeviceContext::Append(on_device_model::mojom::InputPtr input) {
  auto options = on_device_model::mojom::AppendOptions::New();
  options->input = std::move(input);
  // TODO(crbug.com/406585895): Make token limits work consistently even when
  // input is split across multiple Append() calls.
  // tokens_processed_ here may be less than the number of tokens that have
  // actually been sent previously if there are currently outstanding Append
  // calls, or if this was Clone from another OnDeviceContext that had
  // outstanding Append calls.
  // Ideally, this should pass a number of tokens to reserve for later use,
  // rather than max_tokens for this call.
  options->max_tokens =
      opts_.token_limits.max_context_tokens - tokens_processed_;
  mojo::PendingRemote<on_device_model::mojom::ContextClient> pending;
  clients_.Add(this, pending.InitWithNewPipeAndPassReceiver());
  session_->Append(std::move(options), std::move(pending));
}

void OnDeviceContext::OnComplete(uint32_t tokens_processed) {
  tokens_processed_ += tokens_processed;
  clients_.Remove(clients_.current_receiver());
  if (clients_.empty() && callback_) {
    // TODO(crbug.com/406585895): tokens_processed_ will not include tokens that
    // were processed when this OnDeviceContext was created by calling Clone().
    // Ideally, OnComplete would receive the size of the remaining buffer
    // instead.
    std::move(callback_).Run(tokens_processed_);
  }
  base::UmaHistogramCounts10000(
      base::StrCat({"OptimizationGuide.ModelExecution."
                    "OnDeviceContextTokensProcessed.",
                    base::ToString(feature_)}),
      tokens_processed);
}

}  // namespace optimization_guide
