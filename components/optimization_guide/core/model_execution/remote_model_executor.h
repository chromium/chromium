// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTOR_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

// The model execution service.
enum class ModelExecutionServiceType {
  // Use the default backend for model executions.
  kDefault = 0,
  // Use the Private AI compute backend. Currently only supported for ZSS.
  // Please reach out to chrome-browser-privacy-team@ if you'd like to use this
  // backend.
  kPrivateAi = 1,
};

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

// Optional parameters for RemoteModelExecutor::ExecuteModel.
struct ModelExecutionOptions {
  bool operator==(const ModelExecutionOptions& other) const = default;

  const std::optional<base::TimeDelta> execution_timeout;
  const ModelExecutionServiceType service_type =
      ModelExecutionServiceType::kDefault;
};

// The result type of streaming model execution.
struct OptimizationGuideModelStreamingResult {
  OptimizationGuideModelStreamingResult();
  explicit OptimizationGuideModelStreamingResult(
      base::expected<proto::Any /*response_metadata*/,
                     OptimizationGuideModelExecutionError> response,
      std::unique_ptr<proto::ModelExecutionInfo> execution_info);
  OptimizationGuideModelStreamingResult(
      OptimizationGuideModelStreamingResult&& other);
  OptimizationGuideModelStreamingResult& operator=(
      OptimizationGuideModelStreamingResult&& other);
  ~OptimizationGuideModelStreamingResult();

  base::expected<proto::Any /*response_metadata*/,
                 OptimizationGuideModelExecutionError>
      response;
  std::unique_ptr<proto::ModelExecutionInfo> execution_info;
};

// The callback for receiving streamed model execution results.
using OptimizationGuideModelExecutionStreamingCallback =
    base::RepeatingCallback<void(OptimizationGuideModelStreamingResult)>;

// Configuration options for remote streaming model executions.
struct StreamingModelExecutionOptions {
  // Max time the persistent WebSocket stays open without traffic.
  base::TimeDelta idle_disconnect_timeout = base::Minutes(10);

  // If true, the session will proactively initiate the WebSocket
  // handshake upon creation before the first request is sent,
  // reducing first token latency.
  bool prewarm_connection = false;
};

// A model execution session that maintains a persistent streaming connection.
class RemoteModelExecutionSession {
 public:
  enum class ConnectionState {
    kDisconnected,
    kConnecting,
    kConnected,
  };

  // Observer interface to monitor WebSocket connection status.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the connection state changes.
    virtual void OnConnectionStateChanged(ConnectionState state) = 0;
  };

  virtual ~RemoteModelExecutionSession() = default;

  // Sends input through this session.
  virtual void Send(const google::protobuf::MessageLite& message) = 0;

  // Observer management for connection state changes.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

// Interface for remote model execution.
class RemoteModelExecutor {
 public:
  virtual ~RemoteModelExecutor() = default;

  // Executes the model for `feature` with `request_metadata` and invokes the
  // `callback` with the result.
  virtual void ExecuteModel(
      ModelBasedCapabilityKey feature,
      const google::protobuf::MessageLite& request_metadata,
      const ModelExecutionOptions& options,
      OptimizationGuideModelExecutionResultCallback callback) = 0;

  // Session-based Streaming Execution:
  // Establishes a persistent bidi streaming session for execution.
  virtual std::unique_ptr<RemoteModelExecutionSession> StartStreamingSession(
      ModelBasedCapabilityKey feature,
      const StreamingModelExecutionOptions& options,
      OptimizationGuideModelExecutionStreamingCallback callback) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTOR_H_
