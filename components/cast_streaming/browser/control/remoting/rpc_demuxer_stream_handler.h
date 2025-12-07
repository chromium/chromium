// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_REMOTING_RPC_DEMUXER_STREAM_HANDLER_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_REMOTING_RPC_DEMUXER_STREAM_HANDLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/cast_streaming/browser/common/demuxer_stream_client.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "media/cast/openscreen/rpc_call_message_handler.h"
#include "third_party/openscreen/src/cast/streaming/public/rpc_messenger.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace openscreen::cast {
class RpcMessage;
}  // namespace openscreen::cast

namespace cast_streaming::remoting {

// Wrapper around all RPC operations associated with a DemuxerStream. This one
// instance handles interactions with both audio and video DemuxerStream
// instances.
class RpcDemuxerStreamHandler
    : public media::cast::RpcDemuxerStreamCBMessageHandler {
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
  RpcDemuxerStreamHandler(scoped_refptr<base::SequencedTaskRunner> task_runner,
                          Client* client,
                          HandleFactory handle_factory,
                          RpcProcessMessageCB process_message_cb);

  ~RpcDemuxerStreamHandler() override;

  // To be called when the RPC_ACQUIRE_DEMUXER message is received. Acts to
  // immediately create and send an RPC message for initialization of each
  // valid demuxer stream.
  void OnRpcAcquireDemuxer(
      openscreen::cast::RpcMessenger::Handle audio_stream_handle,
      openscreen::cast::RpcMessenger::Handle video_stream_handle);

  // To be called when the RPC_DS_ENABLEBITSTREAMCONVERTER_CALLBACK message is
  // received.
  void OnRpcBitstreamConverterEnabled(
      openscreen::cast::RpcMessenger::Handle handle,
      bool success);

  base::WeakPtr<DemuxerStreamClient> GetAudioClient();
  base::WeakPtr<DemuxerStreamClient> GetVideoClient();

 private:
  friend class RpcDemuxerStreamHandlerTest;

  class MessageProcessor : public DemuxerStreamClient {
   public:
    enum class Type { kUnknown = 0, kAudio, kVideo };

    // Creates a new instance.
    //
    // |local_handle| is a new handle created for this instance, and with which
    // remote messages arriving here will have their handle set.
    // |remote_handle| is the handle received from the remote sender in the
    // OnRpcAcquiredDemuxer() call and used as the handle for sending messages
    // back to the sender.
    MessageProcessor(scoped_refptr<base::SequencedTaskRunner> task_runner,
                     Client* client,
                     RpcProcessMessageCB process_message_cb,
                     openscreen::cast::RpcMessenger::Handle local_handle,
                     openscreen::cast::RpcMessenger::Handle remote_handle,
                     Type type);
    ~MessageProcessor() override;

    bool OnRpcInitializeCallback(
        std::optional<media::AudioDecoderConfig> audio_config,
        std::optional<media::VideoDecoderConfig> video_config);
    bool OnRpcReadUntilCallback(
        std::optional<media::AudioDecoderConfig> audio_config,
        std::optional<media::VideoDecoderConfig> video_config,
        uint32_t total_frames_received);
    void OnBitstreamConverterEnabled(bool success);

    base::WeakPtr<MessageProcessor> GetWeakPtr();

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
    bool is_read_until_call_ongoing() const {
      return is_read_until_call_ongoing_;
    }

   private:
    void OnBufferRequestTimeout();

    // DemuxerStreamClient implementation.
    //
    // OnNoBuffersAvailable() has the following send behavior:
    // - When first called, an immediate call will be made.
    // - If called again before the ACK for the above call has been received,
    //   the |is_read_until_call_pending_| flag is set and a new call will be
    //   made when this ACK is received.
    // - If the ACK is not received quickly, retries will be made at regular
    //   intervals with increasingly large requested frame counts.
    //
    // This retry behavior is required due to occasionally dropped messages
    // immediately following a FlushUntil() call.
    void EnableBitstreamConverter(BitstreamConverterEnabledCB cb) override;
    void OnNoBuffersAvailable() override;
    void OnError() override;

    base::TimeTicks last_request_time_;

    scoped_refptr<base::SequencedTaskRunner> task_runner_;
    raw_ptr<Client> client_;
    RpcProcessMessageCB process_message_cb_;
    openscreen::cast::RpcMessenger::Handle local_handle_;
    openscreen::cast::RpcMessenger::Handle remote_handle_;
    Type type_ = Type::kUnknown;

    uint32_t total_frames_received_ = 0;

    // Whether or not this class is currently waiting for an Ack from the
    // sender side for a READUNTIL call.
    bool is_read_until_call_ongoing_ = false;

    // Whether or not a new READUNTIL call should be made immediately following
    // the ACK for the currently ongoing READUNTIL call.
    bool is_read_until_call_pending_ = false;

    // The number of consecuitive READUNTIL calls for which an ACK has not been
    // received within a reasonable amount of time.
    int failed_consecutive_read_until_requests_ = 0;

    // Timer to re-execute a call if no response is received.
    base::OneShotTimer call_timeout_timer_;

    // Most recent callback for EnableBitstreamConverter().
    BitstreamConverterEnabledCB bitstream_converter_enabled_cb_;

    base::WeakPtrFactory<MessageProcessor> weak_factory_;
  };

  // Helpers for the above methods of the same name.
  void RequestMoreBuffers(MessageProcessor* message_processor);
  void OnError(MessageProcessor* message_processor);

  // RpcDemuxerStreamCBMessageHandler overrides.
  void OnRpcInitializeCallback(
      openscreen::cast::RpcMessenger::Handle handle,
      std::optional<media::AudioDecoderConfig> audio_config,
      std::optional<media::VideoDecoderConfig> video_config) override;
  void OnRpcReadUntilCallback(
      openscreen::cast::RpcMessenger::Handle handle,
      std::optional<media::AudioDecoderConfig> audio_config,
      std::optional<media::VideoDecoderConfig> video_config,
      uint32_t total_frames_received) override;
  void OnRpcEnableBitstreamConverterCallback(
      openscreen::cast::RpcMessenger::Handle handle,
      bool succeeded) override;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const raw_ptr<Client> client_;
  HandleFactory handle_factory_;
  RpcProcessMessageCB process_message_cb_;

  std::unique_ptr<MessageProcessor> audio_message_processor_;
  std::unique_ptr<MessageProcessor> video_message_processor_;
};

}  // namespace cast_streaming::remoting

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_REMOTING_RPC_DEMUXER_STREAM_HANDLER_H_
