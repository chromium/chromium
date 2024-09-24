// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/mixer_input_connection.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/media/api/audio_provider.h"
#include "chromecast/media/audio/audio_fader.h"
#include "chromecast/media/audio/audio_log.h"
#include "chromecast/media/audio/mixer_service/mixer_service_transport.pb.h"
#include "chromecast/media/audio/net/conversions.h"
#include "chromecast/media/audio/rate_adjuster.h"
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
constexpr base::TimeDelta kDefaultFillTime = base::Milliseconds(5);
constexpr base::TimeDelta kDefaultFadeTime = base::Milliseconds(5);
constexpr base::TimeDelta kInactivityTimeout = base::Seconds(5);
constexpr int64_t kDefaultMaxTimestampError = 2000;
// Max absolute value for timestamp errors, to avoid overflow/underflow.
constexpr int64_t kTimestampErrorLimit = 1000000;
constexpr int kMaxChannels = 32;

constexpr int kAudioMessageHeaderSize =
    mixer_service::MixerSocket::kAudioMessageHeaderSize;

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

int64_t GetMaxTimestampError(const mixer_service::OutputStreamParams& params) {
  int64_t result = params.timestamped_audio_config().max_timestamp_error();
  if (result == 0) {
    result = kDefaultMaxTimestampError;
  }
  return result;
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

void AdjustTimestamp(net::IOBuffer* buffer, int64_t adjustment) {
  void* ptr = buffer->data() + sizeof(int32_t);
  int64_t timestamp;
  memcpy(&timestamp, ptr, sizeof(timestamp));
  timestamp += adjustment;
  memcpy(ptr, &timestamp, sizeof(timestamp));
}

void AdjustTimestampForRateChange(net::IOBuffer* buffer,
                                  double ratio,
                                  int64_t base_timestamp) {
  void* ptr = buffer->data() + sizeof(int32_t);
  int64_t timestamp;
  memcpy(&timestamp, ptr, sizeof(timestamp));
  int64_t diff = timestamp - base_timestamp;
  timestamp = base_timestamp + diff * ratio;
  memcpy(ptr, &timestamp, sizeof(timestamp));
}

float* GetAudioData(net::IOBuffer* buffer) {
  return reinterpret_cast<float*>(buffer->data() + kAudioMessageHeaderSize);
}

::media::ChannelLayout GetChannelLayout(audio_service::ChannelLayout layout,
                                        int num_channels) {
  DCHECK_NE(layout, audio_service::CHANNEL_LAYOUT_BITSTREAM);
  if (layout == audio_service::CHANNEL_LAYOUT_NONE) {
    return mixer::GuessChannelLayout(num_channels);
  }
  return DecoderConfigAdapter::ToMediaChannelLayout(
      ConvertChannelLayout(layout));
}

std::unique_ptr<RateAdjuster> CreateRateAdjuster(
    const mixer_service::OutputStreamParams& params,
    RateAdjuster::RateChangeCallback callback) {
  const auto& c = params.timestamped_audio_config();
  media::RateAdjuster::Config config;
  if (c.has_rate_change_interval()) {
    config.rate_change_interval = base::Microseconds(c.rate_change_interval());
  }
  if (c.has_linear_regression_window()) {
    config.linear_regression_window =
        base::Microseconds(c.linear_regression_window());
  }
  if (c.has_max_ignored_current_error()) {
    config.max_ignored_current_error = c.max_ignored_current_error();
  }
  if (c.has_max_current_error_correction()) {
    config.max_current_error_correction = c.max_current_error_correction();
  }
  if (c.has_min_rate_change()) {
    config.min_rate_change = c.min_rate_change();
  }

  auto adjuster =
      std::make_unique<RateAdjuster>(config, std::move(callback), 1.0);
  int64_t expected_error_samples =
      config.linear_regression_window.InSecondsF() * params.sample_rate() /
      GetFillSize(params);
  adjuster->Reserve(2 * expected_error_samples);
  return adjuster;
}

}  // namespace

class MixerInputConnection::TimestampedFader : public AudioProvider {
  class FaderProvider : public AudioProvider {
   public:
    FaderProvider(TimestampedFader& owner, int num_channels, int sample_rate)
        : owner_(owner),
          num_channels_(num_channels),
          sample_rate_(sample_rate) {}

    // AudioProvider implementation:
    size_t num_channels() const override { return num_channels_; }
    int sample_rate() const override { return sample_rate_; }
    int FillFrames(int num_frames,
                   int64_t playout_timestamp,
                   float* const* channel_data) override;

   private:
    TimestampedFader& owner_;
    const int num_channels_;
    const int sample_rate_;
  };

 public:
  TimestampedFader(MixerInputConnection* mixer_input, int fade_frames)
      : mixer_input_(mixer_input),
        num_channels_(mixer_input_->num_channels()),
        sample_rate_(mixer_input_->sample_rate()),
        fader_provider_(*this, num_channels_, sample_rate_),
        fader_(&fader_provider_, fade_frames, 1.0) {}

  ~TimestampedFader() override = default;

  int BufferedFrames() { return fader_.buffered_frames(); }

  void SetPlaybackRate(double rate) { fader_.set_playback_rate(rate); }

  // AudioProvider implementation:
  int FillFrames(int num_frames,
                 int64_t expected_playout_time,
                 float* const* channels) override NO_THREAD_SAFETY_ANALYSIS {
    int filled = 0;
    while (filled < num_frames) {
      int remaining = num_frames - filled;
      const int64_t playout_time =
          expected_playout_time +
          SamplesToMicroseconds(filled / mixer_input_->playback_rate_,
                                sample_rate_);
      float* fill_channel_data[kMaxChannels];
      for (int c = 0; c < num_channels_; ++c) {
        fill_channel_data[c] = channels[c] + filled;
      }
      int faded = fader_.FillFrames(remaining, playout_time, fill_channel_data);
      filled += faded;
      if (faded == remaining) {
        if (pending_silence_) {
          // Still waiting for fade to complete, but we should re-evaluate how
          // much silence to add for the next fill.
          pending_silence_.emplace(0);
        }
        return filled;
      }

      if (!pending_silence_) {
        // No pending silence, and we are not able to fill any more.
        for (int c = 0; c < num_channels_; ++c) {
          std::fill_n(channels[c] + filled, num_frames - filled, 0.0f);
        }
        return num_frames;
      }

      if (faded == 0) {
        // Fade complete; fill some silence.
        int silence_frames =
            std::min(pending_silence_.value(), num_frames - filled);
        for (int c = 0; c < num_channels_; ++c) {
          std::fill_n(channels[c] + filled, silence_frames, 0.0f);
        }
        filled += silence_frames;
        // Reset since we'll want to re-evaluate for the next fill.
        pending_silence_.reset();
        after_silence_ = true;
      }
    }
    return filled;
  }

  void AddSilence(int frames) { pending_silence_.emplace(frames); }

 private:
  static constexpr size_t kMaxChannels = 32;

  int FillFramesForFader(int num_frames,
                         int64_t playout_timestamp,
                         float* const* channel_data) NO_THREAD_SAFETY_ANALYSIS {
    if (pending_silence_) {
      return 0;
    }
    int filled = mixer_input_->FillAudio(num_frames, playout_timestamp,
                                         channel_data, after_silence_);
    if (filled) {
      after_silence_ = false;
    }
    return filled;
  }

  size_t num_channels() const override { return num_channels_; }
  int sample_rate() const override { return sample_rate_; }

  MixerInputConnection* const mixer_input_;
  const int num_channels_;
  const int sample_rate_;

  std::optional<int> pending_silence_;
  FaderProvider fader_provider_;
  AudioFader fader_;
  bool after_silence_ = true;
};

int MixerInputConnection::TimestampedFader::FaderProvider::FillFrames(
    int num_frames,
    int64_t playout_timestamp,
    float* const* channel_data) {
  return owner_.FillFramesForFader(num_frames, playout_timestamp, channel_data);
}

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
      content_type_(audio_service::ConvertContentType(params.content_type())),
      focus_type_(params.has_focus_type()
                      ? audio_service::ConvertContentType(params.focus_type())
                      : content_type_),
      playout_channel_(params.channel_selection()),
      pts_is_timestamp_(params.has_timestamped_audio_config()),
      max_timestamp_error_(GetMaxTimestampError(params)),
      never_crop_(params.timestamped_audio_config().never_crop()),
      enable_audio_clock_simulation_(pts_is_timestamp_ ||
                                     params.enable_audio_clock_simulation()),
      effective_playout_channel_(playout_channel_),
      io_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      max_queued_frames_(std::max(GetQueueSize(params), algorithm_fill_size_)),
      start_threshold_frames_(GetStartThreshold(params)),
      never_timeout_connection_(params.never_timeout_connection()),
      rate_adjuster_(
          pts_is_timestamp_
              ? CreateRateAdjuster(
                    params,
                    base::BindRepeating(&MixerInputConnection::ChangeAudioRate,
                                        base::Unretained(this)))
              : nullptr),
      fade_frames_(params.has_fade_frames()
                       ? params.fade_frames()
                       : ::media::AudioTimestampHelper::TimeToFrames(
                             kDefaultFadeTime,
                             input_samples_per_second_)),
      timestamped_fader_(
          std::make_unique<TimestampedFader>(this, fade_frames_)),
      rate_shifter_(timestamped_fader_.get(),
                    channel_layout_,
                    num_channels_,
                    input_samples_per_second_,
                    algorithm_fill_size_),
      use_start_timestamp_(params.use_start_timestamp()),
      playback_start_timestamp_(use_start_timestamp_ ? INT64_MAX : INT64_MIN),
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
            << ", pts_is_timestamp: " << pts_is_timestamp_;
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
  if (sample_format_ == audio_service::SAMPLE_FORMAT_FLOAT_P) {
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
  if (message.has_timestamp_adjustment()) {
    AdjustTimestamps(message.timestamp_adjustment().adjustment());
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
  const int frame_size =
      num_channels_ * audio_service::GetSampleSizeBytes(sample_format_);
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
    case audio_service::SAMPLE_FORMAT_INT16_I:
      ConvertInterleavedData<::media::SignedInt16SampleTypeTraits>(
          num_channels_, data, size, dest);
      break;
    case audio_service::SAMPLE_FORMAT_INT32_I:
      ConvertInterleavedData<::media::SignedInt32SampleTypeTraits>(
          num_channels_, data, size, dest);
      break;
    case audio_service::SAMPLE_FORMAT_FLOAT_I:
      ConvertInterleavedData<::media::Float32SampleTypeTraits>(
          num_channels_, data, size, dest);
      break;
    case audio_service::SAMPLE_FORMAT_INT16_P:
      ConvertPlanarData<::media::SignedInt16SampleTypeTraits>(data, size, dest);
      break;
    case audio_service::SAMPLE_FORMAT_INT32_P:
      ConvertPlanarData<::media::SignedInt32SampleTypeTraits>(data, size, dest);
      break;
    case audio_service::SAMPLE_FORMAT_FLOAT_P:
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
  DCHECK_EQ(data - buffer->data(), kAudioMessageHeaderSize);
  if (sample_format_ != audio_service::SAMPLE_FORMAT_FLOAT_P) {
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
  {
    base::AutoLock lock(lock_);
    pending_data_ = nullptr;
    state_ = State::kRemoved;
    SetMediaPlaybackRateLocked(1.0);
    if (mixer_error_) {
      RemoveSelf();
    }
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

  base::AutoLock lock(lock_);
  if (state_ == State::kGotEos || state_ == State::kRemoved) {
    LOG(INFO) << this << " ignore playback rate change after EOS/removed";
    return;
  }
  SetMediaPlaybackRateLocked(rate);
}

void MixerInputConnection::SetMediaPlaybackRateLocked(double rate) {
  double old_rate = playback_rate_;
  rate_shifter_.SetPlaybackRate(rate);
  playback_rate_ = rate_shifter_.playback_rate();
  if (rate_adjuster_) {
    rate_adjuster_->Reset();
    total_filled_frames_ = 0;
  }
  timestamped_fader_->SetPlaybackRate(playback_rate_);
  if (playback_rate_ == 1.0) {
    effective_playout_channel_.store(playout_channel_,
                                     std::memory_order_relaxed);
  } else {
    // Always play all channels when playback is rate-shifted (b/151393870).
    effective_playout_channel_.store(kChannelAll, std::memory_order_relaxed);
  }

  // Adjust timestamps.
  if (pts_is_timestamp_ && !queue_.empty()) {
    double timestamp_ratio = old_rate / playback_rate_;
    auto it = queue_.begin();
    int64_t base_timestamp = GetTimestamp(it->get());
    for (++it; it != queue_.end(); ++it) {
      AdjustTimestampForRateChange(it->get(), timestamp_ratio, base_timestamp);
    }
    if (pending_data_) {
      AdjustTimestampForRateChange(pending_data_.get(), timestamp_ratio,
                                   base_timestamp);
    }
  }
}

void MixerInputConnection::SetAudioClockRate(double rate) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (pts_is_timestamp_) {
    AUDIO_LOG(ERROR) << "Cannot set audio clock rate in timestamped mode";
    return;
  }

  if (!enable_audio_clock_simulation_) {
    AUDIO_LOG(ERROR) << "Audio clock rate simulation not enabled";
    return;
  }
  mixer_->SetSimulatedClockRate(this, rate);
}

double MixerInputConnection::ChangeAudioRate(double desired_clock_rate,
                                             double error_slope,
                                             double current_error) {
  mixer_->SetSimulatedClockRate(this, desired_clock_rate);
  return desired_clock_rate;
}

void MixerInputConnection::AdjustTimestamps(int64_t timestamp_adjustment) {
  if (timestamp_adjustment == 0) {
    return;
  }

  base::AutoLock lock(lock_);
  if (pending_data_) {
    AdjustTimestamp(pending_data_.get(), timestamp_adjustment);
  }
  for (const auto& buffer : queue_) {
    AdjustTimestamp(buffer.get(), timestamp_adjustment);
  }
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
    if (paused_) {
      // Drain data out of the rate shifter.
      rate_shifter_.SetPlaybackRate(1.0);
    } else {
      rate_shifter_.SetPlaybackRate(playback_rate_);
      playback_rate_ = rate_shifter_.playback_rate();
    }
    mixer_rendering_delay_ = RenderingDelay();
    // Clear start timestamp, since a pause should invalidate the start
    // timestamp anyway. The AV sync code can restart (hard correction) on
    // resume if needed.
    use_start_timestamp_ = false;
    playback_start_timestamp_ = INT64_MIN;
    filled_some_since_resume_ = false;
    if (rate_adjuster_) {
      rate_adjuster_->Reset();
      total_filled_frames_ = 0;
    }
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

bool MixerInputConnection::require_clock_rate_simulation() const {
  return enable_audio_clock_simulation_;
}

void MixerInputConnection::WritePcm(scoped_refptr<net::IOBuffer> data) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (inactivity_timer_.IsRunning()) {
    inactivity_timer_.Reset();
  }

  RenderingDelay rendering_delay;
  bool queued;
  {
    base::AutoLock lock(lock_);
    if (state_ == State::kUninitialized ||
        queued_frames_ >= max_queued_frames_) {
      if (pending_data_) {
        if (pts_is_timestamp_) {
          QueueData(std::move(pending_data_));
        } else {
          LOG(ERROR) << "Got unexpected audio data for " << this;
        }
      }
      pending_data_ = std::move(data);
      queued = false;
    } else {
      rendering_delay = QueueData(std::move(data));
      queued = true;
    }
  }

  if (queued) {
    mixer_service::Generic message;
    auto* push_result = message.mutable_push_result();
    push_result->set_delay_timestamp(rendering_delay.timestamp_microseconds);
    push_result->set_delay(rendering_delay.delay_microseconds);
    socket_->SendProto(kPushResult, message);
  }
}

MixerInputConnection::RenderingDelay MixerInputConnection::QueueData(
    scoped_refptr<net::IOBuffer> data) {
  int frames = GetFrameCount(data.get());
  if (frames == 0) {
    AUDIO_LOG(INFO) << "End of stream for " << this;
    state_ = State::kGotEos;
    SetMediaPlaybackRateLocked(1.0);
    if (!started_) {
      io_task_runner_->PostTask(FROM_HERE, ready_for_playback_task_);
    }
  } else if (pts_is_timestamp_ || started_ ||
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
      mixer_rendering_delay_.timestamp_microseconds == INT64_MIN) {
    return RenderingDelay();
  }

  RenderingDelay delay = mixer_rendering_delay_;
  delay.delay_microseconds +=
      SamplesToMicroseconds(ExtraDelayFrames(), input_samples_per_second_);
  return delay;
}

double MixerInputConnection::ExtraDelayFrames() {
  // The next playback timestamp will be the timestamp from the last mixer fill
  // request, plus the time required to play out the other data in the pipeline.
  // The other data includes:
  //   * The number of frames of the last mixer fill (since that will be played
  //     out starting at the last mixer rendering delay).
  //   * Data in the rate shifter.
  //   * Data buffered in the fader.
  //   * Queued data in |queue_|.
  // Note that the delay for data that will be pushed into the rate shifter
  // (ie, fader and queue_) needs to be adjusted for the current playback rate.
  return mixer_read_size_ + rate_shifter_.BufferedFrames() +
         ((timestamped_fader_->BufferedFrames() + queued_frames_) /
          playback_rate_);
}

void MixerInputConnection::InitializeAudioPlayback(
    int read_size,
    RenderingDelay initial_rendering_delay) {
  // Start accepting buffers into the queue.
  bool queued_data = false;
  {
    base::AutoLock lock(lock_);
    mixer_read_size_ = read_size;
    AUDIO_LOG(INFO) << this << " Mixer read size: " << read_size;
    // How many fills is >= 1 read?
    const int fills_per_read = (read_size + fill_size_ - 1) / fill_size_;
    // How many reads is >= 1 fill?
    const int reads_per_fill = (fill_size_ + read_size - 1) / read_size;
    // We need enough data to satisfy all the reads until the next fill, and
    // enough to fill enough data before the next read (in case the next read
    // happens immediately after we hit the start threshold) (+ fader buffer).
    min_start_threshold_ =
        std::max(fills_per_read * fill_size_, reads_per_fill * read_size) +
        fade_frames_;
    if (start_threshold_frames_ < min_start_threshold_) {
      start_threshold_frames_ = min_start_threshold_;
      AUDIO_LOG(INFO) << this << " Updated start threshold: "
                      << start_threshold_frames_;
    }
    if (max_queued_frames_ < start_threshold_frames_) {
      AUDIO_LOG(INFO) << this << " Boost queue size to "
                      << start_threshold_frames_ << " to allow stream to start";
      max_queued_frames_ = start_threshold_frames_;
    }
    mixer_rendering_delay_ = initial_rendering_delay;
    if (state_ == State::kUninitialized) {
      state_ = State::kNormalPlayback;
    } else {
      DCHECK_EQ(state_, State::kRemoved);
    }

    if (pending_data_ && queued_frames_ < max_queued_frames_) {
      next_delay_ = QueueData(std::move(pending_data_));
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

  if (pts_is_timestamp_) {
    remaining_silence_frames_ = 0;
    AUDIO_LOG(INFO) << "Start " << this;
    started_ = true;
    return;
  }

  const bool have_enough_queued_frames =
      (state_ == State::kGotEos || queued_frames_ >= start_threshold_frames_);
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
        base::Microseconds(drop_us), input_samples_per_second_));
    // Only start if we still have enough data to do so.
    started_ = (queued_frames_ >= start_threshold_frames_);

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
        base::Microseconds(silence_duration), input_samples_per_second_);
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
  bool post_pcm_completion = false;
  bool signal_eos = false;
  bool remove_self = false;
  {
    base::AutoLock lock(lock_);

    mixer_read_size_ = num_frames;

    // Playback start check.
    if (!started_ &&
        (state_ == State::kNormalPlayback || state_ == State::kGotEos)) {
      CheckAndStartPlaybackIfNecessary(num_frames, playback_absolute_timestamp);
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

    CHECK_LE(num_channels_, kMaxChannels);
    float* channels[kMaxChannels];
    for (int c = 0; c < num_channels_; ++c) {
      channels[c] = buffer->channel(c) + write_offset;
    }
    filled += rate_shifter_.FillFrames(num_frames, playback_absolute_timestamp,
                                       channels);

    mixer_rendering_delay_ = rendering_delay;

    // See if we can accept more data into the queue.
    if (pending_data_ && queued_frames_ < max_queued_frames_) {
      next_delay_ = QueueData(std::move(pending_data_));
      post_pcm_completion = true;
    } else if (pts_is_timestamp_) {
      next_delay_ = rendering_delay;
      next_delay_.delay_microseconds +=
          SamplesToMicroseconds(ExtraDelayFrames(), input_samples_per_second_);
    }

    // Check if we have played out EOS.
    if (state_ == State::kGotEos && queued_frames_ == 0 &&
        timestamped_fader_->BufferedFrames() == 0 &&
        rate_shifter_.BufferedFrames() == 0) {
      signal_eos = true;
      state_ = State::kSignaledEos;
    }

    // If the caller has removed this source, delete once we have faded out.
    if (state_ == State::kRemoved &&
        timestamped_fader_->BufferedFrames() == 0 &&
        rate_shifter_.BufferedFrames() == 0) {
      if (fed_one_silence_buffer_after_removal_) {
        remove_self = true;
      }
      fed_one_silence_buffer_after_removal_ = true;
    }

    if (pts_is_timestamp_ && queued_frames_ < max_queued_frames_) {
      post_pcm_completion = true;
    }
  }

  if (post_pcm_completion) {
    io_task_runner_->PostTask(FROM_HERE, pcm_completion_task_);
  }
  if (signal_eos) {
    io_task_runner_->PostTask(FROM_HERE, eos_task_);
  }

  if (remove_self) {
    base::AutoLock lock(lock_);
    RemoveSelf();
  }

  return filled;
}

int MixerInputConnection::FillAudio(int num_frames,
                                    int64_t expected_playout_time,
                                    float* const* channels,
                                    bool after_silence) {
  if (paused_ || state_ == State::kRemoved) {
    return 0;
  }

  DCHECK(channels);
  if (pts_is_timestamp_) {
    return FillTimestampedAudio(num_frames, expected_playout_time, channels,
                                after_silence);
  }

  if (!started_ || num_frames == 0) {
    return 0;
  }

  if (in_underrun_ && (queued_frames_ < min_start_threshold_)) {
    // Allow buffer to refill a bit to prevent continuous underrun.
    return 0;
  }

  int num_filled = 0;
  int frames_left = num_frames;
  while (frames_left && !queue_.empty()) {
    int copied = FillFromQueue(frames_left, channels, num_filled);
    frames_left -= copied;
    num_filled += copied;
  }

  LogUnderrun(num_frames, num_filled);
  return num_filled;
}

void MixerInputConnection::LogUnderrun(int num_frames, int filled) {
  if (filled != num_frames) {
    if (!in_underrun_ && state_ == State::kNormalPlayback) {
      AUDIO_LOG(INFO) << "Stream underrun for " << this;
      in_underrun_ = true;
      io_task_runner_->PostTask(FROM_HERE, post_stream_underrun_task_);
    }
  } else if (in_underrun_) {
    AUDIO_LOG(INFO) << "Stream underrun recovered for " << this << " with "
                    << queued_frames_ << " queued frames";
    in_underrun_ = false;
  }
}

int MixerInputConnection::FillTimestampedAudio(int num_frames,
                                               int64_t expected_playout_time,
                                               float* const* channels,
                                               bool after_silence) {
  if (expected_playout_time < 0) {
    // Invalid playout time.
    return 0;
  }
  int filled = 0;
  while (filled < num_frames && !queue_.empty()) {
    net::IOBuffer* buffer = queue_.front().get();

    const int64_t buffer_timestamp = GetTimestamp(buffer);
    const int64_t playout_time =
        expected_playout_time +
        SamplesToMicroseconds(filled / playback_rate_,
                              input_samples_per_second_);
    const int64_t desired_playout_time =
        buffer_timestamp +
        SamplesToMicroseconds(current_buffer_offset_ / playback_rate_,
                              input_samples_per_second_);

    const int64_t error =
        std::clamp(playout_time - desired_playout_time, -kTimestampErrorLimit,
                   kTimestampErrorLimit);
    if (error < -max_timestamp_error_ ||
        (after_silence &&
         error < -1e6 / (input_samples_per_second_ * playback_rate_))) {
      int64_t silence_frames =
          std::round(-error * input_samples_per_second_ * playback_rate_ / 1e6);
      if (filled_some_since_resume_) {
        AUDIO_LOG(INFO) << this << " adding " << silence_frames
                        << " frames of silence, buffer ts = "
                        << buffer_timestamp;
      }
      timestamped_fader_->AddSilence(silence_frames);
      rate_adjuster_->Reset();
      total_filled_frames_ = 0;
      return filled;  // Need to fade out.
    } else if (error > max_timestamp_error_ || (after_silence && error > 0)) {
      // Crop buffer that would be played too late.
      rate_adjuster_->Reset();
      total_filled_frames_ = 0;
      const int buffer_frames = GetFrameCount(buffer) - current_buffer_offset_;
      const int frames_to_crop =
          (never_crop_ ? 0
                       : std::round(error * input_samples_per_second_ *
                                    playback_rate_ / 1e6));
      if (frames_to_crop > 0 && filled_some_since_resume_) {
        AUDIO_LOG(INFO) << this << " cropping " << frames_to_crop
                        << " frames, buffer ts = " << buffer_timestamp;
      }
      if (frames_to_crop >= buffer_frames) {
        queued_frames_ -= buffer_frames;
        queue_.pop_front();
        current_buffer_offset_ = 0;
        if (after_silence) {
          continue;  // Continue to next buffer in the queue.
        }
      } else {
        current_buffer_offset_ += frames_to_crop;
        queued_frames_ -= frames_to_crop;
      }

      if (!after_silence && !never_crop_) {
        timestamped_fader_->AddSilence(0);
        return filled;  // Fade out.
      }
    } else {
      rate_adjuster_->AddError(
          error, SamplesToMicroseconds(total_filled_frames_ / playback_rate_,
                                       input_samples_per_second_));
    }

    int f = FillFromQueue(num_frames - filled, channels, filled);
    filled += f;
    total_filled_frames_ += f;
    after_silence = false;
  }

  if (filled_some_since_resume_) {
    LogUnderrun(num_frames, filled);
  }
  if (filled > 0) {
    if (!filled_some_since_resume_) {
      AUDIO_LOG(INFO) << "Start timestamped playback";
    }
    filled_some_since_resume_ = true;
  }
  return filled;
}

int MixerInputConnection::FillFromQueue(int num_frames,
                                        float* const* channels,
                                        int write_offset) {
  net::IOBuffer* buffer = queue_.front().get();
  const int buffer_frames = GetFrameCount(buffer);
  const int frames_to_copy =
      std::min(num_frames, buffer_frames - current_buffer_offset_);
  DCHECK(frames_to_copy >= 0 && frames_to_copy <= num_frames)
      << " frames_to_copy=" << frames_to_copy << " num_frames=" << num_frames
      << " buffer_frames=" << buffer_frames << " write_offset=" << write_offset
      << " current_buffer_offset_=" << current_buffer_offset_;

  const float* buffer_samples = GetAudioData(buffer);
  for (int c = 0; c < num_channels_; ++c) {
    const float* buffer_channel = buffer_samples + (buffer_frames * c);
    std::copy_n(buffer_channel + current_buffer_offset_, frames_to_copy,
                channels[c] + write_offset);
  }
  queued_frames_ -= frames_to_copy;

  current_buffer_offset_ += frames_to_copy;
  if (current_buffer_offset_ == buffer_frames) {
    queue_.pop_front();
    current_buffer_offset_ = 0;
  }
  return frames_to_copy;
}

void MixerInputConnection::PostPcmCompletion() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  mixer_service::Generic message;
  auto* push_result = message.mutable_push_result();
  {
    base::AutoLock lock(lock_);
    push_result->set_delay_timestamp(next_delay_.timestamp_microseconds);
    push_result->set_delay(next_delay_.delay_microseconds);
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
    RemoveSelf();
  }
}

void MixerInputConnection::RemoveSelf() {
  if (removed_self_) {
    return;
  }
  removed_self_ = true;
  mixer_->RemoveInput(this);
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
