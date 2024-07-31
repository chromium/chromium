// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/openscreen_platform/udp_socket.h"

#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "components/openscreen_platform/network_context.h"
#include "components/openscreen_platform/network_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/address_family.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/openscreen/src/platform/base/udp_packet.h"

// Open Screen expects us to provide linked implementations of some of its
// static create methods, which have to be in their namespace.
namespace openscreen {

// static
ErrorOr<std::unique_ptr<UdpSocket>> UdpSocket::Create(
    TaskRunner& task_runner,
    Client* client,
    const IPEndpoint& local_endpoint) {
  network::mojom::NetworkContext* const network_context =
      openscreen_platform::GetNetworkContext();
  if (!network_context) {
    return Error::Code::kInitializationFailure;
  }

  mojo::PendingRemote<network::mojom::UDPSocketListener> listener_remote;
  mojo::PendingReceiver<network::mojom::UDPSocketListener> pending_listener =
      listener_remote.InitWithNewPipeAndPassReceiver();

  mojo::Remote<network::mojom::UDPSocket> socket;
  network_context->CreateUDPSocket(socket.BindNewPipeAndPassReceiver(),
                                   std::move(listener_remote));

  return ErrorOr<std::unique_ptr<UdpSocket>>(
      std::make_unique<openscreen_platform::UdpSocket>(
          client, local_endpoint, std::move(socket),
          std::move(pending_listener)));
}

}  // namespace openscreen

namespace openscreen_platform {

namespace {

using openscreen::ByteView;
using openscreen::Error;
using openscreen::IPAddress;
using openscreen::IPEndpoint;
using openscreen::UdpPacket;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("openscreen_message", R"(
        semantics {
          sender: "Open Screen"
          description:
            "Open Screen messages are used by the third_party Open Screen "
            "library, in accordance to the specification defined by the Open "
            "Screen protocol. The protocol is available publicly at: "
            "https://github.com/webscreens/openscreenprotocol"
          trigger:
            "Any message that needs to be sent or received by the Open Screen "
            "library."
          data:
            "Messages defined by the Open Screen Protocol specification."
          destination: OTHER
          destination_other:
            "The connection is made to an Open Screen endpoint on the LAN "
            "selected by the user, i.e. via a dialog."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be disabled, but it would not be sent if user "
            "does not connect to a Open Screen endpoint on the local network."
          policy_exception_justification: "Not implemented."
        })");

}  // namespace

UdpSocket::UdpSocket(
    Client* client,
    const IPEndpoint& local_endpoint,
    mojo::Remote<network::mojom::UDPSocket> udp_socket,
    mojo::PendingReceiver<network::mojom::UDPSocketListener> pending_listener)
    : client_(client),
      local_endpoint_(local_endpoint),
      udp_socket_(std::move(udp_socket)),
      pending_listener_(std::move(pending_listener)) {
  DCHECK(client_);
}

UdpSocket::~UdpSocket() = default;

bool UdpSocket::IsIPv4() const {
  return local_endpoint_.address.IsV4();
}

bool UdpSocket::IsIPv6() const {
  return local_endpoint_.address.IsV6();
}

IPEndpoint UdpSocket::GetLocalEndpoint() const {
  return local_endpoint_;
}

void UdpSocket::Bind() {
  udp_socket_->Bind(
      openscreen_platform::ToNetEndPoint(local_endpoint_),
      nullptr /* socket_options */,
      base::BindOnce(&UdpSocket::BindCallback, weak_ptr_factory_.GetWeakPtr()));
}

// mojom::UDPSocket doesn't have a concept of network interface indices, so
// this is a noop.
void UdpSocket::SetMulticastOutboundInterface(
    openscreen::NetworkInterfaceIndex ifindex) {}

// mojom::UDPSocket doesn't have a concept of network interface indices, so
// the ifindex argument is ignored here.
void UdpSocket::JoinMulticastGroup(const IPAddress& address,
                                   openscreen::NetworkInterfaceIndex ifindex) {
  const auto join_address = openscreen_platform::ToNetAddress(address);
  udp_socket_->JoinGroup(join_address,
                         base::BindOnce(&UdpSocket::JoinGroupCallback,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void UdpSocket::SendMessage(ByteView data, const IPEndpoint& dest) {
  const auto send_to_address = openscreen_platform::ToNetEndPoint(dest);
  base::span<const uint8_t> data_span(data.data(), data.size());
  udp_socket_->SendTo(
      send_to_address, data_span,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      base::BindOnce(&UdpSocket::SendCallback, weak_ptr_factory_.GetWeakPtr()));
}

// mojom::UDPSocket doesn't have a concept of DSCP, so this is a noop.
void UdpSocket::SetDscp(openscreen::UdpSocket::DscpMode state) {}

void UdpSocket::OnReceived(
    int32_t net_result,
    const std::optional<net::IPEndPoint>& source_endpoint,
    std::optional<base::span<const uint8_t>> data) {
  if (net_result != net::OK) {
    client_->OnRead(this, Error::Code::kSocketReadFailure);
  } else if (data) {
    UdpPacket packet(data->begin(), data->end());
    if (source_endpoint) {
      packet.set_source(
          openscreen_platform::ToOpenScreenEndPoint(source_endpoint.value()));
    }
    client_->OnRead(this, std::move(packet));
  }

  udp_socket_->ReceiveMore(1);
}

void UdpSocket::BindCallback(int32_t result,
                             const std::optional<net::IPEndPoint>& address) {
  if (result != net::OK) {
    client_->OnError(this, Error(Error::Code::kSocketBindFailure,
                                 net::ErrorToString(result)));
    return;
  }

  // This is an approximate value for number of packets, and may need to be
  // adjusted when we have real world data.
  constexpr int kNumPacketsReadyFor = 30;
  udp_socket_->ReceiveMore(kNumPacketsReadyFor);

  if (address) {
    local_endpoint_ =
        openscreen_platform::ToOpenScreenEndPoint(address.value());
    if (pending_listener_.is_valid()) {
      listener_.Bind(std::move(pending_listener_));
    }
  }
  client_->OnBound(this);
}

void UdpSocket::JoinGroupCallback(int32_t result) {
  if (result != net::OK) {
    client_->OnError(this, Error(Error::Code::kSocketOptionSettingFailure,
                                 net::ErrorToString(result)));
  }
}

void UdpSocket::SendCallback(int32_t result) {
  if (result != net::OK) {
    client_->OnSendError(this, Error(Error::Code::kSocketSendFailure,
                                     net::ErrorToString(result)));
  }
}

}  // namespace openscreen_platform
