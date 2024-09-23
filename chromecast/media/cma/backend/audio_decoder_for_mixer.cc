// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_decoder_for_mixer.h"

#include <algorithm>
#include <limits>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/audio/mixer_service/mixer_service_transport.pb.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/media/audio/net/conversions.h"
#include "chromecast/media/base/default_monotonic_clock.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_for_mixer.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "chromecast/net/io_buffer_pool.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/sample_format.h"
#include "media/filters/audio_renderer_algorithm.h"

#define TRACE_FUNCTION_ENTRY0() TRACE_EVENT0("cma", __FUNCTION__)

#define TRACE_FUNCTION_ENTRY1(arg1) \
  TRACE_EVENT1("cma", __FUNCTION__, #arg1, arg1)

#define TRACE_FUNCTION_ENTRY2(arg1, arg2) \
  TRACE_EVENT2("cma", __FUNCTION__, #arg1, arg1, #arg2, arg2)

namespace chromecast {
namespace media {

namespace {

const int kInitialFillSizeFrames = 512;
const double kPlaybackRateEpsilon = 0.001;

constexpr double kMinAudioClockRate = 0.99;
constexpr double kMaxAudioClockRate = 1.01;

const int64_t kDefaultInputQueueMs = 200;
constexpr base::TimeDelta kFadeTime = base::Milliseconds(5);
const int kDefaultStartThresholdMs = 70;

const CastAudioDecoder::OutputFormat kDecoderSampleFormat =
    CastAudioDecoder::kOutputPlanarFloat;

const int64_t kInvalidTimestamp = std::numeric_limits<int64_t>::min();

const int64_t kNoPendingOutput = -1;

constexpr int kAudioMessageHeaderSize =
    mixer_service::MixerSocket::kAudioMessageHeaderSize;

// TODO(jameswest): Replace numeric playout channel with AudioChannel enum in
// mixer.
int ToPlayoutChannel(AudioChannel audio_channel) {
  switch (audio_channel) {
    case AudioChannel::kAll:
      return kChannelAll;
    case AudioChannel::kLeft:
      return 0;
    case AudioChannel::kRight:
      return 1;
  }
  NOTREACHED();
}

int MaxQueuedFrames(int sample_rate) {
  static int64_t queue_ms = GetSwitchValueNonNegativeInt(
      switches::kMixerSourceInputQueueMs, kDefaultInputQueueMs);

  return ::media::AudioTimestampHelper::TimeToFrames(
      base::Milliseconds(queue_ms), sample_rate);
}

int StartThreshold(int sample_rate) {
  static int64_t start_threshold_ms = GetSwitchValueNonNegativeInt(
      switches::kMixerSourceAudioReadyThresholdMs, kDefaultStartThresholdMs);

  return ::media::AudioTimestampHelper::TimeToFrames(
      base::Milliseconds(start_threshold_ms), sample_rate);
}

}  // namespace

// static
bool MediaPipelineBackend::AudioDecoder::RequiresDecryption() {
  return true;
}

AudioDecoderForMixer::AudioDecoderForMixer(
    MediaPipelineBackendForMixer* backend)
    : backend_(backend),
      task_runner_(backend->GetTaskRunner()),
      buffer_pool_frames_(kInitialFillSizeFrames),
      pending_output_frames_(kNoPendingOutput),
      pool_(new ::media::AudioBufferMemoryPool()),
      weak_factory_(this) {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(backend_);
  DCHECK(task_runner_.get());
  DCHECK(task_runner_->BelongsToCurrentThread());
}

AudioDecoderForMixer::~AudioDecoderForMixer() {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void AudioDecoderForMixer::SetDelegate(
    MediaPipelineBackend::Decoder::Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void AudioDecoderForMixer::Initialize() {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(delegate_);
  stats_ = Statistics();
  pending_buffer_complete_ = false;
  pending_output_frames_ = kNoPendingOutput;
  paused_ = false;
  playback_rate_ = 1.0f;
  reported_ready_for_playback_ = false;
  mixer_delay_ = RenderingDelay();

  next_buffer_delay_ = RenderingDelay();
  last_push_pts_ = kInvalidTimestamp;
  last_push_playout_timestamp_ = kInvalidTimestamp;
}

bool AudioDecoderForMixer::Start(int64_t playback_start_pts,
                                 bool av_sync_enabled) {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(IsValidConfig(input_config_));

  // Create decoder_ if necessary. This can happen if Stop() was called, and
  // SetConfig() was not called since then.
  if (!decoder_) {
    CreateDecoder();
  }
  decoded_config_ = (decoder_ ? decoder_->GetOutputConfig() : input_config_);
  CreateMixerInput(decoded_config_, av_sync_enabled);
  playback_start_pts_ = playback_start_pts;
  av_sync_enabled_ = av_sync_enabled;

  return true;
}

void AudioDecoderForMixer::CreateBufferPool(const AudioConfig& config,
                                            int frame_count) {
  DCHECK_GT(frame_count, 0);
  buffer_pool_frames_ = frame_count;
  buffer_pool_ = base::MakeRefCounted<IOBufferPool>(
      frame_count * sizeof(float) * config.channel_number +
          kAudioMessageHeaderSize,
      std::numeric_limits<size_t>::max(), true /* threadsafe */);
  buffer_pool_->Preallocate(1);
}

void AudioDecoderForMixer::CreateMixerInput(const AudioConfig& config,
                                            bool av_sync_enabled) {
  CreateBufferPool(config, buffer_pool_frames_);
  DCHECK_GT(buffer_pool_frames_, 0);

  mixer_service::OutputStreamParams params;
  params.set_stream_type(
      backend_->Primary()
          ? mixer_service::OutputStreamParams::STREAM_TYPE_DEFAULT
          : mixer_service::OutputStreamParams::STREAM_TYPE_SFX);
  params.set_content_type(
      audio_service::ConvertContentType(backend_->ContentType()));
  params.set_sample_format(audio_service::SAMPLE_FORMAT_FLOAT_P);
  params.set_device_id(backend_->DeviceId());
  params.set_sample_rate(config.samples_per_second);
  params.set_num_channels(config.channel_number);
  params.set_channel_selection(ToPlayoutChannel(backend_->AudioChannel()));
  params.set_fill_size_frames(buffer_pool_frames_);
  params.set_start_threshold_frames(StartThreshold(config.samples_per_second));
  params.set_max_buffered_frames(MaxQueuedFrames(config.samples_per_second));
  params.set_fade_frames(::media::AudioTimestampHelper::TimeToFrames(
      kFadeTime, config.samples_per_second));
  params.set_use_start_timestamp(av_sync_enabled);
  params.set_enable_audio_clock_simulation(av_sync_enabled);

  mixer_input_ =
      std::make_unique<mixer_service::OutputStreamConnection>(this, params);
  mixer_input_->SetVolumeMultiplier(volume_multiplier_);
  mixer_input_->SetAudioClockRate(av_sync_clock_rate_);
  mixer_input_->SetPlaybackRate(playback_rate_);
  mixer_input_->Connect();
}

void AudioDecoderForMixer::StartPlaybackAt(int64_t playback_start_timestamp) {
  LOG(INFO) << __func__
            << " playback_start_timestamp_=" << playback_start_timestamp;
  mixer_input_->SetStartTimestamp(playback_start_timestamp,
                                  playback_start_pts_);
}

void AudioDecoderForMixer::RestartPlaybackAt(int64_t pts, int64_t timestamp) {
  LOG(INFO) << __func__ << " pts=" << pts << " timestamp=" << timestamp;

  next_buffer_delay_ = RenderingDelay();
  last_push_playout_timestamp_ = kInvalidTimestamp;
  mixer_input_->SetStartTimestamp(timestamp, pts);
}

AudioDecoderForMixer::RenderingDelay
AudioDecoderForMixer::GetMixerRenderingDelay() {
  return mixer_delay_;
}

void AudioDecoderForMixer::Stop() {
  TRACE_FUNCTION_ENTRY0();
  decoder_.reset();
  mixer_input_.reset();
  weak_factory_.InvalidateWeakPtrs();

  Initialize();
}

bool AudioDecoderForMixer::Pause() {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(mixer_input_);
  paused_ = true;
  mixer_input_->Pause();
  return true;
}

bool AudioDecoderForMixer::Resume() {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(mixer_input_);
  mixer_input_->Resume();
  next_buffer_delay_ = RenderingDelay();
  last_push_playout_timestamp_ = kInvalidTimestamp;
  paused_ = false;
  return true;
}

float AudioDecoderForMixer::SetPlaybackRate(float rate) {
  if (std::abs(rate - 1.0) < kPlaybackRateEpsilon) {
    // AudioRendererAlgorithm treats values close to 1 as exactly 1.
    rate = 1.0f;
  }
  LOG(INFO) << "SetPlaybackRate to " << rate;
  if (rate <= 0) {
    LOG(ERROR) << "Invalid playback rate " << rate;
    rate = 1.0f;
  }

  playback_rate_ = rate;
  mixer_input_->SetPlaybackRate(rate);
  backend_->NewAudioPlaybackRateInEffect(rate);

  return rate;
}

double AudioDecoderForMixer::SetAvSyncPlaybackRate(double rate) {
  av_sync_clock_rate_ =
      std::max(kMinAudioClockRate, std::min(rate, kMaxAudioClockRate));
  if (mixer_input_)
    mixer_input_->SetAudioClockRate(av_sync_clock_rate_);
  return av_sync_clock_rate_;
}

bool AudioDecoderForMixer::GetTimestampedPts(int64_t* timestamp,
                                             int64_t* pts) const {
  if (paused_ || last_push_playout_timestamp_ == kInvalidTimestamp ||
      last_push_pts_ == kInvalidTimestamp) {
    return false;
  }

  // Note: timestamp may be slightly in the future.
  *timestamp = last_push_playout_timestamp_;
  *pts = last_push_pts_;
  return true;
}

int64_t AudioDecoderForMixer::GetCurrentPts() const {
  return last_push_pts_;
}

AudioDecoderForMixer::BufferStatus AudioDecoderForMixer::PushBuffer(
    CastDecoderBuffer* buffer) {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(buffer);
  DCHECK(!mixer_error_);
  DCHECK(!pending_buffer_complete_);
  DCHECK(mixer_input_);

  uint64_t input_bytes = buffer->end_of_stream() ? 0 : buffer->data_size();
  scoped_refptr<DecoderBufferBase> buffer_base(
      static_cast<DecoderBufferBase*>(buffer));

  // If the buffer is already decoded, do not attempt to decode. Call
  // OnBufferDecoded asynchronously on the main thread.
  if (BypassDecoder()) {
    DCHECK(!decoder_);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AudioDecoderForMixer::OnBufferDecoded,
                                  weak_factory_.GetWeakPtr(), input_bytes,
                                  false /* has_config */,
                                  CastAudioDecoder::Status::kDecodeOk,
                                  AudioConfig(), buffer_base));
    return MediaPipelineBackend::kBufferPending;
  }

  if (!decoder_) {
    return MediaPipelineBackend::kBufferFailed;
  }

  // Decode the buffer.
  decoder_->Decode(std::move(buffer_base),
                   base::BindOnce(&AudioDecoderForMixer::OnBufferDecoded,
                                  base::Unretained(this), input_bytes,
                                  true /* has_config */));
  return MediaPipelineBackend::kBufferPending;
}

void AudioDecoderForMixer::UpdateStatistics(Statistics delta) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  stats_.decoded_bytes += delta.decoded_bytes;
}

void AudioDecoderForMixer::GetStatistics(Statistics* stats) {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(stats);
  DCHECK(task_runner_->BelongsToCurrentThread());
  *stats = stats_;
}

bool AudioDecoderForMixer::SetConfig(const AudioConfig& config) {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!IsValidConfig(config)) {
    LOG(ERROR) << "Invalid audio config passed to SetConfig";
    return false;
  }

  input_config_ = config;
  decoder_.reset();
  CreateDecoder();

  auto decoded_config =
      (decoder_ ? decoder_->GetOutputConfig() : input_config_);
  bool changed_config =
      (decoded_config.samples_per_second !=
           decoded_config_.samples_per_second ||
       decoded_config.channel_number != decoded_config_.channel_number);
  decoded_config_ = decoded_config;
  if (mixer_input_ && changed_config) {
    ResetMixerInputForNewConfig(decoded_config);
  }

  if (pending_buffer_complete_ && changed_config) {
    pending_buffer_complete_ = false;
    delegate_->OnPushBufferComplete(MediaPipelineBackend::kBufferSuccess);
  }
  return true;
}

void AudioDecoderForMixer::ResetMixerInputForNewConfig(
    const AudioConfig& config) {
  // Destroy the old input first to ensure that the mixer output sample rate
  // is updated.
  mixer_input_.reset();

  pending_output_frames_ = kNoPendingOutput;
  next_buffer_delay_ = AudioDecoderForMixer::RenderingDelay();
  int64_t last_timestamp = last_push_playout_timestamp_;
  last_push_playout_timestamp_ = kInvalidTimestamp;

  CreateMixerInput(config, av_sync_enabled_);
  if (av_sync_enabled_) {
    if (last_timestamp != kInvalidTimestamp &&
        last_push_pts_ != kInvalidTimestamp) {
      mixer_input_->SetStartTimestamp(last_timestamp, last_push_pts_);
    } else {
      // Pause + resume starts playback immediately.
      mixer_input_->Pause();
      mixer_input_->Resume();
    }
  }
}

void AudioDecoderForMixer::CreateDecoder() {
  DCHECK(!decoder_);
  DCHECK(IsValidConfig(input_config_));

  // No need to create a decoder if the samples are already decoded.
  if (BypassDecoder()) {
    LOG(INFO) << "Data is not coded. Decoder will not be used.";
    return;
  }

  // Create a decoder.
  decoder_ = CastAudioDecoder::Create(task_runner_, input_config_,
                                      kDecoderSampleFormat);
  if (!decoder_) {
    LOG(ERROR) << "Failed to create audio decoder";
    delegate_->OnDecoderError();
  }
}

bool AudioDecoderForMixer::SetVolume(float multiplier) {
  TRACE_FUNCTION_ENTRY1(multiplier);
  DCHECK(task_runner_->BelongsToCurrentThread());
  volume_multiplier_ = multiplier;
  if (mixer_input_)
    mixer_input_->SetVolumeMultiplier(volume_multiplier_);
  return true;
}

AudioDecoderForMixer::RenderingDelay AudioDecoderForMixer::GetRenderingDelay() {
  TRACE_FUNCTION_ENTRY0();
  if (paused_) {
    return RenderingDelay();
  }

  AudioDecoderForMixer::RenderingDelay delay = next_buffer_delay_;
  if (delay.timestamp_microseconds != INT64_MIN) {
    double usec_per_sample = 1000000.0 / decoded_config_.samples_per_second;
    double queued_output_frames = 0.0;

    // Account for data that is in the process of being pushed to the mixer.
    if (pending_output_frames_ != kNoPendingOutput) {
      queued_output_frames += pending_output_frames_;
    }
    delay.delay_microseconds += queued_output_frames * usec_per_sample;
  }

  return delay;
}

AudioDecoderForMixer::AudioTrackTimestamp
AudioDecoderForMixer::GetAudioTrackTimestamp() {
  return AudioTrackTimestamp();
}

int AudioDecoderForMixer::GetStartThresholdInFrames() {
  return 0;
}

void AudioDecoderForMixer::OnBufferDecoded(
    uint64_t input_bytes,
    bool has_config,
    CastAudioDecoder::Status status,
    const AudioConfig& config,
    scoped_refptr<DecoderBufferBase> decoded) {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_buffer_complete_);

  if (!mixer_input_) {
    LOG(DFATAL) << "Buffer pushed before Start() or after Stop()";
    return;
  }
  if (status == CastAudioDecoder::Status::kDecodeError) {
    LOG(ERROR) << "Decode error";
    delegate_->OnPushBufferComplete(MediaPipelineBackend::kBufferFailed);
    return;
  }
  if (mixer_error_) {
    delegate_->OnPushBufferComplete(MediaPipelineBackend::kBufferFailed);
    return;
  }

  Statistics delta;
  delta.decoded_bytes = input_bytes;
  UpdateStatistics(delta);

  if (has_config) {
    bool changed_config = false;
    if (config.samples_per_second != decoded_config_.samples_per_second) {
      LOG(INFO) << "Input sample rate changed from "
                << decoded_config_.samples_per_second << " to "
                << config.samples_per_second;
      decoded_config_.samples_per_second = config.samples_per_second;
      changed_config = true;
    }
    if (config.channel_number != decoded_config_.channel_number) {
      LOG(INFO) << "Input channel count changed from "
                << decoded_config_.channel_number << " to "
                << config.channel_number;
      decoded_config_.channel_number = config.channel_number;
      changed_config = true;
    }
    if (changed_config) {
      // Config from actual stream doesn't match supposed config from the
      // container. Update the mixer.
      ResetMixerInputForNewConfig(decoded_config_);
    }
  }

  if (!decoded->end_of_stream()) {
    pending_output_frames_ =
        decoded->data_size() / (decoded_config_.channel_number * sizeof(float));
    last_push_pts_ = decoded->timestamp();
    last_push_playout_timestamp_ =
        (next_buffer_delay_.timestamp_microseconds == kInvalidTimestamp
             ? kInvalidTimestamp
             : next_buffer_delay_.timestamp_microseconds +
                   next_buffer_delay_.delay_microseconds);
  }
  WritePcm(std::move(decoded));
}

void AudioDecoderForMixer::CheckBufferComplete() {
  if (!pending_buffer_complete_) {
    return;
  }

  pending_buffer_complete_ = false;
  delegate_->OnPushBufferComplete(MediaPipelineBackend::kBufferSuccess);
}

bool AudioDecoderForMixer::BypassDecoder() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // The mixer input requires planar float PCM data.
  return (input_config_.codec == kCodecPCM &&
          input_config_.sample_format == kSampleFormatPlanarF32);
}

void AudioDecoderForMixer::WritePcm(scoped_refptr<DecoderBufferBase> buffer) {
  DCHECK(mixer_input_);
  DCHECK(buffer_pool_);
  DCHECK(!pending_buffer_complete_);

  if (buffer->end_of_stream()) {
    pending_buffer_complete_ = true;
    mixer_input_->SendAudioBuffer(buffer_pool_->GetBuffer(), 0, 0);
    return;
  }

  const int frame_size = sizeof(float) * decoded_config_.channel_number;
  const int original_frame_count = buffer->data_size() / frame_size;
  if (original_frame_count == 0) {
    // Don't send empty buffers since it is interpreted as EOS.
    delegate_->OnPushBufferComplete(MediaPipelineBackend::kBufferSuccess);
    return;
  }

  const int frame_count = buffer->data_size() / frame_size;
  DCHECK_GT(frame_count, 0);

  if (frame_count > buffer_pool_frames_) {
    CreateBufferPool(decoded_config_, frame_count * 2);
  }

  auto io_buffer = buffer_pool_->GetBuffer();
  memcpy(io_buffer->data() + kAudioMessageHeaderSize, buffer->data(),
         buffer->data_size());

  pending_buffer_complete_ = true;
  mixer_input_->SendAudioBuffer(std::move(io_buffer), frame_count,
                                buffer->timestamp());
}

void AudioDecoderForMixer::FillNextBuffer(void* buffer,
                                          int frames,
                                          int64_t delay_timestamp,
                                          int64_t delay) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  pending_output_frames_ = kNoPendingOutput;
  next_buffer_delay_ = RenderingDelay(delay, delay_timestamp);
  CheckBufferComplete();
}

void AudioDecoderForMixer::OnAudioReadyForPlayback(int64_t mixer_delay) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  mixer_delay_ = RenderingDelay(mixer_delay, MonotonicClockNow());
  if (reported_ready_for_playback_) {
    return;
  }
  reported_ready_for_playback_ = true;
  backend_->OnAudioReadyForPlayback();
}

void AudioDecoderForMixer::OnEosPlayed() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  CheckBufferComplete();
  delegate_->OnEndOfStream();
}

void AudioDecoderForMixer::OnMixerError() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  mixer_error_ = true;
  delegate_->OnDecoderError();
}

}  // namespace media
}  // namespace chromecast
