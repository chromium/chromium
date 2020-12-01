// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/mixer_input_connection.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/media/audio/audio_log.h"
#include "chromecast/media/audio/mixer_service/conversions.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "chromecast/media/cma/backend/mixer/channel_layout.h"
#include "chromecast/media/cma/backend/mixer/stream_mixer.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "chromecast/net/io_buffer_pool.h"
#include "chromecast/public/media/decoder_config.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/filters/audio_renderer_algorithm.h"

namespace chromecast {
namespace media {

namespace {

constexpr int kDefaultQueueSize = 8192;
constexpr base::TimeDelta kDefaultFillTime =
    base::TimeDelta::FromMilliseconds(5);
constexpr base::TimeDelta kDefaultFadeTime =
    base::TimeDelta::FromMilliseconds(5);
constexpr base::TimeDelta kInactivityTimeout = base::TimeDelta::FromSeconds(5);
constexpr double kPlaybackRateEpsilon = 0.001;

constexpr int kAudioMessageHeaderSize =
    mixer_service::MixerSocket::kAudioMessageHeaderSize;

constexpr int kRateShifterOutputFrames = 4096;

enum MessageTypes : int {
  kReadyForPlayback = 1,
  kPushResult,
  kEndOfStream,
  kUnderrun,
  kError,
};

int64_t SamplesToMicroseconds(double samples, int sample_rate) {
  return std::round(samples * 1000000 / sample_rate);
}

int GetFillSize(const mixer_service::OutputStreamParams& params) {
  if (params.has_fill_size_frames()) {
    return params.fill_size_frames();
  }
  // Use 10 milliseconds by default.
  return params.sample_rate() / 100;
}

int GetStartThreshold(const mixer_service::OutputStreamParams& params) {
  return std::max(params.start_threshold_frames(), 0);
}

int GetQueueSize(const mixer_service::OutputStreamParams& params) {
  int queue_size = kDefaultQueueSize;
  if (params.has_max_buffered_frames()) {
    queue_size = params.max_buffered_frames();
  }
  int start_threshold = GetStartThreshold(params);
  if (queue_size < start_threshold) {
    queue_size = start_threshold;
    LOG(INFO) << "Increase queue size to " << start_threshold
              << " to match start threshold";
  }
  return queue_size;
}

template <typename T>
bool ValidAudioData(char* data) {
  if (reinterpret_cast<uintptr_t>(data) % sizeof(T) != 0u) {
    LOG(ERROR) << "Misaligned audio data";
    return false;
  }
  return true;
}

template <typename Traits>
void ConvertInterleavedData(int num_channels,
                            char* data,
                            int data_size,
                            float* dest) {
  DCHECK(ValidAudioData<typename Traits::ValueType>(data));
  int num_frames =
      data_size / (sizeof(typename Traits::ValueType) * num_channels);
  typename Traits::ValueType* source =
      reinterpret_cast<typename Traits::ValueType*>(data);
  for (int c = 0; c < num_channels; ++c) {
    float* channel_data = dest + c * num_frames;
    for (int f = 0, read_pos = c; f < num_frames;
         ++f, read_pos += num_channels) {
      channel_data[f] = Traits::ToFloat(source[read_pos]);
    }
  }
}

template <typename Traits>
void ConvertPlanarData(char* data, int data_size, float* dest) {
  DCHECK(ValidAudioData<typename Traits::ValueType>(data));
  int samples = data_size / sizeof(typename Traits::ValueType);
  typename Traits::ValueType* source =
      reinterpret_cast<typename Traits::ValueType*>(data);
  for (int s = 0; s < samples; ++s) {
    dest[s] = Traits::ToFloat(source[s]);
  }
}

int GetFrameCount(net::IOBuffer* buffer) {
  int32_t num_frames;
  memcpy(&num_frames, buffer->data(), sizeof(num_frames));
  return num_frames;
}

int64_t GetTimestamp(net::IOBuffer* buffer) {
  int64_t timestamp;
  memcpy(&timestamp, buffer->data() + sizeof(int32_t), sizeof(timestamp));
  return timestamp;
}

float* GetAudioData(net::IOBuffer* buffer) {
  return reinterpret_cast<float*>(buffer->data() + kAudioMessageHeaderSize);
}

::media::ChannelLayout GetChannelLayout(mixer_service::ChannelLayout layout,
                                        int num_channels) {
  if (layout == mixer_service::CHANNEL_LAYOUT_NONE) {
    return mixer::GuessChannelLayout(num_channels);
  }
  return DecoderConfigAdapter::ToMediaChannelLayout(
      ConvertChannelLayout(layout));
}

}  // namespace

MixerInputConnection::MixerInputConnection(
    StreamMixer* mixer,
    std::unique_ptr<mixer_service::MixerSocket> socket,
    const mixer_service::OutputStreamParams& params)
    : mixer_(mixer),
      socket_(std::move(socket)),
      ignore_for_stream_count_(params.ignore_for_stream_count()),
      fill_size_(GetFillSize(params)),
      algorithm_fill_size_(std::min(
          static_cast<int64_t>(fill_size_),
          ::media::AudioTimestampHelper::TimeToFrames(kDefaultFillTime,
                                                      params.sample_rate()))),
      num_channels_(params.num_channels()),
      channel_layout_(
          GetChannelLayout(params.channel_layout(), params.num_channels())),
      input_samples_per_second_(params.sample_rate()),
      sample_format_(params.sample_format()),
      primary_(params.stream_type() !=
               mixer_service::OutputStreamParams::STREAM_TYPE_SFX),
      device_id_(params.has_device_id()
                     ? params.device_id()
                     : ::media::AudioDeviceDescription::kDefaultDeviceId),
      content_type_(mixer_service::ConvertContentType(params.content_type())),
      focus_type_(params.has_focus_type()
                      ? mixer_service::ConvertContentType(params.focus_type())
                      : content_type_),
      playout_channel_(params.channel_selection()),
      effective_playout_channel_(playout_channel_),
      io_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      max_queued_frames_(std::max(GetQueueSize(params), algorithm_fill_size_)),
      start_threshold_frames_(GetStartThreshold(params)),
      never_timeout_connection_(params.never_timeout_connection()),
      fader_(this,
             params.has_fade_frames()
                 ? params.fade_frames()
                 : ::media::AudioTimestampHelper::TimeToFrames(
                       kDefaultFadeTime,
                       input_samples_per_second_),
             1.0 /* playback_rate */),
      audio_clock_simulator_(&fader_),
      use_start_timestamp_(params.use_start_timestamp()),
      playback_start_timestamp_(use_start_timestamp_ ? INT64_MAX : INT64_MIN),
      audio_buffer_pool_(
          base::MakeRefCounted<::media::AudioBufferMemoryPool>()),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();

  LOG(INFO) << "Create " << this << " (" << device_id_
            << "), content type: " << content_type_
            << ", focus type: " << focus_type_ << ", fill size: " << fill_size_
            << ", algorithm fill size: " << algorithm_fill_size_
            << ", channel count: " << num_channels_
            << ", input sample rate: " << input_samples_per_second_
            << ", start threshold: " << start_threshold_frames_
            << ", max queue size: " << max_queued_frames_
            << ", socket: " << socket_.get();
  DCHECK(mixer_);
  DCHECK(socket_);
  CHECK_GT(num_channels_, 0);
  CHECK_GT(input_samples_per_second_, 0);
  DCHECK_LE(start_threshold_frames_, max_queued_frames_);

  socket_->SetDelegate(this);

  pcm_completion_task_ =
      base::BindRepeating(&MixerInputConnection::PostPcmCompletion, weak_this_);
  eos_task_ = base::BindRepeating(&MixerInputConnection::PostEos, weak_this_);
  ready_for_playback_task_ = base::BindRepeating(
      &MixerInputConnection::PostAudioReadyForPlayback, weak_this_);
  post_stream_underrun_task_ = base::BindRepeating(
      &MixerInputConnection::PostStreamUnderrun, weak_this_);

  CreateBufferPool(fill_size_);
  mixer_->AddInput(this);

  inactivity_timer_.Start(FROM_HERE, kInactivityTimeout, this,
                          &MixerInputConnection::OnInactivityTimeout);
}

MixerInputConnection::~MixerInputConnection() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  LOG(INFO) << "Delete " << this;
}

void MixerInputConnection::CreateBufferPool(int frame_count) {
  DCHECK_GT(frame_count, 0);
  buffer_pool_frames_ = frame_count;

  int converted_buffer_size =
      kAudioMessageHeaderSize + num_channels_ * sizeof(float) * frame_count;
  buffer_pool_ = base::MakeRefCounted<IOBufferPool>(
      converted_buffer_size, std::numeric_limits<size_t>::max(),
      true /* threadsafe */);
  buffer_pool_->Preallocate(start_threshold_frames_ / frame_count + 1);
  if (sample_format_ == mixer_service::SAMPLE_FORMAT_FLOAT_P) {
    // No format conversion needed, so just use the received buffers directly.
    socket_->UseBufferPool(buffer_pool_);
  }
}

bool MixerInputConnection::HandleMetadata(
    const mixer_service::Generic& message) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (inactivity_timer_.IsRunning()) {
    inactivity_timer_.Reset();
  }

  if (message.has_set_start_timestamp()) {
    RestartPlaybackAt(message.set_start_timestamp().start_timestamp(),
                      message.set_start_timestamp().start_pts());
  }
  if (message.has_set_stream_volume()) {
    mixer_->SetVolumeMultiplier(this, message.set_stream_volume().volume());
  }
  if (message.has_set_playback_rate()) {
    SetMediaPlaybackRate(message.set_playback_rate().playback_rate());
  }
  if (message.has_set_audio_clock_rate()) {
    SetAudioClockRate(message.set_audio_clock_rate().rate());
  }
  if (message.has_set_paused()) {
    SetPaused(message.set_paused().paused());
  }
  if (message.has_eos_played_out()) {
    // Explicit EOS.
    HandleAudioData(nullptr, 0, INT64_MIN);
  }
  return true;
}

bool MixerInputConnection::HandleAudioData(char* data,
                                           size_t size,
                                           int64_t timestamp) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (inactivity_timer_.IsRunning()) {
    inactivity_timer_.Reset();
  }

  const int frame_size =
      num_channels_ * mixer_service::GetSampleSizeBytes(sample_format_);
  if (size % frame_size != 0) {
    LOG(ERROR) << this
               << ": audio data size is not an integer number of frames";
    OnConnectionError();
    return false;
  }

  int32_t num_frames = size / frame_size;
  if (num_frames > buffer_pool_frames_) {
    CreateBufferPool(num_frames * 2);
  }
  auto buffer = buffer_pool_->GetBuffer();

  size_t converted_size =
      num_frames * num_channels_ * sizeof(float) + kAudioMessageHeaderSize;
  DCHECK_LE(converted_size, buffer_pool_->buffer_size());

  memcpy(buffer->data(), &num_frames, sizeof(int32_t));
  memcpy(buffer->data() + sizeof(int32_t), &timestamp, sizeof(timestamp));

  float* dest =
      reinterpret_cast<float*>(buffer->data() + kAudioMessageHeaderSize);
  switch (sample_format_) {
    case mixer_service::SAMPLE_FORMAT_INT16_I:
      ConvertInterleavedData<::media::SignedInt16SampleTypeTraits>(
          num_channels_, data, size, dest);
      break;
    case mixer_service::SAMPLE_FORMAT_INT32_I:
      ConvertInterleavedData<::media::SignedInt32SampleTypeTraits>(
          num_channels_, data, size, dest);
      break;
    case mixer_service::SAMPLE_FORMAT_FLOAT_I:
      ConvertInterleavedData<::media::Float32SampleTypeTraits>(
          num_channels_, data, size, dest);
      break;
    case mixer_service::SAMPLE_FORMAT_INT16_P:
      ConvertPlanarData<::media::SignedInt16SampleTypeTraits>(data, size, dest);
      break;
    case mixer_service::SAMPLE_FORMAT_INT32_P:
      ConvertPlanarData<::media::SignedInt32SampleTypeTraits>(data, size, dest);
      break;
    case mixer_service::SAMPLE_FORMAT_FLOAT_P:
      memcpy(dest, data, size);
      break;
    default:
      NOTREACHED() << "Unhandled sample format " << sample_format_;
  }

  WritePcm(std::move(buffer));
  return true;
}

bool MixerInputConnection::HandleAudioBuffer(
    scoped_refptr<net::IOBuffer> buffer,
    char* data,
    size_t size,
    int64_t timestamp) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (inactivity_timer_.IsRunning()) {
    inactivity_timer_.Reset();
  }

  DCHECK_EQ(data - buffer->data(), kAudioMessageHeaderSize);
  if (sample_format_ != mixer_service::SAMPLE_FORMAT_FLOAT_P) {
    return HandleAudioData(data, size, timestamp);
  }

  int32_t num_frames = size / (sizeof(float) * num_channels_);
  DCHECK_EQ(sizeof(int32_t), 4u);
  memcpy(buffer->data(), &num_frames, sizeof(int32_t));

  WritePcm(std::move(buffer));

  if (num_frames > buffer_pool_frames_) {
    CreateBufferPool(num_frames * 2);
  }
  return true;
}

void MixerInputConnection::OnConnectionError() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (connection_error_) {
    return;
  }
  connection_error_ = true;
  socket_.reset();
  weak_factory_.InvalidateWeakPtrs();

  LOG(INFO) << "Remove " << this;
  bool remove_self = false;
  {
    base::AutoLock lock(lock_);
    pending_data_ = nullptr;
    state_ = State::kRemoved;
    remove_self = mixer_error_;
  }

  if (remove_self) {
    mixer_->RemoveInput(this);
  }
}

void MixerInputConnection::OnInactivityTimeout() {
  if (never_timeout_connection_) {
    return;
  }

  LOG(INFO) << "Timed out " << this << " due to inactivity";
  OnConnectionError();
}

void MixerInputConnection::RestartPlaybackAt(int64_t timestamp, int64_t pts) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  LOG(INFO) << this << " RestartPlaybackAt timestamp=" << timestamp
            << " pts=" << pts;
  {
    base::AutoLock lock(lock_);
    playback_start_pts_ = pts;
    playback_start_timestamp_ = timestamp;
    use_start_timestamp_ = true;
    started_ = false;
    queued_frames_ += current_buffer_offset_;
    current_buffer_offset_ = 0;

    while (!queue_.empty()) {
      net::IOBuffer* buffer = queue_.front().get();
      int64_t frames = GetFrameCount(buffer);
      if (GetTimestamp(buffer) +
              SamplesToMicroseconds(frames, input_samples_per_second_) >=
          pts) {
        break;
      }

      queued_frames_ -= frames;
      queue_.pop_front();
    }
  }
}

void MixerInputConnection::SetMediaPlaybackRate(double rate) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  LOG(INFO) << this << " SetMediaPlaybackRate rate=" << rate;
  DCHECK_GT(rate, 0);

  if (std::abs(rate - 1.0) < kPlaybackRateEpsilon) {
    // AudioRendererAlgorithm treats values close to 1 as exactly 1.
    rate = 1.0;
  }

  base::AutoLock lock(lock_);
  if (rate == playback_rate_) {
    return;
  }

  playback_rate_ = rate;
  skip_next_fill_for_rate_change_ = true;
  rate_shifted_offset_ = 0;
  waiting_for_rate_shifter_fill_ = true;

  if (rate == 1.0) {
    rate_shifter_.reset();
    rate_shifter_input_frames_ = rate_shifter_output_frames_ = 0;
    effective_playout_channel_.store(playout_channel_,
                                     std::memory_order_relaxed);
    return;
  }
  // Always play all channels when playback is rate-shifted (b/151393870).
  effective_playout_channel_.store(kChannelAll, std::memory_order_relaxed);

  rate_shifter_ =
      std::make_unique<::media::AudioRendererAlgorithm>(&media_log_);
  rate_shifter_->Initialize(
      mixer::CreateAudioParameters(
          ::media::AudioParameters::AUDIO_PCM_LINEAR, channel_layout_,
          num_channels_, input_samples_per_second_, algorithm_fill_size_),
      false /* is_encrypted */);
  rate_shifter_input_frames_ = rate_shifter_output_frames_ = 0;

  if (!rate_shifter_output_) {
    rate_shifter_output_ =
        ::media::AudioBus::Create(num_channels_, kRateShifterOutputFrames);
  }
}

void MixerInputConnection::SetAudioClockRate(double rate) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock lock(lock_);
  audio_clock_simulator_.SetRate(rate);
}

void MixerInputConnection::SetPaused(bool paused) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  LOG(INFO) << (paused ? "Pausing " : "Unpausing ") << this;
  if (paused) {
    inactivity_timer_.Stop();
  } else {
    inactivity_timer_.Start(FROM_HERE, kInactivityTimeout, this,
                            &MixerInputConnection::OnInactivityTimeout);
  }

  {
    base::AutoLock lock(lock_);
    if (paused == paused_) {
      return;
    }

    paused_ = paused;
    mixer_rendering_delay_ = RenderingDelay();
    // Clear start timestamp, since a pause should invalidate the start
    // timestamp anyway. The AV sync code can restart (hard correction) on
    // resume if needed.
    use_start_timestamp_ = false;
    playback_start_timestamp_ = INT64_MIN;
  }
  mixer_->UpdateStreamCounts();
}

size_t MixerInputConnection::num_channels() const {
  return num_channels_;
}

::media::ChannelLayout MixerInputConnection::channel_layout() const {
  return channel_layout_;
}

int MixerInputConnection::sample_rate() const {
  return input_samples_per_second_;
}

bool MixerInputConnection::primary() {
  return primary_;
}

const std::string& MixerInputConnection::device_id() {
  return device_id_;
}

AudioContentType MixerInputConnection::content_type() {
  return content_type_;
}

AudioContentType MixerInputConnection::focus_type() {
  return focus_type_;
}

int MixerInputConnection::desired_read_size() {
  return algorithm_fill_size_;
}

int MixerInputConnection::playout_channel() {
  return effective_playout_channel_.load(std::memory_order_relaxed);
}

bool MixerInputConnection::active() {
  base::AutoLock lock(lock_);
  return !ignore_for_stream_count_ && !paused_;
}

void MixerInputConnection::WritePcm(scoped_refptr<net::IOBuffer> data) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  int64_t next_playback_timestamp;
  bool queued;
  {
    base::AutoLock lock(lock_);
    if (state_ == State::kUninitialized ||
        queued_frames_ + fader_.buffered_frames() >= max_queued_frames_) {
      if (pending_data_) {
        LOG(ERROR) << "Got unexpected audio data for " << this;
      }
      pending_data_ = std::move(data);
      queued = false;
    } else {
      next_playback_timestamp = QueueData(std::move(data));
      queued = true;
    }
  }

  if (queued) {
    mixer_service::Generic message;
    message.mutable_push_result()->set_next_playback_timestamp(
        next_playback_timestamp);
    socket_->SendProto(kPushResult, message);
  }
}

int64_t MixerInputConnection::QueueData(scoped_refptr<net::IOBuffer> data) {
  int frames = GetFrameCount(data.get());
  if (frames == 0) {
    AUDIO_LOG(INFO) << "End of stream for " << this;
    state_ = State::kGotEos;
    if (!started_) {
      io_task_runner_->PostTask(FROM_HERE, ready_for_playback_task_);
    }
  } else if (started_ ||
             GetTimestamp(data.get()) +
                     SamplesToMicroseconds(frames, input_samples_per_second_) >=
                 playback_start_pts_) {
    queued_frames_ += frames;
    queue_.push_back(std::move(data));

    if (!started_ && queued_frames_ >= start_threshold_frames_) {
      io_task_runner_->PostTask(FROM_HERE, ready_for_playback_task_);
    }
  }
  // Otherwise, drop |data| since it is before the start PTS.

  if (!started_ || paused_ ||
      mixer_rendering_delay_.timestamp_microseconds == INT64_MIN ||
      waiting_for_rate_shifter_fill_) {
    // Note that if we are waiting for the rate shifter to fill, we can't report
    // accurate rendering delay because we don't know when the audio will start
    // being filled to the mixer again (depends on how long it takes to fill the
    // rate shifter).
    return INT64_MIN;
  }

  // The next playback timestamp will be the timestamp from the last mixer fill
  // request, plus the time required to play out the other data in the pipeline.
  // The other data includes:
  //   * The number of frames of the last mixer fill (since that will be played
  //     out starting at the last mixer rendering delay).
  //   * Data buffered in the fader (this and the previous are included in
  //     |extra_delay_frames_|).
  //   * Queued data in |queue_|.
  //   * Data in the rate shifter, if any.
  double extra_delay_frames = extra_delay_frames_ +
                              queued_frames_ / playback_rate_ +
                              audio_clock_simulator_.DelayFrames();
  if (rate_shifter_) {
    double rate_shifter_delay =
        static_cast<double>(rate_shifter_input_frames_) / playback_rate_ -
        rate_shifter_output_frames_;
    extra_delay_frames += rate_shifter_delay;
  }
  if (skip_next_fill_for_rate_change_) {
    // If a playback rate change happened since the last mixer fill request, we
    // will fill the next mixer request with the data buffered in the fader
    // (if any), which will be faded out, and the rest of the request will be
    // filled with zeros. Add the number of zero frames that will be added to
    // the next playback timestamp.
    extra_delay_frames +=
        std::max(0, mixer_read_size_ - fader_.buffered_frames());
  }

  int64_t mixer_timestamp = mixer_rendering_delay_.timestamp_microseconds +
                            mixer_rendering_delay_.delay_microseconds;
  return mixer_timestamp +
         SamplesToMicroseconds(extra_delay_frames, input_samples_per_second_);
}

void MixerInputConnection::InitializeAudioPlayback(
    int read_size,
    RenderingDelay initial_rendering_delay) {
  // Start accepting buffers into the queue.
  bool queued_data = false;
  {
    base::AutoLock lock(lock_);
    mixer_read_size_ = read_size;
    if (start_threshold_frames_ == 0) {
      start_threshold_frames_ = read_size + fill_size_;
      AUDIO_LOG(INFO) << this << " Updated start threshold: "
                      << start_threshold_frames_;
    }
    mixer_rendering_delay_ = initial_rendering_delay;
    if (state_ == State::kUninitialized) {
      state_ = State::kNormalPlayback;
    } else {
      DCHECK_EQ(state_, State::kRemoved);
    }

    if (pending_data_ &&
        queued_frames_ + fader_.buffered_frames() < max_queued_frames_) {
      next_playback_timestamp_ = QueueData(std::move(pending_data_));
      queued_data = true;
    }
  }

  if (queued_data) {
    io_task_runner_->PostTask(FROM_HERE, pcm_completion_task_);
  }
}

void MixerInputConnection::CheckAndStartPlaybackIfNecessary(
    int num_frames,
    int64_t playback_absolute_timestamp) {
  DCHECK(state_ == State::kNormalPlayback || state_ == State::kGotEos);
  DCHECK(!started_);

  const int frames_needed_to_start = std::max(
      start_threshold_frames_, fader_.FramesNeededFromSource(num_frames));
  if (max_queued_frames_ < frames_needed_to_start) {
    AUDIO_LOG(INFO) << "Boost queue size to " << frames_needed_to_start
                    << " to allow stream to start";
    max_queued_frames_ = frames_needed_to_start;
  }
  const bool have_enough_queued_frames =
      (state_ == State::kGotEos || queued_frames_ >= frames_needed_to_start);
  if (!have_enough_queued_frames) {
    return;
  }

  remaining_silence_frames_ = 0;
  if (!use_start_timestamp_ || (queue_.empty() && state_ == State::kGotEos)) {
    // No start timestamp, so start as soon as there are enough queued frames.
    AUDIO_LOG(INFO) << "Start " << this;
    started_ = true;
    return;
  }

  if (playback_absolute_timestamp +
          SamplesToMicroseconds(num_frames, input_samples_per_second_) <
      playback_start_timestamp_) {
    // Haven't reached the start timestamp yet.
    return;
  }

  DCHECK(!queue_.empty());
  // Reset the current buffer offset to 0 so we can ignore it below. We need to
  // do this here because we may not have started playback even after dropping
  // all necessary frames the last time we checked.
  queued_frames_ += current_buffer_offset_;
  current_buffer_offset_ = 0;

  int64_t desired_pts_now = playback_start_pts_ + (playback_absolute_timestamp -
                                                   playback_start_timestamp_) *
                                                      playback_rate_;
  int64_t actual_pts_now = GetTimestamp(queue_.front().get());
  int64_t drop_us = (desired_pts_now - actual_pts_now) / playback_rate_;

  if (drop_us >= 0) {
    AUDIO_LOG(INFO) << this << " Dropping audio, duration = " << drop_us;
    DropAudio(::media::AudioTimestampHelper::TimeToFrames(
        base::TimeDelta::FromMicroseconds(drop_us), input_samples_per_second_));
    // Only start if we still have enough data to do so.
    started_ = (queued_frames_ >= start_threshold_frames_ &&
                queued_frames_ >= fader_.FramesNeededFromSource(num_frames));

    if (started_) {
      int64_t start_pts = GetTimestamp(queue_.front().get()) +
                          SamplesToMicroseconds(current_buffer_offset_,
                                                input_samples_per_second_) *
                              playback_rate_;
      AUDIO_LOG(INFO) << this << " Start playback of PTS " << start_pts
                      << " at " << playback_absolute_timestamp;
    }
  } else {
    int64_t silence_duration = -drop_us;
    AUDIO_LOG(INFO) << this
                    << " Adding silence. Duration = " << silence_duration;
    remaining_silence_frames_ = ::media::AudioTimestampHelper::TimeToFrames(
        base::TimeDelta::FromMicroseconds(silence_duration),
        input_samples_per_second_);
    // Round to nearest multiple of 4 to preserve buffer alignment.
    remaining_silence_frames_ = ((remaining_silence_frames_ + 2) / 4) * 4;
    started_ = true;
    AUDIO_LOG(INFO) << this << " Should start playback of PTS "
                    << actual_pts_now << " at "
                    << (playback_absolute_timestamp + silence_duration);
  }
}

void MixerInputConnection::DropAudio(int64_t frames_to_drop) {
  while (frames_to_drop && !queue_.empty()) {
    int64_t first_buffer_frames = GetFrameCount(queue_.front().get());
    int64_t frames_left = first_buffer_frames - current_buffer_offset_;

    if (frames_left > frames_to_drop) {
      current_buffer_offset_ += frames_to_drop;
      queued_frames_ -= frames_to_drop;
      frames_to_drop = 0;
      break;
    }

    queued_frames_ -= first_buffer_frames;
    frames_to_drop -= first_buffer_frames;
    queue_.pop_front();
    current_buffer_offset_ = 0;
  }

  if (frames_to_drop > 0) {
    AUDIO_LOG(INFO) << this << " Still need to drop " << frames_to_drop
                    << " frames";
  }
}

int MixerInputConnection::FillAudioPlaybackFrames(
    int num_frames,
    RenderingDelay rendering_delay,
    ::media::AudioBus* buffer) {
  DCHECK(buffer);
  DCHECK_EQ(num_channels_, buffer->channels());
  DCHECK_GE(buffer->frames(), num_frames);

  int64_t playback_absolute_timestamp = rendering_delay.delay_microseconds +
                                        rendering_delay.timestamp_microseconds;

  int filled = 0;
  bool queued_more_data = false;
  bool signal_eos = false;
  bool underrun = false;
  bool remove_self = false;
  {
    base::AutoLock lock(lock_);

    mixer_read_size_ = num_frames;

    // Playback start check.
    if (!started_ &&
        (state_ == State::kNormalPlayback || state_ == State::kGotEos)) {
      CheckAndStartPlaybackIfNecessary(num_frames, playback_absolute_timestamp);
    }

    bool can_complete_fill = true;
    if (started_ && !paused_) {
      can_complete_fill = PrepareDataForFill(num_frames);
    }

    // In normal playback, don't pass data to the fader if we can't satisfy the
    // full request. This will allow us to buffer up more data so we can fully
    // fade in.
    if (state_ == State::kNormalPlayback && !can_complete_fill) {
      AUDIO_LOG_IF(INFO, !zero_fader_frames_) << "Stream underrun for " << this;
      zero_fader_frames_ = true;
      underrun = true;
    } else {
      AUDIO_LOG_IF(INFO, started_ && zero_fader_frames_)
          << "Stream underrun recovered for " << this;
      zero_fader_frames_ = false;
      if (!skip_next_fill_for_rate_change_) {
        waiting_for_rate_shifter_fill_ = false;
      }
    }

    DCHECK_GE(remaining_silence_frames_, 0);
    if (remaining_silence_frames_ >= num_frames) {
      remaining_silence_frames_ -= num_frames;
      return 0;
    }

    int write_offset = 0;
    if (remaining_silence_frames_ > 0) {
      buffer->ZeroFramesPartial(0, remaining_silence_frames_);
      filled += remaining_silence_frames_;
      num_frames -= remaining_silence_frames_;
      write_offset = remaining_silence_frames_;
      remaining_silence_frames_ = 0;
    }

    float* channels[num_channels_];
    for (int c = 0; c < num_channels_; ++c) {
      channels[c] = buffer->channel(c) + write_offset;
    }
    filled += audio_clock_simulator_.FillFrames(
        num_frames, playback_absolute_timestamp, channels);
    skip_next_fill_for_rate_change_ = false;

    mixer_rendering_delay_ = rendering_delay;
    extra_delay_frames_ = mixer_read_size_ + fader_.buffered_frames();

    // See if we can accept more data into the queue.
    if (pending_data_ &&
        queued_frames_ + fader_.buffered_frames() < max_queued_frames_) {
      next_playback_timestamp_ = QueueData(std::move(pending_data_));
      queued_more_data = true;
    }

    // Check if we have played out EOS.
    if (state_ == State::kGotEos && queued_frames_ == 0 &&
        fader_.buffered_frames() == 0) {
      signal_eos = true;
      state_ = State::kSignaledEos;
    }

    // If the caller has removed this source, delete once we have faded out.
    if (state_ == State::kRemoved && fader_.buffered_frames() == 0) {
      if (fed_one_silence_buffer_after_removal_) {
        remove_self = true;
      }
      fed_one_silence_buffer_after_removal_ = true;
    }
  }

  if (queued_more_data) {
    io_task_runner_->PostTask(FROM_HERE, pcm_completion_task_);
  }
  if (signal_eos) {
    io_task_runner_->PostTask(FROM_HERE, eos_task_);
  }
  if (underrun) {
    io_task_runner_->PostTask(FROM_HERE, post_stream_underrun_task_);
  }

  if (remove_self) {
    mixer_->RemoveInput(this);
  }

  return filled;
}

bool MixerInputConnection::PrepareDataForFill(int num_frames) {
  int needed_by_fader = fader_.FramesNeededFromSource(num_frames);
  if (!rate_shifter_) {
    return (queued_frames_ >= needed_by_fader);
  }
  return FillRateShifted(needed_by_fader);
}

int MixerInputConnection::FillFrames(int num_frames,
                                     int64_t playout_timestamp,
                                     float* const* channels) {
  if (zero_fader_frames_ || !started_ || paused_ || state_ == State::kRemoved ||
      skip_next_fill_for_rate_change_ || num_frames == 0) {
    return 0;
  }

  if (!rate_shifter_) {
    return FillAudio(num_frames, channels);
  }

  int filled = std::min(num_frames, rate_shifted_offset_);
  for (int c = 0; c < num_channels_; ++c) {
    float* rate_shifted = rate_shifter_output_->channel(c);
    std::copy_n(rate_shifted, filled, channels[c]);
    std::copy(rate_shifted + filled, rate_shifted + rate_shifted_offset_,
              rate_shifted);
  }
  rate_shifted_offset_ -= filled;

  return filled;
}

bool MixerInputConnection::FillRateShifted(int needed_frames) {
  DCHECK(rate_shifter_output_);
  DCHECK(rate_shifter_);
  if (rate_shifted_offset_ >= needed_frames) {
    return true;
  }

  if (rate_shifter_output_->frames() < needed_frames) {
    AUDIO_LOG(WARNING) << "Rate shifter output is too small; "
                       << rate_shifter_output_->frames() << " < "
                       << needed_frames;
    auto output = ::media::AudioBus::Create(num_channels_, needed_frames);
    rate_shifter_output_->CopyPartialFramesTo(0, rate_shifted_offset_, 0,
                                              output.get());
    rate_shifter_output_ = std::move(output);
  }

  int filled = rate_shifter_->FillBuffer(
      rate_shifter_output_.get(), rate_shifted_offset_,
      needed_frames - rate_shifted_offset_, playback_rate_);
  rate_shifter_output_frames_ += filled;
  rate_shifted_offset_ += filled;

  while (rate_shifted_offset_ < needed_frames) {
    // Get more data and queue it in the rate shifter.
    auto buffer = ::media::AudioBuffer::CreateBuffer(
        ::media::SampleFormat::kSampleFormatPlanarF32, channel_layout_,
        num_channels_, input_samples_per_second_, algorithm_fill_size_,
        audio_buffer_pool_);
    int new_fill = FillAudio(
        algorithm_fill_size_,
        const_cast<float**>(
            reinterpret_cast<float* const*>(buffer->channel_data().data())));
    if (new_fill == 0) {
      break;
    }
    buffer->TrimEnd(algorithm_fill_size_ - new_fill);

    rate_shifter_->EnqueueBuffer(std::move(buffer));
    rate_shifter_input_frames_ += new_fill;

    // Now see if the rate shifter can produce more output.
    filled = rate_shifter_->FillBuffer(
        rate_shifter_output_.get(), rate_shifted_offset_,
        needed_frames - rate_shifted_offset_, playback_rate_);
    rate_shifter_output_frames_ += filled;
    rate_shifted_offset_ += filled;

    if (new_fill != algorithm_fill_size_) {
      // Ran out of queued data.
      break;
    }
  }

  return (rate_shifted_offset_ >= needed_frames);
}

int MixerInputConnection::FillAudio(int num_frames, float* const* channels) {
  DCHECK(channels);

  int num_filled = 0;
  while (num_frames) {
    if (queue_.empty()) {
      return num_filled;
    }

    net::IOBuffer* buffer = queue_.front().get();
    const int buffer_frames = GetFrameCount(buffer);
    const int frames_to_copy =
        std::min(num_frames, buffer_frames - current_buffer_offset_);
    DCHECK(frames_to_copy >= 0 && frames_to_copy <= num_frames)
        << " frames_to_copy=" << frames_to_copy << " num_frames=" << num_frames
        << " buffer_frames=" << buffer_frames << " num_filled=" << num_filled
        << " current_buffer_offset_=" << current_buffer_offset_;

    const float* buffer_samples = GetAudioData(buffer);
    for (int c = 0; c < num_channels_; ++c) {
      const float* buffer_channel = buffer_samples + (buffer_frames * c);
      std::copy_n(buffer_channel + current_buffer_offset_, frames_to_copy,
                  channels[c] + num_filled);
    }

    num_frames -= frames_to_copy;
    queued_frames_ -= frames_to_copy;
    num_filled += frames_to_copy;

    current_buffer_offset_ += frames_to_copy;
    if (current_buffer_offset_ == buffer_frames) {
      queue_.pop_front();
      current_buffer_offset_ = 0;
    }
  }

  return num_filled;
}

void MixerInputConnection::PostPcmCompletion() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  mixer_service::Generic message;
  auto* push_result = message.mutable_push_result();
  {
    base::AutoLock lock(lock_);
    push_result->set_next_playback_timestamp(next_playback_timestamp_);
  }
  socket_->SendProto(kPushResult, message);
}

void MixerInputConnection::PostEos() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  mixer_service::Generic message;
  message.mutable_eos_played_out();
  socket_->SendProto(kEndOfStream, message);
}

void MixerInputConnection::PostAudioReadyForPlayback() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  if (audio_ready_for_playback_fired_) {
    return;
  }
  AUDIO_LOG(INFO) << this << " ready for playback";

  mixer_service::Generic message;
  auto* ready_for_playback = message.mutable_ready_for_playback();
  {
    base::AutoLock lock(lock_);
    ready_for_playback->set_delay_microseconds(
        mixer_rendering_delay_.delay_microseconds);
  }
  socket_->SendProto(kReadyForPlayback, message);
  audio_ready_for_playback_fired_ = true;
}

void MixerInputConnection::PostStreamUnderrun() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  mixer_service::Generic message;
  message.mutable_mixer_underrun()->set_type(
      mixer_service::MixerUnderrun::INPUT_UNDERRUN);
  socket_->SendProto(kUnderrun, message);
}

void MixerInputConnection::PostOutputUnderrun() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  mixer_service::Generic message;
  message.mutable_mixer_underrun()->set_type(
      mixer_service::MixerUnderrun::OUTPUT_UNDERRUN);
  socket_->SendProto(kUnderrun, message);
}

void MixerInputConnection::OnAudioPlaybackError(MixerError error) {
  if (error == MixerError::kInputIgnored) {
    AUDIO_LOG(INFO) << "Mixer input " << this
                    << " now being ignored due to output sample rate change";
  }

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MixerInputConnection::PostError, weak_this_, error));

  base::AutoLock lock(lock_);
  mixer_error_ = true;
  if (state_ == State::kRemoved) {
    mixer_->RemoveInput(this);
  }
}

void MixerInputConnection::OnOutputUnderrun() {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MixerInputConnection::PostOutputUnderrun, weak_this_));
}

void MixerInputConnection::PostError(MixerError error) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  mixer_service::Generic message;
  message.mutable_error()->set_type(mixer_service::Error::INVALID_STREAM_ERROR);
  socket_->SendProto(kError, message);

  OnConnectionError();
}

void MixerInputConnection::FinalizeAudioPlayback() {
  io_task_runner_->DeleteSoon(FROM_HERE, this);
}

}  // namespace media
}  // namespace chromecast
