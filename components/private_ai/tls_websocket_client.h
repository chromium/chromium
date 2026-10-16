// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_TLS_WEBSOCKET_CLIENT_H_
#define COMPONENTS_PRIVATE_AI_TLS_WEBSOCKET_CLIENT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/private_ai/transport.h"
#include "components/streaming_client/streaming_websocket_client.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "url/gurl.h"

namespace private_ai {

class PrivateAiLogger;

// WebSocket client for Oak TLS sessions.
// Handles wrapping and unwrapping raw TLS frames into TlsSessionRequest
// and TlsSessionResponse protobufs over WebSocket.
class TlsWebSocketClient
    : public streaming_client::StreamingWebSocketClient::Delegate {
 public:
  // Same failure kinds as the Noise transport.
  using Error = Transport::TransportError;

  using FrameCallback = base::RepeatingCallback<void(
      base::expected<std::vector<uint8_t>, Error>)>;

  TlsWebSocketClient(const GURL& service_url,
                     network::NetworkContextGetter network_context_getter,
                     PrivateAiLogger* logger,
                     std::optional<bool> has_chrome_proxy = std::nullopt);
  ~TlsWebSocketClient() override;

  TlsWebSocketClient(const TlsWebSocketClient&) = delete;
  TlsWebSocketClient& operator=(const TlsWebSocketClient&) = delete;

  // Virtual for testing.
  virtual void SetFrameCallback(FrameCallback callback);
  virtual void SendFrame(const std::vector<uint8_t>& frame_bytes);

  // streaming_client::StreamingWebSocketClient::Delegate:
  void OnConnected() override;
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
  void OnResponse(base::expected<std::vector<uint8_t>, Error> response);
  void ClosePipe(Error status);
  void RecordConnectionResult(int net_error);

  bool disconnected_ = false;
  const raw_ptr<PrivateAiLogger> logger_;
  FrameCallback frame_callback_;

  streaming_client::StreamingWebSocketClient streaming_client_;

  const std::optional<bool> has_chrome_proxy_;
  bool has_recorded_connection_result_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<TlsWebSocketClient> weak_ptr_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_TLS_WEBSOCKET_CLIENT_H_
