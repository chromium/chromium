// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/session_impl.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/timer/elapsed_timer.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/redactor.h"
#include "components/optimization_guide/core/model_execution/repetition_checker.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_util.h"

namespace optimization_guide {
namespace {

using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

void LogResponseHasRepeats(proto::ModelExecutionFeature feature,
                           bool has_repeats) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats.",
           GetStringNameForModelExecutionFeature(feature)}),
      has_repeats);
}

std::string GenerateExecutionId() {
  return "on-device:" + base::Uuid::GenerateRandomV4().AsLowercaseString();
}

}  // namespace

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
    if (tokens_processed_ <
        static_cast<uint32_t>(
            features::GetOnDeviceModelMaxTokensForContext())) {
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
            /*max_output_tokens=*/std::nullopt,
            /*safety_interval=*/std::nullopt),
        client_.BindNewPipeAndPassRemote());
  }

  raw_ref<SessionImpl> session_;
  std::string input_;
  bool finished_processing_ = false;
  uint32_t expected_tokens_ = 0;
  uint32_t tokens_processed_ = 0;
  bool can_cancel_ = false;
  bool has_cancelled_ = false;
  mojo::Receiver<on_device_model::mojom::ContextClient> client_{this};
};

SessionImpl::SessionImpl(
    StartSessionFn start_session_fn,
    proto::ModelExecutionFeature feature,
    std::optional<proto::OnDeviceModelVersions> on_device_model_versions,
    const OnDeviceModelExecutionConfigInterpreter* config_interpreter,
    base::WeakPtr<OnDeviceModelServiceController> controller,
    const std::optional<proto::FeatureTextSafetyConfiguration>& safety_config,
    ExecuteRemoteFn execute_remote_fn,
    OptimizationGuideLogger* optimization_guide_logger)
    : controller_(controller),
      feature_(feature),
      on_device_model_versions_(on_device_model_versions),
      safety_config_(safety_config),
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
    DestroyOnDeviceState();
    execute_remote_fn_.Run(feature_, *last_message_,
                           /*log_ai_data_request=*/nullptr,
                           std::move(callback));
    return;
  }

  CHECK(on_device_model_versions_);
  *(log_ai_data_request->mutable_model_execution_info()
        ->mutable_on_device_model_execution_info()
        ->mutable_model_versions()) = *on_device_model_versions_;

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
  logger->set_result(ExecuteModelResult::kUsedOnDevice);
  on_device_state_->histogram_logger = std::move(logger);

  if (!input->should_ignore_input_context &&
      on_device_state_->context_processor) {
    logged_request->set_input_context_string(
        on_device_state_->context_processor->input());
  }
  logged_request->set_execution_string(input->input_string);
  // TODO(b/302327957): Probably do some math to get the accurate number here.
  logged_request->set_execution_num_tokens_processed(
      features::GetOnDeviceModelMaxTokensForOutput());

  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_logger_)
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
  on_device_state_->callback = std::move(callback);
  on_device_state_->start = base::TimeTicks::Now();
  on_device_state_->timer_for_first_response.Start(
      FROM_HERE, features::GetOnDeviceModelTimeForInitialResponse(),
      base::BindOnce(&SessionImpl::DestroyOnDeviceStateAndFallbackToRemote,
                     base::Unretained(this), ExecuteModelResult::kTimedOut));

  auto options = on_device_model::mojom::InputOptions::New();
  options->text = input->input_string;
  options->max_tokens = features::GetOnDeviceModelMaxTokensForExecute();
  options->ignore_context = input->should_ignore_input_context;
  options->max_output_tokens = features::GetOnDeviceModelMaxTokensForOutput();
  if (safety_config_) {
    options->safety_interval =
        features::GetOnDeviceModelTextSafetyTokenInterval();
  }
  GetOrCreateSession().Execute(
      std::move(options),
      on_device_state_->receiver.BindNewPipeAndPassRemote());
  on_device_state_->receiver.set_disconnect_handler(
      base::BindOnce(&SessionImpl::OnDisconnect, base::Unretained(this)));
}

// on_device_model::mojom::StreamingResponder:
void SessionImpl::OnResponse(on_device_model::mojom::ResponseChunkPtr chunk) {
  on_device_state_->timer_for_first_response.Stop();
  if (on_device_state_->current_response.empty()) {
    base::TimeDelta time_to_first_response =
        base::TimeTicks::Now() - on_device_state_->start;
    base::UmaHistogramMediumTimes(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.OnDeviceFirstResponseTime.",
             GetStringNameForModelExecutionFeature(feature_)}),
        time_to_first_response);
    on_device_state_->MutableLoggedResponse()
        ->set_time_to_first_response_millis(
            time_to_first_response.InMilliseconds());
  }

  if (!on_device_state_->MutableLoggedResponse()->has_repeats()) {
    // Only continue updating the response if repeats have not been detected.
    on_device_state_->current_response += chunk->text;

    // Check for repeats here instead of SendResponse since we see each new
    // token as it comes in here, and SendResponse will only see tokens if
    // safety info is available.
    int num_repeats = features::GetOnDeviceModelNumRepeats();
    if (num_repeats > 1 &&
        HasRepeatingSuffix(features::GetOnDeviceModelMinRepeatChars(),
                           num_repeats, on_device_state_->current_response)) {
      on_device_state_->MutableLoggedResponse()->set_has_repeats(true);
      LogResponseHasRepeats(feature_, true);
    }
  }

  bool chunk_provided_safety_info = false;
  if (chunk->safety_info) {
    on_device_state_->current_safety_info = std::move(chunk->safety_info);
    chunk_provided_safety_info = true;
  }

  // Only proceed to send the response if we are not evaluating text safety or
  // if there are text safety scores to evaluate.
  if (!safety_config_ || chunk_provided_safety_info) {
    SendResponse(ResponseType::kPartial);
  }
}

void SessionImpl::OnComplete(
    on_device_model::mojom::ResponseSummaryPtr summary) {
  base::TimeDelta time_to_completion =
      base::TimeTicks::Now() - on_device_state_->start;
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceResponseCompleteTime.",
           GetStringNameForModelExecutionFeature(feature_)}),
      time_to_completion);
  on_device_state_->MutableLoggedResponse()->set_time_to_completion_millis(
      time_to_completion.InMilliseconds());
  if (controller_) {
    controller_->access_controller(/*pass_key=*/{})->OnResponseCompleted();
  }

  if (safety_config_ && !summary->safety_info) {
    on_device_state_->receiver.ReportBadMessage(
        "Missing required safety scores on complete");
    CancelPendingResponse(
        ExecuteModelResult::kResponseCompleteButNoRequiredSafetyScores,
        ModelExecutionError::kGenericFailure);
    return;
  }

  if (summary->safety_info) {
    on_device_state_->current_safety_info = std::move(summary->safety_info);
  }
  SendResponse(ResponseType::kComplete);
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
  auto log_ai_data_request = std::move(on_device_state_->log_ai_data_request);
  on_device_state_->ResetRequestState();
  if (callback) {
    OptimizationGuideModelExecutionError og_error =
        OptimizationGuideModelExecutionError::FromModelExecutionError(error);
    std::unique_ptr<ModelQualityLogEntry> log_entry = nullptr;
    if (og_error.ShouldLogModelQuality()) {
      log_entry = std::make_unique<ModelQualityLogEntry>(
          std::move(log_ai_data_request));
      log_entry->set_model_execution_id(GenerateExecutionId());
    }
    callback.Run(base::unexpected(og_error), std::move(log_entry));
  }
}

void SessionImpl::SendResponse(ResponseType response_type) {
  on_device_state_->timer_for_first_response.Stop();
  if (!on_device_state_->callback) {
    on_device_state_->histogram_logger.get();
    return;
  }

  proto::OnDeviceModelServiceResponse* logged_response =
      on_device_state_->MutableLoggedResponse();

  std::string current_response = on_device_state_->current_response;
  logged_response->set_output_string(current_response);

  if (auto* redactor =
          on_device_state_->config_interpreter->GetRedactorForFeature(
              feature_)) {
    auto redact_string_input =
        on_device_state_->config_interpreter->GetStringToCheckForRedacting(
            feature_, *last_message_);
    base::ElapsedTimer elapsed_timer;
    auto redact_result =
        redactor->Redact(redact_string_input, current_response);
    base::UmaHistogramMicrosecondsTimes(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.TimeToProcessRedactions.",
             GetStringNameForModelExecutionFeature(feature_)}),
        elapsed_timer.Elapsed());
    if (redact_result == RedactResult::kReject) {
      if (on_device_state_->histogram_logger) {
        on_device_state_->histogram_logger->set_result(
            ExecuteModelResult::kContainedPII);
        on_device_state_->histogram_logger.reset();
      }
      logged_response->set_status(
          proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_RETRACTED);
      CancelPendingResponse(ExecuteModelResult::kUsedOnDeviceOutputUnsafe,
                            ModelExecutionError::kFiltered);
      return;
    }
  }

  const bool is_complete = response_type != ResponseType::kPartial;
  const bool is_unsupported_language =
      IsTextInUnsupportedOrUndeterminedLanguage(
          on_device_state_->current_safety_info);
  const bool is_unsafe = IsUnsafeText(on_device_state_->current_safety_info);
  if (is_unsafe || is_complete) {
    on_device_state_->AddTextSafetyExecutionLogging(is_unsafe);
  }
  if (is_unsafe || is_unsupported_language) {
    if (on_device_state_->histogram_logger) {
      on_device_state_->histogram_logger->set_result(
          ExecuteModelResult::kUsedOnDeviceOutputUnsafe);
    }

    if (features::GetOnDeviceModelRetractUnsafeContent()) {
      on_device_state_->current_response.clear();
      CancelPendingResponse(ExecuteModelResult::kUsedOnDeviceOutputUnsafe,
                            is_unsupported_language
                                ? ModelExecutionError::kUnsupportedLanguage
                                : ModelExecutionError::kFiltered);

      return;
    }
  }

  auto output = on_device_state_->config_interpreter->ConstructOutputMetadata(
      feature_, current_response);
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

  if (!is_complete &&
      on_device_state_->MutableLoggedResponse()->has_repeats()) {
    if (features::GetOnDeviceModelRetractRepeats()) {
      on_device_state_->current_response.clear();
      logged_response->set_status(
          proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_RETRACTED);
      CancelPendingResponse(ExecuteModelResult::kResponseHadRepeats,
                            ModelExecutionError::kFiltered);
      return;
    }

    // If a repeat is detected, halt the response, and artificially send the
    // OnComplete event.
    on_device_state_->receiver.reset();
    auto summary = on_device_model::mojom::ResponseSummary::New();
    if (on_device_state_->current_safety_info) {
      summary->safety_info = std::move(on_device_state_->current_safety_info);
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SessionImpl::OnComplete,
                       on_device_state_->session_weak_ptr_factory_.GetWeakPtr(),
                       std::move(summary)));
  } else if (is_complete &&
             !on_device_state_->MutableLoggedResponse()->has_repeats()) {
    // Log completed responses with no repeats to calculate percentage of
    // responses that have repeats.
    LogResponseHasRepeats(feature_, false);
  }

  std::unique_ptr<ModelQualityLogEntry> log_entry;
  if (is_complete) {
    // Only bother setting the full response if the request is complete.
    if (on_device_state_->log_ai_data_request) {
      SetExecutionResponse(feature_, *(on_device_state_->log_ai_data_request),
                           *output);
      logged_response->set_status(
          proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_SUCCESS);
      log_entry = std::make_unique<ModelQualityLogEntry>(
          std::move(on_device_state_->log_ai_data_request));
      log_entry->set_model_execution_id(GenerateExecutionId());
      on_device_state_->log_ai_data_request.reset();
    }
  }
  on_device_state_->callback.Run(
      StreamingResponse{
          .response = *output,
          .is_complete = is_complete,
          .provided_by_on_device = true,
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

bool SessionImpl::IsTextInUnsupportedOrUndeterminedLanguage(
    const on_device_model::mojom::SafetyInfoPtr& safety_info) const {
  if (!safety_config_) {
    // No safety config, so no language requirements.
    return false;
  }

  CHECK(safety_config_);
  if (safety_config_->allowed_languages().empty()) {
    // No language requirements.
    return false;
  }

  CHECK(safety_info);
  if (!safety_info->language) {
    // No language detection available, but language detection is required.
    // Treat as an unsupported language.
    return true;
  }

  if (!base::Contains(safety_config_->allowed_languages(),
                      safety_info->language->code)) {
    // Unsupported language.
    return true;
  }

  if (safety_info->language->reliability <
      features::GetOnDeviceModelLanguageDetectionMinimumReliability()) {
    // Unreliable language detection. Treat as an unsupported language.
    return true;
  }

  // Language was detected reliably and is supported.
  return false;
}

bool SessionImpl::IsUnsafeText(
    const on_device_model::mojom::SafetyInfoPtr& safety_info) const {
  if (!safety_config_) {
    // If no safety config and we are allowed here, that means we don't care
    // about the safety scores so just mark the content as safe.
    return false;
  }

  CHECK(safety_info);
  CHECK(!safety_info->class_scores.empty());
  for (const auto& threshold : safety_config_->safety_category_thresholds()) {
    size_t output_index = static_cast<size_t>(threshold.output_index());
    if (static_cast<size_t>(output_index) >= safety_info->class_scores.size()) {
      // Needed to evaluate a score, but output was invalid. Mark it as unsafe.
      return true;
    }

    if (safety_info->class_scores.at(output_index) >= threshold.threshold()) {
      // Output score exceeded threshold.
      return true;
    }
  }

  // If it gets here, everything has passed.
  return false;
}

SessionImpl::OnDeviceState::OnDeviceState(StartSessionFn start_session_fn,
                                          SessionImpl* session)
    : start_session_fn(std::move(start_session_fn)),
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

void SessionImpl::OnDeviceState::AddTextSafetyExecutionLogging(bool is_unsafe) {
  if (!current_safety_info) {
    return;
  }

  CHECK(log_ai_data_request);

  auto* ts_execution_info = log_ai_data_request->mutable_model_execution_info()
                                ->mutable_on_device_model_execution_info()
                                ->add_execution_infos();
  ts_execution_info->mutable_request()
      ->mutable_text_safety_model_request()
      ->set_text(current_response);
  auto* ts_resp = ts_execution_info->mutable_response()
                      ->mutable_text_safety_model_response();
  *ts_resp->mutable_scores() = {current_safety_info->class_scores.begin(),
                                current_safety_info->class_scores.end()};
  ts_resp->set_is_unsafe(is_unsafe);
}

void SessionImpl::OnDeviceState::ResetRequestState() {
  receiver.reset();
  callback.Reset();
  current_response.clear();
  current_safety_info.reset();
  start = base::TimeTicks();
  timer_for_first_response.Stop();
  histogram_logger.reset();
  log_ai_data_request.reset();
  session_weak_ptr_factory_.InvalidateWeakPtrs();
}

SessionImpl::ExecuteModelHistogramLogger::~ExecuteModelHistogramLogger() {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.",
           GetStringNameForModelExecutionFeature(feature_)}),
      result_);
}

}  // namespace optimization_guide
