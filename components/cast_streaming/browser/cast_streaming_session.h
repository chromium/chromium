// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_CAST_STREAMING_SESSION_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_CAST_STREAMING_SESSION_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "components/cast_streaming/browser/control/playback_command_dispatcher.h"
#include "components/cast_streaming/browser/control/remoting/remoting_session_client.h"
#include "components/cast_streaming/browser/control/renderer_controller_config.h"
#include "components/cast_streaming/browser/frame/demuxer_stream_data_provider.h"
#include "components/cast_streaming/browser/public/receiver_config.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "components/openscreen_platform/network_util.h"
#include "components/openscreen_platform/task_runner.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver_constraints.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver_session.h"

namespace cast_streaming {

class StreamConsumer;
class CastMessagePortConverter;

// Entry point for the Cast Streaming Receiver implementation. Used to start a
// Cast Streaming Session for a provided MessagePort server.
class CastStreamingSession {
 public:
  class Client {
   public:
    // Called when the Cast Streaming Session has been successfully initialized.
    // It is guaranteed that at least one of |audio_stream_info| or
    // |video_stream_info| will be set.
    virtual void OnSessionInitialization(
        StreamingInitializationInfo initialization_info,
        std::optional<mojo::ScopedDataPipeConsumerHandle> audio_pipe_consumer,
        std::optional<mojo::ScopedDataPipeConsumerHandle>
            video_pipe_consumer) = 0;

    // Called on every new audio buffer after OnSessionInitialization(). The
    // frame data must be accessed via the |data_pipe| property in StreamInfo.
    virtual void OnAudioBufferReceived(
        media::mojom::DecoderBufferPtr buffer) = 0;

    // Called on every new video buffer after OnSessionInitialization(). The
    // frame data must be accessed via the |data_pipe| property in StreamInfo.
    virtual void OnVideoBufferReceived(
        media::mojom::DecoderBufferPtr buffer) = 0;

    // Called when a session is being renegotiated but has not yet completed
    // configuration.
    virtual void OnSessionReinitializationPending() = 0;

    // Called on receiver session reinitialization. It is guaranteed that at
    // least one of |audio_stream_info| or |video_stream_info| will be set.
    virtual void OnSessionReinitialization(
        StreamingInitializationInfo initialization_info,
        std::optional<mojo::ScopedDataPipeConsumerHandle> audio_pipe_consumer,
        std::optional<mojo::ScopedDataPipeConsumerHandle>
            video_pipe_consumer) = 0;

    // Called when the Cast Streaming Session has ended.
    virtual void OnSessionEnded() = 0;

   protected:
    virtual ~Client();
  };

  CastStreamingSession();
  ~CastStreamingSession();

  CastStreamingSession(const CastStreamingSession&) = delete;
  CastStreamingSession& operator=(const CastStreamingSession&) = delete;

  // Starts the Cast Streaming Session. This can only be called once during the
  // lifespan of this object. |client| must not be null and must outlive this
  // object.
  // * On success, OnSessionInitialization() will be called and
  //   OnAudioFrameReceived() and/or OnVideoFrameReceived() will be called on
  //   every subsequent Frame.
  // * On failure, OnSessionEnded() will be called.
  // * When a new offer is sent by the Cast Streaming Sender,
  //   OnSessionReinitialization() will be called.
  //
  // |av_constraints| specifies the supported media codecs and limitations
  // surrounding this support.
  void Start(Client* client,
             std::optional<RendererControllerConfig> renderer_controls,
             ReceiverConfig av_constraints,
             ReceiverSession::MessagePortProvider message_port_provider,
             scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Stops the Cast Streaming Session. This can only be called once during the
  // lifespan of this object and only after a call to Start().
  void Stop();

  bool is_running() const { return !!receiver_session_; }

  // Return a callback that may be used to request a buffer of the specified
  // type, to be returned asynchronously through the client API. May only be
  // called following a call to Start() and prior to a call to Stop().
  AudioDemuxerStreamDataProvider::RequestBufferCB GetAudioBufferRequester();
  VideoDemuxerStreamDataProvider::RequestBufferCB GetVideoBufferRequester();

  // Returns a callback to be used for pre-loading a single frame and then
  // potentially using it to begin playback of the stream.
  using PreloadBufferCB =
      base::OnceCallback<void(media::mojom::DecoderBufferPtr)>;
  PreloadBufferCB GetAudioBufferPreloader();
  PreloadBufferCB GetVideoBufferPreloader();

 private:
  // Owns the Open Screen ReceiverSession. The Streaming Session is tied to the
  // lifespan of this object.
  class ReceiverSessionClient final
      : public openscreen::cast::ReceiverSession::Client,
        public remoting::RemotingSessionClient::Dispatcher {
   public:
    ReceiverSessionClient(
        CastStreamingSession::Client* client,
        std::optional<RendererControllerConfig> renderer_controls,
        ReceiverConfig av_constraints,
        ReceiverSession::MessagePortProvider message_port_provider,
        scoped_refptr<base::SequencedTaskRunner> task_runner);
    ~ReceiverSessionClient() override;

    ReceiverSessionClient(const ReceiverSessionClient&) = delete;
    ReceiverSessionClient& operator=(const ReceiverSessionClient&) = delete;

    // Requests a new buffer of the specified type, which will be provided
    // Return a callback that may be used to request a buffer of the specified
    // type, to be returned asynchronously through the |client_|.
    void GetAudioBuffer(base::OnceClosure no_frames_available_cb);
    void GetVideoBuffer(base::OnceClosure no_frames_available_cb);

    // Stores the first frame of a DemuxerStream session, and then may use the
    // frame to begin playback fo teh streaming session, depending on its
    // configuration.
    void PreloadAudioBuffer(media::mojom::DecoderBufferPtr buffer);
    void PreloadVideoBuffer(media::mojom::DecoderBufferPtr buffer);

    // Returns a WeakPtr associated with this instance;
    base::WeakPtr<ReceiverSessionClient> GetWeakPtr();

   private:
    bool ongoing_session_has_audio() const { return !!audio_consumer_; }
    bool ongoing_session_has_video() const { return !!video_consumer_; }

    void OnInitializationTimeout();

    // Initializes the audio or video consumer, returning the data pipe to
    // be used to pass DecoderBuffer data to the Renderer process on success.
    std::optional<mojo::ScopedDataPipeConsumerHandle> InitializeAudioConsumer(
        const StreamingInitializationInfo& initialization_info);
    std::optional<mojo::ScopedDataPipeConsumerHandle> InitializeVideoConsumer(
        const StreamingInitializationInfo& initialization_info);

    // Called upon completion of a Flush call initiated by this class.
    void OnFlushComplete();

    // Called when a RPC_FLUSH_UNTIL call is received from the remoting sender.
    void OnFlushUntil(uint32_t audio_count, uint32_t video_count);

    // remoting::PlaybackCommandDispatcher::Client implementation.
    void StartStreamingSession(
        StreamingInitializationInfo initialization_info) override;

    // openscreen::cast::ReceiverSession::Client implementation.
    void OnNegotiated(const openscreen::cast::ReceiverSession* session,
                      openscreen::cast::ReceiverSession::ConfiguredReceivers
                          receivers) override;
    void OnRemotingNegotiated(
        const openscreen::cast::ReceiverSession* session,
        openscreen::cast::ReceiverSession::RemotingNegotiation negotiation)
        override;
    void OnReceiversDestroying(const openscreen::cast::ReceiverSession* session,
                               ReceiversDestroyingReason reason) override;
    void OnError(const openscreen::cast::ReceiverSession* session,
                 const openscreen::Error& error) override;

    void OnDataTimeout();
    void OnCastChannelClosed();

    openscreen_platform::TaskRunner task_runner_;
    openscreen::cast::Environment environment_;
    std::unique_ptr<CastMessagePortConverter> cast_message_port_converter_;
    std::unique_ptr<openscreen::cast::ReceiverSession> receiver_session_;
    base::OneShotTimer init_timeout_timer_;

    // Handles remoting messages.
    std::unique_ptr<PlaybackCommandDispatcher> playback_command_dispatcher_;

    // Timer to trigger connection closure if no data is received for 15
    // seconds.
    base::OneShotTimer data_timeout_timer_;

    bool is_flush_pending_ = false;

    // Populated with the most recent call to StartStreamingSession() if there
    // is an ongoing call to Flush() at the time of its calling. In this case,
    // this callback will be called upon completion of the Flush() call as part
    // of OnFlushComplete().
    base::OnceCallback<void()> start_session_cb_;

    bool is_initialized_ = false;
    const raw_ptr<CastStreamingSession::Client> client_;
    std::unique_ptr<StreamConsumer> audio_consumer_;
    std::unique_ptr<StreamConsumer> video_consumer_;

    // The currently pre-loaded audio or video buffer, if any exists.
    std::optional<media::mojom::DecoderBufferPtr> preloaded_audio_buffer_ =
        std::nullopt;
    std::optional<media::mojom::DecoderBufferPtr> preloaded_video_buffer_ =
        std::nullopt;

    base::WeakPtrFactory<ReceiverSessionClient> weak_factory_;
  };

  std::unique_ptr<ReceiverSessionClient> receiver_session_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_CAST_STREAMING_SESSION_H_
