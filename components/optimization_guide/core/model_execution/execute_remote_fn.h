// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_EXECUTE_REMOTE_FN_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_EXECUTE_REMOTE_FN_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace optimization_guide {

namespace proto {
class LogAiDataRequest;
}  // namespace proto

using ExecuteRemoteFn = base::RepeatingCallback<void(
    ModelBasedCapabilityKey feature,
    const google::protobuf::MessageLite&,
    std::optional<base::TimeDelta> timeout,
    std::unique_ptr<proto::LogAiDataRequest>,
    OptimizationGuideModelExecutionResultCallback)>;

ExecuteRemoteFn CreateNoOpExecuteRemoteFn();

// Convert a non-streaming remote execution result to a streaming result.
void InvokeStreamingCallbackWithRemoteResult(
    OptimizationGuideModelExecutionResultStreamingCallback callback,
    OptimizationGuideModelExecutionResult result,
    std::unique_ptr<ModelQualityLogEntry> log_entry);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_EXECUTE_REMOTE_FN_H_
