// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_RECEIVER_SESSION_IMPL_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_RECEIVER_SESSION_IMPL_H_

#include "components/cast_streaming/browser/cast_streaming_session.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "components/cast_streaming/public/mojom/cast_streaming_session.mojom.h"
#include "components/cast_streaming/public/mojom/renderer_controller.mojom.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cast_streaming {

// Owns the CastStreamingSession and sends buffers to the renderer process via
// a Mojo service.
class ReceiverSessionImpl final
    : public cast_streaming::CastStreamingSession::Client,
      public ReceiverSession {
 public:
  // |av_constraints| specifies the supported media codecs and limitations
  // surrounding this support.
  ReceiverSessionImpl(
      std::unique_ptr<ReceiverSession::AVConstraints> av_constraints,
      MessagePortProvider message_port_provider,
      ReceiverSession::Client* client);
  ~ReceiverSessionImpl() override;

  ReceiverSessionImpl(const ReceiverSessionImpl&) = delete;
  ReceiverSessionImpl& operator=(const ReceiverSessionImpl&) = delete;

  // ReceiverSession implementation.
  void StartStreamingAsync(mojo::AssociatedRemote<mojom::CastStreamingReceiver>
                               cast_streaming_receiver) override;
  void StartStreamingAsync(mojo::AssociatedRemote<mojom::CastStreamingReceiver>
                               cast_streaming_receiver,
                           mojo::AssociatedRemote<mojom::RendererController>
                               renderer_controller) override;
  RendererController* GetRendererControls() override;

 private:
  class RendererControllerImpl : public ReceiverSession::RendererController {
   public:
    RendererControllerImpl();
    ~RendererControllerImpl() override;

    mojo::PendingReceiver<media::mojom::Renderer> Bind();

    // ReceiverSession::RendererController overrides.
    bool IsValid() const override;
    void StartPlayingFrom(base::TimeDelta time) override;
    void SetPlaybackRate(double playback_rate) override;
    void SetVolume(float volume) override;

   private:
    mojo::Remote<media::mojom::Renderer> renderer_controls_;
  };

  // Handler for |cast_streaming_receiver_| disconnect.
  void OnMojoDisconnect();

  // Callback for mojom::CastStreamingReceiver::EnableReceiver()
  void OnReceiverEnabled();

  // cast_streaming::CastStreamingSession::Client implementation.
  void OnSessionInitialization(
      absl::optional<cast_streaming::CastStreamingSession::AudioStreamInfo>
          audio_stream_info,
      absl::optional<cast_streaming::CastStreamingSession::VideoStreamInfo>
          video_stream_info) override;
  void OnAudioBufferReceived(media::mojom::DecoderBufferPtr buffer) override;
  void OnVideoBufferReceived(media::mojom::DecoderBufferPtr buffer) override;
  void OnSessionReinitialization(
      absl::optional<cast_streaming::CastStreamingSession::AudioStreamInfo>
          audio_stream_info,
      absl::optional<cast_streaming::CastStreamingSession::VideoStreamInfo>
          video_stream_info) override;
  void OnSessionEnded() override;

  // Populated in the ctor, and empty following a call to either
  // OnReceiverEnabled() or OnMojoDisconnect().
  MessagePortProvider message_port_provider_;
  std::unique_ptr<ReceiverSession::AVConstraints> av_constraints_;

  mojo::AssociatedRemote<mojom::CastStreamingReceiver> cast_streaming_receiver_;
  cast_streaming::CastStreamingSession cast_streaming_session_;

  mojo::Remote<mojom::CastStreamingBufferReceiver> audio_remote_;
  mojo::Remote<mojom::CastStreamingBufferReceiver> video_remote_;

  ReceiverSession::Client* const client_;
  std::unique_ptr<RendererControllerImpl> external_renderer_controls_;
  absl::optional<RendererControllerConfig> renderer_control_config_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_RECEIVER_SESSION_IMPL_H_
