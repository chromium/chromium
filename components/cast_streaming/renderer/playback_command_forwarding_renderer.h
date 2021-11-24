// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_PLAYBACK_COMMAND_FORWARDING_RENDERER_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_PLAYBACK_COMMAND_FORWARDING_RENDERER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "media/base/renderer.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace cast_streaming {

// For high-level details, see documentation in
// PlaybackCommandForwardingRendererFactory.h.
class PlaybackCommandForwardingRenderer : public media::Renderer {
 public:
  // |renderer| is the Renderer to which the Initialize() call should be
  // delegated.
  // |task_runner| is the task runner on which mojo calls should be run.
  // |pending_receiver_controls| is the remote Receiver which will be providing
  // playback commands to this instance.
  PlaybackCommandForwardingRenderer(
      std::unique_ptr<media::Renderer> renderer,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
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
  void SetLatencyHint(absl::optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;

 private:
  // Class responsible for receiving Renderer commands from a remote source and
  // acting on |real_renderer_| appropriately. This logic has been separated
  // from the parent class to avoid the complexity associated with having
  // both media::Renderer and media::mojo::Renderer implemented side-by-side.
  class PlaybackController : public media::mojom::Renderer {
   public:
    PlaybackController(
        mojo::PendingReceiver<media::mojom::Renderer> pending_rederer_controls,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
        media::Renderer* real_renderer);
    ~PlaybackController() override;

    // media::mojom::Renderer overrides.
    void StartPlayingFrom(::base::TimeDelta time) override;
    void SetPlaybackRate(double playback_rate) override;

    // The following overrides are not currently implemented.
    //
    // TODO(b/182429524): Implement these methods.
    void Initialize(
        ::mojo::PendingAssociatedRemote<media::mojom::RendererClient> client,
        absl::optional<
            std::vector<::mojo::PendingRemote<::media::mojom::DemuxerStream>>>
            streams,
        media::mojom::MediaUrlParamsPtr media_url_params,
        InitializeCallback callback) override;
    void Flush(FlushCallback callback) override;
    void SetVolume(float volume) override;
    void SetCdm(const absl::optional<::base::UnguessableToken>& cdm_id,
                SetCdmCallback callback) override;

   private:
    media::Renderer* const real_renderer_;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
    mojo::Receiver<media::mojom::Renderer> playback_controller_;
    base::WeakPtrFactory<PlaybackController> weak_factory_;
  };

  void OnRealRendererInitializationComplete(media::PipelineStatus status);

  // Renderer to which playback calls should be forwarded.
  std::unique_ptr<media::Renderer> real_renderer_;

  // Callback provided to this class as part of the Initialize() call. Called in
  // OnRealRendererInitializationComplete().
  media::PipelineStatusCallback init_cb_;

  // Provided in the ctor and passed to |playback_controller_| upon its creation
  // in OnRealRendererInitializationComplete().
  mojo::PendingReceiver<media::mojom::Renderer> pending_renderer_controls_;

  // Task runner on which all mojo callbacks will be run.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Created as part of OnRealRendererInitializationComplete().
  std::unique_ptr<PlaybackController> playback_controller_;

  base::WeakPtrFactory<PlaybackCommandForwardingRenderer> weak_factory_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_PLAYBACK_COMMAND_FORWARDING_RENDERER_H_
