// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace optimization_guide {

// The result type of model execution.
using OptimizationGuideModelExecutionResult =
    base::expected<const proto::Any /*response_metadata*/,
                   OptimizationGuideModelExecutionError>;

// A response type used for OptimizationGuideModelExecutor::Session.
struct StreamingResponse {
  // The response proto. This may be incomplete until `is_complete` is true.
  // This will contain the full response up to this point in the stream. Callers
  // should replace any previous streamed response with the new value while
  // `is_complete` is false.
  const proto::Any response;

  // True if streaming has finished.
  bool is_complete = false;
};

struct OptimizationGuideModelStreamingExecutionResult {
  OptimizationGuideModelStreamingExecutionResult();
  explicit OptimizationGuideModelStreamingExecutionResult(
      base::expected<const StreamingResponse,
                     OptimizationGuideModelExecutionError> response,
      bool provided_by_on_device,
      std::unique_ptr<ModelQualityLogEntry> log_entry = nullptr);

  ~OptimizationGuideModelStreamingExecutionResult();
  OptimizationGuideModelStreamingExecutionResult(
      OptimizationGuideModelStreamingExecutionResult&& src);

  base::expected<const StreamingResponse, OptimizationGuideModelExecutionError>
      response;
  // True if the response was computed on-device.
  bool provided_by_on_device = false;
  // The log entry will be null until `StreamingResponse.is_complete` is true.
  std::unique_ptr<ModelQualityLogEntry> log_entry;
};

// The callback for receiving the model execution result and model quality log
// entry.
using OptimizationGuideModelExecutionResultCallback =
    base::OnceCallback<void(OptimizationGuideModelExecutionResult,
                            std::unique_ptr<ModelQualityLogEntry>)>;

// A callback for receiving a score from the model, or nullopt if the model
// is not running.
using OptimizationGuideModelScoreCallback =
    base::OnceCallback<void(std::optional<float>)>;

// The callback for receiving streamed output from the model. The log entry will
// be null until `StreamingResponse.is_complete` is true.
using OptimizationGuideModelExecutionResultStreamingCallback =
    base::RepeatingCallback<void(
        OptimizationGuideModelStreamingExecutionResult)>;

// The callback for receiving the token size of the given input.
using OptimizationGuideModelSizeInTokenCallback =
    base::OnceCallback<void(uint32_t)>;

// Params used to control sampling output tokens for the on-device model.
struct SamplingParams {
  uint32_t top_k = 1;
  float temperature = 0.0f;
};

// Params to control model config per-session.
struct SessionConfigParams {
  std::optional<SamplingParams> sampling_params;

  enum class ExecutionMode {
    // Allows for infrastructure to choose what is most appropriate.
    kDefault,
    // Only allows for on-device execution.
    kOnDeviceOnly,
    // Only allows for server execution.
    kServerOnly,
  };

  // How the execution of this feature should be configured.
  ExecutionMode execution_mode = ExecutionMode::kDefault;

  // The amount of time to wait before the initial response is received from the
  // on device model. If unset, a default value will be used.
  //
  // If `execution_mode` allows, model execution will fall back to the server
  // instead of failing entirely when this timeout is reached.
  std::optional<base::TimeDelta> on_device_execution_timeout;
};

// Reasons why the on-device model was not available for use.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OnDeviceModelEligibilityReason {
  kUnknown = 0,
  // Success.
  kSuccess = 1,
  // The feature flag gating on-device model execution was disabled.
  kFeatureNotEnabled = 2,
  // There was no on-device model available.
  kModelNotAvailable = 3,
  // The on-device model was available but there was not an execution config
  // available for the feature.
  kConfigNotAvailableForFeature = 4,
  // The GPU is blocked.
  kGpuBlocked = 5,
  // The on-device model process crashed too many times for this version.
  kTooManyRecentCrashes = 6,
  // The on-device model took too long too many times for this version.
  kTooManyRecentTimeouts = 7,
  // The on-device safety model was required but not available.
  kSafetyModelNotAvailable = 8,
  // The on-device safety model was available but there was not a safety config
  // available for the feature.
  kSafetyConfigNotAvailableForFeature = 9,
  // The on-device language detection model was required but not available.
  kLanguageDetectionModelNotAvailable = 10,
  // On-device model execution for this feature was not enabled.
  kFeatureExecutionNotEnabled = 11,
  // On-device model adaptation was required but not available.
  kModelAdaptationNotAvailable = 12,
  // Validation has not completed for the model yet.
  kValidationPending = 13,
  // Validation failed for the model.
  kValidationFailed = 14,
  // There was no on-device model available, but it may be downloaded and
  // installed later.
  kModelToBeInstalled = 15,

  // This must be kept in sync with
  // OptimizationGuideOnDeviceModelEligibilityReason in optimization/enums.xml.

  // Insert new values before this line.
  kMaxValue = kModelToBeInstalled,
};

// Observer that is notified when the on-device model availability changes for
// the on-device eligible features.
class OnDeviceModelAvailabilityObserver : public base::CheckedObserver {
 public:
  // Notifies the consumers whenever the on-device model availability for the
  // `feature` changes. `reason` indicates the current availability of the
  // model. This could be invoked without the model availability state toggling.
  // This is not called automatically when the observer is added initially.
  // Consumers should call `OnDeviceModelServiceController::CanCreateSession` to
  // check the initial (or current) model availability state.
  virtual void OnDeviceModelAvailabilityChanged(
      ModelBasedCapabilityKey feature,
      OnDeviceModelEligibilityReason reason) = 0;
};

// The model's configured limits on tokens.
struct TokenLimits {
  // The full combined limit for input and output tokens.
  uint32_t max_tokens = 0;
  // The maximum number of tokens that can be used by AddContext.
  uint32_t max_context_tokens = 0;
  // The maximum number of tokens that can be used by ExecuteModel.
  uint32_t max_execute_tokens = 0;
  // The maximum number of tokens that can be generated as output.
  uint32_t max_output_tokens = 0;
};

// Interface for model execution.
class OptimizationGuideModelExecutor {
 public:
  virtual ~OptimizationGuideModelExecutor() = default;

  // A model session that will save context for future ExecuteModel() calls.
  class Session {
   public:
    virtual ~Session() = default;

    virtual const TokenLimits& GetTokenLimits() const = 0;

    // Adds context to this session. This will be saved for future Execute()
    // calls. Calling multiple times will replace previous calls to
    // AddContext(). Calling this while a ExecuteModel() call is still streaming
    // a response will cancel the ongoing ExecuteModel() call by calling its
    // `callback` with the kCancelled error.
    virtual void AddContext(
        const google::protobuf::MessageLite& request_metadata) = 0;

    // Gets the probability score of the first token in `text` on top of the
    // current context. Returns nullopt if there is no on-device session (such
    // as due to a disconnect).
    virtual void Score(const std::string& text,
                       OptimizationGuideModelScoreCallback callback) = 0;

    // Execute the model with `request_metadata` and streams the result to
    // `callback`. The execute call will include context from the last
    // AddContext() call. Data provided to the last AddContext() call does not
    // need to be provided here. Calling this while another ExecuteModel() call
    // is still streaming a response will cancel the previous call by calling
    // `callback` with the kCancelled error.
    virtual void ExecuteModel(
        const google::protobuf::MessageLite& request_metadata,
        OptimizationGuideModelExecutionResultStreamingCallback callback) = 0;

    // Call `GetSizeInTokens()` from the model to get the size of the given text
    // in tokens. The result will be passed back through the callback.
    virtual void GetSizeInTokens(
        const std::string& text,
        OptimizationGuideModelSizeInTokenCallback callback) = 0;

    // Gets the size in tokens used by request_metadata as it would be formatted
    // by a call to `AddContext()`. The result will be passed back through the
    // callback.
    virtual void GetContextSizeInTokens(
        const google::protobuf::MessageLite& request_metadata,
        OptimizationGuideModelSizeInTokenCallback callback) = 0;

    // Return the sampling params for the current session.
    virtual const SamplingParams GetSamplingParams() const = 0;

    // Returns the feature_metadata from the
    // OnDeviceModelExecutionFeatureConfig.
    virtual const proto::Any& GetOnDeviceFeatureMetadata() const = 0;
  };

  // Whether an on-device session can be created for `feature`. An optional
  // `on_device_model_eligibility_reason` parameter can be provided for more
  // detailed reasons for why an on-device session could not be created.
  virtual bool CanCreateOnDeviceSession(
      ModelBasedCapabilityKey feature,
      OnDeviceModelEligibilityReason* on_device_model_eligibility_reason) = 0;

  // Starts a session which allows streaming input and output from the model.
  // May return nullptr if model execution is not supported. This session should
  // not outlive OptimizationGuideModelExecutor.
  virtual std::unique_ptr<Session> StartSession(
      ModelBasedCapabilityKey feature,
      const std::optional<SessionConfigParams>& config_params) = 0;

  // Executes the model for `feature` with `request_metadata` and invokes the
  // `callback` with the result.
  virtual void ExecuteModel(
      ModelBasedCapabilityKey feature,
      const google::protobuf::MessageLite& request_metadata,
      OptimizationGuideModelExecutionResultCallback callback) = 0;

  // Observer for on-device model availability changes.
  virtual void AddOnDeviceModelAvailabilityChangeObserver(
      optimization_guide::ModelBasedCapabilityKey feature,
      OnDeviceModelAvailabilityObserver* observer) {}
  virtual void RemoveOnDeviceModelAvailabilityChangeObserver(
      optimization_guide::ModelBasedCapabilityKey feature,
      OnDeviceModelAvailabilityObserver* observer) {}
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_
