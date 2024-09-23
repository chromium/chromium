// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/service/cast_renderer.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/base/audio_device_ids.h"
#include "chromecast/media/base/video_mode_switcher.h"
#include "chromecast/media/base/video_resolution_policy.h"
#include "chromecast/media/cdm/cast_cdm_context.h"
#include "chromecast/media/cma/base/balanced_media_task_runner_factory.h"
#include "chromecast/media/cma/base/demuxer_stream_adapter.h"
#include "chromecast/media/cma/pipeline/media_pipeline_impl.h"
#include "chromecast/media/cma/pipeline/video_pipeline_client.h"
#include "chromecast/media/service/video_geometry_setter_service.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/media_resource.h"
#include "media/base/renderer_client.h"

namespace chromecast {
namespace media {

namespace {

// Maximum difference between audio frame PTS and video frame PTS
// for frames read from the DemuxerStream.
const base::TimeDelta kMaxDeltaFetcher(base::Milliseconds(2000));

void VideoModeSwitchCompletionCb(::media::PipelineStatusCallback init_cb,
                                 bool success) {
  if (!success) {
    LOG(ERROR) << "Video mode switch failed.";
    std::move(init_cb).Run(::media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }
  LOG(INFO) << "Video mode switched successfully.";
  std::move(init_cb).Run(::media::PIPELINE_OK);
}
}  // namespace

CastRenderer::CastRenderer(
    CmaBackendFactory* backend_factory,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    VideoModeSwitcher* video_mode_switcher,
    VideoResolutionPolicy* video_resolution_policy,
    const base::UnguessableToken& overlay_plane_id,
    ::media::mojom::FrameInterfaceFactory* frame_interfaces,
    bool is_buffering_enabled)
    : backend_factory_(backend_factory),
      task_runner_(task_runner),
      video_mode_switcher_(video_mode_switcher),
      video_resolution_policy_(video_resolution_policy),
      overlay_plane_id_(overlay_plane_id),
      frame_interfaces_(frame_interfaces),
      client_(nullptr),
      cast_cdm_context_(nullptr),
      media_task_runner_factory_(
          new BalancedMediaTaskRunnerFactory(kMaxDeltaFetcher)),
      video_geometry_setter_service_(nullptr),
      is_buffering_enabled_(is_buffering_enabled),
      weak_factory_(this) {
  DCHECK(backend_factory_);
  LOG(INFO) << __FUNCTION__ << ": " << this;

  if (video_resolution_policy_)
    video_resolution_policy_->AddObserver(this);

  if (frame_interfaces_) {
    frame_interfaces_->BindEmbedderReceiver(mojo::GenericPendingReceiver(
        service_connector_.BindNewPipeAndPassReceiver()));
  }
}

CastRenderer::~CastRenderer() {
  LOG(INFO) << __FUNCTION__ << ": " << this;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (video_resolution_policy_)
    video_resolution_policy_->RemoveObserver(this);
}

void CastRenderer::SetVideoGeometrySetterService(
    VideoGeometrySetterService* video_geometry_setter_service) {
  video_geometry_setter_service_ = video_geometry_setter_service;
}

void CastRenderer::Initialize(::media::MediaResource* media_resource,
                              ::media::RendererClient* client,
                              ::media::PipelineStatusCallback init_cb) {
  LOG(INFO) << __FUNCTION__ << ": " << this;
  DCHECK(task_runner_->BelongsToCurrentThread());

  init_cb_ = std::move(init_cb);

  if (video_geometry_setter_service_) {
    video_geometry_setter_service_->GetVideoGeometryChangeSubscriber(
        video_geometry_change_subcriber_remote_.BindNewPipeAndPassReceiver());
    DCHECK(video_geometry_change_subcriber_remote_);

    video_geometry_change_subcriber_remote_->SubscribeToVideoGeometryChange(
        overlay_plane_id_,
        video_geometry_change_client_receiver_.BindNewPipeAndPassRemote(),
        base::BindOnce(&CastRenderer::OnSubscribeToVideoGeometryChange,
                       base::Unretained(this), media_resource, client));
  } else {
    OnSubscribeToVideoGeometryChange(media_resource, client);
  }
}

void CastRenderer::OnSubscribeToVideoGeometryChange(
    ::media::MediaResource* media_resource,
    ::media::RendererClient* client) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!application_media_info_manager_remote_);

  // Retrieve application_media_info_manager_remote_ if it is available via
  // CastApplicationMediaInfoManager.

  if (frame_interfaces_) {
    frame_interfaces_->BindEmbedderReceiver(mojo::GenericPendingReceiver(
        application_media_info_manager_remote_.BindNewPipeAndPassReceiver()));
  }

  if (application_media_info_manager_remote_) {
    application_media_info_manager_remote_->GetCastApplicationMediaInfo(
        base::BindOnce(&CastRenderer::OnApplicationMediaInfoReceived,
                       weak_factory_.GetWeakPtr(), media_resource, client));
  } else {
    // If a CastRenderer is created for a purpose other than a web application,
    // the CastApplicationMediaInfoManager interface is not available, and
    // default CastApplicationMediaInfo value below will be used.
    OnApplicationMediaInfoReceived(
        media_resource, client,
        ::media::mojom::CastApplicationMediaInfo::New(
            std::string(), true /* mixer_audio_enabled */,
            false /* is_audio_only_session */));
  }
}

void CastRenderer::OnApplicationMediaInfoReceived(
    ::media::MediaResource* media_resource,
    ::media::RendererClient* client,
    ::media::mojom::CastApplicationMediaInfoPtr application_media_info) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  LOG(INFO) << __FUNCTION__ << ": " << this
            << " session_id=" << application_media_info->application_session_id
            << ", mixer_audio_enabled="
            << application_media_info->mixer_audio_enabled;
  // Create pipeline backend.
  backend_task_runner_.reset(new TaskRunnerImpl());
  // TODO(erickung): crbug.com/443956. Need to provide right LoadType.
  LoadType load_type = kLoadTypeMediaSource;
  MediaPipelineDeviceParams::MediaSyncType sync_type =
      (load_type == kLoadTypeMediaStream || !is_buffering_enabled_)
          ? MediaPipelineDeviceParams::kModeIgnorePts
          : MediaPipelineDeviceParams::kModeSyncPts;

  MediaPipelineDeviceParams params(
      sync_type, backend_task_runner_.get(), AudioContentType::kMedia,
      ::media::AudioDeviceDescription::kDefaultDeviceId);
  params.session_id = application_media_info->application_session_id;
  params.pass_through_audio_support_desired =
      !application_media_info->mixer_audio_enabled;

  auto backend = backend_factory_->CreateBackend(params);

  // Create pipeline.
  MediaPipelineClient pipeline_client;
  pipeline_client.error_cb =
      base::BindRepeating(&CastRenderer::OnError, weak_factory_.GetWeakPtr());
  pipeline_client.buffering_state_cb = base::BindRepeating(
      &CastRenderer::OnBufferingStateChange, weak_factory_.GetWeakPtr());
  pipeline_.reset(new MediaPipelineImpl());
  pipeline_->SetClient(std::move(pipeline_client));
  pipeline_->Initialize(load_type, std::move(backend), is_buffering_enabled_);

  // TODO(servolk): Implement support for multiple streams. For now use the
  // first enabled audio and video streams to preserve the existing behavior.
  ::media::DemuxerStream* audio_stream =
      media_resource->GetFirstStream(::media::DemuxerStream::AUDIO);
  ::media::DemuxerStream* video_stream =
      media_resource->GetFirstStream(::media::DemuxerStream::VIDEO);

  // Initialize audio.
  if (audio_stream) {
    AvPipelineClient audio_client;
    audio_client.waiting_cb = base::BindRepeating(&CastRenderer::OnWaiting,
                                                  weak_factory_.GetWeakPtr());
    audio_client.eos_cb = base::BindRepeating(
        &CastRenderer::OnEnded, weak_factory_.GetWeakPtr(), STREAM_AUDIO);
    audio_client.playback_error_cb =
        base::BindRepeating(&CastRenderer::OnError, weak_factory_.GetWeakPtr());
    audio_client.statistics_cb = base::BindRepeating(
        &CastRenderer::OnStatisticsUpdate, weak_factory_.GetWeakPtr());
    std::unique_ptr<CodedFrameProvider> frame_provider(new DemuxerStreamAdapter(
        task_runner_, media_task_runner_factory_, audio_stream));

    ::media::PipelineStatus status = pipeline_->InitializeAudio(
        audio_stream->audio_decoder_config(), std::move(audio_client),
        std::move(frame_provider));
    if (status != ::media::PIPELINE_OK) {
      RunInitCallback(status);
      return;
    }
    audio_stream->EnableBitstreamConverter();
  }

  // Initialize video.
  if (video_stream) {
    VideoPipelineClient video_client;
    video_client.av_pipeline_client.waiting_cb = base::BindRepeating(
        &CastRenderer::OnWaiting, weak_factory_.GetWeakPtr());
    video_client.av_pipeline_client.eos_cb = base::BindRepeating(
        &CastRenderer::OnEnded, weak_factory_.GetWeakPtr(), STREAM_VIDEO);
    video_client.av_pipeline_client.playback_error_cb =
        base::BindRepeating(&CastRenderer::OnError, weak_factory_.GetWeakPtr());
    video_client.av_pipeline_client.statistics_cb = base::BindRepeating(
        &CastRenderer::OnStatisticsUpdate, weak_factory_.GetWeakPtr());
    video_client.natural_size_changed_cb = base::BindRepeating(
        &CastRenderer::OnVideoNaturalSizeChange, weak_factory_.GetWeakPtr());
    // TODO(alokp): Change MediaPipelineImpl API to accept a single config
    // after CmaRenderer is deprecated.
    std::vector<::media::VideoDecoderConfig> video_configs;
    video_configs.push_back(video_stream->video_decoder_config());
    std::unique_ptr<CodedFrameProvider> frame_provider(new DemuxerStreamAdapter(
        task_runner_, media_task_runner_factory_, video_stream));

    ::media::PipelineStatus status = pipeline_->InitializeVideo(
        video_configs, std::move(video_client), std::move(frame_provider));
    if (status != ::media::PIPELINE_OK) {
      RunInitCallback(status);
      return;
    }
    video_stream->EnableBitstreamConverter();
  }

  if (cast_cdm_context_) {
    pipeline_->SetCdm(cast_cdm_context_);
    cast_cdm_context_ = nullptr;
  }

  if (pending_volume_.has_value()) {
    pipeline_->SetVolume(pending_volume_.value());
    pending_volume_.reset();
  }

  client_ = client;

  if (video_stream && video_mode_switcher_) {
    std::vector<::media::VideoDecoderConfig> video_configs;
    video_configs.push_back(video_stream->video_decoder_config());
    auto mode_switch_completion_cb =
        base::BindOnce(&CastRenderer::OnVideoInitializationFinished,
                       weak_factory_.GetWeakPtr());
    video_mode_switcher_->SwitchMode(
        video_configs, base::BindOnce(&VideoModeSwitchCompletionCb,
                                      std::move(mode_switch_completion_cb)));
  } else if (video_stream) {
    // No mode switch needed.
    OnVideoInitializationFinished(::media::PIPELINE_OK);
  } else {
    RunInitCallback(::media::PIPELINE_OK);
  }
}

void CastRenderer::RunInitCallback(::media::PipelineStatus status) {
  if (init_cb_)
    std::move(init_cb_).Run(status);
}

void CastRenderer::OnVideoInitializationFinished(
    ::media::PipelineStatus status) {
  RunInitCallback(status);
  if (status == ::media::PIPELINE_OK) {
    // Force compositor to treat video as opaque (needed for overlay codepath).
    OnVideoOpacityChange(true);
  }
}

void CastRenderer::SetCdm(::media::CdmContext* cdm_context,
                          CdmAttachedCB cdm_attached_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(cdm_context);

  auto* cast_cdm_context = static_cast<CastCdmContext*>(cdm_context);

  if (!pipeline_) {
    // If the pipeline has not yet been created in Initialize(), cache
    // |cast_cdm_context| and pass it in when Initialize() is called.
    cast_cdm_context_ = cast_cdm_context;
  } else {
    pipeline_->SetCdm(cast_cdm_context);
  }

  std::move(cdm_attached_cb).Run(true);
}

void CastRenderer::SetLatencyHint(std::optional<base::TimeDelta> latency_hint) {
}

void CastRenderer::Flush(base::OnceClosure flush_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(pipeline_);
  pipeline_->Flush(std::move(flush_cb));
}

void CastRenderer::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(pipeline_);

  eos_[STREAM_AUDIO] = !pipeline_->HasAudio();
  eos_[STREAM_VIDEO] = !pipeline_->HasVideo();
  pipeline_->StartPlayingFrom(time);
}

void CastRenderer::SetPlaybackRate(double playback_rate) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  pipeline_->SetPlaybackRate(playback_rate);
}

void CastRenderer::SetVolume(float volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // If pipeline is not initialized, cache the volume and delay the volume set
  // until media pipeline is setup.
  if (!pipeline_) {
    pending_volume_ = volume;
    return;
  }

  pipeline_->SetVolume(volume);
}

base::TimeDelta CastRenderer::GetMediaTime() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(pipeline_);
  return pipeline_->GetMediaTime();
}

::media::RendererType CastRenderer::GetRendererType() {
  return ::media::RendererType::kCast;
}

void CastRenderer::OnVideoResolutionPolicyChanged() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!video_resolution_policy_)
    return;

  if (video_resolution_policy_->ShouldBlock(video_res_))
    OnError(::media::PIPELINE_ERROR_DECODE);
}

void CastRenderer::OnVideoGeometryChange(const gfx::RectF& rect_f,
                                         gfx::OverlayTransform transform) {
  GetOverlayCompositedCallback().Run(rect_f, transform);
}

void CastRenderer::OnError(::media::PipelineStatus status) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (client_) {
    client_->OnError(status);
  }
}

void CastRenderer::OnEnded(Stream stream) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!eos_[stream]);
  eos_[stream] = true;
  LOG(INFO) << __FUNCTION__ << ": eos_audio=" << eos_[STREAM_AUDIO]
            << " eos_video=" << eos_[STREAM_VIDEO];
  if (eos_[STREAM_AUDIO] && eos_[STREAM_VIDEO])
    client_->OnEnded();
}

void CastRenderer::OnStatisticsUpdate(
    const ::media::PipelineStatistics& stats) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnStatisticsUpdate(stats);
}

void CastRenderer::OnBufferingStateChange(
    ::media::BufferingState state,
    ::media::BufferingStateChangeReason reason) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnBufferingStateChange(state, reason);
}

void CastRenderer::OnWaiting(::media::WaitingReason reason) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnWaiting(reason);
}

void CastRenderer::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnVideoNaturalSizeChange(size);

  video_res_ = size;
  OnVideoResolutionPolicyChanged();
}

void CastRenderer::OnVideoOpacityChange(bool opaque) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(opaque);
  client_->OnVideoOpacityChange(opaque);
}

// static
void CastRenderer::SetOverlayCompositedCallback(
    const OverlayCompositedCallback& cb) {
  GetOverlayCompositedCallback() = cb;
}

}  // namespace media
}  // namespace chromecast
