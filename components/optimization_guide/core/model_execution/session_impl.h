// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SESSION_IMPL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SESSION_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

class OptimizationGuideLogger;

namespace optimization_guide {
class OnDeviceModelExecutionConfigInterpreter;
class OnDeviceModelServiceController;

using ExecuteRemoteFn = base::RepeatingCallback<void(
    proto::ModelExecutionFeature feature,
    const google::protobuf::MessageLite&,
    std::unique_ptr<proto::LogAiDataRequest>,
    OptimizationGuideModelExecutionResultStreamingCallback)>;

// Session implementation that uses either the on device model or the server
// model.
class SessionImpl : public OptimizationGuideModelExecutor::Session,
                        public on_device_model::mojom::StreamingResponder {
 public:
  using StartSessionFn = base::RepeatingCallback<void(
      mojo::PendingReceiver<on_device_model::mojom::Session>)>;

  // Possible outcomes of AddContext(). Maps to histogram enum
  // "OptimizationGuideOnDeviceAddContextResult".
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AddContextResult {
    kUsingServer = 0,
    kUsingOnDevice = 1,
    kFailedConstructingInput = 2,
    kMaxValue = kFailedConstructingInput,
  };

  // Possible outcomes of ExecuteModel().
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ExecuteModelResult {
    // The server was used.
    kUsedServer = 0,
    // On-device was used, and it completed successfully.
    kUsedOnDevice = 1,
    // Failed constructing message, and used server.
    kFailedConstructingMessage = 2,
    // Got a response from on-device, but failed constructing the message.
    kFailedConstructingResponseMessage = 3,
    // Timed out and used server.
    kTimedOut = 4,
    // Received a disconnect while waiting for response and used server.
    kDisconnectAndFallbackToServer = 5,
    // Received a disconnect whiel waiting for response and cancelled.
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

    // Please update OptimizationGuideOnDeviceExecuteModelResult in
    // optimization/enums.xml.

    kMaxValue = kUsedOnDeviceOutputUnsupportedLanguage,
  };

  SessionImpl(
      StartSessionFn start_session_fn,
      proto::ModelExecutionFeature feature,
      std::optional<proto::OnDeviceModelVersions> on_device_model_versions,
      const OnDeviceModelExecutionConfigInterpreter* config_interpreter,
      base::WeakPtr<OnDeviceModelServiceController> controller,
      const std::optional<proto::FeatureTextSafetyConfiguration>& safety_config,
      ExecuteRemoteFn execute_remote_fn,
      OptimizationGuideLogger* optimization_guide_logger);
  ~SessionImpl() override;

  // optimization_guide::OptimizationGuideModelExecutor::Session:
  void AddContext(
      const google::protobuf::MessageLite& request_metadata) override;
  void ExecuteModel(
      const google::protobuf::MessageLite& request_metadata,
      OptimizationGuideModelExecutionResultStreamingCallback callback) override;

  // on_device_model::mojom::StreamingResponder:
  void OnResponse(on_device_model::mojom::ResponseChunkPtr chunk) override;
  void OnComplete(on_device_model::mojom::ResponseSummaryPtr summary) override;

  // Returns true if the on-device model should be used.
  bool ShouldUseOnDeviceModel() const;

 private:
  class ContextProcessor;

  // Type of response.
  enum class ResponseType {
    // This is a partial response. That is, one of `kComplete` or
    // `kCompleteUnsafeOutput` will follow.
    kPartial,

    // The response completed successfully.
    kComplete,

    // The response completed, but the output is considered unsafe.
    kCompleteUnsafeOutput,
  };

  // Used to log the result of ExecuteModel.
  class ExecuteModelHistogramLogger {
   public:
    explicit ExecuteModelHistogramLogger(proto::ModelExecutionFeature feature)
        : feature_(feature) {}
    ~ExecuteModelHistogramLogger();

    void set_result(ExecuteModelResult result) { result_ = result; }

   private:
    const proto::ModelExecutionFeature feature_;
    ExecuteModelResult result_ = ExecuteModelResult::kUsedServer;
  };

  // Captures all state used for the on device model.
  struct OnDeviceState {
    OnDeviceState(StartSessionFn start_session_fn, SessionImpl* session);
    ~OnDeviceState();

    // Returns true if ExecuteModel() was called and the complete response
    // has not been received.
    bool did_execute_and_waiting_for_on_complete() const {
      return start != base::TimeTicks();
    }

    // Returns the mutable on-device model service response for logging.
    proto::OnDeviceModelServiceResponse* MutableLoggedResponse();

    // Adds an execution info for the text safety model based on `this`.
    void AddTextSafetyExecutionLogging(bool is_unsafe);

    // Resets all state related to a request.
    void ResetRequestState();

    mojo::Remote<on_device_model::mojom::Session> session;
    raw_ptr<const OnDeviceModelExecutionConfigInterpreter> config_interpreter;
    StartSessionFn start_session_fn;
    std::unique_ptr<ContextProcessor> context_processor;
    mojo::Receiver<on_device_model::mojom::StreamingResponder> receiver;
    std::string current_response;
    on_device_model::mojom::SafetyInfoPtr current_safety_info;
    OptimizationGuideModelExecutionResultStreamingCallback callback;
    // If true, the context is added before execution. This is set to true if
    // a disconnect happens.
    bool add_context_before_execute = false;
    // Time ExecuteModel() was called.
    base::TimeTicks start;
    // Timer used to detect when no response has been received and fallback
    // to remote execution.
    base::OneShotTimer timer_for_first_response;
    // Used to log the result of ExecuteModel().
    std::unique_ptr<ExecuteModelHistogramLogger> histogram_logger;
    // Used to log execution information for the request.
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request;

    // Factory for weak pointers related to this session that are invalidated
    // with the request state.
    base::WeakPtrFactory<SessionImpl> session_weak_ptr_factory_;
  };

  AddContextResult AddContextImpl(
      const google::protobuf::MessageLite& request_metadata);

  // Gets the active session or restarts a session if the session is reset.
  on_device_model::mojom::Session& GetOrCreateSession();

  // Cancels any pending response and resets response state.
  void CancelPendingResponse(
      ExecuteModelResult result,
      OptimizationGuideModelExecutionError::ModelExecutionError error =
          OptimizationGuideModelExecutionError::ModelExecutionError::
              kCancelled);

  // Called when the connection to the service is dropped.
  void OnDisconnect();

  // Sends `current_response_` to the client.
  void SendResponse(ResponseType response_type);

  void DestroyOnDeviceStateAndFallbackToRemote(ExecuteModelResult result);

  void DestroyOnDeviceState();

  // Returns a new message created by merging `request` into `context_`. This
  // is a bit tricky since we don't know the type of MessageLite.
  std::unique_ptr<google::protobuf::MessageLite> MergeContext(
      const google::protobuf::MessageLite& request);

  // Whether the text is in a language not supported by the safety classifier,
  // or the language could not be detected despite the classifier requiring one
  // or more specific languages.
  bool IsTextInUnsupportedOrUndeterminedLanguage(
      const on_device_model::mojom::SafetyInfoPtr& safety_info) const;

  // Whether the text is unsafe.
  bool IsUnsafeText(
      const on_device_model::mojom::SafetyInfoPtr& safety_info) const;

  base::WeakPtr<OnDeviceModelServiceController> controller_;
  const proto::ModelExecutionFeature feature_;
  const std::optional<proto::OnDeviceModelVersions> on_device_model_versions_;

  std::optional<proto::FeatureTextSafetyConfiguration> safety_config_;

  ExecuteRemoteFn execute_remote_fn_;

  std::unique_ptr<google::protobuf::MessageLite> context_;
  base::TimeTicks context_start_time_;

  // Last message executed.
  std::unique_ptr<google::protobuf::MessageLite> last_message_;

  // Has a value when using the on device model.
  std::optional<OnDeviceState> on_device_state_;

  // Logger is owned by the Optimization Guide Keyed Service, which should
  // outlive this session.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SESSION_IMPL_H_
