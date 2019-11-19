// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/receiver/cma_backend_shim.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "chromecast/media/audio/mixer_service/conversions.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/media/stream_id.h"
#include "chromecast/public/volume_control.h"
#include "media/base/decoder_buffer.h"
#include "media/base/timestamp_constants.h"

#define POST_DELEGATE_TASK(callback, ...)                               \
  if (delegate_task_runner_) {                                          \
    delegate_task_runner_->PostTask(                                    \
        FROM_HERE, base::BindOnce(callback, delegate_, ##__VA_ARGS__)); \
  }

#define POST_MEDIA_TASK(callback, ...) \
  media_task_runner_->PostTask(        \
      FROM_HERE,                       \
      base::BindOnce(callback, base::Unretained(this), ##__VA_ARGS__))

namespace chromecast {
namespace media {

namespace {

MediaPipelineDeviceParams::AudioStreamType ConvertStreamType(
    mixer_service::OutputStreamParams::StreamType type) {
  if (type == mixer_service::OutputStreamParams::STREAM_TYPE_SFX) {
    return MediaPipelineDeviceParams::kAudioStreamSoundEffects;
  }
  return MediaPipelineDeviceParams::kAudioStreamNormal;
}

AudioChannel ConvertChannelSelection(int channel_selection) {
  switch (channel_selection) {
    case -1:
      return AudioChannel::kAll;
    case 0:
      return AudioChannel::kLeft;
    case 1:
      return AudioChannel::kRight;
    default:
      NOTREACHED();
      return AudioChannel::kAll;
  }
}

}  // namespace

namespace mixer_service {

CmaBackendShim::CmaBackendShim(
    base::WeakPtr<Delegate> delegate,
    scoped_refptr<base::SequencedTaskRunner> delegate_task_runner,
    const mixer_service::OutputStreamParams& params,
    MediaPipelineBackendManager* backend_manager)
    : delegate_(std::move(delegate)),
      delegate_task_runner_(std::move(delegate_task_runner)),
      params_(params),
      backend_manager_(backend_manager),
      media_task_runner_(backend_manager_->task_runner()),
      backend_task_runner_(backend_manager_->task_runner()),
      audio_decoder_(nullptr) {
  DETACH_FROM_SEQUENCE(media_sequence_checker_);
  POST_MEDIA_TASK(&CmaBackendShim::InitializeOnMediaThread);
}

CmaBackendShim::~CmaBackendShim() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  cma_backend_->Stop();
}

void CmaBackendShim::Remove() {
  POST_MEDIA_TASK(&CmaBackendShim::DestroyOnMediaThread);
}

void CmaBackendShim::InitializeOnMediaThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  MediaPipelineDeviceParams device_params(
      MediaPipelineDeviceParams::kModeIgnorePts,
      ConvertStreamType(params_.stream_type()), &backend_task_runner_,
      ConvertContentType(params_.content_type()), params_.device_id());
  device_params.audio_channel =
      ConvertChannelSelection(params_.channel_selection());
  cma_backend_ = backend_manager_->CreateCmaBackend(device_params);

  audio_decoder_ = cma_backend_->CreateAudioDecoder();
  if (!audio_decoder_) {
    LOG(ERROR) << "Failed to create CMA audio decoder";
    POST_DELEGATE_TASK(&Delegate::OnAudioPlaybackError);
    return;
  }
  audio_decoder_->SetDelegate(this);

  AudioConfig audio_config;
  audio_config.id = kPrimary;
  audio_config.codec = kCodecPCM;
  audio_config.channel_layout =
      ChannelLayoutFromChannelNumber(params_.num_channels());
  audio_config.sample_format = ConvertSampleFormat(params_.sample_format());
  audio_config.bytes_per_channel = GetSampleSizeBytes(params_.sample_format());
  audio_config.channel_number = params_.num_channels();
  audio_config.samples_per_second = params_.sample_rate();
  if (!audio_decoder_->SetConfig(audio_config)) {
    LOG(ERROR) << "Failed to set CMA audio config";
    POST_DELEGATE_TASK(&Delegate::OnAudioPlaybackError);
    return;
  }
  audio_decoder_->SetVolume(1.0f);

  if (!cma_backend_->Initialize() || !cma_backend_->Start(0)) {
    LOG(ERROR) << "Failed to start CMA backend";
    POST_DELEGATE_TASK(&Delegate::OnAudioPlaybackError);
    return;
  }
}

void CmaBackendShim::DestroyOnMediaThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  delete this;
}

void CmaBackendShim::AddData(char* data, int size) {
  scoped_refptr<::media::DecoderBuffer> buffer;
  if (size == 0) {
    buffer = ::media::DecoderBuffer::CreateEOSBuffer();
  } else {
    buffer = ::media::DecoderBuffer::CopyFrom(
        reinterpret_cast<const uint8_t*>(data), size);
    buffer->set_timestamp(::media::kNoTimestamp);
  }
  POST_MEDIA_TASK(&CmaBackendShim::AddDataOnMediaThread, std::move(buffer));
}

void CmaBackendShim::AddDataOnMediaThread(
    scoped_refptr<::media::DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!audio_decoder_) {
    return;
  }
  pushed_buffer_ =
      base::MakeRefCounted<DecoderBufferAdapter>(std::move(buffer));
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

void CmaBackendShim::SetVolumeMultiplierOnMediaThread(float multiplier) {
  if (!audio_decoder_) {
    return;
  }
  audio_decoder_->SetVolume(multiplier);
}

void CmaBackendShim::OnPushBufferComplete(BufferStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  pushed_buffer_.reset();
  if (status != MediaPipelineBackend::kBufferSuccess) {
    POST_DELEGATE_TASK(&Delegate::OnAudioPlaybackError);
    return;
  }
  if (!audio_decoder_) {
    return;
  }

  RenderingDelay delay = audio_decoder_->GetRenderingDelay();
  POST_DELEGATE_TASK(&Delegate::OnBufferPushed, delay);
}

void CmaBackendShim::OnEndOfStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  POST_DELEGATE_TASK(&Delegate::PlayedEos);
}

void CmaBackendShim::OnDecoderError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  POST_DELEGATE_TASK(&Delegate::OnAudioPlaybackError);
}

void CmaBackendShim::OnKeyStatusChanged(const std::string& /* key_id */,
                                        CastKeyStatus /* key_status */,
                                        uint32_t /* system_code */) {
  // Ignored.
}

void CmaBackendShim::OnVideoResolutionChanged(const Size& /* size */) {
  // Ignored.
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
