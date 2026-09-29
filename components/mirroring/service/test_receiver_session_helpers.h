// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_TEST_RECEIVER_SESSION_HELPERS_H_
#define COMPONENTS_MIRRORING_SERVICE_TEST_RECEIVER_SESSION_HELPERS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/filters/vpx_video_decoder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/openscreen/src/cast/streaming/public/constants.h"
#include "third_party/openscreen/src/cast/streaming/public/environment.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver_session.h"
#include "third_party/openscreen/src/platform/base/ip_address.h"

namespace mirroring {

// In-memory OpenScreen Environment without physical UDP socket binding.
class TestReceiverEnvironment : public openscreen::cast::Environment {
 public:
  TestReceiverEnvironment(openscreen::ClockNowFunctionPtr now_function,
                          openscreen::TaskRunner& task_runner);
  ~TestReceiverEnvironment() override;

  void DeliverPacket(const openscreen::IPEndpoint& source,
                     base::span<const uint8_t> packet);

  void SendPacket(openscreen::ByteView packet,
                  openscreen::cast::PacketMetadata metadata) override;

  void set_packet_send_cb(
      base::RepeatingCallback<void(const std::vector<uint8_t>&)> cb) {
    packet_send_cb_ = std::move(cb);
  }

  openscreen::IPEndpoint GetBoundLocalEndpoint() const override;

 private:
  base::RepeatingCallback<void(const std::vector<uint8_t>&)> packet_send_cb_;
};

// Bridges OpenScreen MessagePort with Mojo CastMessageChannel.
class TestReceiverMessagePort final : public openscreen::cast::MessagePort,
                                      public mojom::CastMessageChannel {
 public:
  TestReceiverMessagePort(
      mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
      mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel);
  ~TestReceiverMessagePort() override;

  void SetClient(Client& client) override;
  void ResetClient() override;

  void PostMessage(const std::string& destination_sender_id,
                   const std::string& message_namespace,
                   const std::string& message) override;

  // mojom::CastMessageChannel implementation.
  void OnMessage(mojom::CastMessagePtr message) override;

 private:
  const mojo::Remote<mojom::CastMessageChannel> outbound_channel_;
  const mojo::Receiver<mojom::CastMessageChannel> inbound_channel_;
  raw_ptr<Client> client_ = nullptr;
};

// OpenScreen Receiver::Consumer that consumes and decodes frames using
// media::VpxVideoDecoder.
class ReceiverConsumer : public openscreen::cast::Receiver::Consumer {
 public:
  explicit ReceiverConsumer(media::VideoCodec codec);
  ~ReceiverConsumer() override;

  void SetReceiver(openscreen::cast::Receiver* receiver,
                   openscreen::cast::Environment* environment);

  // openscreen::cast::Receiver::Consumer implementation.
  void OnFramesReady(size_t next_frame_buffer_size) override;

  const std::vector<scoped_refptr<media::VideoFrame>>& decoded_frames() const {
    return decoded_frames_;
  }

  void set_on_frame_decoded(
      base::RepeatingCallback<void(scoped_refptr<media::VideoFrame>)> cb) {
    on_frame_decoded_ = std::move(cb);
  }

 private:
  void OnFrameDecoded(scoped_refptr<media::VideoFrame> frame);

  raw_ptr<openscreen::cast::Receiver> receiver_ = nullptr;
  raw_ptr<openscreen::cast::Environment> environment_ = nullptr;
  const media::VideoCodec codec_;
  std::unique_ptr<media::VpxVideoDecoder> decoder_;
  std::vector<scoped_refptr<media::VideoFrame>> decoded_frames_;
  base::RepeatingCallback<void(scoped_refptr<media::VideoFrame>)>
      on_frame_decoded_;
};

// OpenScreen ReceiverSession::Client implementation for testing.
class ReceiverSessionClientHelper
    : public openscreen::cast::ReceiverSession::Client {
 public:
  ReceiverSessionClientHelper();
  ~ReceiverSessionClientHelper() override;

  // openscreen::cast::ReceiverSession::Client implementation.
  void OnNegotiated(const openscreen::cast::ReceiverSession* session,
                    openscreen::cast::ReceiverSession::ConfiguredReceivers
                        receivers) override;
  void OnReceiversDestroying(
      const openscreen::cast::ReceiverSession* session,
      openscreen::cast::ReceiverSession::Client::ReceiversDestroyingReason
          reason) override;
  void OnError(const openscreen::cast::ReceiverSession* session,
               const openscreen::Error& error) override;

  void set_on_negotiated(base::OnceClosure cb) {
    on_negotiated_ = std::move(cb);
  }

  void set_consumer(ReceiverConsumer* consumer,
                    openscreen::cast::Environment* env) {
    consumer_ = consumer;
    receiver_env_ = env;
  }

  const openscreen::cast::ReceiverSession::ConfiguredReceivers&
  configured_receivers() const {
    return configured_receivers_;
  }

 private:
  raw_ptr<ReceiverConsumer> consumer_ = nullptr;
  raw_ptr<openscreen::cast::Environment> receiver_env_ = nullptr;
  openscreen::cast::ReceiverSession::ConfiguredReceivers configured_receivers_;
  base::OnceClosure on_negotiated_;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_TEST_RECEIVER_SESSION_HELPERS_H_
