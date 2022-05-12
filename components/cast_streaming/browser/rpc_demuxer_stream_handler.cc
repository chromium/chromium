// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/rpc_demuxer_stream_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "components/cast_streaming/public/remoting_message_factories.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace cast_streaming::remoting {
namespace {

// The number of frames requested in each ReadUntil RPC message.
constexpr int kNumFramesInEachReadUntil = 16;

}  // namespace

RpcDemuxerStreamHandler::RpcDemuxerStreamHandler(
    Client* client,
    HandleFactory handle_factory,
    RpcProcessMessageCB message_processor)
    : client_(client),
      handle_factory_(std::move(handle_factory)),
      message_processor_(std::move(message_processor)),
      weak_factory_(this) {
  DCHECK(handle_factory_);
  DCHECK(message_processor_);
}

RpcDemuxerStreamHandler::~RpcDemuxerStreamHandler() = default;

void RpcDemuxerStreamHandler::OnRpcAcquireDemuxer(
    openscreen::cast::RpcMessenger::Handle audio_stream_handle,
    openscreen::cast::RpcMessenger::Handle video_stream_handle) {
  // Initialization of the Demuxer happens automatically, so immediately
  // initialize the DemuxerStreams.
  if (audio_stream_handle != openscreen::cast::RpcMessenger::kInvalidHandle) {
    audio_message_processor_ = std::make_unique<MessageProcessor>(
        client_, handle_factory_.Run(), audio_stream_handle,
        MessageProcessor::Type::kAudio);
    std::unique_ptr<openscreen::cast::RpcMessage> message =
        remoting::CreateMessageForDemuxerStreamInitialize(
            audio_message_processor_->local_handle());
    message_processor_.Run(audio_message_processor_->remote_handle(),
                           std::move(message));
  }

  if (video_stream_handle != openscreen::cast::RpcMessenger::kInvalidHandle) {
    video_message_processor_ = std::make_unique<MessageProcessor>(
        client_, handle_factory_.Run(), video_stream_handle,
        MessageProcessor::Type::kVideo);
    std::unique_ptr<openscreen::cast::RpcMessage> message =
        remoting::CreateMessageForDemuxerStreamInitialize(
            video_message_processor_->local_handle());
    message_processor_.Run(video_message_processor_->remote_handle(),
                           std::move(message));
  }
}

void RpcDemuxerStreamHandler::RequestMoreAudioBuffers() {
  if (!audio_message_processor_) {
    return;
  }

  RequestMoreBuffers(audio_message_processor_.get());
}

void RpcDemuxerStreamHandler::RequestMoreVideoBuffers() {
  if (!video_message_processor_) {
    return;
  }

  RequestMoreBuffers(video_message_processor_.get());
}

void RpcDemuxerStreamHandler::RequestMoreBuffers(
    MessageProcessor* message_processor) {
  if (message_processor->is_read_until_call_pending()) {
    return;
  }

  message_processor->set_read_until_call_pending();
  auto message = CreateMessageForDemuxerStreamReadUntil(
      message_processor->local_handle(),
      message_processor->total_frames_received() + kNumFramesInEachReadUntil);
  message_processor_.Run(message_processor->remote_handle(),
                         std::move(message));
}

void RpcDemuxerStreamHandler::OnAudioError() {
  if (!audio_message_processor_) {
    return;
  }

  OnError(audio_message_processor_.get());
}

void RpcDemuxerStreamHandler::OnVideoError() {
  if (!video_message_processor_) {
    return;
  }

  OnError(video_message_processor_.get());
}

void RpcDemuxerStreamHandler::OnError(MessageProcessor* message_processor) {
  auto message = CreateMessageForDemuxerStreamError();
  message_processor_.Run(message_processor->remote_handle(),
                         std::move(message));
}

base::WeakPtr<RpcDemuxerStreamHandler> RpcDemuxerStreamHandler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void RpcDemuxerStreamHandler::OnRpcInitializeCallback(
    openscreen::cast::RpcMessenger::Handle handle,
    absl::optional<media::AudioDecoderConfig> audio_config,
    absl::optional<media::VideoDecoderConfig> video_config) {
  if (audio_message_processor_ &&
      handle == audio_message_processor_->local_handle()) {
    audio_message_processor_->OnRpcInitializeCallback(std::move(audio_config),
                                                      std::move(video_config));
  } else if (video_message_processor_ &&
             handle == video_message_processor_->local_handle()) {
    video_message_processor_->OnRpcInitializeCallback(std::move(audio_config),
                                                      std::move(video_config));
  } else {
    LOG(WARNING) << "OnRpcInitializeCallback received for invalid handle";
  }
}

void RpcDemuxerStreamHandler::OnRpcReadUntilCallback(
    openscreen::cast::RpcMessenger::Handle handle,
    absl::optional<media::AudioDecoderConfig> audio_config,
    absl::optional<media::VideoDecoderConfig> video_config,
    uint32_t total_frames_received) {
  if (audio_message_processor_ &&
      handle == audio_message_processor_->local_handle()) {
    audio_message_processor_->OnRpcReadUntilCallback(std::move(audio_config),
                                                     std::move(video_config),
                                                     total_frames_received);
  } else if (video_message_processor_ &&
             handle == video_message_processor_->local_handle()) {
    video_message_processor_->OnRpcReadUntilCallback(std::move(audio_config),
                                                     std::move(video_config),
                                                     total_frames_received);
  } else {
    LOG(WARNING) << "OnRpcReadUntilCallback received for invalid handle";
  }
}

RpcDemuxerStreamHandler::Client::~Client() = default;

RpcDemuxerStreamHandler::MessageProcessor::MessageProcessor(
    Client* client,
    openscreen::cast::RpcMessenger::Handle local_handle,
    openscreen::cast::RpcMessenger::Handle remote_handle,
    Type type)
    : client_(client),
      local_handle_(local_handle),
      remote_handle_(remote_handle),
      type_(type) {
  DCHECK(client_);
  DCHECK_NE(local_handle_, openscreen::cast::RpcMessenger::kInvalidHandle);
  DCHECK_NE(remote_handle_, openscreen::cast::RpcMessenger::kInvalidHandle);
}

bool RpcDemuxerStreamHandler::MessageProcessor::OnRpcInitializeCallback(
    absl::optional<media::AudioDecoderConfig> audio_config,
    absl::optional<media::VideoDecoderConfig> video_config) {
  if (audio_config && type_ != Type::kAudio) {
    LOG(WARNING) << "Received an audio config for a video DemuxerStream";
    return false;
  } else if (video_config && type_ != Type::kVideo) {
    LOG(WARNING) << "Received a video config for an audio DemuxerStream";
    return false;
  }

  if (audio_config) {
    client_->OnNewAudioConfig(std::move(audio_config.value()));
  } else if (video_config) {
    client_->OnNewVideoConfig(std::move(video_config.value()));
  }

  return true;
}

bool RpcDemuxerStreamHandler::MessageProcessor::OnRpcReadUntilCallback(
    absl::optional<media::AudioDecoderConfig> audio_config,
    absl::optional<media::VideoDecoderConfig> video_config,
    uint32_t total_frames_received) {
  if (!OnRpcInitializeCallback(std::move(audio_config),
                               std::move(video_config))) {
    return false;
  }

  total_frames_received_ = total_frames_received;
  is_read_until_call_pending_ = false;
  return true;
}

}  // namespace cast_streaming::remoting
