// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/playback_command_forwarding_renderer.h"

#include "base/notreached.h"

namespace cast_streaming {

PlaybackCommandForwardingRenderer::PlaybackCommandForwardingRenderer(
    std::unique_ptr<media::Renderer> renderer,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingReceiver<media::mojom::Renderer> pending_renderer_controls)
    : real_renderer_(std::move(renderer)),
      pending_renderer_controls_(std::move(pending_renderer_controls)),
      task_runner_(std::move(task_runner)),
      weak_factory_(this) {
  DCHECK(real_renderer_);
  DCHECK(pending_renderer_controls_);
}

PlaybackCommandForwardingRenderer::~PlaybackCommandForwardingRenderer() =
    default;

void PlaybackCommandForwardingRenderer::Initialize(
    media::MediaResource* media_resource,
    media::RendererClient* client,
    media::PipelineStatusCallback init_cb) {
  DCHECK(!init_cb_);

  init_cb_ = std::move(init_cb);
  real_renderer_->Initialize(
      media_resource, client,
      base::BindOnce(&PlaybackCommandForwardingRenderer::
                         OnRealRendererInitializationComplete,
                     weak_factory_.GetWeakPtr()));
}

void PlaybackCommandForwardingRenderer::SetCdm(media::CdmContext* cdm_context,
                                               CdmAttachedCB cdm_attached_cb) {
  // CDM should not be set for current mirroring use cases.
  NOTREACHED();
}

void PlaybackCommandForwardingRenderer::SetLatencyHint(
    absl::optional<base::TimeDelta> latency_hint) {
  // Not relevant for current mirroring use cases.
}

void PlaybackCommandForwardingRenderer::Flush(base::OnceClosure flush_cb) {}

void PlaybackCommandForwardingRenderer::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void PlaybackCommandForwardingRenderer::SetPlaybackRate(double playback_rate) {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void PlaybackCommandForwardingRenderer::SetVolume(float volume) {}

base::TimeDelta PlaybackCommandForwardingRenderer::GetMediaTime() {
  return real_renderer_->GetMediaTime();
}

void PlaybackCommandForwardingRenderer::OnRealRendererInitializationComplete(
    media::PipelineStatus status) {
  DCHECK(init_cb_);
  DCHECK(!playback_controller_);

  playback_controller_ = std::make_unique<PlaybackController>(
      std::move(pending_renderer_controls_), task_runner_,
      real_renderer_.get());

  std::move(init_cb_).Run(status);
}

// NOTE: The mojo pipe CANNOT be bound to task runner |task_runner_| - this will
// result in runtime errors on the release branch due to an outdated mojo
// implementation. Calls must instead be bounced to the correct task runner in
// each receiver method.
// TODO(b/205307190): Bind the mojo pipe to the task runner directly.
PlaybackCommandForwardingRenderer::PlaybackController::PlaybackController(
    mojo::PendingReceiver<media::mojom::Renderer> pending_renderer_controls,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    media::Renderer* real_renderer)
    : real_renderer_(real_renderer),
      task_runner_(std::move(task_runner)),
      playback_controller_(this, std::move(pending_renderer_controls)),
      weak_factory_(this) {
  DCHECK(real_renderer_);
  DCHECK(task_runner_);
}

PlaybackCommandForwardingRenderer::PlaybackController::~PlaybackController() =
    default;

void PlaybackCommandForwardingRenderer::PlaybackController::Initialize(
    ::mojo::PendingAssociatedRemote<media::mojom::RendererClient> client,
    absl::optional<
        std::vector<::mojo::PendingRemote<::media::mojom::DemuxerStream>>>
        streams,
    media::mojom::MediaUrlParamsPtr media_url_params,
    InitializeCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
}

void PlaybackCommandForwardingRenderer::PlaybackController::Flush(
    FlushCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

void PlaybackCommandForwardingRenderer::PlaybackController::StartPlayingFrom(
    ::base::TimeDelta time) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PlaybackCommandForwardingRenderer::
                                      PlaybackController::StartPlayingFrom,
                                  weak_factory_.GetWeakPtr(), time));
    return;
  }

  real_renderer_->StartPlayingFrom(time);
}

void PlaybackCommandForwardingRenderer::PlaybackController::SetPlaybackRate(
    double playback_rate) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PlaybackCommandForwardingRenderer::
                                      PlaybackController::SetPlaybackRate,
                                  weak_factory_.GetWeakPtr(), playback_rate));
    return;
  }

  real_renderer_->SetPlaybackRate(playback_rate);
}

void PlaybackCommandForwardingRenderer::PlaybackController::SetVolume(
    float volume) {
  NOTIMPLEMENTED();
}

void PlaybackCommandForwardingRenderer::PlaybackController::SetCdm(
    const absl::optional<::base::UnguessableToken>& cdm_id,
    SetCdmCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
}

}  // namespace cast_streaming
