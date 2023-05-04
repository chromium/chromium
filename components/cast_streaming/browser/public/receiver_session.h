// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_RECEIVER_SESSION_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_RECEIVER_SESSION_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/cast_streaming/common/public/mojom/demuxer_connector.mojom.h"
#include "components/cast_streaming/common/public/mojom/renderer_controller.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace cast_api_bindings {
class MessagePort;
}

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
}  // namespace media

namespace cast_streaming {

class ReceiverConfig;

// This interface handles a single Cast Streaming Receiver Session over a given
// |message_port| and with a given |demuxer_connector|. On destruction,
// the Cast Streaming Receiver Session will be terminated if it was ever
// started.
class ReceiverSession {
 public:
  class Client {
   public:
    virtual ~Client() = default;

    // Called when the associated config is set or updated by the remote sender
    // device.
    virtual void OnAudioConfigUpdated(
        const media::AudioDecoderConfig& audio_config) = 0;
    virtual void OnVideoConfigUpdated(
        const media::VideoDecoderConfig& video_config) = 0;

    // Called when the streaming session ends.
    virtual void OnStreamingSessionEnded() = 0;
  };

  // Provides controls for a media::Renderer instance. Methods are a subset of
  // those provided by a media::Renderer.
  class RendererController {
   public:
    virtual ~RendererController() = default;

    // Returns true if calls may be made to this object.
    virtual bool IsValid() const = 0;

    // Sets the output volume. The default volume should be 1. May only be
    // called if this object is valid.
    virtual void SetVolume(float volume) = 0;
  };

  using MessagePortProvider =
      base::OnceCallback<std::unique_ptr<cast_api_bindings::MessagePort>()>;

  virtual ~ReceiverSession() = default;

  // |av_constraints| specifies the supported media codecs, an ordering to
  // signify the receiver's preferences of which codecs should be used, and any
  // limitations surrounding this support.
  // |message_port_provider| creates a new MessagePort to be used for sending
  // and receiving Cast messages.
  static std::unique_ptr<ReceiverSession> Create(
      ReceiverConfig av_constraints,
      MessagePortProvider message_port_provider,
      Client* client = nullptr);

  // Schedules a call to begin streaming, following initial internal
  // initialization of the component. Following this initialization, audio
  // and/or video frames will be sent over a Mojo channel. May only be called
  // when remoting is NOT enabled.
  virtual void StartStreamingAsync(
      mojo::AssociatedRemote<mojom::DemuxerConnector> demuxer_connector) = 0;

  // As above, but also sets the |renderer_controller| to be used to control a
  // renderer-process |PlaybackCommandForwardingRenderer|. This control may then
  // be done through the RenderControls returned by GetRendererControls() below.
  // May only be called when Remoting IS enabled.
  virtual void StartStreamingAsync(
      mojo::AssociatedRemote<mojom::DemuxerConnector> demuxer_connector,
      mojo::AssociatedRemote<mojom::RendererController>
          renderer_controller) = 0;

  // Returns a RendererController through which commands may be injected into
  // the renderer-process PlaybackCommandForwardingRenderer.
  virtual RendererController* GetRendererControls() = 0;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_RECEIVER_SESSION_H_
