// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/execute_remote_fn.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"

namespace optimization_guide {

namespace {

void NoOpExecuteRemoteFn(
    ModelBasedCapabilityKey feature,
    const google::protobuf::MessageLite& request,
    std::optional<base::TimeDelta> timeout,
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
    OptimizationGuideModelExecutionResultCallback callback) {
  auto execution_info = std::make_unique<proto::ModelExecutionInfo>();
  execution_info->set_model_execution_error_enum(
      static_cast<uint32_t>(OptimizationGuideModelExecutionError::
                                ModelExecutionError::kGenericFailure));
  std::move(callback).Run(
      OptimizationGuideModelExecutionResult(
          base::unexpected(
              OptimizationGuideModelExecutionError::FromModelExecutionError(
                  OptimizationGuideModelExecutionError::ModelExecutionError::
                      kGenericFailure)),
          std::move(execution_info)),
      nullptr);
}

}  // namespace

void InvokeStreamingCallbackWithRemoteResult(
    OptimizationGuideModelExecutionResultStreamingCallback callback,
    OptimizationGuideModelExecutionResult result,
    std::unique_ptr<ModelQualityLogEntry> log_entry) {
  OptimizationGuideModelStreamingExecutionResult streaming_result;
  if (log_entry) {
    // TODO: crbug.com/372535824 - This function should just get execution info.
    if (log_entry->log_ai_data_request() &&
        log_entry->log_ai_data_request()->has_model_execution_info()) {
      streaming_result.execution_info =
          std::make_unique<proto::ModelExecutionInfo>(
              log_entry->log_ai_data_request()->model_execution_info());
    }
    ModelQualityLogEntry::Drop(std::move(log_entry));
  }
  if (result.response.has_value()) {
    streaming_result.response = base::ok(
        StreamingResponse{.response = *result.response, .is_complete = true});
  } else {
    streaming_result.response = base::unexpected(result.response.error());
  }
  callback.Run(std::move(streaming_result));
}

ExecuteRemoteFn CreateNoOpExecuteRemoteFn() {
  return base::BindRepeating(&NoOpExecuteRemoteFn);
}

}  // namespace optimization_guide
