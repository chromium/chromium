// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_MOCK_ON_DEVICE_CAPABILITY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_MOCK_ON_DEVICE_CAPABILITY_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

class MockOnDeviceCapability : public OnDeviceCapability {
 public:
  MockOnDeviceCapability();
  MockOnDeviceCapability(const MockOnDeviceCapability&) = delete;
  MockOnDeviceCapability& operator=(const MockOnDeviceCapability&) = delete;
  ~MockOnDeviceCapability() override;

  MOCK_METHOD(std::unique_ptr<OnDeviceSession>,
              StartSession,
              (mojom::OnDeviceFeature feature,
               const SessionConfigParams& config_params,
               base::WeakPtr<OptimizationGuideLogger> logger),
              (override));

  MOCK_METHOD(OnDeviceModelEligibilityReason,
              GetOnDeviceModelEligibility,
              (mojom::OnDeviceFeature),
              (override));

  MOCK_METHOD(void,
              GetOnDeviceModelEligibilityAsync,
              (mojom::OnDeviceFeature,
               const on_device_model::Capabilities&,
               base::OnceCallback<void(OnDeviceModelEligibilityReason)>),
              (override));

  MOCK_METHOD(std::optional<SamplingParamsConfig>,
              GetSamplingParamsConfig,
              (mojom::OnDeviceFeature),
              (override));

  MOCK_METHOD(std::optional<const proto::Any>,
              GetFeatureMetadata,
              (mojom::OnDeviceFeature feature),
              (override));
};

class MockSession : public OnDeviceSession {
 public:
  // Constructs an unconfigured mock.
  MockSession();
  // Constructs a MockSession that delegates to the given session.
  // The delegate should be an object that will outlive the MockSession.
  explicit MockSession(OnDeviceSession* delegate);

  ~MockSession() override;

  // Utility method to create a successful result.
  static OptimizationGuideModelStreamingExecutionResult SuccessResult(
      proto::Any response);
  // Utility method to create a generic failure result.
  static OptimizationGuideModelStreamingExecutionResult FailResult();

  // Configure this mock to delegate to another implementation.
  // The delegate should be an object that will outlive the MockSession.
  // This should be called *before* other ON_CALL statements.
  void Delegate(OnDeviceSession* impl);

  MOCK_METHOD(const TokenLimits&, GetTokenLimits, (), (const, override));
  MOCK_METHOD(void,
              SetInput,
              (MultimodalMessage request, SetInputCallback callback));
  MOCK_METHOD(void,
              AddContext,
              (const google::protobuf::MessageLite& request_metadata));
  MOCK_METHOD(void,
              Score,
              (const std::string& text,
               OptimizationGuideModelScoreCallback callback));
  MOCK_METHOD(
      void,
      ExecuteModel,
      (const google::protobuf::MessageLite& request_metadata,
       OptimizationGuideModelExecutionResultStreamingCallback callback));
  MOCK_METHOD(
      void,
      ExecuteModelWithResponseConstraint,
      (const google::protobuf::MessageLite& request_metadata,
       on_device_model::mojom::ResponseConstraintPtr constraint,
       OptimizationGuideModelExecutionResultStreamingCallback callback));
  MOCK_METHOD(void,
              GetSizeInTokens,
              (const std::string& text,
               OptimizationGuideModelSizeInTokenCallback callback));
  MOCK_METHOD(void,
              GetExecutionInputSizeInTokens,
              (MultimodalMessageReadView request_metadata,
               OptimizationGuideModelSizeInTokenCallback callback));
  MOCK_METHOD(void,
              GetContextSizeInTokens,
              (MultimodalMessageReadView request_metadata,
               OptimizationGuideModelSizeInTokenCallback callback));
  MOCK_METHOD(const SamplingParams, GetSamplingParams, (), (const override));
  MOCK_METHOD(on_device_model::Capabilities,
              GetCapabilities,
              (),
              (const override));
  MOCK_METHOD(const proto::Any&,
              GetOnDeviceFeatureMetadata,
              (),
              (const override));
  MOCK_METHOD(std::unique_ptr<OnDeviceSession>, Clone, (), (override));
  MOCK_METHOD(void,
              SetPriority,
              (on_device_model::mojom::Priority priority),
              (override));
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_MOCK_ON_DEVICE_CAPABILITY_H_
