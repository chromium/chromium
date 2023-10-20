// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace optimization_guide {

// The result type of model execution.
using OptimizationGuideModelExecutionResult =
    base::expected<const proto::Any /*response_metadata*/,
                   OptimizationGuideModelExecutionError>;

// The callback for receiving the model execution result.
using OptimizationGuideModelExecutionResultCallback =
    base::OnceCallback<void(OptimizationGuideModelExecutionResult)>;

// Interface for model execution.
class OptimizationGuideModelExecutor {
 public:
  // Executes the model for `feature` with `request_metadata` and invokes the
  // `callback` with the result.
  virtual void ExecuteModel(
      proto::ModelExecutionFeature feature,
      const google::protobuf::MessageLite& request_metadata,
      OptimizationGuideModelExecutionResultCallback callback) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_
