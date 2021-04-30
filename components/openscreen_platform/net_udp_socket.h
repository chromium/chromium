// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPENSCREEN_PLATFORM_NET_UDP_SOCKET_H_
#define COMPONENTS_OPENSCREEN_PLATFORM_NET_UDP_SOCKET_H_

#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/udp_socket.h"
#include "third_party/openscreen/src/platform/api/udp_socket.h"

namespace net {
class IPEndPoint;
}

namespace openscreen_platform {

class NetUdpSocket : public openscreen::UdpSocket {
 public:
  NetUdpSocket(Client* client, const openscreen::IPEndpoint& local_endpoint);
  ~NetUdpSocket() final;

  NetUdpSocket& operator=(const NetUdpSocket&) = delete;
  NetUdpSocket& operator=(NetUdpSocket&&) = delete;

 private:
  void SendErrorToClient(openscreen::Error::Code openscreen_error,
                         int net_error);
  void DoRead();
  bool HandleRecvFromResult(int result);
  void OnRecvFromCompleted(int result);
  void OnSendToCompleted(int result);

  // openscreen::UdpSocket implementation.
  bool IsIPv4() const final;
  bool IsIPv6() const final;
  openscreen::IPEndpoint GetLocalEndpoint() const final;
  void Bind() final;
  void SetMulticastOutboundInterface(
      openscreen::NetworkInterfaceIndex ifindex) final;
  void JoinMulticastGroup(const openscreen::IPAddress& address,
                          openscreen::NetworkInterfaceIndex ifindex) final;
  void SendMessage(const void* data,
                   size_t length,
                   const openscreen::IPEndpoint& dest) final;
  void SetDscp(openscreen::UdpSocket::DscpMode state) final;

  Client* const client_;

  // The local endpoint can change as a result of Bind() calls.
  openscreen::IPEndpoint local_endpoint_;
  net::UDPSocket udp_socket_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  net::IPEndPoint from_address_;
  bool send_pending_ = false;
};

}  // namespace openscreen_platform

#endif  // COMPONENTS_OPENSCREEN_PLATFORM_NET_UDP_SOCKET_H_
