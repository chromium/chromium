// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/session_impl.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/timer/elapsed_timer.h"
#include "base/token.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/redactor.h"
#include "components/optimization_guide/core/model_execution/repetition_checker.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/model_execution/substitution.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {
namespace {

using google::protobuf::RepeatedPtrField;
using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

void LogResponseHasRepeats(ModelBasedCapabilityKey feature, bool has_repeats) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats.",
           GetStringNameForModelExecutionFeature(feature)}),
      has_repeats);
}

std::string GenerateExecutionId() {
  return "on-device:" + base::Uuid::GenerateRandomV4().AsLowercaseString();
}

void InvokeStreamingCallbackWithRemoteResult(
    OptimizationGuideModelExecutionResultStreamingCallback callback,
    OptimizationGuideModelExecutionResult result,
    std::unique_ptr<ModelQualityLogEntry> log_entry) {
  OptimizationGuideModelStreamingExecutionResult streaming_result;
  streaming_result.log_entry = std::move(log_entry);
  if (result.has_value()) {
    streaming_result.response =
        base::ok(StreamingResponse{.response = *result, .is_complete = true});
  } else {
    streaming_result.response = base::unexpected(result.error());
  }
  callback.Run(std::move(streaming_result));
}

SamplingParams ResolveSamplingParams(
    const std::optional<SessionConfigParams>& config_params,
    const std::optional<SessionImpl::OnDeviceOptions>& on_device_opts) {
  if (config_params && config_params->sampling_params) {
    return config_params->sampling_params.value();
  }
  if (on_device_opts) {
    if (auto feature_params = on_device_opts->adapter->MaybeSamplingParams()) {
      return feature_params.value();
    }
  }
  return SamplingParams{
      .top_k = static_cast<uint32_t>(features::GetOnDeviceModelDefaultTopK()),
      .temperature =
          static_cast<float>(features::GetOnDeviceModelDefaultTemperature()),
  };
}

}  // namespace

// Handles incrementally processing context. After the min context size has been
// processed, any pending context processing will be cancelled if an
// ExecuteModel() call is made.
class SessionImpl::ContextProcessor
    : public on_device_model::mojom::ContextClient {
 public:
  ContextProcessor(SessionImpl& session, on_device_model::mojom::InputPtr input)
      : session_(session), input_(std::move(input)) {
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

    if (has_cancelled_) {
      return;
    }

    // This means input has been fully processed.
    if (tokens_processed < expected_tokens_) {
      finished_processing_ = true;
      return;
    }

    // Once the initial context is complete, we can cancel future context
    // processing.
    can_cancel_ = true;
    if (tokens_processed_ < session_->GetTokenLimits().max_context_tokens) {
      AddContext(features::GetOnDeviceModelContextTokenChunkSize());
    }
  }

  // Returns whether the full context was processed.
  bool MaybeCancelProcessing() {
    has_cancelled_ = true;
    if (can_cancel_) {
      client_.reset();
    }
    return finished_processing_;
  }

  std::string input() { return OnDeviceInputToString(*input_); }

  uint32_t tokens_processed() const { return tokens_processed_; }

 private:
  void AddContext(uint32_t num_tokens) {
    expected_tokens_ = num_tokens;
    client_.reset();
    if (!session_->ShouldUseOnDeviceModel()) {
      return;
    }
    auto options = on_device_model::mojom::InputOptions::New();
    options->input = input_.Clone();
    options->max_tokens = num_tokens;
    options->token_offset = tokens_processed_;
    session_->GetOrCreateSession().AddContext(
        std::move(options), client_.BindNewPipeAndPassRemote());
  }

  raw_ref<SessionImpl> session_;
  on_device_model::mojom::InputPtr input_;
  bool finished_processing_ = false;
  uint32_t expected_tokens_ = 0;
  uint32_t tokens_processed_ = 0;
  bool can_cancel_ = false;
  bool has_cancelled_ = false;
  mojo::Receiver<on_device_model::mojom::ContextClient> client_{this};
};

SessionImpl::OnDeviceModelClient::~OnDeviceModelClient() = default;

SessionImpl::OnDeviceOptions::OnDeviceOptions() = default;
SessionImpl::OnDeviceOptions::OnDeviceOptions(OnDeviceOptions&&) = default;
SessionImpl::OnDeviceOptions::~OnDeviceOptions() = default;

bool SessionImpl::OnDeviceOptions::ShouldUse() const {
  return model_client->ShouldUse();
}

SessionImpl::SessionImpl(
    ModelBasedCapabilityKey feature,
    std::optional<OnDeviceOptions> on_device_opts,
    ExecuteRemoteFn execute_remote_fn,
    base::WeakPtr<OptimizationGuideLogger> optimization_guide_logger,
    base::WeakPtr<ModelQualityLogsUploaderService>
        model_quality_uploader_service,
    const std::optional<SessionConfigParams>& config_params)
    : feature_(feature),
      execute_remote_fn_(std::move(execute_remote_fn)),
      optimization_guide_logger_(optimization_guide_logger),
      model_quality_uploader_service_(model_quality_uploader_service),
      sampling_params_(ResolveSamplingParams(config_params, on_device_opts)) {
  if (config_params && config_params->on_device_execution_timeout) {
    on_device_execution_timeout_ =
        *(config_params->on_device_execution_timeout);
  } else {
    on_device_execution_timeout_ =
        features::GetOnDeviceModelTimeForInitialResponse();
  }
  if (on_device_opts && on_device_opts->ShouldUse()) {
    on_device_state_.emplace(std::move(*on_device_opts), this);
    // Prewarm the initial session to make sure the service is started.
    GetOrCreateSession();
  }
  if (optimization_guide_logger_ &&
      optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_logger_.get())
        << "Starting on-device session for "
        << std::string(GetStringNameForModelExecutionFeature(feature_));
  }
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

const TokenLimits& SessionImpl::GetTokenLimits() const {
  if (!on_device_state_) {
    static const TokenLimits null_limits{};
    return null_limits;
  }
  return on_device_state_->opts.token_limits;
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

  // Cancel any pending response.
  CancelPendingResponse(ExecuteModelResult::kCancelled);

  if (!ShouldUseOnDeviceModel()) {
    DestroyOnDeviceState();
    return AddContextResult::kUsingServer;
  }

  on_device_state_->add_context_before_execute = false;
  auto input = on_device_state_->opts.adapter->ConstructInputString(
      *context_, /*want_input_context=*/true);
  if (!input) {
    // Use server if can't construct input.
    DestroyOnDeviceState();
    return AddContextResult::kFailedConstructingInput;
  }

  // Only the latest context is used, so restart the mojo session here.
  on_device_state_->session.reset();

  // As the session was just destroyed, clear the contextprocessor as
  // it will be using the wrong session, and we don't care about old context
  // at this point.
  on_device_state_->context_processor.reset();

  on_device_state_->context_processor =
      std::make_unique<ContextProcessor>(*this, std::move(input->input));
  return AddContextResult::kUsingOnDevice;
}

void SessionImpl::Score(const std::string& text,
                        OptimizationGuideModelScoreCallback callback) {
  // Fail if not using on device, or no session was started yet.
  if (!on_device_state_ || !on_device_state_->session ||
      // Fail if context is incomplete
      on_device_state_->add_context_before_execute ||
      // Fail if execute was called
      context_start_time_ == base::TimeTicks()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  on_device_state_->session->Score(text, base::BindOnce([](float score) {
                                           return std::optional<float>(score);
                                         }).Then(std::move(callback)));
}

void SessionImpl::ExecuteModel(
    const google::protobuf::MessageLite& request_metadata,
    optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
        callback) {
  std::unique_ptr<ExecuteModelHistogramLogger> logger =
      std::make_unique<ExecuteModelHistogramLogger>(feature_);
  last_message_ = MergeContext(request_metadata);

  auto log_ai_data_request = std::make_unique<proto::LogAiDataRequest>();
  SetExecutionRequest(feature_, *log_ai_data_request, *last_message_);
  proto::OnDeviceModelServiceRequest* logged_request =
      log_ai_data_request->mutable_model_execution_info()
          ->mutable_on_device_model_execution_info()
          ->add_execution_infos()
          ->mutable_request()
          ->mutable_on_device_model_service_request();

  if (context_start_time_ != base::TimeTicks()) {
    base::TimeDelta context_start_to_execution =
        base::TimeTicks::Now() - context_start_time_;
    base::UmaHistogramLongTimes(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.ContextStartToExecutionTime.",
             GetStringNameForModelExecutionFeature(feature_)}),
        context_start_to_execution);
    logged_request
        ->set_time_from_input_context_processed_to_request_initiated_millis(
            context_start_to_execution.InMilliseconds());
    // Only interested in logging the first request after adding context.
    context_start_time_ = base::TimeTicks();
  }

  if (!ShouldUseOnDeviceModel()) {
    CancelPendingResponse(ExecuteModelResult::kCancelled);
    DestroyOnDeviceState();
    execute_remote_fn_.Run(
        feature_, *last_message_,
        /*log_ai_data_request=*/nullptr,
        base::BindOnce(&InvokeStreamingCallbackWithRemoteResult,
                       std::move(callback)));
    return;
  }

  *(log_ai_data_request->mutable_model_execution_info()
        ->mutable_on_device_model_execution_info()
        ->mutable_model_versions()) = on_device_state_->opts.model_versions;

  if (on_device_state_->add_context_before_execute) {
    CHECK(context_);
    std::unique_ptr<google::protobuf::MessageLite> context =
        std::move(context_);
    // Note that this will CancelPendingResponse, so it must be called before
    // switching to the new pending response below.
    AddContext(*context);
    CHECK(!on_device_state_->add_context_before_execute);
  }

  // Make sure to cancel any pending response.
  CancelPendingResponse(ExecuteModelResult::kCancelled);
  // Set new pending response.
  on_device_state_->histogram_logger = std::move(logger);
  on_device_state_->callback = std::move(callback);

  auto input = on_device_state_->opts.adapter->ConstructInputString(
      *last_message_, /*want_input_context=*/false);
  if (!input) {
    // Use server if can't construct input.
    DestroyOnDeviceStateAndFallbackToRemote(
        ExecuteModelResult::kFailedConstructingMessage);
    return;
  }

  // Cancel any optional context still processing.
  if (on_device_state_->context_processor) {
    bool finished_processing =
        on_device_state_->context_processor->MaybeCancelProcessing();
    base::UmaHistogramCounts10000(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.OnDeviceContextTokensProcessed.",
             GetStringNameForModelExecutionFeature(feature_)}),
        on_device_state_->context_processor->tokens_processed());
    base::UmaHistogramBoolean(
        base::StrCat({"OptimizationGuide.ModelExecution."
                      "OnDeviceContextFinishedProcessing.",
                      GetStringNameForModelExecutionFeature(feature_)}),
        finished_processing);
    logged_request->set_input_context_num_tokens_processed(
        on_device_state_->context_processor->tokens_processed());
  }

  // Note: if on-device fails for some reason, the result will be changed.
  on_device_state_->histogram_logger->set_result(
      ExecuteModelResult::kUsedOnDevice);

  if (!input->should_ignore_input_context &&
      on_device_state_->context_processor) {
    logged_request->set_input_context_string(
        on_device_state_->context_processor->input());
  }
  logged_request->set_execution_string(input->ToString());
  // TODO(b/302327957): Probably do some math to get the accurate number here.
  logged_request->set_execution_num_tokens_processed(
      on_device_state_->opts.token_limits.max_execute_tokens);

  if (optimization_guide_logger_ &&
      optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_logger_.get())
        << "Executing model "
        << (input->should_ignore_input_context
                ? ""
                : base::StringPrintf(
                      "with input context of %d tokens:\n%s\n",
                      logged_request->input_context_num_tokens_processed(),
                      logged_request->input_context_string().c_str()))
        << "with string:\n"
        << logged_request->execution_string();
  }

  on_device_state_->log_ai_data_request = std::move(log_ai_data_request);
  on_device_state_->start = base::TimeTicks::Now();
  on_device_state_->timer_for_first_response.Start(
      FROM_HERE, on_device_execution_timeout_,
      base::BindOnce(&SessionImpl::OnSessionTimedOut, base::Unretained(this)));

  auto options = on_device_model::mojom::InputOptions::New();
  options->input = std::move(input->input);
  options->max_tokens = on_device_state_->opts.token_limits.max_execute_tokens;
  options->ignore_context = input->should_ignore_input_context;
  options->max_output_tokens =
      on_device_state_->opts.token_limits.max_output_tokens;
  options->top_k = sampling_params_.top_k;
  options->temperature = sampling_params_.temperature;

  on_device_state_->opts.safety_checker->RunRequestChecks(
      *on_device_state_->opts.model_client, *last_message_,
      base::BindOnce(&SessionImpl::OnRequestSafetyResult,
                     on_device_state_->session_weak_ptr_factory_.GetWeakPtr(),
                     std::move(options)));
}

void SessionImpl::OnRequestSafetyResult(
    on_device_model::mojom::InputOptionsPtr options,
    SafetyChecker::Result safety_result) {
  if (safety_result.failed_to_run) {
    DestroyOnDeviceStateAndFallbackToRemote(
        ExecuteModelResult::kFailedConstructingMessage);
    return;
  }
  // Log the check executions.
  on_device_state_->AddModelExecutionLogs(std::move(safety_result.logs));

  // Handle the result.
  if (safety_result.is_unsafe || safety_result.is_unsupported_language) {
    if (on_device_state_->histogram_logger) {
      on_device_state_->histogram_logger->set_result(
          ExecuteModelResult::kRequestUnsafe);
    }
    if (features::GetOnDeviceModelRetractUnsafeContent()) {
      CancelPendingResponse(ExecuteModelResult::kRequestUnsafe,
                            safety_result.is_unsupported_language
                                ? ModelExecutionError::kUnsupportedLanguage
                                : ModelExecutionError::kFiltered);
      return;
    }
  }
  BeginRequestExecution(std::move(options));
}

void SessionImpl::BeginRequestExecution(
    on_device_model::mojom::InputOptionsPtr options) {
  GetOrCreateSession().Execute(
      std::move(options),
      on_device_state_->receiver.BindNewPipeAndPassRemote());
  on_device_state_->receiver.set_disconnect_handler(
      base::BindOnce(&SessionImpl::OnDisconnect, base::Unretained(this)));
}

// on_device_model::mojom::StreamingResponder:
void SessionImpl::OnResponse(on_device_model::mojom::ResponseChunkPtr chunk) {
  on_device_state_->timer_for_first_response.Stop();

  proto::OnDeviceModelServiceResponse* logged_response =
      on_device_state_->MutableLoggedResponse();

  if (on_device_state_->current_response.empty()) {
    base::TimeDelta time_to_first_response =
        base::TimeTicks::Now() - on_device_state_->start;
    base::UmaHistogramMediumTimes(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.OnDeviceFirstResponseTime.",
             GetStringNameForModelExecutionFeature(feature_)}),
        time_to_first_response);
    logged_response->set_time_to_first_response_millis(
        time_to_first_response.InMilliseconds());
  }

  on_device_state_->current_response += chunk->text;
  on_device_state_->num_unchecked_response_tokens++;

  if (HasRepeatingSuffix(on_device_state_->current_response)) {
    // If a repeat is detected, halt the response, and cancel/finish early.
    on_device_state_->receiver.reset();
    logged_response->set_has_repeats(true);
    if (features::GetOnDeviceModelRetractRepeats()) {
      logged_response->set_status(
          proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_RETRACTED);
      CancelPendingResponse(ExecuteModelResult::kResponseHadRepeats,
                            ModelExecutionError::kFiltered);
      return;
    }

    // Artificially send the OnComplete event to finish processing.
    OnComplete(on_device_model::mojom::ResponseSummary::New());
    return;
  }

  uint32_t interval = on_device_state_->opts.safety_checker->TokenInterval();
  if (interval == 0 ||
      on_device_state_->num_unchecked_response_tokens < interval) {
    // Not enough new data to be worth re-evaluating yet.
    return;
  }

  RunRawOutputSafetyCheck();
}

void SessionImpl::OnComplete(
    on_device_model::mojom::ResponseSummaryPtr summary) {
  // Stop timer, just in case we didn't already via OnResponse().
  on_device_state_->timer_for_first_response.Stop();

  proto::OnDeviceModelServiceResponse* logged_response =
      on_device_state_->MutableLoggedResponse();
  LogResponseHasRepeats(feature_, logged_response->has_repeats());

  base::TimeDelta time_to_completion =
      base::TimeTicks::Now() - on_device_state_->start;
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceResponseCompleteTime.",
           GetStringNameForModelExecutionFeature(feature_)}),
      time_to_completion);
  logged_response->set_time_to_completion_millis(
      time_to_completion.InMilliseconds());
  on_device_state_->opts.model_client->OnResponseCompleted();

  on_device_state_->model_response_complete = true;

  if (on_device_state_->num_unchecked_response_tokens == 0) {
    // We've already requested the evaluation. Check if it finished.
    MaybeSendCompleteResponse();
    return;
  }
  RunRawOutputSafetyCheck();
}

void SessionImpl::RunRawOutputSafetyCheck() {
  on_device_state_->num_unchecked_response_tokens = 0;
  on_device_state_->opts.safety_checker->RunRawOutputCheck(
      *on_device_state_->opts.model_client, on_device_state_->current_response,
      base::BindOnce(&SessionImpl::OnRawOutputSafetyResult,
                     on_device_state_->session_weak_ptr_factory_.GetWeakPtr(),
                     on_device_state_->current_response.size()));
}

void SessionImpl::OnRawOutputSafetyResult(size_t raw_output_size,
                                          SafetyChecker::Result safety_result) {
  if (safety_result.failed_to_run) {
    DestroyOnDeviceStateAndFallbackToRemote(
        ExecuteModelResult::kFailedConstructingMessage);
    return;
  }
  if (safety_result.is_unsafe || safety_result.is_unsupported_language) {
    if (on_device_state_->histogram_logger) {
      on_device_state_->histogram_logger->set_result(
          ExecuteModelResult::kUsedOnDeviceOutputUnsafe);
    }
    on_device_state_->AddModelExecutionLogs(std::move(safety_result.logs));
    if (features::GetOnDeviceModelRetractUnsafeContent()) {
      CancelPendingResponse(ExecuteModelResult::kUsedOnDeviceOutputUnsafe,
                            safety_result.is_unsupported_language
                                ? ModelExecutionError::kUnsupportedLanguage
                                : ModelExecutionError::kFiltered);

      return;
    }
  }
  on_device_state_->latest_safe_raw_output.length = raw_output_size;
  on_device_state_->latest_safe_raw_output.logs = std::move(safety_result.logs);
  SendResponse(ResponseType::kPartial);
  MaybeSendCompleteResponse();
}

void SessionImpl::MaybeSendCompleteResponse() {
  if (on_device_state_ && on_device_state_->model_response_complete &&
      on_device_state_->latest_safe_raw_output.length ==
          on_device_state_->current_response.size()) {
    on_device_state_->AddModelExecutionLogs(
        std::move(on_device_state_->latest_safe_raw_output.logs));
    on_device_state_->latest_safe_raw_output.logs.Clear();
    SendResponse(ResponseType::kComplete);
  }
}

on_device_model::mojom::Session& SessionImpl::GetOrCreateSession() {
  CHECK(ShouldUseOnDeviceModel());
  if (!on_device_state_->session) {
    on_device_state_->opts.model_client->GetModelRemote()->StartSession(
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
        ExecuteModelResult::kDisconnectAndMaybeFallback);
    return;
  }

  if (context_) {
    // Persist the current context, so that ExecuteModel() can be called
    // without adding the same context.
    on_device_state_->add_context_before_execute = true;
  }
  on_device_state_->session.reset();

  if (!on_device_state_->model_response_complete) {
    // Only cancel the request if the model response is not complete yet. We can
    // get in this state if there is an outstanding remote text safety request.
    CancelPendingResponse(ExecuteModelResult::kDisconnectAndCancel);
  }
}

void SessionImpl::CancelPendingResponse(ExecuteModelResult result,
                                        ModelExecutionError error) {
  if (!on_device_state_) {
    return;
  }
  if (on_device_state_->histogram_logger) {
    on_device_state_->histogram_logger->set_result(result);
  }
  auto callback = std::move(on_device_state_->callback);
  auto log_ai_data_request = std::move(on_device_state_->log_ai_data_request);
  on_device_state_->ResetRequestState();
  if (callback) {
    OptimizationGuideModelExecutionError og_error =
        OptimizationGuideModelExecutionError::FromModelExecutionError(error);
    std::unique_ptr<ModelQualityLogEntry> log_entry = nullptr;
    if (og_error.ShouldLogModelQuality()) {
      log_entry = std::make_unique<ModelQualityLogEntry>(
          std::move(log_ai_data_request), model_quality_uploader_service_);
      log_entry->set_model_execution_id(GenerateExecutionId());
    }
    callback.Run(OptimizationGuideModelStreamingExecutionResult(
        base::unexpected(og_error), /*provided_by_on_device=*/true,
        std::move(log_entry)));
  }
}

void SessionImpl::SendResponse(ResponseType response_type) {
  const bool is_complete = response_type != ResponseType::kPartial;

  if (!is_complete &&
      features::ShouldUseTextSafetyRemoteFallbackForEligibleFeatures()) {
    // We don't send streaming responses in this mode.
    return;
  }

  if (!on_device_state_->opts.adapter->ShouldParseResponse(is_complete)) {
    return;
  }

  std::string safe_response = on_device_state_->current_response.substr(
      0, on_device_state_->latest_safe_raw_output.length);
  on_device_state_->MutableLoggedResponse()->set_output_string(safe_response);
  on_device_state_->opts.adapter->ParseResponse(
      *last_message_, safe_response,
      base::BindOnce(&SessionImpl::OnParsedResponse,
                     on_device_state_->session_weak_ptr_factory_.GetWeakPtr(),
                     is_complete));
}

void SessionImpl::OnParsedResponse(
    bool is_complete,
    base::expected<proto::Any, ResponseParsingError> output) {
  if (!output.has_value()) {
    switch (output.error()) {
      case ResponseParsingError::kRejectedPii:
        on_device_state_->MutableLoggedResponse()->set_status(
            proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_RETRACTED);
        CancelPendingResponse(ExecuteModelResult::kContainedPII,
                              ModelExecutionError::kFiltered);
        return;
      case ResponseParsingError::kFailed:
        CancelPendingResponse(
            ExecuteModelResult::kFailedConstructingResponseMessage,
            ModelExecutionError::kGenericFailure);
        return;
    }
  }
  on_device_state_->opts.safety_checker->RunResponseChecks(
      *on_device_state_->opts.model_client, *last_message_, *output,
      base::BindOnce(&SessionImpl::OnResponseSafetyResult,
                     on_device_state_->session_weak_ptr_factory_.GetWeakPtr(),
                     is_complete, *output));
}

void SessionImpl::OnResponseSafetyResult(bool is_complete,
                                         proto::Any output,
                                         SafetyChecker::Result safety_result) {
  if (safety_result.failed_to_run) {
    DestroyOnDeviceStateAndFallbackToRemote(
        ExecuteModelResult::kFailedConstructingMessage);
    return;
  }
  if (is_complete || safety_result.is_unsafe ||
      safety_result.is_unsupported_language) {
    on_device_state_->AddModelExecutionLogs(std::move(safety_result.logs));
  }
  if (safety_result.is_unsafe || safety_result.is_unsupported_language) {
    if (on_device_state_->histogram_logger) {
      on_device_state_->histogram_logger->set_result(
          ExecuteModelResult::kUsedOnDeviceOutputUnsafe);
    }
    if (features::GetOnDeviceModelRetractUnsafeContent()) {
      CancelPendingResponse(ExecuteModelResult::kUsedOnDeviceOutputUnsafe,
                            safety_result.is_unsupported_language
                                ? ModelExecutionError::kUnsupportedLanguage
                                : ModelExecutionError::kFiltered);

      return;
    }
  }
  if (!is_complete) {
    SendPartialResponseCallback(output);
    return;
  }

  if (features::ShouldUseTextSafetyRemoteFallbackForEligibleFeatures()) {
    RunTextSafetyRemoteFallbackAndCompletionCallback(std::move(output));
    return;
  }

  SendSuccessCompletionCallback(output);
}

void SessionImpl::SendPartialResponseCallback(
    const proto::Any& success_response_metadata) {
  on_device_state_->callback.Run(OptimizationGuideModelStreamingExecutionResult(
      base::ok(StreamingResponse{.response = success_response_metadata,
                                 .is_complete = false}),
      /*provided_by_on_device=*/true, /*log_entry=*/nullptr));
}

void SessionImpl::SendSuccessCompletionCallback(
    const proto::Any& success_response_metadata) {
  // Complete the log entry and promise it to the ModelQualityUploaderService.
  std::unique_ptr<ModelQualityLogEntry> log_entry;
  if (on_device_state_->log_ai_data_request) {
    SetExecutionResponse(feature_, *(on_device_state_->log_ai_data_request),
                         success_response_metadata);
    on_device_state_->MutableLoggedResponse()->set_status(
        proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_SUCCESS);
    log_entry = std::make_unique<ModelQualityLogEntry>(
        std::move(on_device_state_->log_ai_data_request),
        model_quality_uploader_service_);
    log_entry->set_model_execution_id(GenerateExecutionId());
    on_device_state_->log_ai_data_request.reset();
  }

  // Return the execution response.
  on_device_state_->callback.Run(OptimizationGuideModelStreamingExecutionResult(
      base::ok(StreamingResponse{.response = success_response_metadata,
                                 .is_complete = true}),
      /*provided_by_on_device=*/true, std::move(log_entry)));

  on_device_state_->ResetRequestState();
}

bool SessionImpl::ShouldUseOnDeviceModel() const {
  return on_device_state_ && on_device_state_->opts.model_client->ShouldUse();
}

void SessionImpl::OnSessionTimedOut() {
  on_device_state_->opts.model_client->OnSessionTimedOut();
  DestroyOnDeviceStateAndFallbackToRemote(ExecuteModelResult::kTimedOut);
}

void SessionImpl::DestroyOnDeviceStateAndFallbackToRemote(
    ExecuteModelResult result) {
  if (on_device_state_->histogram_logger) {
    on_device_state_->histogram_logger->set_result(result);
  }
  auto log_ai_data_request = std::move(on_device_state_->log_ai_data_request);
  auto callback = std::move(on_device_state_->callback);
  DestroyOnDeviceState();
  execute_remote_fn_.Run(
      feature_, *last_message_, std::move(log_ai_data_request),
      base::BindOnce(&InvokeStreamingCallbackWithRemoteResult,
                     std::move(callback)));
}

void SessionImpl::DestroyOnDeviceState() {
  DCHECK(!on_device_state_ || !on_device_state_->callback);
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

void SessionImpl::RunTextSafetyRemoteFallbackAndCompletionCallback(
    proto::Any success_response_metadata) {
  auto ts_request = on_device_state_->opts.adapter->ConstructTextSafetyRequest(
      *last_message_, on_device_state_->current_response);
  if (!ts_request) {
    CancelPendingResponse(
        ExecuteModelResult::kFailedConstructingRemoteTextSafetyRequest,
        ModelExecutionError::kGenericFailure);
    return;
  }

  proto::InternalOnDeviceModelExecutionInfo remote_ts_model_execution_info;
  auto* ts_request_log = remote_ts_model_execution_info.mutable_request()
                             ->mutable_text_safety_model_request();
  ts_request_log->set_text(ts_request->text());
  ts_request_log->set_url(ts_request->url());

  execute_remote_fn_.Run(
      ModelBasedCapabilityKey::kTextSafety, *ts_request,
      /*log_ai_data_request=*/nullptr,
      base::BindOnce(&SessionImpl::OnTextSafetyRemoteResponse,
                     on_device_state_->session_weak_ptr_factory_.GetWeakPtr(),
                     std::move(remote_ts_model_execution_info),
                     std::move(success_response_metadata)));
}

void SessionImpl::OnTextSafetyRemoteResponse(
    proto::InternalOnDeviceModelExecutionInfo remote_ts_model_execution_info,
    proto::Any success_response_metadata,
    OptimizationGuideModelExecutionResult result,
    std::unique_ptr<ModelQualityLogEntry> remote_log_entry) {
  bool is_unsafe =
      !result.has_value() &&
      result.error().error() ==
          OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered;
  if (on_device_state_->log_ai_data_request) {
    if (remote_log_entry) {
      auto* ts_response_log = remote_ts_model_execution_info.mutable_response()
                                  ->mutable_text_safety_model_response();
      ts_response_log->set_server_execution_id(
          remote_log_entry->model_execution_id());
      ts_response_log->set_is_unsafe(is_unsafe);
    }
    *(on_device_state_->log_ai_data_request->mutable_model_execution_info()
          ->mutable_on_device_model_execution_info()
          ->add_execution_infos()) = remote_ts_model_execution_info;
  }

  if (is_unsafe) {
    CancelPendingResponse(ExecuteModelResult::kUsedOnDeviceOutputUnsafe,
                          ModelExecutionError::kFiltered);
    return;
  }

  if (!result.has_value()) {
    CancelPendingResponse(ExecuteModelResult::kTextSafetyRemoteRequestFailed,
                          ModelExecutionError::kGenericFailure);
    return;
  }

  SendSuccessCompletionCallback(success_response_metadata);
}

SessionImpl::OnDeviceState::OnDeviceState(OnDeviceOptions&& options,
                                          SessionImpl* session)
    : opts(std::move(options)),
      receiver(session),
      session_weak_ptr_factory_(session) {}

SessionImpl::OnDeviceState::~OnDeviceState() = default;

proto::OnDeviceModelServiceResponse*
SessionImpl::OnDeviceState::MutableLoggedResponse() {
  CHECK(log_ai_data_request);
  CHECK_GT(log_ai_data_request->model_execution_info()
               .on_device_model_execution_info()
               .execution_infos_size(),
           0);
  return log_ai_data_request->mutable_model_execution_info()
      ->mutable_on_device_model_execution_info()
      ->mutable_execution_infos(0)
      ->mutable_response()
      ->mutable_on_device_model_service_response();
}

void SessionImpl::OnDeviceState::AddModelExecutionLog(
    const proto::InternalOnDeviceModelExecutionInfo& log) {
  CHECK(log_ai_data_request);

  log_ai_data_request->mutable_model_execution_info()
      ->mutable_on_device_model_execution_info()
      ->add_execution_infos()
      ->CopyFrom(log);
}

void SessionImpl::OnDeviceState::AddModelExecutionLogs(
    google::protobuf::RepeatedPtrField<
        proto::InternalOnDeviceModelExecutionInfo> logs) {
  CHECK(log_ai_data_request);

  log_ai_data_request->mutable_model_execution_info()
      ->mutable_on_device_model_execution_info()
      ->mutable_execution_infos()
      ->MergeFrom(std::move(logs));
}

void SessionImpl::OnDeviceState::ResetRequestState() {
  receiver.reset();
  callback.Reset();
  current_response.clear();
  start = base::TimeTicks();
  timer_for_first_response.Stop();
  histogram_logger.reset();
  log_ai_data_request.reset();
  num_unchecked_response_tokens = 0;
  latest_safe_raw_output.length = 0;
  latest_safe_raw_output.logs.Clear();
  model_response_complete = false;
  session_weak_ptr_factory_.InvalidateWeakPtrs();
}

SessionImpl::OnDeviceState::SafeRawOutput::SafeRawOutput() = default;
SessionImpl::OnDeviceState::SafeRawOutput::~SafeRawOutput() = default;

SessionImpl::ExecuteModelHistogramLogger::~ExecuteModelHistogramLogger() {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.",
           GetStringNameForModelExecutionFeature(feature_)}),
      result_);
}

void SessionImpl::GetSizeInTokens(
    const std::string& text,
    OptimizationGuideModelSizeInTokenCallback callback) {
  auto input = on_device_model::mojom::Input::New();
  input->pieces.push_back(text);
  GetOrCreateSession().GetSizeInTokens(std::move(input), std::move(callback));
}

void SessionImpl::GetContextSizeInTokens(
    const google::protobuf::MessageLite& request,
    OptimizationGuideModelSizeInTokenCallback callback) {
  auto input = on_device_state_->opts.adapter->ConstructInputString(
      request, /*want_input_context=*/true);
  if (!input) {
    std::move(callback).Run(0);
    return;
  }
  GetOrCreateSession().GetSizeInTokens(std::move(input->input),
                                       std::move(callback));
}

const proto::Any& SessionImpl::GetOnDeviceFeatureMetadata() const {
  return on_device_state_->opts.adapter->GetFeatureMetadata();
}

const SamplingParams SessionImpl::GetSamplingParams() const {
  return sampling_params_;
}

}  // namespace optimization_guide
