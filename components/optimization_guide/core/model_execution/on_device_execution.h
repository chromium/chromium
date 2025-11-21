// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_EXECUTION_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_EXECUTION_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_context.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/repetition_checker.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/model_execution/substitution.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

// The state for an ongoing ExecuteModel() call.
class OnDeviceExecution final
    : public on_device_model::mojom::StreamingResponder,
      public on_device_model::mojom::ContextClient {
 public:
  // Possible outcomes of ExecuteModel().
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Result {
    // On-device was not used.
    kOnDeviceNotUsed = 0,
    // On-device was used, and it completed successfully.
    kUsedOnDevice = 1,
    // Failed constructing message, and used server.
    kFailedConstructingMessage = 2,
    // Got a response from on-device, but failed constructing the message.
    kFailedConstructingResponseMessage = 3,
    // Timed out and used server.
    kTimedOut = 4,
    // Received a disconnect while waiting for response. This may trigger
    // fallback to another model, e.g. on the server, if configured.
    kDisconnectAndMaybeFallback = 5,
    // Received a disconnect while waiting for response and cancelled.
    kDisconnectAndCancel = 6,
    // Response was cancelled because ExecuteModel() was called while waiting
    // for response.
    kCancelled = 7,
    // SessionImpl was destroyed while waiting for a response.
    kDestroyedWhileWaitingForResponse = 8,
    // On-device was used, it completed successfully, but the output is
    // considered unsafe.
    kUsedOnDeviceOutputUnsafe = 9,
    // On-device was used, but the output was rejected (because contained PII).
    kContainedPII = 10,
    // On-device was used, but the output was rejected because it had repeats.
    kResponseHadRepeats = 11,
    // On-device was used and the output was complete but the output was
    // rejected since it did not have the required safety scores.
    kResponseCompleteButNoRequiredSafetyScores = 12,
    // On-device was used and completed successfully, but the output was not in
    // a language that could be reliably evaluated for safety.
    kUsedOnDeviceOutputUnsupportedLanguage = 13,
    // On-device was used and completed successfully, but failed constructing
    // the text safety remote request.
    kFailedConstructingRemoteTextSafetyRequest = 14,
    // On-device was used and completed successfully, but the text safety remote
    // request failed for some reason.
    kTextSafetyRemoteRequestFailed = 15,
    // On-device was used, but the request was considered unsafe.
    kRequestUnsafe = 16,

    // Please update OptimizationGuideOnDeviceResult in
    // optimization/enums.xml.
    kMaxValue = kRequestUnsafe,
  };

  // Used to log the result of ExecuteModel.
  class ResultLogger {
   public:
    explicit ResultLogger(mojom::OnDeviceFeature feature) : feature_(feature) {}
    ~ResultLogger();

    void set_result(Result result) { result_ = result; }

   private:
    const mojom::OnDeviceFeature feature_;
    Result result_ = Result::kOnDeviceNotUsed;
  };

  explicit OnDeviceExecution(
      mojom::OnDeviceFeature feature,
      OnDeviceOptions opts,
      MultimodalMessage message,
      on_device_model::mojom::ResponseConstraintPtr constraint,
      std::unique_ptr<ResultLogger> logger,
      OptimizationGuideModelExecutionResultStreamingCallback callback,
      base::OnceCallback<void(bool)> cleanup_callback);
  ~OnDeviceExecution() final;

  // Begin processing the request.
  void BeginExecution(OnDeviceContext& context);

  // Cancels the execution.
  void Cancel();

 private:
  // Returns the mutable on-device model service request for logging.
  proto::OnDeviceModelServiceRequest* MutableLoggedRequest();

  // Returns the mutable on-device model service response for logging.
  proto::OnDeviceModelServiceResponse* MutableLoggedResponse();

  // Adds a collection of model execution logs to the request log.
  void AddModelExecutionLogs(google::protobuf::RepeatedPtrField<
                             proto::InternalOnDeviceModelExecutionInfo> logs);

  // Callback invoked with RequestSafetyCheck result.
  // Calls BeginRequestExecution if safety checks pass.
  void OnRequestSafetyResult(on_device_model::mojom::GenerateOptionsPtr options,
                             SafetyChecker::Result safety_result);

  // Begins request execution (leads to OnResponse/OnComplete, which will
  // call RunRawOutputSafetyCheck).
  void BeginRequestExecution(
      on_device_model::mojom::GenerateOptionsPtr options);

  // on_device_model::mojom::StreamingResponder:
  void OnResponse(on_device_model::mojom::ResponseChunkPtr chunk) override;
  void OnComplete(on_device_model::mojom::ResponseSummaryPtr summary) override;

  // on_device_model::mojom::ContextClient:
  void OnComplete(uint32_t tokens_processed) override;

  void OnResponderDisconnect();

  // Evaluates raw output safety (leads to OnRawOutputSafetyResult).
  void RunRawOutputSafetyCheck(ResponseCompleteness completeness);

  // Called when output safety check completes.
  // Calls MaybeParseResponse when there is more safe output.
  void OnRawOutputSafetyResult(size_t raw_output_size,
                               ResponseCompleteness completeness,
                               SafetyChecker::Result safety_result);

  // Called to parse the latest safe raw output.
  // Leads to OnParsedResponse.
  void MaybeParseResponse(ResponseCompleteness completeness);

  // Called when a response has finished parsing.
  // Begins response safety evaluation, leads to OnResponseSafetyResult.
  void OnParsedResponse(
      ResponseCompleteness completeness,
      base::expected<proto::Any, ResponseParsingError> output);

  // Called when response safety check completes.
  // Either fails, sends the result or calls RunTextSafetyRemoteFallback.
  void OnResponseSafetyResult(ResponseCompleteness completeness,
                              proto::Any output,
                              SafetyChecker::Result safety_result);

  // Terminates on-device processing as unhealthy and falls back to remote
  // execution to provide the result to the caller.
  void FallbackToRemote(Result result);

  // Sends an error result and terminates on-device processing as healthy.
  void CancelPendingResponse(
      Result result,
      OptimizationGuideModelExecutionError::ModelExecutionError error =
          OptimizationGuideModelExecutionError::ModelExecutionError::
              kCancelled);

  // Sends the partial response callback, and does NOT terminate processing.
  void SendPartialResponseCallback(const proto::Any& success_response_metadata);

  // Sends a successful result and terminates on-device processing as healthy.
  void SendSuccessCompletionCallback(
      const proto::Any& success_response_metadata);

  // Called after terminating to release all held resources and notify owner
  // that this object is safe to destroy.
  void Cleanup(bool healthy);

  const mojom::OnDeviceFeature feature_;
  const OnDeviceOptions opts_;

  mojo::Remote<on_device_model::mojom::Session> session_;

  // The request message.
  MultimodalMessage last_message_;
  // A constraint defining structured output requirements for the response.
  on_device_model::mojom::ResponseConstraintPtr constraint_;
  // Time ExecuteModel() was called.
  base::TimeTicks start_;
  // Time we receive the first token.
  base::TimeTicks first_response_time_;
  // Used to log the result of ExecuteModel().
  std::unique_ptr<ResultLogger> histogram_logger_;
  // Used to log execution information for the request.
  proto::ModelExecutionInfo exec_log_;

  // Response received so far.
  std::string current_response_;

  // How many tokens (response chunks) have been added.
  size_t num_response_tokens_ = 0;
  // How many tokens (response chunks) have been added since the last safety
  // evaluation was requested.
  size_t num_unchecked_response_tokens_ = 0;

  struct SafeRawOutput {
    SafeRawOutput();
    ~SafeRawOutput();
    // How much of 'current_response' was checked.
    size_t length = 0;
  };
  // The longest response that has passed the raw output text safety check.
  SafeRawOutput latest_safe_raw_output_;

  // The last position in the response that has been streamed to the
  // responder.
  size_t latest_response_pos_ = 0;

  // The number of tokens in the returned output.
  size_t output_token_count_ = 0;
  // The number of tokens in execute portion of the input.
  size_t execute_input_token_count_ = 0;

  // A buffer to hold trailing newlines.
  NewlineBuffer newline_buffer_;

  // Callback to provide the execution result.
  OptimizationGuideModelExecutionResultStreamingCallback callback_;

  // Callback to notify the owning session that on-device execution has
  // terminated, and that this object is safe to destroy.
  // Should pass true to indicate healthy completion, or false if unhealthy.
  base::OnceCallback<void(bool)> cleanup_callback_;

  mojo::Receiver<on_device_model::mojom::StreamingResponder> receiver_{this};
  mojo::Receiver<on_device_model::mojom::ContextClient> context_receiver_{this};

  // Factory for weak pointers related to this session that are invalidated
  // with the request state.
  base::WeakPtrFactory<OnDeviceExecution> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_EXECUTION_H_
