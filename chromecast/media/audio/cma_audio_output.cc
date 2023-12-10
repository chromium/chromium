// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cma_audio_output.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/logging.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/api/cma_backend_factory.h"
#include "chromecast/media/base/cast_decoder_buffer_impl.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "chromecast/public/volume_control.h"
#include "media/audio/audio_device_description.h"

namespace chromecast {
namespace media {

namespace {

AudioContentType GetContentType(const std::string& device_id) {
  if (::media::AudioDeviceDescription::IsCommunicationsDevice(device_id)) {
    return AudioContentType::kCommunication;
  }
  return AudioContentType::kMedia;
}

int GetSampleSize(SampleFormat sample_format) {
  switch (sample_format) {
    case kUnknownSampleFormat:
      return 0;
    case kSampleFormatU8:
    case kSampleFormatPlanarU8:
      return sizeof(uint8_t);
    case kSampleFormatS16:
    case kSampleFormatPlanarS16:
      return sizeof(int16_t);
    case kSampleFormatS32:
    case kSampleFormatPlanarS32:
      return sizeof(int32_t);
    case kSampleFormatF32:
    case kSampleFormatPlanarF32:
      return sizeof(float);
    case kSampleFormatS24:
      return 3;
  }
}

}  // namespace

CmaAudioOutput::CmaAudioOutput(
    const ::media::AudioParameters& audio_params,
    SampleFormat sample_format,
    const std::string& device_id,
    const std::string& application_session_id,
    MediaPipelineDeviceParams::MediaSyncType sync_type,
    bool use_hw_av_sync,
    int audio_track_session_id,
    CmaBackendFactory* cma_backend_factory,
    CmaBackend::Decoder::Delegate* delegate)
    : audio_params_(audio_params),
      sample_size_(GetSampleSize(sample_format)),
      use_hw_av_sync_(use_hw_av_sync),
      delegate_(delegate),
      timestamp_helper_(audio_params_.sample_rate()) {
  DCHECK(delegate_);
  Initialize(sample_format, device_id, application_session_id, sync_type,
             audio_track_session_id, cma_backend_factory);
}

CmaAudioOutput::~CmaAudioOutput() = default;

void CmaAudioOutput::Initialize(
    SampleFormat sample_format,
    const std::string& device_id,
    const std::string& application_session_id,
    MediaPipelineDeviceParams::MediaSyncType sync_type,
    int audio_track_session_id,
    CmaBackendFactory* cma_backend_factory) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(cma_backend_factory);

  auto cma_backend_task_runner = std::make_unique<TaskRunnerImpl>();
  MediaPipelineDeviceParams device_params(
      sync_type, MediaPipelineDeviceParams::kAudioStreamNormal,
      cma_backend_task_runner.get(), GetContentType(device_id), device_id);
  device_params.session_id = application_session_id;
  auto cma_backend = cma_backend_factory->CreateBackend(device_params);
  if (!cma_backend) {
    return;
  }

  auto* audio_decoder = cma_backend->CreateAudioDecoder();
  if (!audio_decoder) {
    return;
  }
  audio_decoder->SetDelegate(delegate_);

  AudioConfig audio_config;
  audio_config.codec = kCodecPCM;
  audio_config.channel_layout =
      DecoderConfigAdapter::ToChannelLayout(audio_params_.channel_layout());
  audio_config.sample_format = sample_format;
  audio_config.bytes_per_channel = sample_size_;
  audio_config.channel_number = audio_params_.channels();
  audio_config.samples_per_second = audio_params_.sample_rate();
  audio_config.use_hw_av_sync = use_hw_av_sync_;
  audio_config.audio_track_session_id = audio_track_session_id;
  DCHECK(IsValidConfig(audio_config));
  // Need to first set the config of the audio decoder then initialize the cma
  // backend if succeed.
  if (!audio_decoder->SetConfig(audio_config) || !cma_backend->Initialize()) {
    return;
  }

  cma_backend_task_runner_ = std::move(cma_backend_task_runner);
  cma_backend_ = std::move(cma_backend);
  audio_decoder_ = audio_decoder;

  timestamp_helper_.SetBaseTimestamp(base::TimeDelta());
}

bool CmaAudioOutput::Start(int64_t start_pts) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (!cma_backend_ || !cma_backend_->Start(start_pts)) {
    return false;
  }
  return true;
}

void CmaAudioOutput::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (cma_backend_) {
    cma_backend_->Stop();
  }
}

bool CmaAudioOutput::Pause() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (!cma_backend_ || !cma_backend_->Pause()) {
    return false;
  }
  return true;
}

bool CmaAudioOutput::Resume() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (!cma_backend_ || !cma_backend_->Resume()) {
    return false;
  }
  return true;
}

bool CmaAudioOutput::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (!audio_decoder_) {
    return false;
  }
  return audio_decoder_->SetVolume(volume);
}

void CmaAudioOutput::PushBuffer(
    scoped_refptr<CastDecoderBufferImpl> decoder_buffer,
    bool is_silence) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK_GT(decoder_buffer->data_size(), 0u);
  DCHECK_EQ(
      decoder_buffer->data_size() % (sample_size_ * audio_params_.channels()),
      0u);
  DCHECK(audio_decoder_);

  if (!is_silence) {
    if (!use_hw_av_sync_) {
      // Keep the timestamp of the buffer if the stream is on hardware av sync
      // mode.
      decoder_buffer->set_timestamp(timestamp_helper_.GetTimestamp());
    }
    int frame_count =
        decoder_buffer->data_size() / (sample_size_ * audio_params_.channels());
    timestamp_helper_.AddFrames(frame_count);
  }
  CmaBackend::BufferStatus status =
      audio_decoder_->PushBuffer(std::move(decoder_buffer));
  if (status != CmaBackend::BufferStatus::kBufferPending)
    delegate_->OnPushBufferComplete(status);
}

CmaBackend::AudioDecoder::RenderingDelay CmaAudioOutput::GetRenderingDelay() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(audio_decoder_);
  return audio_decoder_->GetRenderingDelay();
}

CmaBackend::AudioDecoder::AudioTrackTimestamp
CmaAudioOutput::GetAudioTrackTimestamp() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(audio_decoder_);
  return audio_decoder_->GetAudioTrackTimestamp();
}

int64_t CmaAudioOutput::GetTotalFrames() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  return timestamp_helper_.frame_count();
}

}  // namespace media
}  // namespace chromecast
