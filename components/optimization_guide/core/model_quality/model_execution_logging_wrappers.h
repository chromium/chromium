// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_EXECUTION_LOGGING_WRAPPERS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_EXECUTION_LOGGING_WRAPPERS_H_

#include <algorithm>
#include <memory>
#include <type_traits>

#include "base/functional/bind.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"

namespace optimization_guide {

template <class ModelExecutionProto>
using ModelExecutionCallbackWithLogging =
    base::OnceCallback<void(OptimizationGuideModelExecutionResult,
                            std::unique_ptr<ModelExecutionProto>)>;

// Calls `executor->ExecuteModel` and logs the request, response, and
// `ModelExecutionInfo` to `ModelExecutionProto`. The `ModelExecutionProto`
// should be a proto that has fields named `request` and `response` matching the
// request and response proto types for the feature being executed, and a field
// named `model_execution_info` of type
// optimization_guide::proto::ModelExecutionInfo. The `ModelExecutionProto` will
// be created and filled in by the wrapper, then passed to the callback once the
// model execution completes. **The caller is responsible for storing the
// ModelExecutionProto in the ModelQualityLogEntry once the callback is
// called.**
template <class ModelExecutionProto, class RequestProto>
void ExecuteModelWithLogging(
    RemoteModelExecutor* executor,
    ModelBasedCapabilityKey feature,
    const RequestProto& request,
    std::optional<base::TimeDelta> execution_timeout,
    ModelExecutionCallbackWithLogging<ModelExecutionProto> callback) {
  auto model_execution_proto = std::make_unique<ModelExecutionProto>();
  *model_execution_proto->mutable_request() = request;
  OptimizationGuideModelExecutionResultCallback internal_callback =
      base::BindOnce(
          [](ModelExecutionCallbackWithLogging<ModelExecutionProto> callback,
             std::unique_ptr<ModelExecutionProto> model_execution_proto,
             OptimizationGuideModelExecutionResult result,
             std::unique_ptr<ModelQualityLogEntry> log_entry) {
            CHECK(model_execution_proto);
            // Fill in the model_execution_proto.
            if (result.execution_info) {
              *model_execution_proto->mutable_model_execution_info() =
                  *result.execution_info;
            }
            if (result.response.has_value()) {
              using ResponseProto = std::remove_reference<
                  decltype(*model_execution_proto->mutable_response())>::type;
              auto response =
                  optimization_guide::ParsedAnyMetadata<ResponseProto>(
                      result.response.value());
              if (response) {
                *model_execution_proto->mutable_response() = *response;
              }
            } else {
              model_execution_proto->mutable_model_execution_info()
                  ->set_model_execution_error_enum(
                      static_cast<uint32_t>(result.response.error().error()));
            }
            // Drop the ModelQualityLogEntry from the result.
            // TODO: crbug.com/372535824 - Remove entirely. Instead, the caller
            // is responsible for creating a new ModelQualityLogEntry and
            // storing the ModelExecutionProto in it.
            ModelQualityLogEntry::Drop(std::move(log_entry));
            std::move(callback).Run(std::move(result),
                                    std::move(model_execution_proto));
          },
          std::move(callback), std::move(model_execution_proto));
  executor->ExecuteModel(feature, request,
                         {.execution_timeout = execution_timeout},
                         std::move(internal_callback));
}

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_EXECUTION_LOGGING_WRAPPERS_H_
