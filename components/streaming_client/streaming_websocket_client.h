// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAMING_CLIENT_STREAMING_WEBSOCKET_CLIENT_H_
#define COMPONENTS_STREAMING_CLIENT_STREAMING_WEBSOCKET_CLIENT_H_

#include <cstdint>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "url/gurl.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace streaming_client {

// Reusable WebSocket client class that maintains a persistent connection
// with a service to stream binary protobuf messages.
class StreamingWebSocketClient
    : public network::mojom::WebSocketHandshakeClient,
      public network::mojom::WebSocketClient {
 public:
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    // Called when a full binary message is received from the server.
    // Note: Delegate callbacks are invoked synchronously from Mojo message
    // handlers. Implementations should post a task if invoking a callback that
    // might destroy the client.
    virtual void OnMessage(std::vector<uint8_t> message) = 0;

    // Called when the connection handshake fails.
    virtual void OnConnectionError(const std::string& message,
                                   int net_error,
                                   int response_code) = 0;

    // Called when the WebSocket channel is dropped by the server.
    // `elapsed` has a value if the connection was open, representing the
    // duration the connection was open.
    virtual void OnDropChannel(bool was_clean,
                               uint16_t code,
                               const std::string& reason,
                               std::optional<base::TimeDelta> elapsed) = 0;

    // Called when an error occurs during reading, writing, or frame validation.
    virtual void OnError(const std::string& message) = 0;

    // Called when the Mojo pipe disconnects.
    virtual void OnClose() = 0;

    // Called when the WebSocket connection is successfully established.
    virtual void OnConnected();

    // Called when establishing the connection to get additional HTTP headers.
    virtual std::vector<network::mojom::HttpHeaderPtr> GetAdditionalHeaders();
  };

  StreamingWebSocketClient(const GURL& service_url,
                           network::mojom::NetworkContext* network_context,
                           net::NetworkTrafficAnnotationTag traffic_annotation,
                           Delegate* delegate);
  ~StreamingWebSocketClient() override;

  StreamingWebSocketClient(const StreamingWebSocketClient&) = delete;
  StreamingWebSocketClient& operator=(const StreamingWebSocketClient&) = delete;

  // Initiates the WebSocket handshake if not already connected or connecting.
  // Virtual for testing.
  virtual void Connect();

  // Sends a binary request to the server. If not connected, initiates the
  // connection automatically and queues the request.
  // Virtual for testing.
  virtual void Send(std::vector<uint8_t> request);

  // Closes the connection and resets internal state.
  // Virtual for testing.
  virtual void Close();

  const GURL& service_url() const { return service_url_; }

  // TODO(crbug.com/553134125): Refactor to be test-only.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }
  Delegate* delegate_for_testing() const { return delegate_; }

 private:
  enum class State {
    kInitialized,
    kConnecting,
    kOpen,
    kDisconnected,
  };

  void InternalWrite(base::span<const uint8_t> data);
  void ReadFromDataPipe(MojoResult result,
                        const mojo::HandleSignalsState& state);
  void ProcessCompletedResponse();
  void ClosePipe();
  void OnError(const std::string& message);
  void OnMojoPipeDisconnect();

  // network::mojom::WebSocketHandshakeClient:
  void OnOpeningHandshakeStarted(
      network::mojom::WebSocketHandshakeRequestPtr request) override;
  void OnFailure(const std::string& message,
                 int net_error,
                 int response_code) override;
  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::WebSocket> socket,
      mojo::PendingReceiver<network::mojom::WebSocketClient> client_receiver,
      network::mojom::WebSocketHandshakeResponsePtr response,
      mojo::ScopedDataPipeConsumerHandle readable,
      mojo::ScopedDataPipeProducerHandle writable) override;

  // network::mojom::WebSocketClient:
  void OnDataFrame(bool finish,
                   network::mojom::WebSocketMessageType type,
                   uint64_t data_len) override;
  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const std::string& reason) override;
  void OnClosingHandshake() override;

  State state_ = State::kInitialized;
  base::TimeTicks connection_open_time_;
  const GURL service_url_;
  const raw_ptr<network::mojom::NetworkContext> network_context_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
  raw_ptr<Delegate> delegate_;

  std::vector<uint8_t> pending_read_data_;
  size_t pending_read_data_index_ = 0;
  bool pending_read_finished_ = false;

  std::queue<std::vector<uint8_t>> pending_write_data_;

  mojo::Receiver<network::mojom::WebSocketHandshakeClient> handshake_receiver_{
      this};
  mojo::Receiver<network::mojom::WebSocketClient> client_receiver_{this};
  mojo::Remote<network::mojom::WebSocket> websocket_;
  mojo::ScopedDataPipeConsumerHandle readable_;
  mojo::SimpleWatcher readable_watcher_;
  mojo::ScopedDataPipeProducerHandle writable_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace streaming_client

#endif  // COMPONENTS_STREAMING_CLIENT_STREAMING_WEBSOCKET_CLIENT_H_
