// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INFERENCE_WEBSOCKET_CLIENT_H_
#define COMPONENTS_PRIVATE_INFERENCE_WEBSOCKET_CLIENT_H_

#include <queue>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "url/gurl.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace private_inference {

class WebSocketClient : public network::mojom::WebSocketHandshakeClient,
                        public network::mojom::WebSocketClient {
 public:
  enum class SocketStatus {
    // Response received successfully.
    kOk,
    // Socket was closed by the server.
    kSocketClosed,
    // An error occurred on the client. Socket is now closed.
    kError,
  };

  using NetworkContextFactory =
      base::RepeatingCallback<network::mojom::NetworkContext*()>;

  // Called when a response is received or the socket status has changed.
  // When SocketStatus is kOk, then the vector contains the response from the
  // server. Otherwise the vector is empty.
  using OnResponseCallback =
      base::RepeatingCallback<void(SocketStatus, std::vector<uint8_t>)>;

  WebSocketClient(const GURL& service_url,
                  NetworkContextFactory network_context_factory,
                  OnResponseCallback on_response);

  ~WebSocketClient() override;

  // Sends a message over the WebSocket.
  void Write(base::span<const uint8_t> data);

 private:
  enum class State {
    kInitialized,
    kConnecting,
    kOpen,
    kDisconnected,
  };

  void Connect();
  void InternalWrite(base::span<const uint8_t> data);
  void ReadFromDataPipe(MojoResult result,
                        const mojo::HandleSignalsState& state);
  void ProcessCompletedResponse();
  void ClosePipe(SocketStatus status);
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
  const GURL service_url_;
  const NetworkContextFactory network_context_factory_;
  const OnResponseCallback on_response_;

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

}  // namespace private_inference

#endif  // COMPONENTS_PRIVATE_INFERENCE_WEBSOCKET_CLIENT_H_
