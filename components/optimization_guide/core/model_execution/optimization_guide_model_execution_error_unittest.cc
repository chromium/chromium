// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

OptimizationGuideModelExecutionError CreateModelExecutionErrorFromErrorState(
    proto::ErrorState error_state) {
  proto::ErrorResponse error_response;
  error_response.set_error_state(error_state);
  return OptimizationGuideModelExecutionError::FromModelExecutionServerError(
      error_response);
}
}  // namespace

TEST(OptimizationGuideModelExecutionErrorTest, ShouldLogModelQuality) {
  EXPECT_TRUE(CreateModelExecutionErrorFromErrorState(
                  proto::ErrorState::ERROR_STATE_FILTERED)
                  .ShouldLogModelQuality());
  EXPECT_TRUE(CreateModelExecutionErrorFromErrorState(
                  proto::ErrorState::ERROR_STATE_UNSUPPORTED_LANGUAGE)
                  .ShouldLogModelQuality());
  EXPECT_TRUE(CreateModelExecutionErrorFromErrorState(
                  proto::ErrorState::ERROR_STATE_INTERNAL_SERVER_ERROR_RETRY)
                  .ShouldLogModelQuality());
  EXPECT_TRUE(CreateModelExecutionErrorFromErrorState(
                  proto::ErrorState::ERROR_STATE_INTERNAL_SERVER_ERROR_NO_RETRY)
                  .ShouldLogModelQuality());
  EXPECT_FALSE(CreateModelExecutionErrorFromErrorState(
                   proto::ErrorState::ERROR_STATE_REQUEST_THROTTLED)
                   .ShouldLogModelQuality());
  EXPECT_FALSE(CreateModelExecutionErrorFromErrorState(
                   proto::ErrorState::ERROR_STATE_DISABLED)
                   .ShouldLogModelQuality());
}

}  // namespace optimization_guide
