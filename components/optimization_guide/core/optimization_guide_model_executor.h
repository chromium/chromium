// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace optimization_guide {

// The result type of model execution.
using OptimizationGuideModelExecutionResult =
    base::expected<const proto::Any /*response_metadata*/,
                   OptimizationGuideModelExecutionError>;

// A response type used for OptimizationGuideModelExecutor::Session.
struct StreamingResponse {
  // The response proto. This may be incomplete until `is_complete` is true.
  // This will contain the full response up to this point in the stream. Callers
  // should replace any previous streamed response with the new value while
  // `is_complete` is false.
  const proto::Any response;

  // True if streaming has finished.
  bool is_complete = false;
};

using OptimizationGuideModelStreamingExecutionResult =
    base::expected<const StreamingResponse,
                   OptimizationGuideModelExecutionError>;

// The callback for receiving the model execution result and model quality log
// entry.
using OptimizationGuideModelExecutionResultCallback =
    base::OnceCallback<void(OptimizationGuideModelExecutionResult,
                            std::unique_ptr<ModelQualityLogEntry>)>;

// The callback for receiving streamed output from the model. The log entry will
// be null until `StreamingResponse.is_complete` is true.
using OptimizationGuideModelExecutionResultStreamingCallback =
    base::RepeatingCallback<void(OptimizationGuideModelStreamingExecutionResult,
                                 std::unique_ptr<ModelQualityLogEntry>)>;

// Interface for model execution.
class OptimizationGuideModelExecutor {
 public:
  virtual ~OptimizationGuideModelExecutor() = default;

  // A model session that will save context for future ExecuteModel() calls.
  class Session {
   public:
    virtual ~Session() = default;

    // Adds context to this session. This will be saved for future Execute()
    // calls. Calling multiple times will replace previous calls to
    // AddContext(). Calling this while a ExecuteModel() call is still streaming
    // a response will cancel the ongoing ExecuteModel() call by calling its
    // `callback` with the kCancelled error.
    virtual void AddContext(
        const google::protobuf::MessageLite& request_metadata) = 0;

    // Execute the model with `request_metadata` and streams the result to
    // `callback`. The execute call will include context from the last
    // AddContext() call. Data provided to the last AddContext() call does not
    // need to be provided here. Calling this while another ExecuteModel() call
    // is still streaming a response will cancel the previous call by calling
    // `callback` with the kCancelled error.
    virtual void ExecuteModel(
        const google::protobuf::MessageLite& request_metadata,
        OptimizationGuideModelExecutionResultStreamingCallback callback) = 0;
  };

  // Starts a session which allows streaming input and output from the model.
  // May return nullptr if model execution is not supported. This session should
  // not outlive OptimizationGuideModelExecutor.
  virtual std::unique_ptr<Session> StartSession(
      proto::ModelExecutionFeature feature) = 0;

  // Executes the model for `feature` with `request_metadata` and invokes the
  // `callback` with the result.
  virtual void ExecuteModel(
      proto::ModelExecutionFeature feature,
      const google::protobuf::MessageLite& request_metadata,
      OptimizationGuideModelExecutionResultCallback callback) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_MODEL_EXECUTOR_H_
