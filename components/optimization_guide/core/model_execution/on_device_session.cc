// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_session.h"

#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

namespace optimization_guide {

using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

// Handles incrementally processing context. After the min context size has been
// processed, any pending context processing will be cancelled if an
// ExecuteModel() call is made.
class OnDeviceSession::ContextProcessor
    : public on_device_model::mojom::ContextClient {
 public:
  ContextProcessor(OnDeviceSession& session, const std::string& input)
      : session_(session), input_(input) {
    int min_context = features::GetOnDeviceModelMinTokensForContext();
    if (min_context > 0) {
      AddContext(min_context);
    } else {
      // If no min context is required, start processing the context as
      // optional.
      OnComplete(0);
    }
  }

  // on_device_model::mojom::ContextClient:
  void OnComplete(uint32_t tokens_processed) override {
    // This means input has been fully processed.
    if (tokens_processed < expected_tokens_) {
      return;
    }

    // Once the initial context is complete, we can cancel future context
    // processing.
    can_cancel_ = true;
    tokens_processed_ += tokens_processed;
    if (tokens_processed_ <
        static_cast<uint32_t>(
            features::GetOnDeviceModelMaxTokensForContext())) {
      AddContext(features::GetOnDeviceModelContextTokenChunkSize());
    }
  }

  void MaybeCancelProcessing() {
    if (can_cancel_) {
      client_.reset();
    }
  }

 private:
  void AddContext(uint32_t num_tokens) {
    expected_tokens_ = num_tokens;
    client_.reset();
    session_->GetOrCreateSession().AddContext(
        on_device_model::mojom::InputOptions::New(
            input_, num_tokens, tokens_processed_, /*ignore_context=*/false),
        client_.BindNewPipeAndPassRemote());
  }

  raw_ref<OnDeviceSession> session_;
  std::string input_;
  uint32_t expected_tokens_ = 0;
  uint32_t tokens_processed_ = 0;
  bool can_cancel_ = false;
  mojo::Receiver<on_device_model::mojom::ContextClient> client_{this};
};

OnDeviceSession::OnDeviceSession(
    StartSessionFn start_session_fn,
    proto::ModelExecutionFeature feature,
    const OnDeviceModelExecutionConfigInterpreter* config_interpreter)
    : feature_(feature),
      config_interpreter_(config_interpreter),
      start_session_fn_(std::move(start_session_fn)) {
  // Prewarm the initial session to make sure the service is started.
  GetOrCreateSession();
}
OnDeviceSession::~OnDeviceSession() = default;

void OnDeviceSession::SetDisconnectHandler(base::OnceClosure on_disconnect) {
  on_disconnect_ = std::move(on_disconnect);
}

void OnDeviceSession::AddContext(
    const google::protobuf::MessageLite& request_metadata) {
  context_.reset(request_metadata.New());
  context_->CheckTypeAndMergeFrom(request_metadata);

  auto input = config_interpreter_->ConstructInputString(
      feature_, *context_, /*want_input_context=*/true);
  if (!input) {
    // TODO(b/302402576): Add error handling.
    LOG(ERROR) << "Error constructing input string.";
    return;
  }

  // Cancel any pending response.
  OnError(ModelExecutionError::kCancelled);

  // Only the latest context is used, so restart the mojo session here.
  session_.reset();
  context_processor_ =
      std::make_unique<ContextProcessor>(*this, input->input_string);
}

void OnDeviceSession::ExecuteModel(
    const google::protobuf::MessageLite& request_metadata,
    optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
        callback) {
  auto request = MergeContext(request_metadata);
  auto input = config_interpreter_->ConstructInputString(
      feature_, *request, /*want_input_context=*/false);
  if (!input) {
    // TODO(b/302402576): Add error handling.
    LOG(ERROR) << "Error constructing input string.";
    return;
  }

  // Make sure to cancel any pending response.
  OnError(ModelExecutionError::kCancelled);

  // Cancel any optional context still processing.
  if (context_processor_) {
    context_processor_->MaybeCancelProcessing();
  }

  callback_ = std::move(callback);
  GetOrCreateSession().Execute(
      on_device_model::mojom::InputOptions::New(
          input->input_string,
          /*max_tokens=*/std::nullopt, /*token_offset=*/std::nullopt,
          input->should_ignore_input_context),
      receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(
      base::BindOnce(&OnDeviceSession::OnError, base::Unretained(this),
                     ModelExecutionError::kCancelled));
}

// on_device_model::mojom::StreamingResponder:
void OnDeviceSession::OnResponse(const std::string& response) {
  current_response_ += response;
  SendResponse(/*is_complete=*/false);
}

void OnDeviceSession::OnComplete() {
  SendResponse(/*is_complete=*/true);
  ResetResponse();
}

on_device_model::mojom::Session& OnDeviceSession::GetOrCreateSession() {
  if (!session_) {
    start_session_fn_.Run(session_.BindNewPipeAndPassReceiver());
    session_.set_disconnect_handler(
        base::BindOnce(&OnDeviceSession::OnDisconnect, base::Unretained(this)));
  }
  return *session_;
}

void OnDeviceSession::OnDisconnect() {
  if (on_disconnect_) {
    std::move(on_disconnect_).Run();
  }
}

void OnDeviceSession::ResetResponse() {
  receiver_.reset();
  callback_.Reset();
  current_response_ = "";
}

void OnDeviceSession::OnError(ModelExecutionError error) {
  if (callback_) {
    callback_.Run(
        base::unexpected(
            OptimizationGuideModelExecutionError::FromModelExecutionError(
                error)),
        nullptr);
  }
  ResetResponse();
}

void OnDeviceSession::SendResponse(bool is_complete) {
  if (!callback_) {
    return;
  }

  auto output =
      config_interpreter_->ConstructOutputMetadata(feature_, current_response_);
  if (!output) {
    OnError(ModelExecutionError::kGenericFailure);
    return;
  }

  // TODO(b/302327957): Add logging.
  callback_.Run(
      StreamingResponse{
          .response = *output,
          .is_complete = is_complete,
      },
      nullptr);
}

}  // namespace optimization_guide
