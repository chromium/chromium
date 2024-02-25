// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_CONTROL_PLAYBACK_COMMAND_FORWARDING_RENDERER_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_CONTROL_PLAYBACK_COMMAND_FORWARDING_RENDERER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace cast_streaming {

// For high-level details, see documentation in
// PlaybackCommandForwardingRendererFactory.h.
class PlaybackCommandForwardingRenderer : public media::Renderer,
                                          public media::RendererClient {
 public:
  // |renderer| is the Renderer to which the Initialize() call should be
  // delegated.
  // |task_runner| is the task runner on which mojo calls should be run.
  // |pending_receiver_controls| is the remote Receiver which will be providing
  // playback commands to this instance.
  PlaybackCommandForwardingRenderer(
      std::unique_ptr<media::Renderer> renderer,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      mojo::PendingReceiver<media::mojom::Renderer> pending_rederer_controls);
  PlaybackCommandForwardingRenderer(const PlaybackCommandForwardingRenderer&) =
      delete;
  PlaybackCommandForwardingRenderer(PlaybackCommandForwardingRenderer&&) =
      delete;

  ~PlaybackCommandForwardingRenderer() override;

  PlaybackCommandForwardingRenderer& operator=(
      const PlaybackCommandForwardingRenderer&) = delete;
  PlaybackCommandForwardingRenderer& operator=(
      PlaybackCommandForwardingRenderer&&) = delete;

  // Renderer overrides.
  //
  // Calls into |real_renderer_|'s method of the same name.
  void Initialize(media::MediaResource* media_resource,
                  media::RendererClient* client,
                  media::PipelineStatusCallback init_cb) override;

  // Further Renderer overrides as no-ops. In the remoting scenario, these
  // commands will be received from the end-user's device over mojo.
  void SetCdm(media::CdmContext* cdm_context,
              CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  media::RendererType GetRendererType() override;

 private:
  // Private namespace function not defined here because its details are not
  // important.
  friend class RendererCommandForwarder;

  // media::mojom::Renderer implementation, with methods renamed to avoid
  // intersection with media::Renderer types.
  //
  // Calls are all forwarded to |real_renderer_|;
  void MojoRendererInitialize(
      ::mojo::PendingAssociatedRemote<media::mojom::RendererClient> client,
      std::optional<
          std::vector<::mojo::PendingRemote<::media::mojom::DemuxerStream>>>
          streams,
      media::mojom::MediaUrlParamsPtr media_url_params,
      media::mojom::Renderer::InitializeCallback callback);
  void MojoRendererStartPlayingFrom(::base::TimeDelta time);
  void MojoRendererSetPlaybackRate(double playback_rate);
  void MojoRendererFlush(media::mojom::Renderer::FlushCallback callback);
  void MojoRendererSetVolume(float volume);
  void MojoRendererSetCdm(const std::optional<::base::UnguessableToken>& cdm_id,
                          media::mojom::Renderer::SetCdmCallback callback);

  // media::RendererClient overrides.
  //
  // Each of these simply forwards the call to both |remote_renderer_client_|
  // and |upstream_renderer_client_|.
  void OnError(media::PipelineStatus status) override;
  void OnFallback(media::PipelineStatus status) override;
  void OnEnded() override;
  void OnStatisticsUpdate(const media::PipelineStatistics& stats) override;
  void OnBufferingStateChange(
      media::BufferingState state,
      media::BufferingStateChangeReason reason) override;
  void OnWaiting(media::WaitingReason reason) override;
  void OnAudioConfigChange(const media::AudioDecoderConfig& config) override;
  void OnVideoConfigChange(const media::VideoDecoderConfig& config) override;
  void OnVideoNaturalSizeChange(const gfx::Size& size) override;
  void OnVideoOpacityChange(bool opaque) override;
  void OnVideoFrameRateChange(std::optional<int> fps) override;

  void OnRealRendererInitializationComplete(media::PipelineStatus status);

  // Sends an OnTimeUpdate() call to |remote_renderer_client_|.
  void SendTimestampUpdate();

  // Helper to create |send_timestamp_update_caller_| on |task_runner_|.
  void InitializeSendTimestampUpdateCaller();

  void OnMojoDisconnect();

  // Renderer to which playback calls should be forwarded.
  std::unique_ptr<media::Renderer> real_renderer_;

  // Callback provided to this class as part of the Initialize() call. Called in
  // OnRealRendererInitializationComplete().
  media::PipelineStatusCallback init_cb_;

  // Provided in the ctor and passed to |playback_controller_| upon its creation
  // in OnRealRendererInitializationComplete().
  mojo::PendingReceiver<media::mojom::Renderer> pending_renderer_controls_;

  // Task runner on which all mojo callbacks will be run.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Created as part of OnRealRendererInitializationComplete().
  std::unique_ptr<media::mojom::Renderer> playback_controller_;

  // Channel to provide data back to the remote caller, set during
  // MojoRendererInitialize().
  mojo::AssociatedRemote<media::mojom::RendererClient> remote_renderer_client_;

  raw_ptr<RendererClient> upstream_renderer_client_;

  base::RepeatingTimer send_timestamp_update_caller_;

  base::WeakPtrFactory<PlaybackCommandForwardingRenderer> weak_factory_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_CONTROL_PLAYBACK_COMMAND_FORWARDING_RENDERER_H_
