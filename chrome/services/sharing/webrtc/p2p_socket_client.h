// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_WEBRTC_P2P_SOCKET_CLIENT_H_
#define CHROME_SERVICES_SHARING_WEBRTC_P2P_SOCKET_CLIENT_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "services/network/public/mojom/p2p.mojom.h"

namespace sharing {

class P2PSocketClientDelegate;

// P2P socket that routes all calls over Mojo.
// The object runs on the WebRTC worker thread.
// TODO(crbug.com/40115622): reuse code from blink instead.
class P2PSocketClient : public network::mojom::P2PSocketClient {
 public:
  P2PSocketClient(const mojo::SharedRemote<network::mojom::P2PSocketManager>&
                      socket_manager,
                  const net::NetworkTrafficAnnotationTag& traffic_annotation);
  P2PSocketClient(const P2PSocketClient&) = delete;
  P2PSocketClient& operator=(const P2PSocketClient&) = delete;
  ~P2PSocketClient() override;

  // Initialize socket of the specified |type| and connected to the
  // specified |address|. |address| matters only when |type| is set to
  // P2P_SOCKET_TCP_CLIENT.
  virtual void Init(network::P2PSocketType type,
                    const net::IPEndPoint& local_address,
                    uint16_t min_port,
                    uint16_t max_port,
                    const network::P2PHostAndIPEndPoint& remote_address,
                    P2PSocketClientDelegate* delegate);

  // Send the |data| to the |address| using Differentiated Services Code Point
  // |dscp|. Return value is the unique packet_id for this packet.
  uint64_t Send(const net::IPEndPoint& address,
                base::span<const uint8_t> data,
                const rtc::PacketOptions& options);

  // Setting socket options.
  void SetOption(network::P2PSocketOption option, int value);

  // Must be called before the socket is destroyed. The delegate may
  // not be called after |closed_task| is executed.
  void Close();

  int GetSocketID() const;

  void SetDelegate(P2PSocketClientDelegate* delegate);

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_OPENING,
    STATE_OPEN,
    STATE_CLOSED,
    STATE_ERROR,
  };

  // Helper function to be called by Send to handle different threading
  // condition.
  void SendWithPacketId(const net::IPEndPoint& address,
                        base::span<const uint8_t> data,
                        const rtc::PacketOptions& options,
                        uint64_t packet_id);

  // network::mojom::P2PSocketClient interface.
  void SocketCreated(const net::IPEndPoint& local_address,
                     const net::IPEndPoint& remote_address) override;
  void SendComplete(const network::P2PSendPacketMetrics& send_metrics) override;
  void SendBatchComplete(const std::vector<::network::P2PSendPacketMetrics>&
                             send_metrics_batch) override;
  void DataReceived(
      std::vector<network::mojom::P2PReceivedPacketPtr> packets) override;

  void OnConnectionError();

  mojo::SharedRemote<network::mojom::P2PSocketManager> socket_manager_;
  THREAD_CHECKER(thread_checker_);
  int socket_id_;
  raw_ptr<P2PSocketClientDelegate> delegate_;
  State state_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  // These two fields are used to identify packets for tracing.
  uint32_t random_socket_id_;
  uint32_t next_packet_id_;

  mojo::Remote<network::mojom::P2PSocket> socket_;
  mojo::Receiver<network::mojom::P2PSocketClient> receiver_{this};
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_WEBRTC_P2P_SOCKET_CLIENT_H_
