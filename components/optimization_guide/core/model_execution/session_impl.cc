// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/session_impl.h"

#include "base/metrics/histogram_functions.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"

namespace optimization_guide {

using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

// Handles incrementally processing context. After the min context size has been
// processed, any pending context processing will be cancelled if an
// ExecuteModel() call is made.
class SessionImpl::ContextProcessor
    : public on_device_model::mojom::ContextClient {
 public:
  ContextProcessor(SessionImpl& session, const std::string& input)
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
    tokens_processed_ += tokens_processed;

    // This means input has been fully processed.
    if (tokens_processed < expected_tokens_) {
      return;
    }

    // Once the initial context is complete, we can cancel future context
    // processing.
    can_cancel_ = true;
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

  uint32_t tokens_processed() const { return tokens_processed_; }

 private:
  void AddContext(uint32_t num_tokens) {
    expected_tokens_ = num_tokens;
    client_.reset();
    if (!session_->ShouldUseOnDeviceModel()) {
      return;
    }
    session_->GetOrCreateSession().AddContext(
        on_device_model::mojom::InputOptions::New(
            input_, num_tokens, tokens_processed_, /*ignore_context=*/false),
        client_.BindNewPipeAndPassRemote());
  }

  raw_ref<SessionImpl> session_;
  std::string input_;
  uint32_t expected_tokens_ = 0;
  uint32_t tokens_processed_ = 0;
  bool can_cancel_ = false;
  mojo::Receiver<on_device_model::mojom::ContextClient> client_{this};
};

SessionImpl::SessionImpl(
    StartSessionFn start_session_fn,
    proto::ModelExecutionFeature feature,
    const OnDeviceModelExecutionConfigInterpreter* config_interpreter,
    base::WeakPtr<OnDeviceModelServiceController> controller,
    ExecuteRemoteFn execute_remote_fn)
    : controller_(controller),
      feature_(feature),
      execute_remote_fn_(std::move(execute_remote_fn)) {
  if (controller_ && controller_->ShouldStartNewSession()) {
    on_device_state_.emplace(std::move(start_session_fn), this);
    on_device_state_->config_interpreter = config_interpreter;
    // Prewarm the initial session to make sure the service is started.
    GetOrCreateSession();
  }
}

SessionImpl::~SessionImpl() = default;

void SessionImpl::AddContext(
    const google::protobuf::MessageLite& request_metadata) {
  context_.reset(request_metadata.New());
  context_->CheckTypeAndMergeFrom(request_metadata);

  if (!ShouldUseOnDeviceModel()) {
    DestroyOnDeviceState();
    return;
  }

  on_device_state_->add_context_before_execute = false;
  auto input = on_device_state_->config_interpreter->ConstructInputString(
      feature_, *context_, /*want_input_context=*/true);
  if (!input) {
    // TODO(b/313666843): add metrics.
    // Use server if can't construct input.
    DestroyOnDeviceState();
    return;
  }

  // Cancel any pending response.
  CancelPendingResponse();

  // Only the latest context is used, so restart the mojo session here.
  on_device_state_->session.reset();

  // As the session was just destroyed, clear the contextprocessor as
  // it will be using the wrong session, and we don't care about old context
  // at this point.
  on_device_state_->context_processor.reset();

  on_device_state_->context_processor =
      std::make_unique<ContextProcessor>(*this, input->input_string);
}

void SessionImpl::ExecuteModel(
    const google::protobuf::MessageLite& request_metadata,
    optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
        callback) {
  last_message_ = MergeContext(request_metadata);

  if (!ShouldUseOnDeviceModel()) {
    DestroyOnDeviceState();
    execute_remote_fn_.Run(feature_, *last_message_, std::move(callback));
    return;
  }

  if (on_device_state_->add_context_before_execute) {
    CHECK(context_);
    std::unique_ptr<google::protobuf::MessageLite> context =
        std::move(context_);
    AddContext(*context);
    CHECK(!on_device_state_->add_context_before_execute);
  }

  auto input = on_device_state_->config_interpreter->ConstructInputString(
      feature_, *last_message_, /*want_input_context=*/false);
  if (!input) {
    // TODO(b/313666843): add metrics.
    // Use server if can't construct input.
    on_device_state_->callback = std::move(callback);
    FallbackToRemote();
    return;
  }

  // Make sure to cancel any pending response.
  CancelPendingResponse();

  // Cancel any optional context still processing.
  if (on_device_state_->context_processor) {
    on_device_state_->context_processor->MaybeCancelProcessing();
    base::UmaHistogramCounts10000(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.OnDeviceContextTokensProcessed.",
             GetStringNameForModelExecutionFeature(feature_)}),
        on_device_state_->context_processor->tokens_processed());
  }

  on_device_state_->callback = std::move(callback);
  on_device_state_->start = base::TimeTicks::Now();
  on_device_state_->timer_for_first_response.Start(
      FROM_HERE, features::GetOnDeviceModelTimeForInitialResponse(), this,
      &SessionImpl::FallbackToRemote);
  GetOrCreateSession().Execute(
      on_device_model::mojom::InputOptions::New(
          input->input_string, features::GetOnDeviceModelMaxTokensForExecute(),
          /*token_offset=*/std::nullopt, input->should_ignore_input_context),
      on_device_state_->receiver.BindNewPipeAndPassRemote());
  on_device_state_->receiver.set_disconnect_handler(
      base::BindOnce(&SessionImpl::OnDisconnect, base::Unretained(this)));
}

// on_device_model::mojom::StreamingResponder:
void SessionImpl::OnResponse(const std::string& response) {
  on_device_state_->timer_for_first_response.Stop();
  if (on_device_state_->current_response.empty()) {
    base::UmaHistogramMediumTimes(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.OnDeviceFirstResponseTime.",
             GetStringNameForModelExecutionFeature(feature_)}),
        base::TimeTicks::Now() - on_device_state_->start);
  }
  on_device_state_->current_response += response;
  SendResponse(/*is_complete=*/false);
}

void SessionImpl::OnComplete() {
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceResponseCompleteTime.",
           GetStringNameForModelExecutionFeature(feature_)}),
      base::TimeTicks::Now() - on_device_state_->start);
  if (controller_) {
    controller_->OnResponseCompleted({}, *this);
  }
  SendResponse(/*is_complete=*/true);
  ResetResponse();
}

on_device_model::mojom::Session& SessionImpl::GetOrCreateSession() {
  CHECK(ShouldUseOnDeviceModel());
  if (!on_device_state_->session) {
    on_device_state_->start_session_fn.Run(
        on_device_state_->session.BindNewPipeAndPassReceiver());
    on_device_state_->session.set_disconnect_handler(
        base::BindOnce(&SessionImpl::OnDisconnect, base::Unretained(this)));
  }
  return *on_device_state_->session;
}

void SessionImpl::OnDisconnect() {
  if (context_) {
    // Persist the current context, so that ExecuteModel() can be called
    // without adding the same context.
    on_device_state_->add_context_before_execute = true;
  }
  on_device_state_->session.reset();
  CancelPendingResponse();
}

void SessionImpl::ResetResponse() {
  on_device_state_->receiver.reset();
  on_device_state_->callback.Reset();
  on_device_state_->current_response.clear();
  on_device_state_->start = base::TimeTicks();
  on_device_state_->timer_for_first_response.Stop();
}

void SessionImpl::CancelPendingResponse(ModelExecutionError error) {
  auto callback = std::move(on_device_state_->callback);
  ResetResponse();
  if (callback) {
    callback.Run(
        base::unexpected(
            OptimizationGuideModelExecutionError::FromModelExecutionError(
                error)),
        nullptr);
  }
}

void SessionImpl::SendResponse(bool is_complete) {
  on_device_state_->timer_for_first_response.Stop();
  if (!on_device_state_->callback) {
    return;
  }

  auto output = on_device_state_->config_interpreter->ConstructOutputMetadata(
      feature_, on_device_state_->current_response);
  if (!output) {
    // TODO(b/313666843): add metrics.
    CancelPendingResponse(ModelExecutionError::kGenericFailure);
    return;
  }

  // TODO(b/302327957): Add logging.
  on_device_state_->callback.Run(
      StreamingResponse{
          .response = *output,
          .is_complete = is_complete,
      },
      nullptr);
}

bool SessionImpl::ShouldUseOnDeviceModel() const {
  return controller_ && controller_->ShouldStartNewSession() &&
         on_device_state_;
}

void SessionImpl::FallbackToRemote() {
  // TODO(b/313666843): add metrics.
  auto callback = std::move(on_device_state_->callback);
  DestroyOnDeviceState();
  execute_remote_fn_.Run(feature_, *last_message_, std::move(callback));
}

void SessionImpl::DestroyOnDeviceState() {
  on_device_state_.reset();
}

std::unique_ptr<google::protobuf::MessageLite> SessionImpl::MergeContext(
    const google::protobuf::MessageLite& request) {
  // Create a message of the correct type.
  auto message = base::WrapUnique(request.New());
  // First merge in the current context.
  if (context_) {
    message->CheckTypeAndMergeFrom(*context_);
  }
  // Then merge in the request.
  message->CheckTypeAndMergeFrom(request);
  return message;
}

SessionImpl::OnDeviceState::OnDeviceState(
    StartSessionFn start_session_fn,
    on_device_model::mojom::StreamingResponder* session)
    : start_session_fn(std::move(start_session_fn)), receiver(session) {}

SessionImpl::OnDeviceState::~OnDeviceState() = default;

}  // namespace optimization_guide
