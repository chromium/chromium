// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

class MockOptimizationGuideModelExecutor
    : public OptimizationGuideModelExecutor {
 public:
  MockOptimizationGuideModelExecutor();
  ~MockOptimizationGuideModelExecutor() override;

  MOCK_METHOD(bool,
              CanCreateOnDeviceSession,
              (ModelBasedCapabilityKey feature,
               OnDeviceModelEligibilityReason* debug_reason),
              (override));

  MOCK_METHOD(std::unique_ptr<Session>,
              StartSession,
              (ModelBasedCapabilityKey feature,
               const std::optional<SessionConfigParams>& config_params),
              (override));

  MOCK_METHOD(void,
              ExecuteModel,
              (ModelBasedCapabilityKey feature,
               const google::protobuf::MessageLite& request_metadata,
               const std::optional<base::TimeDelta>& execution_timeout,
               OptimizationGuideModelExecutionResultCallback callback),
              (override));
};

class MockSession : public OptimizationGuideModelExecutor::Session {
 public:
  // Constructs an unconfigured mock.
  MockSession();
  // Constructs a MockSession that delegates to the given session.
  // The delegate should be an object that will outlive the MockSession.
  explicit MockSession(OptimizationGuideModelExecutor::Session* delegate);

  ~MockSession() override;

  // Utility method to create a successful result.
  static OptimizationGuideModelStreamingExecutionResult SuccessResult(
      proto::Any response);
  // Utility method to create a generic failure result.
  static OptimizationGuideModelStreamingExecutionResult FailResult();

  // Configure this mock to delegate to another implementation.
  // The delegate should be an object that will outlive the MockSession.
  // This should be called *before* other ON_CALL statements.
  void Delegate(OptimizationGuideModelExecutor::Session* impl);

  MOCK_METHOD(const optimization_guide::TokenLimits&,
              GetTokenLimits,
              (),
              (const, override));
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
  MOCK_METHOD(void,
              GetSizeInTokens,
              (const std::string& text,
               OptimizationGuideModelSizeInTokenCallback callback));
  MOCK_METHOD(void,
              GetExecutionInputSizeInTokens,
              (const google::protobuf::MessageLite& request_metadata,
               OptimizationGuideModelSizeInTokenCallback callback));
  MOCK_METHOD(void,
              GetContextSizeInTokens,
              (const google::protobuf::MessageLite& request_metadata,
               OptimizationGuideModelSizeInTokenCallback callback));
  MOCK_METHOD(const optimization_guide::SamplingParams,
              GetSamplingParams,
              (),
              (const override));
  MOCK_METHOD(const proto::Any&,
              GetOnDeviceFeatureMetadata,
              (),
              (const override));
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_
