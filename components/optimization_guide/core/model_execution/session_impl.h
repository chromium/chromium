// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SESSION_IMPL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SESSION_IMPL_H_

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
#include "components/optimization_guide/core/model_execution/on_device_execution.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/model_execution/substitution.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

class OnDeviceContext;

// Session implementation that uses either the on device model or the server
// model.
class SessionImpl : public OnDeviceSession {
 public:
  // Possible outcomes of AddContext(). Maps to histogram enum
  // "OptimizationGuideOnDeviceAddContextResult".
  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  enum class AddContextResult {
    kUsingServer = 0,
    kUsingOnDevice = 1,
    kFailedConstructingInput = 2,
    kMaxValue = kFailedConstructingInput,
  };

  SessionImpl(mojom::OnDeviceFeature feature, OnDeviceOptions on_device_opts);
  SessionImpl(mojom::OnDeviceFeature feature,
              const SamplingParams& sampling_params);
  ~SessionImpl() override;

  // optimization_guide::OnDeviceSession:
  const TokenLimits& GetTokenLimits() const override;
  const proto::Any& GetOnDeviceFeatureMetadata() const override;
  void SetInput(MultimodalMessage request, SetInputCallback callback) override;
  void AddContext(
      const google::protobuf::MessageLite& request_metadata) override;
  void Score(const std::string& text,
             OptimizationGuideModelScoreCallback callback) override;
  void ExecuteModel(
      const google::protobuf::MessageLite& request_metadata,
      OptimizationGuideModelExecutionResultStreamingCallback callback) override;
  void ExecuteModelWithResponseConstraint(
      const google::protobuf::MessageLite& request_metadata,
      on_device_model::mojom::ResponseConstraintPtr constraint,
      OptimizationGuideModelExecutionResultStreamingCallback callback) override;
  void GetSizeInTokens(
      const std::string& text,
      OptimizationGuideModelSizeInTokenCallback callback) override;
  void GetExecutionInputSizeInTokens(
      MultimodalMessageReadView request_metadata,
      OptimizationGuideModelSizeInTokenCallback callback) override;
  void GetContextSizeInTokens(
      MultimodalMessageReadView request_metadata,
      OptimizationGuideModelSizeInTokenCallback callback) override;
  const SamplingParams GetSamplingParams() const override;
  on_device_model::Capabilities GetCapabilities() const override;
  std::unique_ptr<OnDeviceSession> Clone() override;
  void SetPriority(on_device_model::mojom::Priority priority) override;

  // Returns true if the on-device model should be used.
  bool ShouldUseOnDeviceModel() const;

 private:
  AddContextResult AddContextImpl(MultimodalMessage request,
                                  SetInputCallback callback);

  void DestroyOnDeviceState();

  // Called when an on-device execution flow terminates, and can be cleaned up.
  void OnDeviceExecutionTerminated(bool healthy);

  // Helper function to get the size of request in tokens with boolean flag to
  // control if we are extracting the context or the execution text.
  void GetSizeInTokensInternal(
      MultimodalMessageReadView request,
      OptimizationGuideModelSizeInTokenCallback callback,
      bool want_input_context);

  const mojom::OnDeviceFeature feature_;

  MultimodalMessage context_;
  base::TimeTicks context_start_time_;

  // Manages the on-device session holding the processed context.
  // If this is null, on-device executions cannot be started.
  std::unique_ptr<OnDeviceContext> on_device_context_;

  // Manages state for an ongoing on-device execution.
  std::optional<OnDeviceExecution> on_device_execution_;

  // Params used to control output sampling for the on device model.
  const SamplingParams sampling_params_;

  // Capabilities for this session of the on device model.
  on_device_model::Capabilities capabilities_;

  base::WeakPtrFactory<SessionImpl> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SESSION_IMPL_H_
