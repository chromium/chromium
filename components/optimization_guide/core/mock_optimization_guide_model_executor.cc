// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"

#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

MockOptimizationGuideModelExecutor::MockOptimizationGuideModelExecutor() =
    default;
MockOptimizationGuideModelExecutor::~MockOptimizationGuideModelExecutor() =
    default;

MockSession::MockSession() = default;
MockSession::MockSession(OptimizationGuideModelExecutor::Session* delegate) {
  Delegate(delegate);
}
MockSession::~MockSession() = default;

OptimizationGuideModelStreamingExecutionResult MockSession::SuccessResult(
    proto::Any response) {
  return OptimizationGuideModelStreamingExecutionResult(
      base::ok(StreamingResponse{
          .response = std::move(response),
          .is_complete = true,
      }),
      /*provided_by_on_device=*/true,
      /*log_entry*/ nullptr);
}

OptimizationGuideModelStreamingExecutionResult MockSession::FailResult() {
  return OptimizationGuideModelStreamingExecutionResult(
      base::unexpected(
          OptimizationGuideModelExecutionError::FromModelExecutionError(
              OptimizationGuideModelExecutionError::ModelExecutionError::
                  kGenericFailure)),
      /*provided_by_on_device=*/true,
      /*log_entry*/ nullptr);
}

void MockSession::Delegate(OptimizationGuideModelExecutor::Session* impl) {
  ON_CALL(*this, GetTokenLimits).WillByDefault([impl]() -> const TokenLimits& {
    return impl->GetTokenLimits();
  });
  ON_CALL(*this, SetInput)
      .WillByDefault(
          [impl](MultimodalMessage input, SetInputCallback callback) {
            impl->SetInput(std::move(input), std::move(callback));
          });
  ON_CALL(*this, AddContext).WillByDefault([impl](const auto& input) {
    impl->AddContext(input);
  });
  ON_CALL(*this, Score).WillByDefault([impl](const auto& input, auto callback) {
    impl->Score(input, std::move(callback));
  });
  ON_CALL(*this, ExecuteModel)
      .WillByDefault([impl](const auto& input, auto callback) {
        impl->ExecuteModel(input, std::move(callback));
      });
  ON_CALL(*this, ExecuteModelWithResponseConstraint)
      .WillByDefault([impl](const auto& input, auto constraint, auto callback) {
        impl->ExecuteModelWithResponseConstraint(input, std::move(constraint),
                                                 std::move(callback));
      });
  ON_CALL(*this, GetSizeInTokens)
      .WillByDefault([impl](const auto& input, auto callback) {
        impl->GetSizeInTokens(input, std::move(callback));
      });
  ON_CALL(*this, GetExecutionInputSizeInTokens)
      .WillByDefault([impl](MultimodalMessageReadView input, auto callback) {
        impl->GetExecutionInputSizeInTokens(input, std::move(callback));
      });
  ON_CALL(*this, GetContextSizeInTokens)
      .WillByDefault([impl](MultimodalMessageReadView input, auto callback) {
        impl->GetContextSizeInTokens(input, std::move(callback));
      });
  ON_CALL(*this, GetSamplingParams).WillByDefault([impl]() {
    return impl->GetSamplingParams();
  });
  ON_CALL(*this, GetOnDeviceFeatureMetadata)
      .WillByDefault([impl]() -> const proto::Any& {
        return impl->GetOnDeviceFeatureMetadata();
      });
  ON_CALL(*this, SetPriority)
      .WillByDefault([impl](on_device_model::mojom::Priority priority) {
        impl->SetPriority(priority);
      });
}

}  // namespace optimization_guide
