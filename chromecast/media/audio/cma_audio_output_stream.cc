// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cma_audio_output_stream.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/base/monotonic_clock.h"
#include "chromecast/media/cma/backend/cma_backend_factory.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "media/base/decoder_buffer.h"

namespace chromecast {
namespace media {

namespace {

constexpr base::TimeDelta kRenderBufferSize = base::TimeDelta::FromSeconds(4);

AudioContentType GetContentType(const std::string& device_id) {
  if (::media::AudioDeviceDescription::IsCommunicationsDevice(device_id)) {
    return AudioContentType::kCommunication;
  }
  return AudioContentType::kMedia;
}

}  // namespace

CmaAudioOutputStream::CmaAudioOutputStream(
    const ::media::AudioParameters& audio_params,
    base::TimeDelta buffer_duration,
    const std::string& device_id,
    CmaBackendFactory* cma_backend_factory)
    : is_audio_prefetch_(audio_params.effects() &
                         ::media::AudioParameters::AUDIO_PREFETCH),
      audio_params_(audio_params),
      device_id_(device_id),
      cma_backend_factory_(cma_backend_factory),
      timestamp_helper_(audio_params_.sample_rate()),
      buffer_duration_(buffer_duration),
      render_buffer_size_estimate_(kRenderBufferSize) {
  DCHECK(cma_backend_factory_);
  DETACH_FROM_THREAD(media_thread_checker_);

  LOG(INFO) << "Enable audio prefetch: " << is_audio_prefetch_;
}

CmaAudioOutputStream::~CmaAudioOutputStream() = default;

void CmaAudioOutputStream::SetRunning(bool running) {
  base::AutoLock lock(running_lock_);
  running_ = running;
}

void CmaAudioOutputStream::Initialize(
    const std::string& application_session_id,
    chromecast::mojom::MultiroomInfoPtr multiroom_info) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(cma_backend_factory_);

  if (cma_backend_state_ != CmaBackendState::kUninitialized)
    return;

  cma_backend_task_runner_ = std::make_unique<TaskRunnerImpl>();
  MediaPipelineDeviceParams device_params(
      MediaPipelineDeviceParams::kModeIgnorePts,
      MediaPipelineDeviceParams::kAudioStreamNormal,
      cma_backend_task_runner_.get(), GetContentType(device_id_), device_id_);
  device_params.session_id = application_session_id;
  device_params.multiroom = multiroom_info->multiroom;
  device_params.audio_channel = multiroom_info->audio_channel;
  device_params.output_delay_us = multiroom_info->output_delay.InMicroseconds();
  cma_backend_ = cma_backend_factory_->CreateBackend(device_params);
  if (!cma_backend_) {
    encountered_error_ = true;
    return;
  }

  audio_decoder_ = cma_backend_->CreateAudioDecoder();
  if (!audio_decoder_) {
    encountered_error_ = true;
    return;
  }
  audio_decoder_->SetDelegate(this);

  AudioConfig audio_config;
  audio_config.codec = kCodecPCM;
  audio_config.channel_layout =
      ChannelLayoutFromChannelNumber(audio_params_.channels());
  audio_config.sample_format = kSampleFormatS16;
  audio_config.bytes_per_channel = 2;
  audio_config.channel_number = audio_params_.channels();
  audio_config.samples_per_second = audio_params_.sample_rate();
  DCHECK(IsValidConfig(audio_config));
  if (!audio_decoder_->SetConfig(audio_config)) {
    encountered_error_ = true;
    return;
  }

  if (!cma_backend_->Initialize()) {
    encountered_error_ = true;
    return;
  }
  cma_backend_state_ = CmaBackendState::kStopped;

  audio_bus_ = ::media::AudioBus::Create(audio_params_);
  timestamp_helper_.SetBaseTimestamp(base::TimeDelta());
}

void CmaAudioOutputStream::Start(
    ::media::AudioOutputStream::AudioSourceCallback* source_callback) {
  DCHECK(source_callback);
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (cma_backend_state_ == CmaBackendState::kPendingClose)
    return;

  source_callback_ = source_callback;
  if (encountered_error_) {
    source_callback_->OnError();
    return;
  }

  if (cma_backend_state_ == CmaBackendState::kPaused ||
      cma_backend_state_ == CmaBackendState::kStopped) {
    if (cma_backend_state_ == CmaBackendState::kPaused) {
      cma_backend_->Resume();
    } else {
      cma_backend_->Start(0);
      render_buffer_size_estimate_ = kRenderBufferSize;
    }
    next_push_time_ = base::TimeTicks::Now();
    last_push_complete_time_ = base::TimeTicks::Now();
    cma_backend_state_ = CmaBackendState::kStarted;
  }

  if (!push_in_progress_) {
    PushBuffer();
  }
}

void CmaAudioOutputStream::Stop(base::WaitableEvent* finished) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  // Prevent further pushes to the audio buffer after stopping.
  push_timer_.Stop();
  // Don't actually stop the backend.  Stop() gets called when the stream is
  // paused.  We rely on Flush() to stop the backend.
  if (cma_backend_) {
    cma_backend_->Pause();
    cma_backend_state_ = CmaBackendState::kPaused;
  }
  source_callback_ = nullptr;
  finished->Signal();
}

void CmaAudioOutputStream::Flush(base::WaitableEvent* finished) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  // Prevent further pushes to the audio buffer after stopping.
  push_timer_.Stop();

  if (cma_backend_ && (cma_backend_state_ == CmaBackendState::kPaused ||
                       cma_backend_state_ == CmaBackendState::kStarted)) {
    cma_backend_->Stop();
    cma_backend_state_ = CmaBackendState::kStopped;
  }
  push_in_progress_ = false;
  source_callback_ = nullptr;
  finished->Signal();
}

void CmaAudioOutputStream::Close(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  // Prevent further pushes to the audio buffer after stopping.
  push_timer_.Stop();
  // Only stop the backend if it was started.
  if (cma_backend_ && cma_backend_state_ != CmaBackendState::kStopped) {
    cma_backend_->Stop();
  }
  push_in_progress_ = false;
  source_callback_ = nullptr;
  cma_backend_state_ = CmaBackendState::kPendingClose;

  cma_backend_task_runner_.reset();
  cma_backend_.reset();
  audio_bus_.reset();

  std::move(closure).Run();
}

void CmaAudioOutputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (!audio_decoder_) {
    return;
  }
  if (encountered_error_) {
    return;
  }
  audio_decoder_->SetVolume(volume);
}

void CmaAudioOutputStream::PushBuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);

  // Acquire running_lock_ for the scope of this push call to
  // prevent the source callback from closing the output stream
  // mid-push.
  base::AutoLock lock(running_lock_);
  DCHECK(!push_in_progress_);

  // Do not fill more buffers if we have stopped running.
  if (!running_)
    return;

  // It is possible that this function is called when we are stopped.
  // Return quickly if so.
  if (!source_callback_ || encountered_error_ ||
      cma_backend_state_ != CmaBackendState::kStarted) {
    return;
  }

  CmaBackend::AudioDecoder::RenderingDelay rendering_delay =
      audio_decoder_->GetRenderingDelay();

  base::TimeDelta delay;
  if (rendering_delay.delay_microseconds < 0 ||
      rendering_delay.timestamp_microseconds < 0) {
    // This occurs immediately after start/resume when there isn't a good
    // estimate of the buffer delay.  Use the last known good delay.
    delay = last_rendering_delay_;
  } else {
    // The rendering delay to account for buffering is not included in
    // rendering_delay.delay_microseconds but is in delay_timestamp which isn't
    // used by AudioOutputStreamImpl.
    delay = base::TimeDelta::FromMicroseconds(
        rendering_delay.delay_microseconds +
        rendering_delay.timestamp_microseconds - MonotonicClockNow());
    if (delay.InMicroseconds() < 0) {
      delay = base::TimeDelta();
    }
  }
  last_rendering_delay_ = delay;

  int frame_count = source_callback_->OnMoreData(delay, base::TimeTicks(), 0,
                                                 audio_bus_.get());

  DVLOG(3) << "frames_filled=" << frame_count << " with latency=" << delay;

  if (frame_count == 0) {
    OnPushBufferComplete(CmaBackend::BufferStatus::kBufferFailed);
    return;
  }
  auto decoder_buffer =
      base::MakeRefCounted<DecoderBufferAdapter>(new ::media::DecoderBuffer(
          frame_count * audio_bus_->channels() * sizeof(int16_t)));
  audio_bus_->ToInterleaved<::media::SignedInt16SampleTypeTraits>(
      frame_count, reinterpret_cast<int16_t*>(decoder_buffer->writable_data()));
  decoder_buffer->set_timestamp(timestamp_helper_.GetTimestamp());
  timestamp_helper_.AddFrames(frame_count);

  push_in_progress_ = true;
  BufferStatus status = audio_decoder_->PushBuffer(std::move(decoder_buffer));
  if (status != CmaBackend::BufferStatus::kBufferPending)
    OnPushBufferComplete(status);
}

void CmaAudioOutputStream::OnPushBufferComplete(BufferStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK_NE(status, CmaBackend::BufferStatus::kBufferPending);

  push_in_progress_ = false;

  if (!source_callback_ || encountered_error_)
    return;

  DCHECK_EQ(cma_backend_state_, CmaBackendState::kStarted);

  if (status != CmaBackend::BufferStatus::kBufferSuccess) {
    source_callback_->OnError();
    return;
  }

  // Schedule next push buffer.
  const base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delay;
  if (is_audio_prefetch_) {
    // For multizone-playback, we don't care about AV sync and want to pre-fetch
    // audio.
    render_buffer_size_estimate_ -= buffer_duration_;
    render_buffer_size_estimate_ += now - last_push_complete_time_;
    last_push_complete_time_ = now;

    if (render_buffer_size_estimate_ >= buffer_duration_) {
      delay = base::TimeDelta::FromSeconds(0);
    } else {
      delay = buffer_duration_;
    }
  } else {
    next_push_time_ = std::max(now, next_push_time_ + buffer_duration_);
    delay = next_push_time_ - now;
  }

  DVLOG(3) << "render_buffer_size_estimate_=" << render_buffer_size_estimate_
           << " delay=" << delay << " buffer_duration_=" << buffer_duration_;

  push_timer_.Start(FROM_HERE, delay, this, &CmaAudioOutputStream::PushBuffer);
}

void CmaAudioOutputStream::OnDecoderError() {
  DLOG(INFO) << this << ": " << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);

  encountered_error_ = true;
  if (source_callback_)
    source_callback_->OnError();
}

}  // namespace media
}  // namespace chromecast
