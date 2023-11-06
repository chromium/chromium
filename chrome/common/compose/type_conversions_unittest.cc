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

TEST(ComposeTypeConversions, OptimizationGuideHasResult) {
  Status result = ComposeStatusFromOptimizationGuideResult(
      Result(optimization_guide::proto::Any()));
  ASSERT_EQ(Status::kOk, result);
}

TEST(ComposeTypeConversions, OptimizationGuideTransientError) {
  std::vector<Error::ModelExecutionError> errors = {
      Error::ModelExecutionError::kUnknown,
      Error::ModelExecutionError::kRequestThrottled,
      Error::ModelExecutionError::kGenericFailure,
  };
  for (auto error : errors) {
    Status result = ComposeStatusFromOptimizationGuideResult(
        base::unexpected(Error::FromModelExecutionError(error)));
    EXPECT_EQ(Status::kTryAgainLater, result);
  }
}

TEST(ComposeTypeConversions, OptimizationGuidePermanentError) {
  Status result = ComposeStatusFromOptimizationGuideResult(
      base::unexpected(Error::FromModelExecutionError(
          Error::ModelExecutionError::kInvalidRequest)));
  EXPECT_EQ(Status::kNotSuccessful, result);

  result = ComposeStatusFromOptimizationGuideResult(
      base::unexpected(Error::FromModelExecutionError(
          Error::ModelExecutionError::kPermissionDenied)));
  EXPECT_EQ(Status::kPermissionDenied, result);
}
