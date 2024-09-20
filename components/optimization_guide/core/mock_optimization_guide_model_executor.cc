// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"

#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

MockOptimizationGuideModelExecutor::MockOptimizationGuideModelExecutor() =
    default;
MockOptimizationGuideModelExecutor::~MockOptimizationGuideModelExecutor() =
    default;

MockSession::MockSession() = default;
MockSession::~MockSession() = default;

MockSessionWrapper::MockSessionWrapper(MockSession* session)
    : session_(session) {}
MockSessionWrapper::~MockSessionWrapper() = default;

const optimization_guide::TokenLimits& MockSessionWrapper::GetTokenLimits()
    const {
  return session_->GetTokenLimits();
}
void MockSessionWrapper::AddContext(
    const google::protobuf::MessageLite& request_metadata) {
  session_->AddContext(request_metadata);
}
void MockSessionWrapper::Score(const std::string& text,
                               OptimizationGuideModelScoreCallback callback) {
  session_->Score(text, std::move(callback));
}
void MockSessionWrapper::ExecuteModel(
    const google::protobuf::MessageLite& request_metadata,
    OptimizationGuideModelExecutionResultStreamingCallback callback) {
  session_->ExecuteModel(request_metadata, std::move(callback));
}
void MockSessionWrapper::GetSizeInTokens(
    const std::string& text,
    OptimizationGuideModelSizeInTokenCallback callback) {
  session_->GetSizeInTokens(text, std::move(callback));
}
void MockSessionWrapper::GetContextSizeInTokens(
    const google::protobuf::MessageLite& request,
    OptimizationGuideModelSizeInTokenCallback callback) {
  session_->GetContextSizeInTokens(request, std::move(callback));
}
const SamplingParams MockSessionWrapper::GetSamplingParams() const {
  return session_->GetSamplingParams();
}
const proto::Any& MockSessionWrapper::GetOnDeviceFeatureMetadata() const {
  return session_->GetOnDeviceFeatureMetadata();
}

}  // namespace optimization_guide
