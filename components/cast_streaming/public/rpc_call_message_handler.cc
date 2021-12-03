// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/public/rpc_call_message_handler.h"

#include "base/logging.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace cast_streaming {
namespace remoting {

RpcCallMessageHandler::~RpcCallMessageHandler() = default;

void DispatchRpcCall(std::unique_ptr<openscreen::cast::RpcMessage> message,
                     RpcCallMessageHandler* client) {
  DCHECK(message);
  DCHECK(client);

  switch (message->proc()) {
    case openscreen::cast::RpcMessage::RPC_R_INITIALIZE:
      client->OnRpcInitialize();
      return;
    case openscreen::cast::RpcMessage::RPC_R_FLUSHUNTIL: {
      DCHECK(message->has_renderer_flushuntil_rpc());
      const openscreen::cast::RendererFlushUntil flush_message =
          message->renderer_flushuntil_rpc();
      client->OnRpcFlush(flush_message.audio_count(),
                         flush_message.video_count());
      return;
    }
    case openscreen::cast::RpcMessage::RPC_R_STARTPLAYINGFROM: {
      const base::TimeDelta time =
          base::Microseconds(message->integer64_value());
      client->OnRpcStartPlayingFrom(time);
      return;
    }
    case openscreen::cast::RpcMessage::RPC_R_SETPLAYBACKRATE:
      client->OnRpcSetPlaybackRate(message->double_value());
      return;
    case openscreen::cast::RpcMessage::RPC_R_SETVOLUME:
      client->OnRpcSetVolume(message->double_value());
      return;
    default:
      break;
  }

  DVLOG(1) << __func__ << ": Unknown RPC message. proc=" << message->proc();
}

}  // namespace remoting
}  // namespace cast_streaming
