// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_WEBRTC_IPC_PACKET_SOCKET_FACTORY_H_
#define CHROME_SERVICES_SHARING_WEBRTC_IPC_PACKET_SOCKET_FACTORY_H_

#include "mojo/public/cpp/bindings/shared_remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "third_party/webrtc/api/packet_socket_factory.h"

namespace sharing {

// IpcPacketSocketFactory implements webrtc::PacketSocketFactory
// interface for libjingle using IPC-based P2P sockets. The class must
// be used on a thread that is a libjingle thread (implements
// webrtc::Thread) and also has associated base::MessageLoop. Each
// socket created by the factory must be used on the thread it was
// created on.
// TODO(crbug.com/40115622): reuse code from blink instead.
class IpcPacketSocketFactory : public webrtc::PacketSocketFactory {
 public:
  IpcPacketSocketFactory(
      const mojo::SharedRemote<network::mojom::P2PSocketManager>&
          socket_manager,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  IpcPacketSocketFactory(const IpcPacketSocketFactory&) = delete;
  IpcPacketSocketFactory& operator=(const IpcPacketSocketFactory&) = delete;
  ~IpcPacketSocketFactory() override;

  webrtc::AsyncPacketSocket* CreateUdpSocket(
      const webrtc::SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port) override;
  webrtc::AsyncListenSocket* CreateServerTcpSocket(
      const webrtc::SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port,
      int opts) override;
  webrtc::AsyncPacketSocket* CreateClientTcpSocket(
      const webrtc::SocketAddress& local_address,
      const webrtc::SocketAddress& remote_address,
      const webrtc::PacketSocketTcpOptions& opts) override;
  std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAsyncDnsResolver()
      override;

 private:
  mojo::SharedRemote<network::mojom::P2PSocketManager> socket_manager_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_WEBRTC_IPC_PACKET_SOCKET_FACTORY_H_
