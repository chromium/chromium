// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/renderer/starboard_renderer.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_resource.h"
#include "media/base/pipeline_status.h"
#include "media/base/status.h"

namespace chromecast {
namespace media {

StarboardRenderer::StarboardRenderer(
    std::unique_ptr<chromecast::media::StarboardApiWrapper> starboard,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    const base::UnguessableToken& overlay_plane_id,
    bool enable_buffering,
    VideoGeometrySetterService* geometry_setter_service,
    chromecast::metrics::CastMetricsHelper* cast_metrics_helper)
    : starboard_(std::move(starboard)),
      media_task_runner_(std::move(media_task_runner)),
      geometry_change_handler_(geometry_setter_service,
                               starboard_.get(),
                               overlay_plane_id),
      cast_metrics_helper_(cast_metrics_helper),
      enable_buffering_(enable_buffering) {
  CHECK(starboard_);
  CHECK(media_task_runner_);
  CHECK(cast_metrics_helper_);
  LOG(INFO) << "Constructed StarboardRenderer. Buffering is "
            << (enable_buffering_ ? "enabled" : "disabled");
}

StarboardRenderer::~StarboardRenderer() {
  CHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // player_manager_ being non-null implies that StarboardRenderer::Initialize
  // was called.
  if (player_manager_ && !end_reported_) {
    cast_metrics_helper_->RecordApplicationEvent("Cast.Platform.Ended");
  }
}

void StarboardRenderer::Initialize(::media::MediaResource* media_resource,
                                   ::media::RendererClient* client,
                                   ::media::PipelineStatusCallback init_cb) {
  CHECK(media_task_runner_->RunsTasksInCurrentSequence());
  CHECK(client);

  ::media::DemuxerStream* audio_stream =
      media_resource->GetFirstStream(::media::DemuxerStream::Type::AUDIO);
  ::media::DemuxerStream* video_stream =
      media_resource->GetFirstStream(::media::DemuxerStream::Type::VIDEO);

  player_manager_ = StarboardPlayerManager::Create(
      starboard_.get(), audio_stream, video_stream, client,
      cast_metrics_helper_, media_task_runner_, enable_buffering_);

  if (!player_manager_) {
    LOG(ERROR) << "Unable to create StarboardPlayerManager";
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(init_cb),
                       ::media::PIPELINE_ERROR_INITIALIZATION_FAILED));
    return;
  }

  geometry_change_handler_.SetSbPlayer(player_manager_->GetSbPlayer());

  if (pending_volume_.has_value()) {
    player_manager_->SetVolume(*pending_volume_);
    pending_volume_ = std::nullopt;
  }

  // Initialization succeeded. Inform the client and update the opacity (if
  // necessary) only after reporting success.
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(init_cb), ::media::PIPELINE_OK));
  if (video_stream) {
    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<StarboardRenderer> renderer,
                          ::media::RendererClient* client) {
                         if (renderer) {
                           client->OnVideoOpacityChange(true);
                         }
                       },
                       weak_factory_.GetWeakPtr(), client));
  }
}

void StarboardRenderer::SetCdm(::media::CdmContext* cdm_context,
                               CdmAttachedCB cdm_attached_cb) {
  // StarboardDecryptorCast does not do actual decryption, so we do not need to
  // access it.
  std::move(cdm_attached_cb).Run(true);
}

void StarboardRenderer::SetLatencyHint(
    std::optional<base::TimeDelta> /*latency_hint*/) {}

void StarboardRenderer::Flush(base::OnceClosure flush_cb) {
  CHECK(media_task_runner_->RunsTasksInCurrentSequence());
  player_manager_->Flush();
  std::move(flush_cb).Run();
  cast_metrics_helper_->RecordApplicationEvent("Cast.Platform.Ended");
  end_reported_ = true;
}

void StarboardRenderer::StartPlayingFrom(base::TimeDelta time) {
  CHECK(media_task_runner_->RunsTasksInCurrentSequence());
  LOG(INFO) << "StartPlayingFrom t=" << time;
  player_manager_->StartPlayingFrom(time);
  cast_metrics_helper_->RecordApplicationEvent("Cast.Platform.Playing");

  end_reported_ = false;
}

void StarboardRenderer::SetPlaybackRate(double playback_rate) {
  CHECK(media_task_runner_->RunsTasksInCurrentSequence());
  player_manager_->SetPlaybackRate(playback_rate);

  if (playback_rate == 0.0f) {
    cast_metrics_helper_->RecordApplicationEvent("Cast.Platform.Pause");
  } else {
    cast_metrics_helper_->RecordApplicationEvent("Cast.Platform.Playing");
  }

  end_reported_ = false;
}

void StarboardRenderer::SetVolume(float volume) {
  CHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (player_manager_ == nullptr) {
    pending_volume_ = volume;
  } else {
    player_manager_->SetVolume(volume);
  }
}

base::TimeDelta StarboardRenderer::GetMediaTime() {
  // The documentation for ::media::Renderer::GetMediaTime mentions that this
  // function in particular can be called from any thread. However, since cast
  // uses a MojoRenderer, this should always be called from the media thread.
  //
  // Note that CastRenderer::GetMediaTime makes this same assumption here:
  // https://source.chromium.org/chromium/chromium/src/+/main:chromecast/media/service/cast_renderer.cc;l=353;drc=27c605b83ca683345a58ec734e98223ae4e7adf7
  CHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (!player_manager_) {
    LOG(ERROR) << "StarboardRenderer was not successfully initialized before "
                  "GetMediaTime was called. Returning 0.";
    return base::Microseconds(0);
  }
  return player_manager_->GetMediaTime();
}

::media::RendererType StarboardRenderer::GetRendererType() {
  // TODO(crbug.com/422850895): Add a new RendererType before hooking this up to
  // production code.
  return ::media::RendererType::kCast;
}

}  // namespace media
}  // namespace chromecast
