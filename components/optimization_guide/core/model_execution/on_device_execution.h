// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_EXECUTION_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_EXECUTION_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_context.h"
#include "components/optimization_guide/core/model_execution/on_device_telemetry_logger.h"
#include "components/optimization_guide/core/model_execution/repetition_checker.h"
#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

// The state for an ongoing ExecuteModel() call.
class OnDeviceExecution final
    : public on_device_model::mojom::StreamingResponder,
      public on_device_model::mojom::ContextClient {
 public:
  explicit OnDeviceExecution(
      mojom::OnDeviceFeature feature,
      OnDeviceOptions opts,
      MultimodalMessage message,
      on_device_model::mojom::ResponseConstraintPtr constraint,
      std::unique_ptr<OnDeviceRequestTelemetryLogger> logger,
      OptimizationGuideModelExecutionResultStreamingCallback callback,
      base::OnceClosure cleanup_callback);
  ~OnDeviceExecution() final;

  // Begin processing the request.
  void BeginExecution(OnDeviceContext& context);

  // Cancels the execution.
  void Cancel();

 private:
  using Result = OnDeviceRequestTelemetryLogger::Result;
  // Returns the mutable on-device model service request for logging.
  proto::OnDeviceModelServiceRequest* MutableLoggedRequest();

  // Returns the mutable on-device model service response for logging.
  proto::OnDeviceModelServiceResponse* MutableLoggedResponse();

  // on_device_model::mojom::StreamingResponder:
  void OnResponse(on_device_model::mojom::ResponseChunkPtr chunk) override;
  void OnComplete(on_device_model::mojom::ResponseSummaryPtr summary) override;
  void OnToolCalls(
      std::vector<on_device_model::mojom::ToolCallPtr> tool_calls) override;

  // on_device_model::mojom::ContextClient:
  void OnComplete(uint32_t tokens_processed) override;

  // Called on StreamingResponder mojo pipe disconnect.
  void OnResponderDisconnect(uint32_t custom_reason,
                             const std::string& description);

  // Called to parse the latest raw output.
  // Leads to OnParsedResponse.
  void MaybeParseResponse(ResponseCompleteness completeness);

  // Called when a response has finished parsing.
  void OnParsedResponse(
      ResponseCompleteness completeness,
      base::expected<proto::Any, ResponseParsingError> output);

  // Sends an error result and terminates on-device processing as healthy.
  void CancelPendingResponse(Result result,
                             OnDeviceError error = OnDeviceError::kCancelled);

  // Sends the partial response callback, and does NOT terminate processing.
  void SendPartialResponseCallback(const proto::Any& success_response_metadata);

  // Sends a successful result and terminates on-device processing as healthy.
  void SendSuccessCompletionCallback(
      const proto::Any& success_response_metadata);

  // Called after terminating to release all held resources and notify owner
  // that this object is safe to destroy.
  void Cleanup();

  const mojom::OnDeviceFeature feature_;
  const OnDeviceOptions opts_;

  mojo::Remote<on_device_model::mojom::Session> session_;

  // The request message.
  MultimodalMessage last_message_;
  // A constraint defining structured output requirements for the response.
  on_device_model::mojom::ResponseConstraintPtr constraint_;
  // Handles logging for the execution.
  std::unique_ptr<OnDeviceRequestTelemetryLogger> telemetry_logger_;
  // Used to log execution information for the request.
  proto::ModelExecutionInfo exec_log_;

  // Response received so far.
  std::string current_response_;

  // How many tokens (response chunks) have been added.
  size_t num_response_tokens_ = 0;

  // The last position in the response that has been streamed to the
  // responder.
  size_t latest_response_pos_ = 0;

  // The number of tokens in the returned output.
  size_t output_token_count_ = 0;
  // The number of tokens in execute portion of the input.
  size_t execute_input_token_count_ = 0;

  // A buffer to hold trailing newlines.
  NewlineBuffer newline_buffer_;

  // Callback to provide the execution result.
  OptimizationGuideModelExecutionResultStreamingCallback callback_;

  // Callback to notify the owning session that on-device execution has
  // terminated, and that this object is safe to destroy.
  base::OnceClosure cleanup_callback_;

  mojo::Receiver<on_device_model::mojom::StreamingResponder> receiver_{this};
  mojo::Receiver<on_device_model::mojom::ContextClient> context_receiver_{this};

  // Factory for weak pointers related to this session that are invalidated
  // with the request state.
  base::WeakPtrFactory<OnDeviceExecution> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_EXECUTION_H_
