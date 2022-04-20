// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/public/rpc_call_message_handler.h"

#include "base/check.h"
#include "base/logging.h"
#include "components/cast_streaming/public/remoting_proto_enum_utils.h"
#include "components/cast_streaming/public/remoting_proto_utils.h"
#include "media/base/demuxer_stream.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace cast_streaming {
namespace remoting {
namespace {

template <typename T>
absl::optional<media::AudioDecoderConfig> ExtractAudioConfig(
    const T& config_container) {
  if (!config_container.has_audio_decoder_config()) {
    return absl::nullopt;
  }

  const auto& audio_message = config_container.audio_decoder_config();
  media::AudioDecoderConfig config;
  ConvertProtoToAudioDecoderConfig(audio_message, &config);
  if (!config.IsValidConfig()) {
    return absl::nullopt;
  }

  return config;
}

template <typename T>
absl::optional<media::VideoDecoderConfig> ExtractVideoConfig(
    const T& config_container) {
  if (!config_container.has_video_decoder_config()) {
    return absl::nullopt;
  }

  const auto& video_message = config_container.video_decoder_config();
  media::VideoDecoderConfig config;
  ConvertProtoToVideoDecoderConfig(video_message, &config);
  if (!config.IsValidConfig()) {
    return absl::nullopt;
  }

  return config;
}

}  // namespace

RpcInitializationCallMessageHandler::~RpcInitializationCallMessageHandler() =
    default;

RpcRendererCallMessageHandler::~RpcRendererCallMessageHandler() = default;

RpcDemuxerStreamCBMessageHandler::~RpcDemuxerStreamCBMessageHandler() = default;

bool DispatchInitializationRpcCall(
    openscreen::cast::RpcMessage* message,
    RpcInitializationCallMessageHandler* client) {
  DCHECK(message);
  DCHECK(client);

  switch (message->proc()) {
    case openscreen::cast::RpcMessage::RPC_ACQUIRE_RENDERER: {
      if (!message->has_integer_value()) {
        return false;
      }
      client->OnRpcAcquireRenderer(message->integer_value());
      return true;
    }
    case openscreen::cast::RpcMessage::RPC_ACQUIRE_DEMUXER: {
      if (!message->has_acquire_demuxer_rpc()) {
        LOG(ERROR) << "RPC_ACQUIRE_DEMUXER with no acquire_demuxer_rpc() "
                      "property received";
        return false;
      }
      const int audio_stream_handle =
          message->acquire_demuxer_rpc().audio_demuxer_handle();
      const int video_stream_handle =
          message->acquire_demuxer_rpc().video_demuxer_handle();
      client->OnRpcAcquireDemuxer(audio_stream_handle, video_stream_handle);
      return true;
    }
    default:
      return false;
  }
}

bool DispatchRendererRpcCall(openscreen::cast::RpcMessage* message,
                             RpcRendererCallMessageHandler* client) {
  DCHECK(message);
  DCHECK(client);

  switch (message->proc()) {
    case openscreen::cast::RpcMessage::RPC_R_INITIALIZE:
      client->OnRpcInitialize();
      return true;
    case openscreen::cast::RpcMessage::RPC_R_FLUSHUNTIL: {
      if (!message->has_renderer_flushuntil_rpc()) {
        LOG(ERROR) << "RPC_R_FLUSHUNTIL with no renderer_flushuntil_rpc() "
                      "property received";
        return false;
      }
      const openscreen::cast::RendererFlushUntil flush_message =
          message->renderer_flushuntil_rpc();
      client->OnRpcFlush(flush_message.audio_count(),
                         flush_message.video_count());
      return true;
    }
    case openscreen::cast::RpcMessage::RPC_R_STARTPLAYINGFROM: {
      if (!message->has_integer64_value()) {
        return false;
      }
      const base::TimeDelta time =
          base::Microseconds(message->integer64_value());
      client->OnRpcStartPlayingFrom(time);
      return true;
    }
    case openscreen::cast::RpcMessage::RPC_R_SETPLAYBACKRATE: {
      if (!message->has_double_value()) {
        return false;
      }
      client->OnRpcSetPlaybackRate(message->double_value());
      return true;
    }
    case openscreen::cast::RpcMessage::RPC_R_SETVOLUME: {
      if (!message->has_double_value()) {
        return false;
      }
      client->OnRpcSetVolume(message->double_value());
      return true;
    }
    default:
      return false;
  }
}

bool DispatchDemuxerStreamCBRpcCall(openscreen::cast::RpcMessage* message,
                                    RpcDemuxerStreamCBMessageHandler* client) {
  DCHECK(message);
  DCHECK(client);

  switch (message->proc()) {
    case openscreen::cast::RpcMessage::RPC_DS_INITIALIZE_CALLBACK: {
      if (!message->has_demuxerstream_initializecb_rpc()) {
        LOG(ERROR) << "RPC_DS_INITIALIZE_CALLBACK with no "
                      "demuxerstream_initializecb_rpc() property received";
        return false;
      }
      const auto& callback_message = message->demuxerstream_initializecb_rpc();
      client->OnRpcInitializeCallback(message->handle(),
                                      ExtractAudioConfig(callback_message),
                                      ExtractVideoConfig(callback_message));
      return true;
    }
    case openscreen::cast::RpcMessage::RPC_DS_READUNTIL_CALLBACK: {
      if (!message->has_demuxerstream_readuntilcb_rpc()) {
        LOG(ERROR) << "RPC_DS_READUNTIL with no "
                      "demuxerstream_readuntilcb_rpc() property received";
        return false;
      }
      const auto& rpc_message = message->demuxerstream_readuntilcb_rpc();
      auto audio_config = ExtractAudioConfig(rpc_message);
      auto video_config = ExtractVideoConfig(rpc_message);

      const auto status = ToDemuxerStreamStatus(rpc_message.status());
      if ((audio_config || video_config) &&
          status != media::DemuxerStream::kConfigChanged) {
        LOG(ERROR) << "RPC_DS_READUNTIL with status != kConfigChanged contains "
                      "a new config";
        return false;
      } else if (!audio_config && !video_config &&
                 status == media::DemuxerStream::kConfigChanged) {
        LOG(ERROR) << "RPC_DS_READUNTIL with status = kConfigChanged contains "
                      "no config";
        return false;
      }
      const uint32_t total_frames_received = rpc_message.count();
      client->OnRpcReadUntilCallback(message->handle(), std::move(audio_config),
                                     std::move(video_config),
                                     total_frames_received);
      return true;
    }
    default:
      return false;
  }
}

}  // namespace remoting
}  // namespace cast_streaming
