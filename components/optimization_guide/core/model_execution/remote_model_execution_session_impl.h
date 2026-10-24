// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTION_SESSION_IMPL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTION_SESSION_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/core/feature_registry/feature_keys.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/streaming_client/streaming_websocket_client.h"
#include "services/network/public/cpp/network_context_getter.h"

class OptimizationGuideLogger;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace optimization_guide {

// The status of establishing a streaming WebSocket connection. Only used for
// UMA logging. Entries should not be renumbered and numeric values should never
// be reused.
// LINT.IfChange(StreamingConnectionStatus)
enum class StreamingConnectionStatusUma {
  kConnected = 0,
  kAccessTokenFailure = 1,
  kHandshakeFailed = 2,
  kClientErrorDuringConnect = 3,
  kMojoPipeDisconnectedDuringConnect = 4,
  kCancelledByDestruction = 5,
  kMaxValue = kCancelledByDestruction,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/enums.xml:OptimizationGuideStreamingConnectionStatus)

// The reason an active streaming WebSocket connection disconnected. Only used
// for UMA logging. Entries should not be renumbered and numeric values should
// never be reused.
// LINT.IfChange(StreamingDisconnectReason)
enum class StreamingDisconnectReasonUma {
  kSessionDestroyed = 0,
  kIdleTimeout = 1,
  kServerDropClean = 2,
  kServerDropUnclean = 3,
  kClientError = 4,
  kMojoPipeDisconnected = 5,
  kMaxValue = kMojoPipeDisconnected,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/enums.xml:OptimizationGuideStreamingDisconnectReason)

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
      network::NetworkContextGetter network_context_getter,
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
  void NotifyObservers();
  void HandleDisconnection(
      std::optional<OptimizationGuideModelExecutionError> error,
      std::variant<StreamingConnectionStatusUma, StreamingDisconnectReasonUma>
          disconnect_metric);
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

  const raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  ConnectionState connection_state_ = ConnectionState::kDisconnected;
  base::ReentrantObserverList<Observer> observers_;

  base::RetainingOneShotTimer idle_timer_;
  std::string access_token_;

  base::circular_deque<std::vector<uint8_t>> pending_requests_;

  std::unique_ptr<streaming_client::StreamingWebSocketClient> client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RemoteModelExecutionSessionImpl> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTION_SESSION_IMPL_H_
