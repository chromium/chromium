// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTION_SESSION_IMPL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTION_SESSION_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/streaming_client/streaming_websocket_client.h"
#include "url/gurl.h"

class OptimizationGuideLogger;

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace signin {
class IdentityManager;
}  // namespace signin

namespace optimization_guide {

inline constexpr char
    kOptimizationGuideServiceModelExecutionDefaultStreamURL[] =
        "https://chromemodelexecution-pa.googleapis.com/v1:StreamExecute";

// Overrides the Optimization Guide model execution streaming URL.
inline constexpr char kOptimizationGuideServiceModelExecutionStreamURLSwitch[] =
    "optimization-guide-service-model-execution-stream-url";

// Returns the URL endpoint used for the streaming model execution service.
GURL GetModelExecutionServiceStreamURL();

// RemoteModelExecutionSession implementation that communicates with MES
// via WebSocket using StreamingWebSocketClient.
class RemoteModelExecutionSessionImpl
    : public RemoteModelExecutionSession,
      public streaming_client::StreamingWebSocketClient::Delegate {
 public:
  RemoteModelExecutionSessionImpl(
      ModelBasedCapabilityKey feature,
      const StreamingModelExecutionOptions& options,
      OptimizationGuideModelExecutionStreamingCallback callback,
      network::mojom::NetworkContext* network_context,
      signin::IdentityManager* identity_manager,
      OptimizationGuideLogger* logger = nullptr);

  // Constructor for tests. Allows injecting a mock `StreamingWebSocketClient`.
  RemoteModelExecutionSessionImpl(
      ModelBasedCapabilityKey feature,
      const StreamingModelExecutionOptions& options,
      OptimizationGuideModelExecutionStreamingCallback callback,
      signin::IdentityManager* identity_manager,
      std::unique_ptr<streaming_client::StreamingWebSocketClient> client,
      OptimizationGuideLogger* logger = nullptr);

  ~RemoteModelExecutionSessionImpl() override;

  RemoteModelExecutionSessionImpl(const RemoteModelExecutionSessionImpl&) =
      delete;
  RemoteModelExecutionSessionImpl& operator=(
      const RemoteModelExecutionSessionImpl&) = delete;

  // RemoteModelExecutionSession:
  void Send(const google::protobuf::MessageLite& message) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // streaming_client::StreamingWebSocketClient::Delegate:
  void OnMessage(std::vector<uint8_t> message) override;
  void OnConnectionError(const std::string& message,
                         int net_error,
                         int response_code) override;
  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const std::string& reason,
                     std::optional<base::TimeDelta> elapsed) override;
  void OnError(const std::string& message) override;
  void OnClose() override;
  void OnConnected() override;
  std::vector<network::mojom::HttpHeaderPtr> GetAdditionalHeaders() override;

  ConnectionState connection_state() const { return connection_state_; }

 private:
  void SetConnectionState(ConnectionState state);
  void HandleDisconnection(
      std::optional<OptimizationGuideModelExecutionError> error);
  void ResetIdleTimer();
  void OnIdleTimeout();
  void StartConnection();
  void OnAccessTokenReceived(const std::string& access_token);
  void SendPendingRequests();
  void DispatchResponse(OptimizationGuideModelStreamingResult result);
  void DispatchError(OptimizationGuideModelExecutionError error);

  const ModelBasedCapabilityKey feature_;
  const StreamingModelExecutionOptions options_;
  OptimizationGuideModelExecutionStreamingCallback callback_;
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // TODO(crbug.com/553134125): Add debug logging with the logger.
  const raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  ConnectionState connection_state_ = ConnectionState::kDisconnected;
  base::ObserverList<Observer> observers_;

  base::RetainingOneShotTimer idle_timer_;
  std::string access_token_;

  std::vector<std::vector<uint8_t>> pending_requests_;

  std::unique_ptr<streaming_client::StreamingWebSocketClient> client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RemoteModelExecutionSessionImpl> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTION_SESSION_IMPL_H_
