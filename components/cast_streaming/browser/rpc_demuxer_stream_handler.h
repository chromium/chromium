// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_RPC_DEMUXER_STREAM_HANDLER_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_RPC_DEMUXER_STREAM_HANDLER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/cast_streaming/public/rpc_call_message_handler.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "third_party/openscreen/src/cast/streaming/rpc_messenger.h"

namespace openscreen::cast {
class RpcMessage;
}  // namespace openscreen::cast

namespace cast_streaming::remoting {

// Wrapper around all RPC operations associated with a DemuxerStream. This one
// instance handles interactions with both audio and video DemuxerStream
// instances.
class RpcDemuxerStreamHandler : public RpcDemuxerStreamCBMessageHandler {
 public:
  // Class responsible for handling callbacks from this class upon a change in
  // config.
  class Client {
   public:
    virtual ~Client();

    virtual void OnNewAudioConfig(media::AudioDecoderConfig new_config) = 0;
    virtual void OnNewVideoConfig(media::VideoDecoderConfig new_config) = 0;
  };

  // Creates a new instance of this class. |client| is expected to outlive this
  // class.
  using HandleFactory =
      base::RepeatingCallback<openscreen::cast::RpcMessenger::Handle()>;
  using RpcProcessMessageCB = base::RepeatingCallback<void(
      openscreen::cast::RpcMessenger::Handle,
      std::unique_ptr<openscreen::cast::RpcMessage>)>;
  RpcDemuxerStreamHandler(Client* client,
                          HandleFactory handle_factory,
                          RpcProcessMessageCB message_processor);

  ~RpcDemuxerStreamHandler() override;

  // To be called when the RPC_ACQUIRE_DEMUXER message is received. Acts to
  // immediately create and send an RPC message for initialization of each
  // valid demuxer stream.
  void OnRpcAcquireDemuxer(
      openscreen::cast::RpcMessenger::Handle audio_stream_handle,
      openscreen::cast::RpcMessenger::Handle video_stream_handle);

  // To be called when no further buffers are available for reading to request
  // more be sent.
  void RequestMoreAudioBuffers();
  void RequestMoreVideoBuffers();

  // Called when a DemuxerStream error occurs to inform the sender device.
  void OnAudioError();
  void OnVideoError();

  base::WeakPtr<RpcDemuxerStreamHandler> GetWeakPtr();

 private:
  class MessageProcessor {
   public:
    enum class Type { kUnknown = 0, kAudio, kVideo };

    // Creates a new instance.
    //
    // |local_handle| is a new handle created for this instance, and with which
    // remote messages arriving here will have their handle set.
    // |remote_handle| is the handle received from the remote sender in the
    // OnRpcAcquiredDemuxer() call and used as the handle for sending messages
    // back to the sender.
    MessageProcessor(Client* client,
                     openscreen::cast::RpcMessenger::Handle local_handle,
                     openscreen::cast::RpcMessenger::Handle remote_handle,
                     Type type);

    bool OnRpcInitializeCallback(
        absl::optional<media::AudioDecoderConfig> audio_config,
        absl::optional<media::VideoDecoderConfig> video_config);
    bool OnRpcReadUntilCallback(
        absl::optional<media::AudioDecoderConfig> audio_config,
        absl::optional<media::VideoDecoderConfig> video_config,
        uint32_t total_frames_received);

    uint32_t total_frames_received() const { return total_frames_received_; }

    openscreen::cast::RpcMessenger::Handle local_handle() const {
      return local_handle_;
    }
    openscreen::cast::RpcMessenger::Handle remote_handle() const {
      return remote_handle_;
    }

    bool is_read_until_call_pending() const {
      return is_read_until_call_pending_;
    }
    void set_read_until_call_pending() { is_read_until_call_pending_ = true; }

   private:
    Client* client_;
    openscreen::cast::RpcMessenger::Handle local_handle_;
    openscreen::cast::RpcMessenger::Handle remote_handle_;
    Type type_ = Type::kUnknown;

    uint32_t total_frames_received_ = 0;

    bool is_read_until_call_pending_ = false;
  };

  // Helpers for the above methods of the same name.
  void RequestMoreBuffers(MessageProcessor* message_processor);
  void OnError(MessageProcessor* message_processor);

  // RpcDemuxerStreamCBMessageHandler overrides.
  void OnRpcInitializeCallback(
      openscreen::cast::RpcMessenger::Handle handle,
      absl::optional<media::AudioDecoderConfig> audio_config,
      absl::optional<media::VideoDecoderConfig> video_config) override;
  void OnRpcReadUntilCallback(
      openscreen::cast::RpcMessenger::Handle handle,
      absl::optional<media::AudioDecoderConfig> audio_config,
      absl::optional<media::VideoDecoderConfig> video_config,
      uint32_t total_frames_received) override;

  Client* const client_;
  HandleFactory handle_factory_;
  RpcProcessMessageCB message_processor_;

  std::unique_ptr<MessageProcessor> audio_message_processor_;
  std::unique_ptr<MessageProcessor> video_message_processor_;

  base::WeakPtrFactory<RpcDemuxerStreamHandler> weak_factory_;
};

}  // namespace cast_streaming::remoting

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_RPC_DEMUXER_STREAM_HANDLER_H_
