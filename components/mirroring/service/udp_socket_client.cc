// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/udp_socket_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "net/base/address_family.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace mirroring {

namespace {

// The minimal number of packets asking for receiving.
constexpr int kNumPacketsAsking = 1024;

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation("cast_udp_socket", R"(
      semantics {
        sender: "Cast Streaming"
        description:
          "Media streaming protocol for LAN transport of screen mirroring "
          "audio/video. This is also used by browser features that wish to "
          "send browser content for remote display, and such features are "
          "generally started/stopped from the Media Router dialog."
        trigger:
          "User invokes feature from the Media Router dialog (right click on "
          "page, 'Cast...')."
        data:
          "Media and related protocol-level control and performance messages."
        destination: OTHER
        destination_other:
          "A playback receiver, such as a Chromecast device."
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled in settings."
        chrome_policy {
          EnableMediaRouter {
            EnableMediaRouter: false
          }
        }
      })");
}

}  // namespace

UdpSocketClient::UdpSocketClient(const net::IPEndPoint& remote_endpoint,
                                 network::mojom::NetworkContext* context,
                                 base::OnceClosure error_callback)
    : remote_endpoint_(remote_endpoint),
      network_context_(context),
      error_callback_(std::move(error_callback)),
      bytes_sent_(0),
      allow_sending_(false),
      num_packets_pending_receive_(0) {
  DCHECK(network_context_);
}

UdpSocketClient::~UdpSocketClient() {}

bool UdpSocketClient::SendPacket(media::cast::PacketRef packet,
                                 const base::RepeatingClosure& cb) {
  DVLOG(3) << __func__;
  DCHECK(resume_send_callback_.is_null());

  bytes_sent_ += packet->data.size();
  if (!allow_sending_) {
    resume_send_callback_ = cb;
    return false;
  }

  DCHECK(udp_socket_);
  udp_socket_->Send(
      packet->data,
      net::MutableNetworkTrafficAnnotationTag(GetNetworkTrafficAnnotationTag()),
      base::BindOnce(&UdpSocketClient::OnPacketSent,
                     weak_factory_.GetWeakPtr()));
  return true;
}

void UdpSocketClient::OnPacketSent(int result) {
  if (result != net::OK)
    VLOG(2) << __func__ << ": error=" << result;

  // Block the further sending if too many send requests are pending.
  if (result == net::ERR_INSUFFICIENT_RESOURCES) {
    allow_sending_ = false;
    return;
  }

  allow_sending_ = true;
  if (!resume_send_callback_.is_null())
    std::move(resume_send_callback_).Run();
}

int64_t UdpSocketClient::GetBytesSent() {
  return bytes_sent_;
}

void UdpSocketClient::StartReceiving(
    const media::cast::PacketReceiverCallbackWithStatus& packet_receiver) {
  DVLOG(1) << __func__;
  packet_receiver_callback_ = packet_receiver;
  network_context_->CreateUDPSocket(udp_socket_.BindNewPipeAndPassReceiver(),
                                    receiver_.BindNewPipeAndPassRemote());
  network::mojom::UDPSocketOptionsPtr options;
  udp_socket_->Connect(remote_endpoint_, std::move(options),
                       base::BindOnce(&UdpSocketClient::OnSocketConnected,
                                      weak_factory_.GetWeakPtr()));
}

void UdpSocketClient::OnSocketConnected(
    int result,
    const base::Optional<net::IPEndPoint>& addr) {
  DVLOG(2) << __func__ << ": result=" << result;

  if (result == net::OK) {
    allow_sending_ = true;
    if (!resume_send_callback_.is_null())
      std::move(resume_send_callback_).Run();
  } else {
    allow_sending_ = false;
    VLOG(1) << "Socket connect error=" << result;
    if (!error_callback_.is_null())
      std::move(error_callback_).Run();
    return;
  }

  if (!packet_receiver_callback_.is_null()) {
    udp_socket_->ReceiveMore(kNumPacketsAsking);
    num_packets_pending_receive_ = kNumPacketsAsking;
  }
}

void UdpSocketClient::StopReceiving() {
  packet_receiver_callback_.Reset();
  if (receiver_.is_bound())
    receiver_.reset();
  if (udp_socket_.is_bound())
    udp_socket_.reset();
  num_packets_pending_receive_ = 0;
}

void UdpSocketClient::OnReceived(
    int32_t result,
    const base::Optional<net::IPEndPoint>& src_addr,
    base::Optional<base::span<const uint8_t>> data) {
  DVLOG(3) << __func__ << ": result=" << result;
  DCHECK_GT(num_packets_pending_receive_, 0);
  DCHECK(!packet_receiver_callback_.is_null());

  --num_packets_pending_receive_;
  if (num_packets_pending_receive_ < kNumPacketsAsking) {
    udp_socket_->ReceiveMore(kNumPacketsAsking);
    num_packets_pending_receive_ += kNumPacketsAsking;
  }
  if (result != net::OK)
    return;
  std::unique_ptr<media::cast::Packet> packet(
      new media::cast::Packet(data->begin(), data->end()));
  packet_receiver_callback_.Run(std::move(packet));
}

}  // namespace mirroring
