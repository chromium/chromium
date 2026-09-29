// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/test_receiver_session_helpers.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder_config.h"
#include "third_party/openscreen/src/platform/base/udp_packet.h"

namespace mirroring {

namespace {
constexpr char kDefaultSenderId[] = "sender-123";
}  // namespace

TestReceiverEnvironment::TestReceiverEnvironment(
    openscreen::ClockNowFunctionPtr now_function,
    openscreen::TaskRunner& task_runner)
    : Environment(now_function, task_runner) {
  SetSocketStateForTesting(SocketState::kReady);
}

TestReceiverEnvironment::~TestReceiverEnvironment() = default;

void TestReceiverEnvironment::DeliverPacket(
    const openscreen::IPEndpoint& source,
    base::span<const uint8_t> packet) {
  openscreen::UdpPacket udp_packet(packet.begin(), packet.end());
  udp_packet.set_source(source);
  static_cast<openscreen::UdpSocket::Client*>(this)->OnRead(
      nullptr, std::move(udp_packet));
}

void TestReceiverEnvironment::SendPacket(
    openscreen::ByteView packet,
    openscreen::cast::PacketMetadata metadata) {
  if (packet_send_cb_) {
    packet_send_cb_.Run(std::vector<uint8_t>(packet.begin(), packet.end()));
  }
}

openscreen::IPEndpoint TestReceiverEnvironment::GetBoundLocalEndpoint() const {
  return openscreen::IPEndpoint{openscreen::IPAddress{127, 0, 0, 1}, 1234};
}

TestReceiverMessagePort::TestReceiverMessagePort(
    mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
    mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel)
    : outbound_channel_(std::move(outbound_channel)),
      inbound_channel_(this, std::move(inbound_channel)) {}

TestReceiverMessagePort::~TestReceiverMessagePort() = default;

void TestReceiverMessagePort::SetClient(Client& client) {
  client_ = &client;
}

void TestReceiverMessagePort::ResetClient() {
  client_ = nullptr;
}

void TestReceiverMessagePort::PostMessage(
    const std::string& destination_sender_id,
    const std::string& message_namespace,
    const std::string& message) {
  auto message_mojom = mojom::CastMessage::New();
  message_mojom->message_namespace = message_namespace;
  message_mojom->json_format_data = message;
  outbound_channel_->OnMessage(std::move(message_mojom));
}

void TestReceiverMessagePort::OnMessage(mojom::CastMessagePtr message) {
  if (client_) {
    client_->OnMessage(kDefaultSenderId, message->message_namespace,
                       message->json_format_data);
  }
}

ReceiverConsumer::ReceiverConsumer(media::VideoCodec codec)
    : codec_(codec), decoder_(std::make_unique<media::VpxVideoDecoder>()) {
  const media::VideoCodecProfile profile = (codec_ == media::VideoCodec::kVP9)
                                               ? media::VP9PROFILE_PROFILE0
                                               : media::VP8PROFILE_ANY;
  const gfx::Size initial_size(1920, 1080);
  const media::VideoDecoderConfig config(
      codec_, profile, media::VideoDecoderConfig::AlphaMode::kIsOpaque,
      media::VideoColorSpace::JPEG(), media::VideoTransformation(),
      initial_size, gfx::Rect(initial_size), initial_size,
      media::EmptyExtraData(), media::EncryptionScheme::kUnencrypted);

  decoder_->Initialize(config, /*low_delay=*/true, nullptr,
                       base::BindOnce([](media::DecoderStatus status) {
                         CHECK(status.is_ok());
                       }),
                       base::BindRepeating(&ReceiverConsumer::OnFrameDecoded,
                                           base::Unretained(this)),
                       base::NullCallback());
}

ReceiverConsumer::~ReceiverConsumer() = default;

void ReceiverConsumer::SetReceiver(openscreen::cast::Receiver* receiver,
                                   openscreen::cast::Environment* environment) {
  receiver_ = receiver;
  environment_ = environment;
}

void ReceiverConsumer::OnFramesReady(size_t next_frame_buffer_size) {
  if (!receiver_) {
    return;
  }

  std::vector<uint8_t> frame_buffer(next_frame_buffer_size);
  openscreen::cast::EncodedFrame frame = receiver_->ConsumeNextFrame(
      openscreen::ByteBuffer(frame_buffer.data(), frame_buffer.size()));

  receiver_->ReportPlayoutEvent(frame.frame_id, frame.rtp_timestamp,
                                environment_->now());

  const openscreen::cast::RtpTimeDelta delta =
      frame.rtp_timestamp - openscreen::cast::RtpTimeTicks();
  const auto duration = delta.ToDuration<std::chrono::microseconds>(90000);

  scoped_refptr<media::DecoderBuffer> buffer = media::DecoderBuffer::CopyFrom(
      base::span(frame_buffer).first(frame.data.size()));
  buffer->set_timestamp(base::Microseconds(duration.count()));
  buffer->set_is_key_frame(
      frame.dependency ==
      openscreen::cast::EncodedFrame::Dependency::kKeyFrame);

  decoder_->Decode(std::move(buffer),
                   base::BindOnce([](media::DecoderStatus status) {
                     CHECK(status.is_ok());
                   }));
}

void ReceiverConsumer::OnFrameDecoded(scoped_refptr<media::VideoFrame> frame) {
  if (frame) {
    decoded_frames_.push_back(std::move(frame));
    if (on_frame_decoded_) {
      on_frame_decoded_.Run(decoded_frames_.back());
    }
  }
}

ReceiverSessionClientHelper::ReceiverSessionClientHelper() = default;

ReceiverSessionClientHelper::~ReceiverSessionClientHelper() = default;

void ReceiverSessionClientHelper::OnNegotiated(
    const openscreen::cast::ReceiverSession* session,
    openscreen::cast::ReceiverSession::ConfiguredReceivers receivers) {
  configured_receivers_ = receivers;
  if (consumer_ && configured_receivers_.video_receiver) {
    consumer_->SetReceiver(configured_receivers_.video_receiver, receiver_env_);
    configured_receivers_.video_receiver->SetConsumer(consumer_);
  }
  if (on_negotiated_) {
    std::move(on_negotiated_).Run();
  }
}

void ReceiverSessionClientHelper::OnReceiversDestroying(
    const openscreen::cast::ReceiverSession* session,
    openscreen::cast::ReceiverSession::Client::ReceiversDestroyingReason
        reason) {
  configured_receivers_ = {};
  consumer_ = nullptr;
  receiver_env_ = nullptr;
}

void ReceiverSessionClientHelper::OnError(
    const openscreen::cast::ReceiverSession* session,
    const openscreen::Error& error) {}

}  // namespace mirroring
