// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_output_device.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromecast/media/audio/audio_io_thread.h"
#include "chromecast/media/audio/audio_output_service/audio_output_service.pb.h"
#include "chromecast/media/audio/audio_output_service/output_stream_connection.h"
#include "chromecast/media/base/default_monotonic_clock.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "net/base/io_buffer.h"

namespace chromecast {
namespace media {

namespace {

constexpr base::TimeDelta kNoBufferReadDelay = base::Milliseconds(50);

// When to treat lack of audio data in buffer as a underflow.
constexpr base::TimeDelta kBufferUnderflowThreshold = base::Milliseconds(400);

// Initial renderer buffer size estimation. The value should be smaller than the
// audio renderer start capacity.
constexpr base::TimeDelta kPrefetchRendererBufferSize = base::Seconds(4);
constexpr base::TimeDelta kDefaultRendererBufferSize = base::Milliseconds(160);

}  // namespace

// Internal helper class. The constructor/StartRender/StopRender are called on
// caller's thread. The other functions including destructor should be used on
// an IO thread.
class CastAudioOutputDevice::Internal
    : public audio_output_service::OutputStreamConnection::Delegate {
 public:
  Internal(const ::media::AudioParameters& audio_params,
           RenderCallback* render_callback)
      : audio_params_(audio_params), render_callback_(render_callback) {
    DCHECK(render_callback_);
  }

  Internal(const Internal&) = delete;
  Internal& operator=(const Internal&) = delete;
  ~Internal() override = default;

  // One time init on IO thread.
  void Initialize(
      mojo::PendingRemote<mojom::AudioSocketBroker> audio_socket_broker,
      const std::string& session_id) {
    audio_output_service::CmaBackendParams cma_backend_params;

    audio_output_service::AudioDecoderConfig* audio_config =
        cma_backend_params.mutable_audio_decoder_config();
    audio_config->set_audio_codec(audio_service::AudioCodec::AUDIO_CODEC_PCM);
    audio_config->set_sample_rate(audio_params_.sample_rate());
    audio_config->set_sample_format(
        audio_service::SampleFormat::SAMPLE_FORMAT_INT16_I);
    audio_config->set_num_channels(audio_params_.channels());

    audio_output_service::ApplicationMediaInfo* app_media_info =
        cma_backend_params.mutable_application_media_info();
    app_media_info->set_application_session_id(session_id);

    audio_bus_ = ::media::AudioBus::Create(audio_params_);
    output_connection_ =
        std::make_unique<audio_output_service::OutputStreamConnection>(
            this, cma_backend_params, std::move(audio_socket_broker));
    output_connection_->Connect();
  }

  void StartRender() {
    base::AutoLock lock(callback_lock_);
    active_render_callback_ = render_callback_;
  }

  void StopRender() {
    base::AutoLock lock(callback_lock_);
    active_render_callback_ = nullptr;
  }

  void Start() {
    playback_started_ = true;
    media_pos_frames_ = 0;
    renderer_buffer_size_estimate_ =
        (audio_params_.effects() & ::media::AudioParameters::AUDIO_PREFETCH)
            ? kPrefetchRendererBufferSize
            : kDefaultRendererBufferSize;
    DCHECK_GT(renderer_buffer_size_estimate_,
              audio_params_.GetBufferDuration());

    media_start_time_ = base::TimeTicks::Now();
    if (!backend_initialized_) {
      // Wait for initialization to complete before sending messages through
      // `output_connection_`.
      return;
    }
    output_connection_->StartPlayingFrom(0);
  }

  void Pause() {
    paused_ = true;

    if (!backend_initialized_) {
      return;
    }

    output_connection_->SetPlaybackRate(0.0f);
    push_timer_.Stop();
  }

  void Play() {
    paused_ = false;
    if (!playback_started_) {
      Start();
    }
    if (!backend_initialized_) {
      return;
    }

    last_read_buffer_timestamp_ = base::TimeTicks();
    output_connection_->SetPlaybackRate(1.0f);
  }

  void Flush() {
    if (!backend_initialized_) {
      return;
    }
    media_pos_frames_ = 0;
    media_start_time_ = base::TimeTicks::Now();
    playback_started_ = false;
    paused_ = false;
    push_timer_.Stop();
    output_connection_->StopPlayback();
  }

  void SetVolume(double volume) {
    volume_ = volume;
    if (!backend_initialized_) {
      return;
    }
    output_connection_->SetVolume(volume_);
  }

 private:
  // audio_output_service::OutputStreamConnection::Delegate implementation:
  void OnBackendInitialized(
      const audio_output_service::BackendInitializationStatus& status)
      override {
    if (status.status() !=
        audio_output_service::BackendInitializationStatus::SUCCESS) {
      LOG(ERROR) << "Error initializing the audio backend.";
      base::AutoLock lock(callback_lock_);
      if (active_render_callback_) {
        active_render_callback_->OnRenderError();
      }
      return;
    }

    DCHECK(output_connection_);
    backend_initialized_ = true;
    output_connection_->SetVolume(volume_);
    if (paused_) {
      output_connection_->SetPlaybackRate(0.0f);
    }
    if (!playback_started_) {
      // Wait for Start() to be called before schedule reading buffers.
      return;
    }
    output_connection_->StartPlayingFrom(0);
    if (!paused_) {
      last_read_buffer_timestamp_ = base::TimeTicks();
      output_connection_->SetPlaybackRate(1.0f);
    }
  }

  void OnNextBuffer(int64_t media_timestamp_microseconds,
                    int64_t reference_timestamp_microseconds,
                    int64_t delay_microseconds,
                    int64_t delay_timestamp_microseconds) override {
    rendering_delay_ = base::Microseconds(delay_microseconds);
    rendering_delay_timestamp_us_ = delay_timestamp_microseconds;

    TryPushBuffer();
  }

  void TryPushBuffer() {
    if (paused_ || !backend_initialized_ || !playback_started_ ||
        push_timer_.IsRunning()) {
      return;
    }

    PushBuffer();
  }

  void PushBuffer() {
    auto now = base::TimeTicks::Now();
    if (last_read_buffer_timestamp_.is_null()) {
      last_read_buffer_timestamp_ = now;
    }

    base::TimeDelta elapsed_time = now - last_read_buffer_timestamp_;
    base::TimeDelta time_before_underrun =
        audio_params_.GetBufferDuration() -
        (renderer_buffer_size_estimate_ + elapsed_time);
    if (time_before_underrun > base::TimeDelta()) {
      // Let renderer buffer more data.
      push_timer_.Start(FROM_HERE, now + time_before_underrun, this,
                        &Internal::TryPushBuffer,
                        base::subtle::DelayPolicy::kPrecise);
      return;
    }

    int frames_filled = ReadBuffer(GetDelay(), audio_bus_.get());
    renderer_buffer_size_estimate_ += elapsed_time;
    last_read_buffer_timestamp_ = now;
    auto media_pos = ::media::AudioTimestampHelper::FramesToTime(
        media_pos_frames_, audio_params_.sample_rate());
    if (frames_filled) {
      renderer_buffer_size_estimate_ -=
          ::media::AudioTimestampHelper::FramesToTime(
              frames_filled, audio_params_.sample_rate());
      DCHECK_GE(renderer_buffer_size_estimate_, base::TimeDelta());

      size_t filled_bytes = frames_filled * audio_params_.GetBytesPerFrame(
                                                ::media::kSampleFormatS16);
      size_t io_buffer_size =
          audio_output_service::OutputSocket::kAudioMessageHeaderSize +
          filled_bytes;
      auto io_buffer =
          base::MakeRefCounted<net::IOBufferWithSize>(io_buffer_size);
      audio_bus_->ToInterleaved<::media::SignedInt16SampleTypeTraits>(
          frames_filled,
          reinterpret_cast<int16_t*>(
              io_buffer->data() +
              audio_output_service::OutputSocket::kAudioMessageHeaderSize));

      DCHECK(output_connection_);
      output_connection_->SendAudioBuffer(std::move(io_buffer), filled_bytes,
                                          media_pos.InMicroseconds());
      media_pos_frames_ += frames_filled;

      // No need to schedule buffer read here since
      // `OnNextBuffer` will be called once the current
      // buffer is pushed to media backend.
      return;
    }

    // Avoid spam calling Render() since each call will advance the |AudioClock|
    // a little bit if 0 frames are rendered.
    // Wait until some rendered data is consumed before retrying Render().
    base::TimeDelta time_left_in_buffer =
        media_pos - (base::TimeTicks::Now() - media_start_time_);
    if (time_left_in_buffer > kBufferUnderflowThreshold) {
      push_timer_.Start(FROM_HERE, base::TimeTicks::Now() + time_left_in_buffer, this,
                        &Internal::TryPushBuffer,
                        base::subtle::DelayPolicy::kPrecise);
      return;
    }

    // No frames filled, schedule read immediately with a small delay.
    push_timer_.Start(FROM_HERE, base::TimeTicks::Now() + kNoBufferReadDelay,
                      this, &Internal::TryPushBuffer,
                      base::subtle::DelayPolicy::kPrecise);
  }

  int ReadBuffer(base::TimeDelta delay, ::media::AudioBus* audio_bus) {
    DCHECK(audio_bus);
    base::AutoLock lock(callback_lock_);
    if (!active_render_callback_) {
      return 0;
    }
    return active_render_callback_->Render(delay, base::TimeTicks(),
                                           /*glitch_info=*/{}, audio_bus);
  }

  base::TimeDelta GetDelay() {
    base::TimeDelta delay;
    if (rendering_delay_ < base::TimeDelta() ||
        rendering_delay_timestamp_us_ < 0) {
      delay = base::TimeDelta();
    } else {
      delay =
          rendering_delay_ + base::Microseconds(rendering_delay_timestamp_us_ -
                                                MonotonicClockNow());
      if (delay < base::TimeDelta()) {
        delay = base::TimeDelta();
      }
    }
    return delay;
  }

  scoped_refptr<CastAudioOutputDevice> output_device_;
  std::unique_ptr<audio_output_service::OutputStreamConnection>
      output_connection_;

  mojo::PendingRemote<mojom::AudioSocketBroker> pending_socket_broker_;
  ::media::AudioParameters audio_params_;
  size_t media_pos_frames_ = 0;
  // When we start playing media. Used to determine the current position in the track.
  base::TimeTicks media_start_time_ = base::TimeTicks();
  base::TimeDelta rendering_delay_;

  int64_t rendering_delay_timestamp_us_ = INT64_MIN;
  double volume_ = 1.0;
  bool paused_ = false;
  bool playback_started_ = false;
  bool backend_initialized_ = false;
  base::DeadlineTimer push_timer_;
  std::unique_ptr<::media::AudioBus> audio_bus_;

  // Callback to get audio data.
  RenderCallback* const render_callback_;

  // Estimation of the renderer buffer size. We should not pull too much buffer
  // from renderer to avoid underrun.
  base::TimeDelta renderer_buffer_size_estimate_;

  base::TimeTicks last_read_buffer_timestamp_;

  base::Lock callback_lock_;
  // Nullable callback that is only available during StartRender/StopRender.
  RenderCallback* active_render_callback_ GUARDED_BY(callback_lock_) = nullptr;
};

CastAudioOutputDevice::CastAudioOutputDevice(
    mojo::PendingRemote<mojom::AudioSocketBroker> audio_socket_broker,
    const std::string& session_id)
    : CastAudioOutputDevice(std::move(audio_socket_broker),
                            session_id,
                            AudioIoThread::Get()->task_runner()) {}

CastAudioOutputDevice::CastAudioOutputDevice(
    mojo::PendingRemote<mojom::AudioSocketBroker> audio_socket_broker,
    const std::string& session_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      pending_socket_broker_(std::move(audio_socket_broker)),
      session_id_(session_id) {}

CastAudioOutputDevice::~CastAudioOutputDevice() {
  if (!internal_) {
    return;
  }

  internal_ptr_->StopRender();
}

void CastAudioOutputDevice::Initialize(const ::media::AudioParameters& params,
                                       RenderCallback* callback) {
  DCHECK(callback);
  DCHECK(!internal_);
  DCHECK(!internal_ptr_);

  auto internal = std::make_unique<Internal>(params, callback);
  internal_ptr_ = internal.get();
  internal_.emplace(task_runner_, std::move(internal));

  internal_.AsyncCall(&Internal::Initialize)
      .WithArgs(std::move(pending_socket_broker_), session_id_);
}

void CastAudioOutputDevice::Start() {
  internal_ptr_->StartRender();
  internal_.AsyncCall(&Internal::Start);
}

void CastAudioOutputDevice::Stop() {
  internal_ptr_->StopRender();
  Flush();
}

void CastAudioOutputDevice::Pause() {
  internal_.AsyncCall(&Internal::Pause);
}

void CastAudioOutputDevice::Play() {
  internal_.AsyncCall(&Internal::Play);
}

void CastAudioOutputDevice::Flush() {
  internal_.AsyncCall(&Internal::Flush);
}

bool CastAudioOutputDevice::SetVolume(double volume) {
  internal_.AsyncCall(&Internal::SetVolume).WithArgs(volume);
  return true;
}

::media::OutputDeviceInfo CastAudioOutputDevice::GetOutputDeviceInfo() {
  // Same as the set of parameters returned in
  // CastAudioManager::GetPreferredOutputStreamParameters.
  return ::media::OutputDeviceInfo(
      std::string(), ::media::OUTPUT_DEVICE_STATUS_OK,
      ::media::AudioParameters(::media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               ::media::ChannelLayoutConfig::Stereo(), 48000,
                               480));
}

void CastAudioOutputDevice::GetOutputDeviceInfoAsync(
    OutputDeviceInfoCB info_cb) {
  // Always post to avoid the caller being reentrant.
  base::BindPostTaskToCurrentDefault(
      base::BindOnce(std::move(info_cb), GetOutputDeviceInfo()))
      .Run();
}

bool CastAudioOutputDevice::IsOptimizedForHardwareParameters() {
  return false;
}

bool CastAudioOutputDevice::CurrentThreadIsRenderingThread() {
  return task_runner_->RunsTasksInCurrentSequence();
}

}  // namespace media
}  // namespace chromecast
