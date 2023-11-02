// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/rpc_demuxer_stream_handler.h"

#include <sstream>

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
    RpcProcessMessageCB process_message_cb)
    : client_(client),
      handle_factory_(std::move(handle_factory)),
      process_message_cb_(std::move(process_message_cb)) {
  DCHECK(handle_factory_);
  DCHECK(process_message_cb_);
}

RpcDemuxerStreamHandler::~RpcDemuxerStreamHandler() = default;

void RpcDemuxerStreamHandler::OnRpcAcquireDemuxer(
    openscreen::cast::RpcMessenger::Handle audio_stream_handle,
    openscreen::cast::RpcMessenger::Handle video_stream_handle) {
  // Initialization of the Demuxer happens automatically, so immediately
  // initialize the DemuxerStreams.
  if (audio_stream_handle != openscreen::cast::RpcMessenger::kInvalidHandle) {
    audio_message_processor_ = std::make_unique<MessageProcessor>(
        client_, process_message_cb_, handle_factory_.Run(),
        audio_stream_handle, MessageProcessor::Type::kAudio);
    std::unique_ptr<openscreen::cast::RpcMessage> message =
        remoting::CreateMessageForDemuxerStreamInitialize(
            audio_message_processor_->local_handle());
    process_message_cb_.Run(audio_message_processor_->remote_handle(),
                            std::move(message));
  }

  if (video_stream_handle != openscreen::cast::RpcMessenger::kInvalidHandle) {
    video_message_processor_ = std::make_unique<MessageProcessor>(
        client_, process_message_cb_, handle_factory_.Run(),
        video_stream_handle, MessageProcessor::Type::kVideo);
    std::unique_ptr<openscreen::cast::RpcMessage> message =
        remoting::CreateMessageForDemuxerStreamInitialize(
            video_message_processor_->local_handle());
    process_message_cb_.Run(video_message_processor_->remote_handle(),
                            std::move(message));
  }
}

void RpcDemuxerStreamHandler::OnRpcEnableBitstreamConverterCallback(
    openscreen::cast::RpcMessenger::Handle handle,
    bool succeeded) {
  if (audio_message_processor_ &&
      handle == audio_message_processor_->local_handle()) {
    audio_message_processor_->OnBitstreamConverterEnabled(succeeded);
  } else if (video_message_processor_ &&
             handle == video_message_processor_->local_handle()) {
    video_message_processor_->OnBitstreamConverterEnabled(succeeded);
  } else {
    std::stringstream logstream;
    logstream
        << "OnRpcEnableBitstreamConverterCallback received for invalid handle "
        << handle << ". Valid handles are";
    if (audio_message_processor_) {
      logstream << " '" << audio_message_processor_->local_handle()
                << "'' for audio";
    }
    if (video_message_processor_) {
      logstream << " '" << video_message_processor_->local_handle()
                << "'' for video";
    }

    LOG(WARNING) << logstream.str();
  }
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

void RpcDemuxerStreamHandler::OnRpcBitstreamConverterEnabled(
    openscreen::cast::RpcMessenger::Handle handle,
    bool success) {
  if (audio_message_processor_ &&
      handle == audio_message_processor_->local_handle()) {
    audio_message_processor_->OnBitstreamConverterEnabled(success);
  } else if (video_message_processor_ &&
             handle == video_message_processor_->local_handle()) {
    video_message_processor_->OnBitstreamConverterEnabled(success);
  } else {
    LOG(WARNING)
        << "OnRpcBitstreamConverterEnabled received for invalid handle";
  }
}

base::WeakPtr<DemuxerStreamClient> RpcDemuxerStreamHandler::GetAudioClient() {
  if (!audio_message_processor_) {
    return nullptr;
  }

  return audio_message_processor_->GetWeakPtr();
}

base::WeakPtr<DemuxerStreamClient> RpcDemuxerStreamHandler::GetVideoClient() {
  if (!video_message_processor_) {
    return nullptr;
  }

  return video_message_processor_->GetWeakPtr();
}

RpcDemuxerStreamHandler::Client::~Client() = default;

RpcDemuxerStreamHandler::MessageProcessor::MessageProcessor(
    Client* client,
    RpcProcessMessageCB process_message_cb,
    openscreen::cast::RpcMessenger::Handle local_handle,
    openscreen::cast::RpcMessenger::Handle remote_handle,
    Type type)
    : client_(client),
      process_message_cb_(std::move(process_message_cb)),
      local_handle_(local_handle),
      remote_handle_(remote_handle),
      type_(type),
      weak_factory_(this) {
  DCHECK(client_);
  DCHECK_NE(local_handle_, openscreen::cast::RpcMessenger::kInvalidHandle);
  DCHECK_NE(remote_handle_, openscreen::cast::RpcMessenger::kInvalidHandle);
}

RpcDemuxerStreamHandler::MessageProcessor::~MessageProcessor() = default;

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
    LOG(WARNING) << "Failed to process OnRpcReadUntilCallback.";
    return false;
  }

  total_frames_received_ = total_frames_received;
  is_read_until_call_pending_ = false;
  return true;
}

void RpcDemuxerStreamHandler::MessageProcessor::OnBitstreamConverterEnabled(
    bool success) {
  if (!bitstream_converter_enabled_cb_) {
    return;
  }

  std::move(bitstream_converter_enabled_cb_).Run(success);
}

base::WeakPtr<RpcDemuxerStreamHandler::MessageProcessor>
RpcDemuxerStreamHandler::MessageProcessor::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void RpcDemuxerStreamHandler::MessageProcessor::EnableBitstreamConverter(
    BitstreamConverterEnabledCB cb) {
  DCHECK(!bitstream_converter_enabled_cb_);
  bitstream_converter_enabled_cb_ = std::move(cb);

  auto message = CreateMessageForDemuxerStreamEnableBitstreamConverter();
  process_message_cb_.Run(remote_handle(), std::move(message));
}

void RpcDemuxerStreamHandler::MessageProcessor::OnNoBuffersAvailable() {
  if (is_read_until_call_pending()) {
    return;
  }

  set_read_until_call_pending();
  auto message = CreateMessageForDemuxerStreamReadUntil(
      local_handle(), total_frames_received() + kNumFramesInEachReadUntil);
  process_message_cb_.Run(remote_handle(), std::move(message));
}

void RpcDemuxerStreamHandler::MessageProcessor::OnError() {
  auto message = CreateMessageForDemuxerStreamError();
  process_message_cb_.Run(remote_handle(), std::move(message));
}

}  // namespace cast_streaming::remoting
