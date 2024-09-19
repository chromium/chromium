// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_

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
               OptimizationGuideModelExecutionResultCallback callback),
              (override));
};

class MockSession : public OptimizationGuideModelExecutor::Session {
 public:
  MockSession();
  ~MockSession() override;

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

// A wrapper that passes through calls to the underlying MockSession. Allows for
// easily mocking calls with a single session object.
class MockSessionWrapper : public OptimizationGuideModelExecutor::Session {
 public:
  explicit MockSessionWrapper(MockSession* session);
  ~MockSessionWrapper() override;

  // OptimizationGuideModelExecutor::Session:
  const optimization_guide::TokenLimits& GetTokenLimits() const override;
  void AddContext(
      const google::protobuf::MessageLite& request_metadata) override;
  void Score(const std::string& text,
             optimization_guide::OptimizationGuideModelScoreCallback callback)
      override;
  void ExecuteModel(
      const google::protobuf::MessageLite& request_metadata,
      optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
          callback) override;
  void GetSizeInTokens(
      const std::string& text,
      optimization_guide::OptimizationGuideModelSizeInTokenCallback callback)
      override;
  void GetContextSizeInTokens(
      const google::protobuf::MessageLite& request_metadata,
      optimization_guide::OptimizationGuideModelSizeInTokenCallback callback)
      override;
  const SamplingParams GetSamplingParams() const override;
  const proto::Any& GetOnDeviceFeatureMetadata() const override;

 private:
  raw_ptr<MockSession> session_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_
