// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_PUBLIC_RPC_CALL_MESSAGE_HANDLER_H_
#define COMPONENTS_CAST_STREAMING_PUBLIC_RPC_CALL_MESSAGE_HANDLER_H_

#include "base/time/time.h"

namespace openscreen {
namespace cast {
class RpcMessage;
}  // namespace cast
}  // namespace openscreen

namespace cast_streaming {
namespace remoting {

// This class is responsible for translating between
// openscreen::cast::RpcMessage commands (used by the remoting protocol) and
// chromium types that are more easily usable.
class RpcInitializationCallMessageHandler {
 public:
  virtual ~RpcInitializationCallMessageHandler();

  virtual void OnRpcAcquireRenderer(int handle) = 0;
  virtual void OnRpcAcquireDemuxer(int audio_stream_handle,
                                   int video_stream_handle) = 0;
};

class RpcRendererCallMessageHandler {
 public:
  virtual ~RpcRendererCallMessageHandler();

  virtual void OnRpcInitialize() = 0;
  virtual void OnRpcFlush(uint32_t audio_count, uint32_t video_count) = 0;
  virtual void OnRpcStartPlayingFrom(base::TimeDelta time) = 0;
  virtual void OnRpcSetPlaybackRate(double playback_rate) = 0;
  virtual void OnRpcSetVolume(double volume) = 0;
};

// Processes the incoming |message| and forwards it to the appropriate |client|
// method.
bool DispatchInitializationRpcCall(openscreen::cast::RpcMessage* message,
                                   RpcInitializationCallMessageHandler* client);
bool DispatchRendererRpcCall(openscreen::cast::RpcMessage* message,
                             RpcRendererCallMessageHandler* client);

}  // namespace remoting
}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_PUBLIC_RPC_CALL_MESSAGE_HANDLER_H_
