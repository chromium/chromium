// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/control/renderer_control_multiplexer.h"

#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace cast_streaming {
namespace {

// The delay that should be used between receiving a call to TryStartPlayback
// and attempting to start the Renderer with a StartPlayingFrom() call.
constexpr base::TimeDelta kStartPlaybackDelay = base::Milliseconds(500);

}  // namespace

RendererControlMultiplexer::RendererControlMultiplexer(
    mojo::Remote<media::mojom::Renderer> renderer_remote,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : renderer_remote_(std::move(renderer_remote)),
      task_runner_(std::move(task_runner)),
      weak_factory_(this) {
  renderer_remote_.set_disconnect_handler(base::BindPostTask(
      task_runner_,
      base::BindOnce(&RendererControlMultiplexer::OnMojoDisconnect,
                     weak_factory_.GetWeakPtr())));
}

RendererControlMultiplexer::~RendererControlMultiplexer() = default;

void RendererControlMultiplexer::RegisterController(
    mojo::PendingReceiver<media::mojom::Renderer> controls) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(controls);
  auto bound_controls =
      std::make_unique<mojo::Receiver<media::mojom::Renderer>>(
          this, std::move(controls));
  receiver_list_.push_back(std::move(bound_controls));
}

void RendererControlMultiplexer::TryStartPlayback(base::TimeDelta time) {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RendererControlMultiplexer::TryStartPlaybackAfterDelay,
                     weak_factory_.GetWeakPtr(), std::move(time)),
      kStartPlaybackDelay);
}

void RendererControlMultiplexer::TryStartPlaybackAfterDelay(
    base::TimeDelta time) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (is_playback_ongoing_) {
    return;
  }

  StartPlayingFrom(time + kStartPlaybackDelay);
  SetPlaybackRate(1.0);
}

void RendererControlMultiplexer::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!is_playback_ongoing_) {
    DVLOG(1) << "Will start playing from time: " << time;
    renderer_remote_->StartPlayingFrom(time);
    is_playback_ongoing_ = true;
  }
}

void RendererControlMultiplexer::SetPlaybackRate(double playback_rate) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  renderer_remote_->SetPlaybackRate(playback_rate);
}

void RendererControlMultiplexer::SetVolume(float volume) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  renderer_remote_->SetVolume(volume);
}

void RendererControlMultiplexer::SetCdm(
    const std::optional<::base::UnguessableToken>& cdm_id,
    SetCdmCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  renderer_remote_->SetCdm(cdm_id, std::move(callback));
}

void RendererControlMultiplexer::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  NOTIMPLEMENTED();
}

void RendererControlMultiplexer::Initialize(
    mojo::PendingAssociatedRemote<media::mojom::RendererClient> client,
    std::optional<
        std::vector<::mojo::PendingRemote<::media::mojom::DemuxerStream>>>
        streams,
    media::mojom::MediaUrlParamsPtr media_url_params,
    InitializeCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  renderer_remote_->Initialize(std::move(client), std::move(streams),
                               std::move(media_url_params),
                               std::move(callback));
}

void RendererControlMultiplexer::Flush(FlushCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  renderer_remote_->Flush(
      base::BindOnce(&RendererControlMultiplexer::OnFlushComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void RendererControlMultiplexer::OnMojoDisconnect() {
  receiver_list_.clear();
}

void RendererControlMultiplexer::OnFlushComplete(FlushCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  is_playback_ongoing_ = false;
  std::move(callback).Run();
}

}  // namespace cast_streaming
