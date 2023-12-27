// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/session_impl.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
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

  std::string& input() { return input_; }

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
            input_, num_tokens, tokens_processed_, /*ignore_context=*/false,
            /*max_output_tokens=*/std::nullopt),
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
    ExecuteRemoteFn execute_remote_fn,
    OptimizationGuideLogger* optimization_guide_logger)
    : controller_(controller),
      feature_(feature),
      execute_remote_fn_(std::move(execute_remote_fn)),
      optimization_guide_logger_(optimization_guide_logger) {
  if (controller_ && controller_->ShouldStartNewSession()) {
    on_device_state_.emplace(std::move(start_session_fn), this);
    on_device_state_->config_interpreter = config_interpreter;
    // Prewarm the initial session to make sure the service is started.
    GetOrCreateSession();
  }
  OPTIMIZATION_GUIDE_LOGGER(
      optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
      optimization_guide_logger_)
      << "Starting on-device session for "
      << std::string(GetStringNameForModelExecutionFeature(feature_));
}

SessionImpl::~SessionImpl() {
  if (on_device_state_ &&
      on_device_state_->did_execute_and_waiting_for_on_complete()) {
    if (on_device_state_->histogram_logger) {
      on_device_state_->histogram_logger->set_result(
          ExecuteModelResult::kDestroyedWhileWaitingForResponse);
    }
    base::UmaHistogramMediumTimes(
        base::StrCat({"OptimizationGuide.ModelExecution."
                      "OnDeviceDestroyedWhileWaitingForResponseTime.",
                      GetStringNameForModelExecutionFeature(feature_)}),
        base::TimeTicks::Now() - on_device_state_->start);
  }
}

void SessionImpl::AddContext(
    const google::protobuf::MessageLite& request_metadata) {
  const auto result = AddContextImpl(request_metadata);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceAddContextResult.",
           GetStringNameForModelExecutionFeature(feature_)}),
      result);
}

SessionImpl::AddContextResult SessionImpl::AddContextImpl(
    const google::protobuf::MessageLite& request_metadata) {
  context_.reset(request_metadata.New());
  context_->CheckTypeAndMergeFrom(request_metadata);
  context_start_time_ = base::TimeTicks::Now();

  if (!ShouldUseOnDeviceModel()) {
    DestroyOnDeviceState();
    return AddContextResult::kUsingServer;
  }

  on_device_state_->add_context_before_execute = false;
  auto input = on_device_state_->config_interpreter->ConstructInputString(
      feature_, *context_, /*want_input_context=*/true);
  if (!input) {
    // Use server if can't construct input.
    DestroyOnDeviceState();
    return AddContextResult::kFailedConstructingInput;
  }

  // Cancel any pending response.
  CancelPendingResponse(ExecuteModelResult::kCancelled);

  // Only the latest context is used, so restart the mojo session here.
  on_device_state_->session.reset();

  // As the session was just destroyed, clear the contextprocessor as
  // it will be using the wrong session, and we don't care about old context
  // at this point.
  on_device_state_->context_processor.reset();

  on_device_state_->context_processor =
      std::make_unique<ContextProcessor>(*this, input->input_string);
  return AddContextResult::kUsingOnDevice;
}

void SessionImpl::ExecuteModel(
    const google::protobuf::MessageLite& request_metadata,
    optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
        callback) {
  std::unique_ptr<ExecuteModelHistogramLogger> logger =
      std::make_unique<ExecuteModelHistogramLogger>(feature_);
  last_message_ = MergeContext(request_metadata);
  if (context_start_time_ != base::TimeTicks()) {
    base::UmaHistogramLongTimes(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.ContextStartToExecutionTime.",
             GetStringNameForModelExecutionFeature(feature_)}),
        base::TimeTicks::Now() - context_start_time_);
    // Only interested in logging the first request after adding context.
    context_start_time_ = base::TimeTicks();
  }

  if (!ShouldUseOnDeviceModel()) {
    DestroyOnDeviceState();
    execute_remote_fn_.Run(feature_, *last_message_,
                           /*log_ai_data_request=*/nullptr,
                           std::move(callback));
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
    // Use server if can't construct input.
    on_device_state_->histogram_logger = std::move(logger);
    on_device_state_->callback = std::move(callback);
    DestroyOnDeviceStateAndFallbackToRemote(
        ExecuteModelResult::kFailedConstructingMessage);
    return;
  }

  // Make sure to cancel any pending response.
  CancelPendingResponse(ExecuteModelResult::kCancelled);

  // Cancel any optional context still processing.
  if (on_device_state_->context_processor) {
    on_device_state_->context_processor->MaybeCancelProcessing();
    base::UmaHistogramCounts10000(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.OnDeviceContextTokensProcessed.",
             GetStringNameForModelExecutionFeature(feature_)}),
        on_device_state_->context_processor->tokens_processed());
  }

  // Note: if on-device fails for some reason, the result will be changed.
  logger->set_result(ExecuteModelResult::kUsedOnDevice);
  on_device_state_->histogram_logger = std::move(logger);

  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_logger_)
        << "Executing model "
        << (input->should_ignore_input_context
                ? ""
                : base::StringPrintf("with input context of %d tokens:\n",
                                     on_device_state_->context_processor
                                         ? on_device_state_->context_processor
                                               ->tokens_processed()
                                         : 0))
        << (!input->should_ignore_input_context &&
                    on_device_state_->context_processor
                ? (on_device_state_->context_processor->input() + "\n")
                : "")
        << "with string:\n"
        << input->input_string;
  }

  on_device_state_->log_ai_data_request =
      std::make_unique<proto::LogAiDataRequest>();
  SetExecutionRequest(feature_, *(on_device_state_->log_ai_data_request),
                      *last_message_);
  on_device_state_->callback = std::move(callback);
  on_device_state_->start = base::TimeTicks::Now();
  on_device_state_->timer_for_first_response.Start(
      FROM_HERE, features::GetOnDeviceModelTimeForInitialResponse(),
      base::BindOnce(&SessionImpl::DestroyOnDeviceStateAndFallbackToRemote,
                     base::Unretained(this), ExecuteModelResult::kTimedOut));
  GetOrCreateSession().Execute(
      on_device_model::mojom::InputOptions::New(
          input->input_string, features::GetOnDeviceModelMaxTokensForExecute(),
          /*token_offset=*/std::nullopt, input->should_ignore_input_context,
          features::GetOnDeviceModelMaxTokensForOutput()),
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

void SessionImpl::OnComplete(on_device_model::mojom::ResponseStatus status) {
  on_device_state_->histogram_logger.reset();
  // TODO(b/302395507): Handle a retracted response.
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceResponseCompleteTime.",
           GetStringNameForModelExecutionFeature(feature_)}),
      base::TimeTicks::Now() - on_device_state_->start);
  if (controller_) {
    controller_->access_controller(/*pass_key=*/{})->OnResponseCompleted();
  }
  SendResponse(/*is_complete=*/true);
  on_device_state_->ResetRequestState();
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
  if (on_device_state_->did_execute_and_waiting_for_on_complete() &&
      features::GetOnDeviceFallbackToServerOnDisconnect()) {
    DestroyOnDeviceStateAndFallbackToRemote(
        ExecuteModelResult::kDisconnectAndFallbackToServer);
    return;
  }

  if (context_) {
    // Persist the current context, so that ExecuteModel() can be called
    // without adding the same context.
    on_device_state_->add_context_before_execute = true;
  }
  on_device_state_->session.reset();
  CancelPendingResponse(ExecuteModelResult::kDisconnectAndCancel);
}

void SessionImpl::CancelPendingResponse(ExecuteModelResult result,
                                        ModelExecutionError error) {
  if (on_device_state_->histogram_logger) {
    on_device_state_->histogram_logger->set_result(result);
  }
  auto callback = std::move(on_device_state_->callback);
  on_device_state_->ResetRequestState();
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
    if (on_device_state_->histogram_logger) {
      on_device_state_->histogram_logger->set_result(
          ExecuteModelResult::kFailedConstructingResponseMessage);
      on_device_state_->histogram_logger.reset();
    }
    CancelPendingResponse(
        ExecuteModelResult::kFailedConstructingResponseMessage,
        ModelExecutionError::kGenericFailure);
    return;
  }

  std::unique_ptr<ModelQualityLogEntry> log_entry;
  if (is_complete && on_device_state_->log_ai_data_request) {
    SetExecutionResponse(feature_, *(on_device_state_->log_ai_data_request),
                         *output);
    // Create corresponding log entry for `log_ai_data_request` to pass it with
    // the callback.
    log_entry = std::make_unique<ModelQualityLogEntry>(
        std::move(on_device_state_->log_ai_data_request));
    on_device_state_->log_ai_data_request.reset();
  }

  on_device_state_->callback.Run(
      StreamingResponse{
          .response = *output,
          .is_complete = is_complete,
      },
      std::move(log_entry));
}

bool SessionImpl::ShouldUseOnDeviceModel() const {
  return controller_ && controller_->ShouldStartNewSession() &&
         on_device_state_;
}

void SessionImpl::DestroyOnDeviceStateAndFallbackToRemote(
    ExecuteModelResult result) {
  if (result == ExecuteModelResult::kTimedOut && controller_) {
    controller_->access_controller(/*pass_key=*/{})->OnSessionTimedOut();
  }
  if (on_device_state_->histogram_logger) {
    on_device_state_->histogram_logger->set_result(result);
  }
  auto log_ai_data_request = std::move(on_device_state_->log_ai_data_request);
  auto callback = std::move(on_device_state_->callback);
  DestroyOnDeviceState();
  execute_remote_fn_.Run(feature_, *last_message_,
                         std::move(log_ai_data_request), std::move(callback));
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

void SessionImpl::OnDeviceState::ResetRequestState() {
  receiver.reset();
  callback.Reset();
  current_response.clear();
  start = base::TimeTicks();
  timer_for_first_response.Stop();
  histogram_logger.reset();
}

SessionImpl::ExecuteModelHistogramLogger::~ExecuteModelHistogramLogger() {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.",
           GetStringNameForModelExecutionFeature(feature_)}),
      result_);
}

}  // namespace optimization_guide
