// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/public/rpc_call_message_handler.h"

#include "base/logging.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace cast_streaming {
namespace remoting {

RpcInitializationCallMessageHandler::~RpcInitializationCallMessageHandler() =
    default;

RpcRendererCallMessageHandler::~RpcRendererCallMessageHandler() = default;

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

}  // namespace remoting
}  // namespace cast_streaming
