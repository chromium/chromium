// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_CAPABILITY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_CAPABILITY_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

class OptimizationGuideLogger;

namespace optimization_guide {

// A response type used for OnDeviceSession.
struct StreamingResponse {
  // The response proto. This may be incomplete until `is_complete` is true.
  // This will contain the full response up to this point in the stream. Callers
  // should replace any previous streamed response with the new value while
  // `is_complete` is false.
  const proto::Any response;

  // True if streaming has finished.
  bool is_complete = false;

  // The number of tokens in this response's input. Note this only includes
  // tokens input to the Execute() call, and not the total context tokens.
  size_t input_token_count = 0;
  // The number of tokens in this response.
  size_t output_token_count = 0;
};

struct OptimizationGuideModelStreamingExecutionResult {
  OptimizationGuideModelStreamingExecutionResult();
  explicit OptimizationGuideModelStreamingExecutionResult(
      base::expected<const StreamingResponse,
                     OptimizationGuideModelExecutionError> response,
      bool provided_by_on_device,
      std::unique_ptr<proto::ModelExecutionInfo> execution_info = nullptr);

  ~OptimizationGuideModelStreamingExecutionResult();
  OptimizationGuideModelStreamingExecutionResult(
      OptimizationGuideModelStreamingExecutionResult&& src);

  base::expected<const StreamingResponse, OptimizationGuideModelExecutionError>
      response;

  bool provided_by_on_device = false;

  // The execution info will be null until `StreamingResponse.is_complete` is
  // true.
  std::unique_ptr<proto::ModelExecutionInfo> execution_info;
};

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
    base::OnceCallback<void(std::optional<uint32_t>)>;

// Params used to control sampling output tokens for the on-device model.
struct SamplingParams {
  uint32_t top_k = 1;
  float temperature = 0.0f;
};

// Params to control model config per-session.
struct SessionConfigParams {
  std::optional<SamplingParams> sampling_params;

  // Capabilities that are enabled for this session when using on-device
  // execution.
  on_device_model::Capabilities capabilities;
};

// Reasons why the on-device model was not available for use.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(OnDeviceModelEligibilityReason)
enum class OnDeviceModelEligibilityReason {
  kUnknown = 0,
  // Success.
  kSuccess = 1,
  // The feature flag gating on-device model execution was disabled.
  kFeatureNotEnabled = 2,
  // DEPRECATED: split into kModelNotEligible, kInsufficientDiskSpace and
  // kNoOnDeviceFeatureUsed.
  // There was no on-device model available.
  kDeprecatedModelNotAvailable = 3,
  // The on-device model was available but there was not an execution config
  // available for the feature.
  kConfigNotAvailableForFeature = 4,
  // The GPU is blocked.
  kGpuBlocked = 5,
  // The on-device model process crashed too many times for this version.
  kTooManyRecentCrashes = 6,
  // DEPRECATED
  // The on-device model took too long too many times for this version.
  // kTooManyRecentTimeouts = 7,
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
  //
  // Since model adaptations are bundled with feature configs, for general
  // error cases when server cannot provide model adaptation,
  // `kConfigNotAvailableForFeature` is returned instead.
  // `kModelAdaptationNotAvailable` is only emitted in certain special cases
  // (e.g. if the requested capability will never be supported by the device).
  kModelAdaptationNotAvailable = 12,
  // Validation has not completed for the model yet.
  kValidationPending = 13,
  // Validation failed for the model.
  kValidationFailed = 14,
  // There was no on-device model available, but it may be downloaded and
  // installed later.
  kModelToBeInstalled = 15,
  // The device is not eligible for running the on-device model.
  kModelNotEligible = 16,
  // The device does not have enough space to download and install the
  // on-device model.
  kInsufficientDiskSpace = 17,
  // There was no on-device feature usage so the model has not been
  // downloaded yet.
  kNoOnDeviceFeatureUsed = 18,

  // Insert new values before this line.
  kMaxValue = kNoOnDeviceFeatureUsed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/enums.xml:OptimizationGuideOnDeviceModelEligibilityReason)

std::ostream& operator<<(std::ostream& out,
                         const OnDeviceModelEligibilityReason& val);

// Simplify an eligibility reason to an availability state.
std::optional<mojom::ModelUnavailableReason> AvailabilityFromEligibilityReason(
    OnDeviceModelEligibilityReason);

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
      mojom::OnDeviceFeature feature,
      OnDeviceModelEligibilityReason reason) = 0;
};

// The model's configured limits on tokens.
struct TokenLimits {
  // The full combined limit for input and output tokens.
  uint32_t max_tokens = 0;
  // The number of context tokens guaranteed to be processed before context
  // processing can be cancelled to begin execution.
  uint32_t min_context_tokens = 0;
  // The maximum number of tokens that can be used by AddContext.
  uint32_t max_context_tokens = 0;
  // The maximum number of tokens that can be used by ExecuteModel.
  uint32_t max_execute_tokens = 0;
  // The maximum number of tokens that can be generated as output.
  uint32_t max_output_tokens = 0;
};

// The configuration that specifies the default sampling params.
struct SamplingParamsConfig {
  uint32_t default_top_k;
  float default_temperature;
};

// A model session that will save context for future ExecuteModel() calls.
class OnDeviceSession {
 public:
  virtual ~OnDeviceSession() = default;

  virtual const TokenLimits& GetTokenLimits() const = 0;

  // Sets the input context for this session, replacing any previous context.
  // This will generate prompt text from the feature config's
  // "input_context_substitutions". Data provided here (including images) will
  // be merged with data provided to an ExecuteModel() call and be available
  // for use in later prompt templates based on the request. Calling this will
  // cancel any ongoing executions and invoke their 'callback' methods with
  // the 'kCancelled' error. `callback` will be called with either the number
  // of tokens processed from `request` or an error.
  using SetInputCallback = base::OnceCallback<void(
      base::expected<size_t, OptimizationGuideModelExecutionError>)>;
  virtual void SetInput(MultimodalMessage request,
                        SetInputCallback callback) = 0;

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

  // A consstraint is provided to define structured output requirements for
  // the response.
  virtual void ExecuteModelWithResponseConstraint(
      const google::protobuf::MessageLite& request_metadata,
      on_device_model::mojom::ResponseConstraintPtr constraint,
      OptimizationGuideModelExecutionResultStreamingCallback callback) = 0;

  // Call `GetSizeInTokens()` from the model to get the size of the given text
  // in tokens. The result will be passed back through the callback.
  virtual void GetSizeInTokens(
      const std::string& text,
      OptimizationGuideModelSizeInTokenCallback callback) = 0;

  // Gets the size in tokens used by request_metadata in tokens as it would be
  // formatted by a call to `ExecuteModel()`. The result will be passed back
  // through the callback.
  virtual void GetExecutionInputSizeInTokens(
      MultimodalMessageReadView request_metadata,
      OptimizationGuideModelSizeInTokenCallback callback) = 0;

  // Gets the size in tokens used by request_metadata as it would be formatted
  // by a call to `AddContext()`. The result will be passed back through the
  // callback.
  virtual void GetContextSizeInTokens(
      MultimodalMessageReadView request_metadata,
      OptimizationGuideModelSizeInTokenCallback callback) = 0;

  // Return the sampling params for the current session.
  virtual const SamplingParams GetSamplingParams() const = 0;

  // Return the capabilities for the current session.
  virtual on_device_model::Capabilities GetCapabilities() const = 0;

  // Returns the feature_metadata from the
  // OnDeviceModelExecutionFeatureConfig.
  virtual const proto::Any& GetOnDeviceFeatureMetadata() const = 0;

  // Clones the session and associated context. Note that if the parent
  // session is deleted and cancels context processing after clone, the
  // context will also be cancelled for the clone.
  // TODO: crbug.com/396211270 - Make clone independent of parent.
  virtual std::unique_ptr<OnDeviceSession> Clone() = 0;

  // Sets the priority for this session and any future clones.
  virtual void SetPriority(on_device_model::mojom::Priority priority) = 0;
};

// Provides capability information about the on-device models to be served by
// the Optimization Guide.
class OnDeviceCapability {
 public:
  OnDeviceCapability();
  virtual ~OnDeviceCapability();

  virtual void BindModelBroker(
      mojo::PendingReceiver<mojom::ModelBroker> receiver) {}

  // Starts a session which allows streaming input and output from the model.
  // May return nullptr if model execution is not supported. This session should
  // not outlive OnDeviceCapability.
  virtual std::unique_ptr<OnDeviceSession> StartSession(
      mojom::OnDeviceFeature feature,
      const SessionConfigParams& config_params,
      base::WeakPtr<OptimizationGuideLogger> logger);

  // Observer for on-device model availability changes.
  virtual void AddOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature feature,
      OnDeviceModelAvailabilityObserver* observer);
  virtual void RemoveOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature feature,
      OnDeviceModelAvailabilityObserver* observer);

  // Returns the capabilities for the on-device model, or empty capabilities if
  // no model is available.
  virtual on_device_model::Capabilities GetOnDeviceCapabilities();
  virtual OnDeviceModelEligibilityReason GetOnDeviceModelEligibility(
      mojom::OnDeviceFeature feature);
  // Similar to above, but bumps the priority of related tasks such as computing
  // the performance class before returning the eligibility.
  virtual void GetOnDeviceModelEligibilityAsync(
      mojom::OnDeviceFeature feature,
      const on_device_model::Capabilities& capabilities,
      base::OnceCallback<void(OnDeviceModelEligibilityReason)> callback);
  virtual std::optional<SamplingParamsConfig> GetSamplingParamsConfig(
      mojom::OnDeviceFeature feature);
  virtual std::optional<const optimization_guide::proto::Any>
  GetFeatureMetadata(mojom::OnDeviceFeature feature);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_CAPABILITY_H_
