// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTOR_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

// The result type of model execution.
struct OptimizationGuideModelExecutionResult {
  OptimizationGuideModelExecutionResult();
  explicit OptimizationGuideModelExecutionResult(
      base::expected<const proto::Any /*response_metadata*/,
                     OptimizationGuideModelExecutionError> response,
      std::unique_ptr<proto::ModelExecutionInfo> execution_info);
  OptimizationGuideModelExecutionResult(
      OptimizationGuideModelExecutionResult&& other);
  ~OptimizationGuideModelExecutionResult();
  base::expected<const proto::Any /*response_metadata*/,
                 OptimizationGuideModelExecutionError>
      response;
  std::unique_ptr<proto::ModelExecutionInfo> execution_info;
};

// The callback for receiving the model execution result and model quality log
// entry.
using OptimizationGuideModelExecutionResultCallback =
    base::OnceCallback<void(OptimizationGuideModelExecutionResult,
                            // TODO(372535824): remove this parameter.
                            std::unique_ptr<ModelQualityLogEntry>)>;

// Interface for remote model execution.
class RemoteModelExecutor {
 public:
  virtual ~RemoteModelExecutor() = default;

  // Executes the model for `feature` with `request_metadata` and invokes the
  // `callback` with the result.
  virtual void ExecuteModel(
      ModelBasedCapabilityKey feature,
      const google::protobuf::MessageLite& request_metadata,
      const std::optional<base::TimeDelta>& execution_timeout,
      OptimizationGuideModelExecutionResultCallback callback) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTOR_H_
