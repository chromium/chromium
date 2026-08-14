// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_WEBSOCKET_CLIENT_H_
#define COMPONENTS_PRIVATE_AI_WEBSOCKET_CLIENT_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/private_ai/private_ai_common.h"
#include "components/private_ai/transport.h"
#include "components/streaming_client/streaming_websocket_client.h"
#include "url/gurl.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace private_ai {

class PrivateAiLogger;

class WebSocketClient
    : public Transport,
      public streaming_client::StreamingWebSocketClient::Delegate {
 public:
  WebSocketClient(const GURL& service_url,
                  network::mojom::NetworkContext* network_context,
                  PrivateAiLogger* logger);
  ~WebSocketClient() override;

  WebSocketClient(const WebSocketClient&) = delete;
  WebSocketClient& operator=(const WebSocketClient&) = delete;

  // Transport:
  void SetResponseCallback(ResponseCallback callback) override;
  void Send(const oak::session::v1::SessionRequest& request) override;

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

 private:
  enum class State {
    kInitialized,
    kActive,
    kDisconnected,
  };

  void Send(Request request);
  void OnResponse(
      base::expected<std::vector<uint8_t>, TransportError> response);
  void ClosePipe(TransportError status);

  State state_ = State::kInitialized;
  const raw_ptr<PrivateAiLogger> logger_;
  ResponseCallback response_callback_;

  streaming_client::StreamingWebSocketClient streaming_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebSocketClient> weak_ptr_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_WEBSOCKET_CLIENT_H_
