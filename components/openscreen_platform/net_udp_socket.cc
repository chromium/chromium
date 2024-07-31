// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/openscreen_platform/net_udp_socket.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "components/openscreen_platform/network_util.h"
#include "net/base/net_errors.h"

namespace openscreen {

// static
ErrorOr<std::unique_ptr<UdpSocket>> UdpSocket::Create(
    TaskRunner& task_runner,
    Client* client,
    const IPEndpoint& local_endpoint) {
  return ErrorOr<std::unique_ptr<UdpSocket>>(
      std::make_unique<openscreen_platform::NetUdpSocket>(client,
                                                          local_endpoint));
}

}  // namespace openscreen

namespace openscreen_platform {

NetUdpSocket::NetUdpSocket(openscreen::UdpSocket::Client* client,
                           const openscreen::IPEndpoint& local_endpoint)
    : client_(client),
      local_endpoint_(local_endpoint),
      udp_socket_(net::DatagramSocket::DEFAULT_BIND,
                  nullptr /* net_log */,
                  net::NetLogSource()),
      read_buffer_(base::MakeRefCounted<net::IOBufferWithSize>(
          openscreen::UdpPacket::kUdpMaxPacketSize)) {
  DVLOG(1) << __func__;
  DCHECK(client_);
}

NetUdpSocket::~NetUdpSocket() = default;

void NetUdpSocket::SendErrorToClient(openscreen::Error::Code openscreen_error,
                                     int net_error) {
  DVLOG(1) << __func__;
  client_->OnError(
      this, openscreen::Error(openscreen_error, net::ErrorToString(net_error)));
}

void NetUdpSocket::DoRead() {
  DVLOG(3) << __func__;
  while (HandleRecvFromResult(udp_socket_.RecvFrom(
      read_buffer_.get(), openscreen::UdpPacket::kUdpMaxPacketSize,
      &from_address_,
      base::BindOnce(&NetUdpSocket::OnRecvFromCompleted,
                     base::Unretained(this))))) {
  }
}

bool NetUdpSocket::HandleRecvFromResult(int result) {
  DVLOG(3) << __func__;

  if (result == net::ERR_IO_PENDING) {
    return false;
  }

  if (result < 0) {
    client_->OnRead(
        this, openscreen::Error(openscreen::Error::Code::kSocketReadFailure,
                                net::ErrorToString(result)));
    return false;
  }

  DCHECK_GT(result, 0);

  openscreen::UdpPacket packet(read_buffer_->data(),
                               read_buffer_->data() + result);
  packet.set_source(openscreen_platform::ToOpenScreenEndPoint(from_address_));
  client_->OnRead(this, std::move(packet));
  return true;
}

void NetUdpSocket::OnRecvFromCompleted(int result) {
  DVLOG(3) << __func__;
  if (HandleRecvFromResult(result)) {
    DoRead();
  }
}

void NetUdpSocket::OnSendToCompleted(int result) {
  DVLOG(3) << __func__;
  send_pending_ = false;
  if (result < 0) {
    client_->OnSendError(
        this, openscreen::Error(openscreen::Error::Code::kSocketSendFailure,
                                net::ErrorToString(result)));
  }
}

bool NetUdpSocket::IsIPv4() const {
  DVLOG(2) << __func__;
  return local_endpoint_.address.IsV4();
}

bool NetUdpSocket::IsIPv6() const {
  DVLOG(2) << __func__;
  return local_endpoint_.address.IsV6();
}

openscreen::IPEndpoint NetUdpSocket::GetLocalEndpoint() const {
  DVLOG(2) << __func__;
  return local_endpoint_;
}

void NetUdpSocket::Bind() {
  DVLOG(1) << __func__;
  net::IPEndPoint endpoint =
      openscreen_platform::ToNetEndPoint(local_endpoint_);
  int result = udp_socket_.Open(endpoint.GetFamily());
  if (result != net::OK) {
    SendErrorToClient(openscreen::Error::Code::kSocketBindFailure, result);
    return;
  }

  result = udp_socket_.Bind(endpoint);
  net::IPEndPoint local_endpoint;
  if (result == net::OK) {
    result = udp_socket_.GetLocalAddress(&local_endpoint);
  }

  if (result != net::OK) {
    SendErrorToClient(openscreen::Error::Code::kSocketBindFailure, result);
    return;
  }

  local_endpoint_ = openscreen_platform::ToOpenScreenEndPoint(local_endpoint);
  client_->OnBound(this);
  DoRead();
}

void NetUdpSocket::SetMulticastOutboundInterface(
    openscreen::NetworkInterfaceIndex ifindex) {
  DVLOG(1) << __func__;
  const int result = udp_socket_.SetMulticastInterface(ifindex);
  if (result != net::OK) {
    SendErrorToClient(openscreen::Error::Code::kSocketOptionSettingFailure,
                      result);
  }
}

void NetUdpSocket::JoinMulticastGroup(
    const openscreen::IPAddress& address,
    openscreen::NetworkInterfaceIndex ifindex) {
  DVLOG(1) << __func__;
  const int result = udp_socket_.SetMulticastInterface(ifindex);
  if (result == net::OK) {
    udp_socket_.JoinGroup(openscreen_platform::ToNetAddress(address));
  } else {
    SendErrorToClient(openscreen::Error::Code::kSocketOptionSettingFailure,
                      result);
  }
}

void NetUdpSocket::SendMessage(openscreen::ByteView data,
                               const openscreen::IPEndpoint& dest) {
  DVLOG(3) << __func__;

  if (send_pending_) {
    client_->OnSendError(this,
                         openscreen::Error(openscreen::Error::Code::kAgain));
    return;
  }

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(data.size());
  memcpy(buffer->data(), data.data(), data.size());

  const int result = udp_socket_.SendTo(
      buffer.get(), data.size(), openscreen_platform::ToNetEndPoint(dest),
      base::BindOnce(&NetUdpSocket::OnSendToCompleted, base::Unretained(this)));
  send_pending_ = true;

  if (result != net::ERR_IO_PENDING) {
    OnSendToCompleted(result);
  }
}

void NetUdpSocket::SetDscp(openscreen::UdpSocket::DscpMode state) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace openscreen_platform
