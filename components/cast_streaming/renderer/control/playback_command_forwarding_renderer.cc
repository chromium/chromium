// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/control/playback_command_forwarding_renderer.h"

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"

namespace cast_streaming {
namespace {

constexpr base::TimeDelta kTimeUpdateInterval = base::Milliseconds(250);

}  // namespace

// Class responsible for receiving Renderer commands from a remote source and
// forwarding them to |owning_renderer| unchanged. This class exists only to
// avoid intersection of media::Renderer and media::mojom::Renderer methods in
// PlaybackCommandForwardingRenderer.
//
// NOTE: This class CANNOT be declared in an unnamed namespace or the friend
// declaration in PlaybackCommandForwardingRenderer will no longer function.
class RendererCommandForwarder : public media::mojom::Renderer {
 public:
  // |owning_renderer| is expected to outlive this class.
  RendererCommandForwarder(
      PlaybackCommandForwardingRenderer* owning_renderer,
      mojo::PendingReceiver<media::mojom::Renderer> playback_controller,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : owning_renderer_(owning_renderer),
        playback_controller_(this, std::move(playback_controller)) {
    DCHECK(owning_renderer_);

    playback_controller_.set_disconnect_handler(base::BindPostTask(
        std::move(task_runner),
        base::BindOnce(&PlaybackCommandForwardingRenderer::OnMojoDisconnect,
                       owning_renderer_->weak_factory_.GetWeakPtr())));
  }

  ~RendererCommandForwarder() override = default;

  // media::mojom::Renderer overrides.
  void Initialize(
      ::mojo::PendingAssociatedRemote<media::mojom::RendererClient> client,
      std::optional<
          std::vector<::mojo::PendingRemote<::media::mojom::DemuxerStream>>>
          streams,
      media::mojom::MediaUrlParamsPtr media_url_params,
      InitializeCallback callback) override {
    owning_renderer_->MojoRendererInitialize(
        std::move(client), std::move(streams), std::move(media_url_params),
        std::move(callback));
  }

  void StartPlayingFrom(::base::TimeDelta time) override {
    owning_renderer_->MojoRendererStartPlayingFrom(std::move(time));
  }

  void SetPlaybackRate(double playback_rate) override {
    owning_renderer_->MojoRendererSetPlaybackRate(playback_rate);
  }

  void Flush(FlushCallback callback) override {
    owning_renderer_->MojoRendererFlush(std::move(callback));
  }

  void SetVolume(float volume) override {
    owning_renderer_->MojoRendererSetVolume(volume);
  }

  void SetCdm(const std::optional<::base::UnguessableToken>& cdm_id,
              SetCdmCallback callback) override {
    owning_renderer_->MojoRendererSetCdm(cdm_id, std::move(callback));
  }

  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override {
    NOTIMPLEMENTED();
  }

 private:
  const raw_ptr<PlaybackCommandForwardingRenderer> owning_renderer_;
  mojo::Receiver<media::mojom::Renderer> playback_controller_;
};

PlaybackCommandForwardingRenderer::PlaybackCommandForwardingRenderer(
    std::unique_ptr<media::Renderer> renderer,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingReceiver<media::mojom::Renderer> pending_renderer_controls)
    : real_renderer_(std::move(renderer)),
      pending_renderer_controls_(std::move(pending_renderer_controls)),
      task_runner_(std::move(task_runner)),
      weak_factory_(this) {
  DCHECK(real_renderer_);
  DCHECK(pending_renderer_controls_);
  InitializeSendTimestampUpdateCaller();
}

PlaybackCommandForwardingRenderer::~PlaybackCommandForwardingRenderer() =
    default;

void PlaybackCommandForwardingRenderer::Initialize(
    media::MediaResource* media_resource,
    media::RendererClient* client,
    media::PipelineStatusCallback init_cb) {
  DCHECK(!init_cb_);

  upstream_renderer_client_ = client;
  init_cb_ = std::move(init_cb);
  real_renderer_->Initialize(
      media_resource, this,
      base::BindPostTask(
          task_runner_, base::BindOnce(&PlaybackCommandForwardingRenderer::
                                           OnRealRendererInitializationComplete,
                                       weak_factory_.GetWeakPtr())));
}

void PlaybackCommandForwardingRenderer::SetCdm(media::CdmContext* cdm_context,
                                               CdmAttachedCB cdm_attached_cb) {
  // CDM should not be set for current mirroring use cases.
  NOTREACHED_IN_MIGRATION();
}

void PlaybackCommandForwardingRenderer::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  // Not relevant for current mirroring use cases.
}

void PlaybackCommandForwardingRenderer::Flush(base::OnceClosure flush_cb) {}

void PlaybackCommandForwardingRenderer::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

void PlaybackCommandForwardingRenderer::SetPlaybackRate(double playback_rate) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

void PlaybackCommandForwardingRenderer::SetVolume(float volume) {}

base::TimeDelta PlaybackCommandForwardingRenderer::GetMediaTime() {
  return real_renderer_ ? real_renderer_->GetMediaTime() : base::TimeDelta();
}

media::RendererType PlaybackCommandForwardingRenderer::GetRendererType() {
  return media::RendererType::kCastStreaming;
}

void PlaybackCommandForwardingRenderer::OnRealRendererInitializationComplete(
    media::PipelineStatus status) {
  DCHECK(init_cb_);
  DCHECK(!playback_controller_);

  playback_controller_ = std::make_unique<RendererCommandForwarder>(
      this, std::move(pending_renderer_controls_), task_runner_);

  std::move(init_cb_).Run(status);
}

// NOTE: The mojo pipe CANNOT be bound to task runner |task_runner_| - this will
// result in runtime errors on the release branch due to an outdated mojo
// implementation. Calls must instead be bounced to the correct task runner in
// each receiver method.
// TODO(b/205307190): Bind the mojo pipe to the task runner directly.
void PlaybackCommandForwardingRenderer::MojoRendererInitialize(
    ::mojo::PendingAssociatedRemote<media::mojom::RendererClient> client,
    std::optional<
        std::vector<::mojo::PendingRemote<::media::mojom::DemuxerStream>>>
        streams,
    media::mojom::MediaUrlParamsPtr media_url_params,
    media::mojom::Renderer::InitializeCallback callback) {
  DCHECK(!media_url_params);
  DCHECK(client);

  // NOTE: To maintain existing functionality, and ensure mirroring continues
  // working as currently written with or without this Renderer, the mirroring
  // data stream is provided through the standard Initialize() call, not passed
  // over the mojo pipe here
  DCHECK(!streams || streams.value().empty());

  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PlaybackCommandForwardingRenderer::MojoRendererInitialize,
            weak_factory_.GetWeakPtr(), std::move(client), std::move(streams),
            std::move(media_url_params), std::move(callback)));
    return;
  }

  remote_renderer_client_.Bind(std::move(client));

  // |playback_controller_| which forwards the call here is only set following
  // the completion of real_renderer_->Initialize().
  std::move(callback).Run(true);
}

void PlaybackCommandForwardingRenderer::MojoRendererFlush(
    media::mojom::Renderer::FlushCallback callback) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PlaybackCommandForwardingRenderer::MojoRendererFlush,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  real_renderer_->Flush(std::move(callback));
}

void PlaybackCommandForwardingRenderer::MojoRendererStartPlayingFrom(
    ::base::TimeDelta time) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PlaybackCommandForwardingRenderer::MojoRendererStartPlayingFrom,
            weak_factory_.GetWeakPtr(), time));
    return;
  }

  real_renderer_->StartPlayingFrom(time);
}

void PlaybackCommandForwardingRenderer::MojoRendererSetPlaybackRate(
    double playback_rate) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PlaybackCommandForwardingRenderer::MojoRendererSetPlaybackRate,
            weak_factory_.GetWeakPtr(), playback_rate));
    return;
  }

  real_renderer_->SetPlaybackRate(playback_rate);
}

void PlaybackCommandForwardingRenderer::MojoRendererSetVolume(float volume) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PlaybackCommandForwardingRenderer::MojoRendererSetVolume,
            weak_factory_.GetWeakPtr(), volume));
    return;
  }

  real_renderer_->SetVolume(volume);
}

void PlaybackCommandForwardingRenderer::MojoRendererSetCdm(
    const std::optional<::base::UnguessableToken>& cdm_id,
    media::mojom::Renderer::SetCdmCallback callback) {
  NOTREACHED_IN_MIGRATION()
      << "Use of a CDM is not supported by the remoting protocol.";
}

void PlaybackCommandForwardingRenderer::OnError(media::PipelineStatus status) {
  if (remote_renderer_client_) {
    remote_renderer_client_->OnError(status);
  }
  if (upstream_renderer_client_) {
    upstream_renderer_client_->OnError(status);
  }
}

void PlaybackCommandForwardingRenderer::OnFallback(
    media::PipelineStatus status) {}

void PlaybackCommandForwardingRenderer::OnEnded() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (remote_renderer_client_) {
    remote_renderer_client_->OnEnded();
  }
  if (upstream_renderer_client_) {
    upstream_renderer_client_->OnEnded();
  }
}

void PlaybackCommandForwardingRenderer::OnStatisticsUpdate(
    const media::PipelineStatistics& stats) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (remote_renderer_client_) {
    remote_renderer_client_->OnStatisticsUpdate(stats);
  }
  if (upstream_renderer_client_) {
    upstream_renderer_client_->OnStatisticsUpdate(stats);
  }
}

void PlaybackCommandForwardingRenderer::OnBufferingStateChange(
    media::BufferingState state,
    media::BufferingStateChangeReason reason) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (remote_renderer_client_) {
    remote_renderer_client_->OnBufferingStateChange(state, reason);
  }
  if (upstream_renderer_client_) {
    upstream_renderer_client_->OnBufferingStateChange(state, reason);
  }
}

void PlaybackCommandForwardingRenderer::OnWaiting(media::WaitingReason reason) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (remote_renderer_client_) {
    remote_renderer_client_->OnWaiting(reason);
  }
  if (upstream_renderer_client_) {
    upstream_renderer_client_->OnWaiting(reason);
  }
}

void PlaybackCommandForwardingRenderer::OnAudioConfigChange(
    const media::AudioDecoderConfig& config) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (remote_renderer_client_) {
    remote_renderer_client_->OnAudioConfigChange(config);
  }
  if (upstream_renderer_client_) {
    upstream_renderer_client_->OnAudioConfigChange(config);
  }
}

void PlaybackCommandForwardingRenderer::OnVideoConfigChange(
    const media::VideoDecoderConfig& config) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (remote_renderer_client_) {
    remote_renderer_client_->OnVideoConfigChange(config);
  }
  if (upstream_renderer_client_) {
    upstream_renderer_client_->OnVideoConfigChange(config);
  }
}

void PlaybackCommandForwardingRenderer::OnVideoNaturalSizeChange(
    const gfx::Size& size) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (remote_renderer_client_) {
    remote_renderer_client_->OnVideoNaturalSizeChange(size);
  }
  if (upstream_renderer_client_) {
    upstream_renderer_client_->OnVideoNaturalSizeChange(size);
  }
}

void PlaybackCommandForwardingRenderer::OnVideoOpacityChange(bool opaque) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (remote_renderer_client_) {
    remote_renderer_client_->OnVideoOpacityChange(opaque);
  }
  if (upstream_renderer_client_) {
    upstream_renderer_client_->OnVideoOpacityChange(opaque);
  }
}

void PlaybackCommandForwardingRenderer::OnVideoFrameRateChange(
    std::optional<int> fps) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // media::mojom::RendererClient does not support this call.
  if (upstream_renderer_client_) {
    upstream_renderer_client_->OnVideoFrameRateChange(std::move(fps));
  }
}

void PlaybackCommandForwardingRenderer::SendTimestampUpdate() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!remote_renderer_client_) {
    return;
  }

  // Because |remote_renderer_client_| isn't set until |real_renderer_| is
  // initialized, this call is well defined.
  base::TimeDelta media_time = real_renderer_->GetMediaTime();
  remote_renderer_client_->OnTimeUpdate(media_time, media_time,
                                        base::TimeTicks::Now());
}

void PlaybackCommandForwardingRenderer::InitializeSendTimestampUpdateCaller() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PlaybackCommandForwardingRenderer::
                                      InitializeSendTimestampUpdateCaller,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  send_timestamp_update_caller_.Start(
      FROM_HERE, kTimeUpdateInterval,
      base::BindPostTask(
          task_runner_,
          base::BindRepeating(
              &PlaybackCommandForwardingRenderer::SendTimestampUpdate,
              weak_factory_.GetWeakPtr())));
}

void PlaybackCommandForwardingRenderer::OnMojoDisconnect() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  OnError(media::PIPELINE_ERROR_DISCONNECTED);
  real_renderer_.reset();
}

}  // namespace cast_streaming
