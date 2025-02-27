// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_execution.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/repetition_checker.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

namespace optimization_guide {

namespace {

using google::protobuf::RepeatedPtrField;
using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

void LogRequest(OptimizationGuideLogger* logger,
                const proto::OnDeviceModelServiceRequest& logged_request) {
  if (logger && logger->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION, logger)
        << "Executing model "
        << (logged_request.input_context_string().empty()
                ? ""
                : base::StringPrintf(
                      "with input context of %d tokens:\n%s\n",
                      logged_request.input_context_num_tokens_processed(),
                      logged_request.input_context_string().c_str()))
        << "with string:\n"
        << logged_request.execution_string();
  }
}

void LogRawResponse(OptimizationGuideLogger* logger,
                    ModelBasedCapabilityKey feature,
                    const std::string& raw_response) {
  if (logger && logger->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION, logger)
        << "Model generates raw response with "
        << std::string(GetStringNameForModelExecutionFeature(feature)) << ":\n"
        << raw_response;
  }
}

void LogRepeatedResponse(OptimizationGuideLogger* logger,
                         ModelBasedCapabilityKey feature,
                         const std::string& repeated_response) {
  if (logger && logger->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION, logger)
        << "Model generates repeated response with "
        << std::string(GetStringNameForModelExecutionFeature(feature)) << ":\n"
        << repeated_response;
  }
}

void LogResponseHasRepeats(ModelBasedCapabilityKey feature, bool has_repeats) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats.",
           GetStringNameForModelExecutionFeature(feature)}),
      has_repeats);
}

void LogResponseCompleteTime(ModelBasedCapabilityKey feature,
                             base::TimeDelta time_to_completion) {
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceResponseCompleteTime.",
           GetStringNameForModelExecutionFeature(feature)}),
      time_to_completion);
}

void LogResponseCompleteTokens(ModelBasedCapabilityKey feature,
                               uint32_t tokens) {
  base::UmaHistogramCounts10000(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceResponseCompleteTokens.",
           GetStringNameForModelExecutionFeature(feature)}),
      tokens);
}

std::string GenerateExecutionId() {
  return "on-device:" + base::Uuid::GenerateRandomV4().AsLowercaseString();
}

}  // namespace

void InvokeStreamingCallbackWithRemoteResult(
    OptimizationGuideModelExecutionResultStreamingCallback callback,
    OptimizationGuideModelExecutionResult result,
    std::unique_ptr<ModelQualityLogEntry> log_entry) {
  OptimizationGuideModelStreamingExecutionResult streaming_result;
  if (log_entry && log_entry->log_ai_data_request() &&
      log_entry->log_ai_data_request()->has_model_execution_info()) {
    streaming_result.execution_info =
        std::make_unique<proto::ModelExecutionInfo>(
            log_entry->log_ai_data_request()->model_execution_info());
  }
  streaming_result.log_entry = std::move(log_entry);
  if (result.response.has_value()) {
    streaming_result.response = base::ok(
        StreamingResponse{.response = *result.response, .is_complete = true});
  } else {
    streaming_result.response = base::unexpected(result.response.error());
  }
  callback.Run(std::move(streaming_result));
}

OnDeviceExecution::OnDeviceExecution(
    ModelBasedCapabilityKey feature,
    OnDeviceOptions opts,
    ExecuteRemoteFn execute_remote_fn,
    MultimodalMessage message,
    std::unique_ptr<ResultLogger> logger,
    OptimizationGuideModelExecutionResultStreamingCallback callback,
    base::OnceCallback<void(bool)> cleanup_callback)
    : feature_(feature),
      opts_(std::move(opts)),
      execute_remote_fn_(execute_remote_fn),
      last_message_(std::move(message)),
      histogram_logger_(std::move(logger)),
      callback_(std::move(callback)),
      cleanup_callback_(std::move(cleanup_callback)) {
  log_.mutable_model_execution_info()
      ->mutable_on_device_model_execution_info()
      ->add_execution_infos();
  start_ = base::TimeTicks::Now();
  *(log_.mutable_model_execution_info()
        ->mutable_on_device_model_execution_info()
        ->mutable_model_versions()) = opts_.model_versions;
  // Note: if on-device fails for some reason, the result will be changed.
  histogram_logger_->set_result(Result::kUsedOnDevice);
}

OnDeviceExecution::~OnDeviceExecution() {
  if (callback_) {
    if (histogram_logger_) {
      histogram_logger_->set_result(Result::kDestroyedWhileWaitingForResponse);
    }
    base::UmaHistogramMediumTimes(
        base::StrCat({"OptimizationGuide.ModelExecution."
                      "OnDeviceDestroyedWhileWaitingForResponseTime.",
                      GetStringNameForModelExecutionFeature(feature_)}),
        base::TimeTicks::Now() - start_);
  }
}

proto::OnDeviceModelServiceRequest* OnDeviceExecution::MutableLoggedRequest() {
  CHECK_GT(log_.model_execution_info()
               .on_device_model_execution_info()
               .execution_infos_size(),
           0);
  return log_.mutable_model_execution_info()
      ->mutable_on_device_model_execution_info()
      ->mutable_execution_infos(0)
      ->mutable_request()
      ->mutable_on_device_model_service_request();
}

proto::OnDeviceModelServiceResponse*
OnDeviceExecution::MutableLoggedResponse() {
  CHECK_GT(log_.model_execution_info()
               .on_device_model_execution_info()
               .execution_infos_size(),
           0);
  return log_.mutable_model_execution_info()
      ->mutable_on_device_model_execution_info()
      ->mutable_execution_infos(0)
      ->mutable_response()
      ->mutable_on_device_model_service_response();
}

void OnDeviceExecution::AddModelExecutionLogs(
    google::protobuf::RepeatedPtrField<
        proto::InternalOnDeviceModelExecutionInfo> logs) {
  log_.mutable_model_execution_info()
      ->mutable_on_device_model_execution_info()
      ->mutable_execution_infos()
      ->MergeFrom(std::move(logs));
}

void OnDeviceExecution::Cancel() {
  CancelPendingResponse(Result::kCancelled);
}

void OnDeviceExecution::BeginExecution(OnDeviceContext& context,
                                       const SamplingParams& sampling_params) {
  auto input = opts_.adapter->ConstructInputString(
      last_message_.read(), /*want_input_context=*/false);
  if (!input) {
    FallbackToRemote(Result::kFailedConstructingMessage);
    return;
  }

  auto* logged_request = MutableLoggedRequest();

  // Terminate optional context processing and log the context info.
  context.CloneSession(session_.BindNewPipeAndPassReceiver(), logged_request,
                       input->should_ignore_input_context);

  logged_request->set_execution_string(input->ToString());
  LogRequest(opts_.logger.get(), *logged_request);

  if (input->input->pieces.size() > 0) {
    auto append_options = on_device_model::mojom::AppendOptions::New();
    append_options->input = std::move(input->input);
    append_options->max_tokens = opts_.token_limits.max_execute_tokens;
    session_->Append(std::move(append_options),
                     context_receiver_.BindNewPipeAndPassRemote());
  }

  auto options = on_device_model::mojom::GenerateOptions::New();
  options->max_output_tokens = opts_.token_limits.max_output_tokens;
  options->top_k = sampling_params.top_k;
  options->temperature = sampling_params.temperature;

  opts_.safety_checker->RunRequestChecks(
      last_message_,
      base::BindOnce(&OnDeviceExecution::OnRequestSafetyResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(options)));
}

void OnDeviceExecution::OnRequestSafetyResult(
    on_device_model::mojom::GenerateOptionsPtr options,
    SafetyChecker::Result safety_result) {
  if (safety_result.failed_to_run) {
    FallbackToRemote(Result::kFailedConstructingMessage);
    return;
  }
  // Log the check executions.
  AddModelExecutionLogs(std::move(safety_result.logs));

  // Handle the result.
  if (safety_result.is_unsafe || safety_result.is_unsupported_language) {
    if (histogram_logger_) {
      histogram_logger_->set_result(Result::kRequestUnsafe);
    }
    if (features::GetOnDeviceModelRetractUnsafeContent()) {
      CancelPendingResponse(Result::kRequestUnsafe,
                            safety_result.is_unsupported_language
                                ? ModelExecutionError::kUnsupportedLanguage
                                : ModelExecutionError::kFiltered);
      return;
    }
  }
  BeginRequestExecution(std::move(options));
}

void OnDeviceExecution::BeginRequestExecution(
    on_device_model::mojom::GenerateOptionsPtr options) {
  session_->Generate(std::move(options), receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(base::BindOnce(
      &OnDeviceExecution::OnResponderDisconnect, base::Unretained(this)));
}

// on_device_model::mojom::StreamingResponder:
void OnDeviceExecution::OnResponse(
    on_device_model::mojom::ResponseChunkPtr chunk) {
  proto::OnDeviceModelServiceResponse* logged_response =
      MutableLoggedResponse();

  if (current_response_.empty()) {
    base::TimeDelta time_to_first_response = base::TimeTicks::Now() - start_;
    base::UmaHistogramMediumTimes(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.OnDeviceFirstResponseTime.",
             GetStringNameForModelExecutionFeature(feature_)}),
        time_to_first_response);
    logged_response->set_time_to_first_response_millis(
        time_to_first_response.InMilliseconds());
  }

  current_response_ += chunk->text;
  num_unchecked_response_tokens_++;
  num_response_tokens_++;

  if (HasRepeatingSuffix(current_response_)) {
    // If a repeat is detected, halt the response, and cancel/finish early.
    receiver_.reset();
    logged_response->set_has_repeats(true);
    if (features::GetOnDeviceModelRetractRepeats()) {
      LogRepeatedResponse(opts_.logger.get(), feature_, current_response_);
      logged_response->set_status(
          proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_RETRACTED);
      CancelPendingResponse(Result::kResponseHadRepeats,
                            ModelExecutionError::kResponseLowQuality);
      return;
    }

    // Artificially send the OnComplete event to finish processing.
    OnComplete(on_device_model::mojom::ResponseSummary::New());
    return;
  }

  if (!opts_.safety_checker->safety_cfg().CanCheckPartialOutput(
          num_response_tokens_, num_unchecked_response_tokens_)) {
    // Not enough new data to be worth re-evaluating yet.
    return;
  }

  num_unchecked_response_tokens_ = 0;
  RunRawOutputSafetyCheck(ResponseCompleteness::kPartial);
}

void OnDeviceExecution::OnComplete(
    on_device_model::mojom::ResponseSummaryPtr summary) {
  receiver_.reset();  // Suppress expected disconnect

  bool has_repeats = MutableLoggedResponse()->has_repeats();

  LogResponseHasRepeats(feature_, has_repeats);
  LogResponseCompleteTokens(feature_, num_response_tokens_);
  base::TimeDelta time_to_completion = base::TimeTicks::Now() - start_;
  LogResponseCompleteTime(feature_, time_to_completion);
  MutableLoggedResponse()->set_time_to_completion_millis(
      time_to_completion.InMilliseconds());

  output_token_count_ = summary->output_token_count;

  opts_.model_client->OnResponseCompleted();

  RunRawOutputSafetyCheck(ResponseCompleteness::kComplete);
}

void OnDeviceExecution::OnComplete(uint32_t tokens_processed) {
  MutableLoggedRequest()->set_execution_num_tokens_processed(tokens_processed);
}

void OnDeviceExecution::OnResponderDisconnect() {
  // OnComplete resets the receiver, so this implies that the response is
  // incomplete and there was either a service crash or model eviction.
  receiver_.reset();
  if (features::GetOnDeviceFallbackToServerOnDisconnect()) {
    FallbackToRemote(Result::kDisconnectAndMaybeFallback);
  } else {
    CancelPendingResponse(Result::kDisconnectAndCancel);
  }
}

void OnDeviceExecution::RunRawOutputSafetyCheck(
    ResponseCompleteness completeness) {
  opts_.safety_checker->RunRawOutputCheck(
      current_response_, completeness,
      base::BindOnce(&OnDeviceExecution::OnRawOutputSafetyResult,
                     weak_ptr_factory_.GetWeakPtr(), current_response_.size(),
                     completeness));
}

void OnDeviceExecution::OnRawOutputSafetyResult(
    size_t raw_output_size,
    ResponseCompleteness completeness,
    SafetyChecker::Result safety_result) {
  if (safety_result.failed_to_run) {
    FallbackToRemote(Result::kFailedConstructingMessage);
    return;
  }
  if (safety_result.is_unsafe || safety_result.is_unsupported_language) {
    if (opts_.safety_checker->safety_cfg()
            .OnlyCancelUnsafeResponseOnComplete() &&
        completeness != ResponseCompleteness::kComplete) {
      return;
    }
    if (histogram_logger_) {
      histogram_logger_->set_result(Result::kUsedOnDeviceOutputUnsafe);
    }
    AddModelExecutionLogs(std::move(safety_result.logs));
    if (features::GetOnDeviceModelRetractUnsafeContent()) {
      CancelPendingResponse(Result::kUsedOnDeviceOutputUnsafe,
                            safety_result.is_unsupported_language
                                ? ModelExecutionError::kUnsupportedLanguage
                                : ModelExecutionError::kFiltered);

      return;
    }
  }
  if (completeness == ResponseCompleteness::kComplete) {
    AddModelExecutionLogs(std::move(safety_result.logs));
  }
  latest_safe_raw_output_.length = raw_output_size;
  MaybeParseResponse(completeness);
}

void OnDeviceExecution::MaybeParseResponse(ResponseCompleteness completeness) {
  if (!opts_.adapter->ShouldParseResponse(completeness)) {
    return;
  }

  std::string safe_response =
      current_response_.substr(0, latest_safe_raw_output_.length);
  LogRawResponse(opts_.logger.get(), feature_, safe_response);
  MutableLoggedResponse()->set_output_string(safe_response);
  size_t previous_response_pos = latest_response_pos_;
  latest_response_pos_ = latest_safe_raw_output_.length;
  opts_.adapter->ParseResponse(
      last_message_, safe_response, previous_response_pos,
      base::BindOnce(&OnDeviceExecution::OnParsedResponse,
                     weak_ptr_factory_.GetWeakPtr(), completeness));
}

void OnDeviceExecution::OnParsedResponse(
    ResponseCompleteness completeness,
    base::expected<proto::Any, ResponseParsingError> output) {
  if (!output.has_value()) {
    switch (output.error()) {
      case ResponseParsingError::kRejectedPii:
        MutableLoggedResponse()->set_status(
            proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_RETRACTED);
        CancelPendingResponse(Result::kContainedPII,
                              ModelExecutionError::kFiltered);
        return;
      case ResponseParsingError::kFailed:
        CancelPendingResponse(Result::kFailedConstructingResponseMessage,
                              ModelExecutionError::kGenericFailure);
        return;
    }
  }
  opts_.safety_checker->RunResponseChecks(
      last_message_, *output, completeness,
      base::BindOnce(&OnDeviceExecution::OnResponseSafetyResult,
                     weak_ptr_factory_.GetWeakPtr(), completeness, *output));
}

void OnDeviceExecution::OnResponseSafetyResult(
    ResponseCompleteness completeness,
    proto::Any output,
    SafetyChecker::Result safety_result) {
  if (safety_result.failed_to_run) {
    FallbackToRemote(Result::kFailedConstructingMessage);
    return;
  }
  if (completeness == ResponseCompleteness::kComplete ||
      safety_result.is_unsafe || safety_result.is_unsupported_language) {
    AddModelExecutionLogs(std::move(safety_result.logs));
  }
  if (safety_result.is_unsafe || safety_result.is_unsupported_language) {
    if (opts_.safety_checker->safety_cfg()
            .OnlyCancelUnsafeResponseOnComplete() &&
        completeness != ResponseCompleteness::kComplete) {
      return;
    }
    if (histogram_logger_) {
      histogram_logger_->set_result(Result::kUsedOnDeviceOutputUnsafe);
    }
    if (features::GetOnDeviceModelRetractUnsafeContent()) {
      CancelPendingResponse(Result::kUsedOnDeviceOutputUnsafe,
                            safety_result.is_unsupported_language
                                ? ModelExecutionError::kUnsupportedLanguage
                                : ModelExecutionError::kFiltered);

      return;
    }
  }
  if (completeness == ResponseCompleteness::kPartial) {
    SendPartialResponseCallback(output);
    return;
  }

  SendSuccessCompletionCallback(output);
}

void OnDeviceExecution::FallbackToRemote(Result result) {
  if (histogram_logger_) {
    histogram_logger_->set_result(result);
  }
  auto self = weak_ptr_factory_.GetWeakPtr();
  execute_remote_fn_.Run(
      feature_, last_message_.BuildProtoMessage(), std::nullopt,
      std::make_unique<proto::LogAiDataRequest>(std::move(log_)),
      base::BindOnce(&InvokeStreamingCallbackWithRemoteResult,
                     std::move(callback_)));
  if (self) {
    self->Cleanup(/*healthy=*/false);
  }
}

void OnDeviceExecution::CancelPendingResponse(Result result,
                                              ModelExecutionError error) {
  if (!callback_) {
    return;
  }
  if (histogram_logger_) {
    histogram_logger_->set_result(result);
  }
  OptimizationGuideModelExecutionError og_error =
      OptimizationGuideModelExecutionError::FromModelExecutionError(error);
  std::unique_ptr<ModelQualityLogEntry> log_entry;
  std::unique_ptr<proto::ModelExecutionInfo> model_execution_info;
  if (og_error.ShouldLogModelQuality()) {
    log_entry = std::make_unique<ModelQualityLogEntry>(opts_.log_uploader);
    log_entry->log_ai_data_request()->MergeFrom(log_);
    std::string model_execution_id = GenerateExecutionId();
    log_entry->set_model_execution_id(model_execution_id);
    model_execution_info = std::make_unique<proto::ModelExecutionInfo>(
        log_entry->log_ai_data_request()->model_execution_info());
    model_execution_info->set_execution_id(model_execution_id);
    model_execution_info->set_model_execution_error_enum(
        static_cast<uint32_t>(og_error.error()));
  }
  auto self = weak_ptr_factory_.GetWeakPtr();
  std::move(callback_).Run(OptimizationGuideModelStreamingExecutionResult(
      base::unexpected(og_error), /*provided_by_on_device=*/true,
      std::move(log_entry), std::move(model_execution_info)));
  if (self) {
    self->Cleanup(/*healthy=*/true);
  }
}

void OnDeviceExecution::SendPartialResponseCallback(
    const proto::Any& success_response_metadata) {
  callback_.Run(OptimizationGuideModelStreamingExecutionResult(
      base::ok(StreamingResponse{.response = success_response_metadata,
                                 .is_complete = false}),
      /*provided_by_on_device=*/true, /*log_entry=*/nullptr));
}

void OnDeviceExecution::SendSuccessCompletionCallback(
    const proto::Any& success_response_metadata) {
  // Complete the log entry and promise it to the ModelQualityUploaderService.
  std::unique_ptr<ModelQualityLogEntry> log_entry;
  std::unique_ptr<proto::ModelExecutionInfo> model_execution_info;
  MutableLoggedResponse()->set_status(
      proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_SUCCESS);
  log_entry = std::make_unique<ModelQualityLogEntry>(opts_.log_uploader);
  log_entry->log_ai_data_request()->MergeFrom(log_);
  std::string model_execution_id = GenerateExecutionId();
  log_entry->set_model_execution_id(model_execution_id);
  model_execution_info =
      std::make_unique<proto::ModelExecutionInfo>(log_.model_execution_info());
  model_execution_info->set_execution_id(model_execution_id);
  log_.Clear();

  // Return the execution response.
  auto self = weak_ptr_factory_.GetWeakPtr();
  std::move(callback_).Run(OptimizationGuideModelStreamingExecutionResult(
      base::ok(StreamingResponse{.response = success_response_metadata,
                                 .is_complete = true,
                                 .output_token_count = output_token_count_}),
      /*provided_by_on_device=*/true, std::move(log_entry),
      std::move(model_execution_info)));
  if (self) {
    self->Cleanup(/*healthy=*/true);
  }
}

void OnDeviceExecution::Cleanup(bool healthy) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  session_.reset();
  receiver_.reset();
  context_receiver_.reset();
  callback_.Reset();
  log_.Clear();
  current_response_.clear();
  histogram_logger_.reset();
  std::move(cleanup_callback_).Run(healthy);
}

OnDeviceExecution::SafeRawOutput::SafeRawOutput() = default;
OnDeviceExecution::SafeRawOutput::~SafeRawOutput() = default;

OnDeviceExecution::ResultLogger::~ResultLogger() {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.",
           GetStringNameForModelExecutionFeature(feature_)}),
      result_);
}

}  // namespace optimization_guide
