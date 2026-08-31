// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPENSCREEN_PLATFORM_NET_UDP_SOCKET_H_
#define COMPONENTS_OPENSCREEN_PLATFORM_NET_UDP_SOCKET_H_

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/udp_socket.h"
#include "third_party/openscreen/src/platform/api/udp_socket.h"

namespace net {
class IPEndPoint;
}

namespace openscreen_platform {

class NetUdpSocket final : public openscreen::UdpSocket {
 public:
  NetUdpSocket(Client* client, const openscreen::IPEndpoint& local_endpoint);
  ~NetUdpSocket() override;

  NetUdpSocket& operator=(const NetUdpSocket&) = delete;
  NetUdpSocket& operator=(NetUdpSocket&&) = delete;

 private:
  // Dispatches a callback to `client_` and returns true if `*this` is still
  // alive. Note: `client_` callbacks may synchronously destroy `this`.
  template <typename Callback>
  bool RunClientCallback(Callback&& callback) {
    base::WeakPtr<NetUdpSocket> weak_this = weak_ptr_factory_.GetWeakPtr();
    std::forward<Callback>(callback)(*client_);
    return static_cast<bool>(weak_this);
  }

  void SendErrorToClient(openscreen::Error::Code openscreen_error,
                         int net_error);
  void DoRead();

  // Dispatches read data or error to `client_`. Returns true if more data
  // should be read synchronously, or false if pending, on error, or if `this`
  // was destroyed. Note: `client_` callbacks may synchronously destroy `this`.
  bool HandleRecvFromResult(int result);

  void OnRecvFromCompleted(int result);
  void OnSendToCompleted(int result);

  // openscreen::UdpSocket implementation.
  bool IsIPv4() const override;
  bool IsIPv6() const override;
  openscreen::IPEndpoint GetLocalEndpoint() const override;
  void Bind() override;
  void SetMulticastOutboundInterface(
      openscreen::NetworkInterfaceIndex ifindex) override;
  void JoinMulticastGroup(const openscreen::IPAddress& address,
                          openscreen::NetworkInterfaceIndex ifindex) override;
  void SendMessage(openscreen::ByteView data,
                   const openscreen::IPEndpoint& dest) final;
  void SetDscp(openscreen::UdpSocket::DscpMode state) override;

  const raw_ptr<Client> client_;

  // The local endpoint can change as a result of Bind() calls.
  openscreen::IPEndpoint local_endpoint_;
  net::UDPSocket udp_socket_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  net::IPEndPoint from_address_;
  bool send_pending_ = false;

  base::WeakPtrFactory<NetUdpSocket> weak_ptr_factory_{this};
};

}  // namespace openscreen_platform

#endif  // COMPONENTS_OPENSCREEN_PLATFORM_NET_UDP_SOCKET_H_
