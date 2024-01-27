// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/compose/type_conversions.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using Result = optimization_guide::OptimizationGuideModelExecutionResult;
using Error = optimization_guide::OptimizationGuideModelExecutionError;
using Status = compose::mojom::ComposeStatus;

namespace {
void TestErrorConversion(Error::ModelExecutionError modelError,
                         Status expectedStatus) {
  Status result = ComposeStatusFromOptimizationGuideResult(
      optimization_guide::OptimizationGuideModelStreamingExecutionResult(
          base::unexpected(Error::FromModelExecutionError(modelError)),
          /*provided_by_on_device=*/false));
  EXPECT_EQ(expectedStatus, result);
}
}  // namespace

TEST(ComposeTypeConversions, OptimizationGuideHasResult) {
  Status result = ComposeStatusFromOptimizationGuideResult(
      optimization_guide::OptimizationGuideModelStreamingExecutionResult(
          base::ok(optimization_guide::StreamingResponse{
              .response = optimization_guide::proto::Any()}),
          /*provided_by_on_device=*/false));
  ASSERT_EQ(Status::kOk, result);
}

TEST(ComposeTypeConversions, OptimizationGuideErrors) {
  TestErrorConversion(Error::ModelExecutionError::kUnknown,
                      Status::kServerError);
  TestErrorConversion(Error::ModelExecutionError::kGenericFailure,
                      Status::kServerError);
  TestErrorConversion(Error::ModelExecutionError::kRequestThrottled,
                      Status::kRequestThrottled);
  TestErrorConversion(Error::ModelExecutionError::kRetryableError,
                      Status::kRetryableError);
  TestErrorConversion(Error::ModelExecutionError::kInvalidRequest,
                      Status::kInvalidRequest);
  TestErrorConversion(Error::ModelExecutionError::kPermissionDenied,
                      Status::kPermissionDenied);
  TestErrorConversion(Error::ModelExecutionError::kNonRetryableError,
                      Status::kNonRetryableError);
  TestErrorConversion(Error::ModelExecutionError::kUnsupportedLanguage,
                      Status::kUnsupportedLanguage);
  TestErrorConversion(Error::ModelExecutionError::kFiltered, Status::kFiltered);
  TestErrorConversion(Error::ModelExecutionError::kDisabled, Status::kDisabled);
  TestErrorConversion(Error::ModelExecutionError::kCancelled,
                      Status::kCancelled);
}
