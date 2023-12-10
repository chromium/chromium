// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/audio_output_service/receiver/cma_backend_shim.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/media/api/cma_backend_factory.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/audio/net/conversions.h"
#include "chromecast/media/base/audio_device_ids.h"
#include "chromecast/media/base/cast_decoder_buffer_impl.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/media/stream_id.h"
#include "chromecast/public/volume_control.h"

#define POST_DELEGATE_TASK(callback, ...) \
  delegate_task_runner_->PostTask(        \
      FROM_HERE, base::BindOnce(callback, delegate_, ##__VA_ARGS__));

#define POST_MEDIA_TASK(callback, ...) \
  media_task_runner_->PostTask(        \
      FROM_HERE,                       \
      base::BindOnce(callback, base::Unretained(this), ##__VA_ARGS__))

namespace chromecast {
namespace media {

namespace audio_output_service {

CmaBackendShim::CmaBackendShim(
    base::WeakPtr<Delegate> delegate,
    scoped_refptr<base::SequencedTaskRunner> delegate_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    const audio_output_service::CmaBackendParams& params,
    CmaBackendFactory* cma_backend_factory)
    : delegate_(std::move(delegate)),
      delegate_task_runner_(std::move(delegate_task_runner)),
      cma_backend_factory_(cma_backend_factory),
      media_task_runner_(std::move(media_task_runner)),
      backend_task_runner_(media_task_runner_),
      backend_params_(params),
      audio_decoder_(nullptr) {
  DCHECK(delegate_task_runner_);
  DCHECK(cma_backend_factory_);
  DCHECK(media_task_runner_);

  POST_MEDIA_TASK(&CmaBackendShim::InitializeOnMediaThread);
}

CmaBackendShim::~CmaBackendShim() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (backend_state_ != BackendState::kStopped) {
    cma_backend_->Stop();
  }
}

void CmaBackendShim::Remove() {
  POST_MEDIA_TASK(&CmaBackendShim::DestroyOnMediaThread);
}

void CmaBackendShim::InitializeOnMediaThread() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  MediaPipelineDeviceParams device_params(
      MediaPipelineDeviceParams::kModeSyncPts, &backend_task_runner_,
      AudioContentType::kMedia, kDefaultDeviceId);
  device_params.session_id =
      backend_params_.application_media_info().application_session_id();
  cma_backend_ = cma_backend_factory_->CreateBackend(device_params);

  audio_decoder_ = cma_backend_->CreateAudioDecoder();
  if (!audio_decoder_) {
    LOG(ERROR) << "Failed to create CMA audio decoder";
    POST_DELEGATE_TASK(&Delegate::OnBackendInitialized, /*success=*/false);
    return;
  }
  audio_decoder_->SetDelegate(this);

  bool success = SetAudioConfig();
  if (!success) {
    LOG(ERROR) << "Failed to set audio decoder config.";
    POST_DELEGATE_TASK(&Delegate::OnBackendInitialized, success);
    return;
  }
  audio_decoder_->SetVolume(1.0f);

  success = cma_backend_->Initialize();
  if (!success) {
    LOG(ERROR) << "Failed to initialize CMA backend";
  }
  POST_DELEGATE_TASK(&Delegate::OnBackendInitialized, success);
}

void CmaBackendShim::DestroyOnMediaThread() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  delete this;
}

void CmaBackendShim::AddData(char* data, int size, int64_t timestamp) {
  if (size == 0) {
    POST_MEDIA_TASK(&CmaBackendShim::AddEosDataOnMediaThread);
    return;
  }
  auto buffer = base::MakeRefCounted<CastDecoderBufferImpl>(size);
  memcpy(buffer->writable_data(), reinterpret_cast<const uint8_t*>(data), size);
  buffer->set_timestamp(base::Microseconds(timestamp));
  POST_MEDIA_TASK(&CmaBackendShim::AddDataOnMediaThread, std::move(buffer));
}

void CmaBackendShim::AddEosDataOnMediaThread() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  scoped_refptr<CastDecoderBufferImpl> buffer =
      CastDecoderBufferImpl::CreateEOSBuffer();
  AddDataOnMediaThread(std::move(buffer));
}

void CmaBackendShim::AddDataOnMediaThread(
    scoped_refptr<DecoderBufferBase> buffer) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(backend_state_, BackendState::kStopped);

  if (!audio_decoder_) {
    return;
  }
  pushed_buffer_ = std::move(buffer);
  BufferStatus status = audio_decoder_->PushBuffer(pushed_buffer_);
  if (status != MediaPipelineBackend::kBufferPending) {
    OnPushBufferComplete(status);
  }
}

void CmaBackendShim::SetVolumeMultiplier(float multiplier) {
  multiplier = std::max(0.0f, std::min(multiplier, 1.0f));
  POST_MEDIA_TASK(&CmaBackendShim::SetVolumeMultiplierOnMediaThread,
                  multiplier);
}

void CmaBackendShim::SetPlaybackRate(float playback_rate) {
  POST_MEDIA_TASK(&CmaBackendShim::SetPlaybackRateOnMediaThread, playback_rate);
}

void CmaBackendShim::StartPlayingFrom(int64_t start_pts) {
  POST_MEDIA_TASK(&CmaBackendShim::StartPlayingFromOnMediaThread, start_pts);
}

void CmaBackendShim::Stop() {
  POST_MEDIA_TASK(&CmaBackendShim::StopOnMediaThread);
}

void CmaBackendShim::UpdateAudioConfig(
    const audio_output_service::CmaBackendParams& params) {
  POST_MEDIA_TASK(&CmaBackendShim::UpdateAudioConfigOnMediaThread, params);
}

void CmaBackendShim::SetVolumeMultiplierOnMediaThread(float multiplier) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (!audio_decoder_) {
    return;
  }
  audio_decoder_->SetVolume(multiplier);
}

void CmaBackendShim::SetPlaybackRateOnMediaThread(float playback_rate) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (!cma_backend_ || playback_rate_ == playback_rate) {
    return;
  }
  float old_playback_rate = playback_rate_;
  playback_rate_ = playback_rate;
  if (backend_state_ == BackendState::kPlaying && playback_rate_ == 0.0f) {
    if (!cma_backend_->Pause()) {
      LOG(ERROR) << "Failed to pause CMA backend.";
      POST_DELEGATE_TASK(&Delegate::OnAudioPlaybackError);
    }
    backend_state_ = BackendState::kPaused;
  }

  if (backend_state_ == BackendState::kPaused && old_playback_rate == 0.0f) {
    if (!cma_backend_->Resume()) {
      LOG(ERROR) << "Failed to resume CMA backend.";
      POST_DELEGATE_TASK(&Delegate::OnAudioPlaybackError);
      return;
    }
    backend_state_ = BackendState::kPlaying;
  }

  if (backend_state_ != BackendState::kStopped) {
    UpdateMediaTimeAndRenderingDelay();
  }

  if (playback_rate_ == 0.0f) {
    return;
  }

  if (!cma_backend_->SetPlaybackRate(playback_rate_)) {
    LOG(ERROR) << "Failed to set playback rate for CMA backend.";
    POST_DELEGATE_TASK(&Delegate::OnAudioPlaybackError);
    return;
  }
}

void CmaBackendShim::StartPlayingFromOnMediaThread(int64_t start_pts) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (!cma_backend_) {
    return;
  }
  if (!cma_backend_->Start(start_pts)) {
    LOG(ERROR) << "Failed to start CMA backend";
    POST_DELEGATE_TASK(&Delegate::OnAudioPlaybackError);
    return;
  }
  if (playback_rate_ == 0.0f) {
    if (!cma_backend_->Pause()) {
      LOG(ERROR) << "Failed to pause CMA backend";
      POST_DELEGATE_TASK(&Delegate::OnAudioPlaybackError);
      return;
    }
    backend_state_ = BackendState::kPaused;
  } else {
    backend_state_ = BackendState::kPlaying;
  }
}

void CmaBackendShim::StopOnMediaThread() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (!cma_backend_ || backend_state_ == BackendState::kStopped) {
    return;
  }
  cma_backend_->Stop();
  backend_state_ = BackendState::kStopped;
}

void CmaBackendShim::UpdateAudioConfigOnMediaThread(
    const audio_output_service::CmaBackendParams& params) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  backend_params_.MergeFrom(params);
  if (!audio_decoder_) {
    return;
  }
  if (!SetAudioConfig()) {
    LOG(ERROR) << "Failed to set CMA audio config";
    POST_DELEGATE_TASK(&Delegate::OnAudioPlaybackError);
  }
}

bool CmaBackendShim::SetAudioConfig() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(audio_decoder_);

  const auto& audio_decoder_config = backend_params_.audio_decoder_config();

  AudioConfig audio_config;
  audio_config.id = kPrimary;
  audio_config.codec =
      audio_service::ConvertAudioCodec(audio_decoder_config.audio_codec());
  audio_config.channel_layout =
      ConvertChannelLayout(audio_decoder_config.channel_layout());
  if (audio_config.channel_layout == media::ChannelLayout::UNSUPPORTED) {
    audio_config.channel_layout =
        ChannelLayoutFromChannelNumber(audio_decoder_config.num_channels());
  }
  audio_config.sample_format =
      audio_service::ConvertSampleFormat(audio_decoder_config.sample_format());
  audio_config.bytes_per_channel =
      audio_service::GetSampleSizeBytes(audio_decoder_config.sample_format());
  audio_config.channel_number = audio_decoder_config.num_channels();
  audio_config.samples_per_second = audio_decoder_config.sample_rate();
  audio_config.extra_data =
      std::vector<uint8_t>(audio_decoder_config.extra_data().begin(),
                           audio_decoder_config.extra_data().end());
  return audio_decoder_->SetConfig(audio_config);
}

void CmaBackendShim::UpdateMediaTimeAndRenderingDelay() {
  if (!cma_backend_ || !audio_decoder_) {
    return;
  }
  auto rendering_delay = audio_decoder_->GetRenderingDelay();
  POST_DELEGATE_TASK(&Delegate::UpdateMediaTimeAndRenderingDelay,
                     cma_backend_->GetCurrentPts(),
                     base::TimeTicks::Now().since_origin().InMicroseconds(),
                     rendering_delay.delay_microseconds,
                     rendering_delay.timestamp_microseconds);
}

void CmaBackendShim::OnPushBufferComplete(BufferStatus status) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  pushed_buffer_.reset();
  if (status != MediaPipelineBackend::kBufferSuccess) {
    POST_DELEGATE_TASK(&Delegate::OnAudioPlaybackError);
    return;
  }
  POST_DELEGATE_TASK(&Delegate::OnBufferPushed);
  UpdateMediaTimeAndRenderingDelay();
}

void CmaBackendShim::OnEndOfStream() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void CmaBackendShim::OnDecoderError() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  POST_DELEGATE_TASK(&Delegate::OnAudioPlaybackError);
}

void CmaBackendShim::OnKeyStatusChanged(const std::string& /* key_id */,
                                        CastKeyStatus /* key_status */,
                                        uint32_t /* system_code */) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void CmaBackendShim::OnVideoResolutionChanged(const Size& /* size */) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast
