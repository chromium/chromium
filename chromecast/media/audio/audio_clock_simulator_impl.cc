// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "chromecast/media/api/audio_clock_simulator.h"
#include "chromecast/media/api/audio_provider.h"
#include "media/base/audio_bus.h"
#include "media/base/multi_channel_resampler.h"

namespace chromecast {
namespace media {

namespace {
constexpr size_t kMaxChannels = 32;
constexpr size_t kDefaultResampleBufferFrames = 2048;

class AudioClockSimulatorImpl : public AudioClockSimulator {
 public:
  explicit AudioClockSimulatorImpl(AudioProvider* provider)
      : provider_(provider),
        sample_rate_(provider_->sample_rate()),
        num_channels_(provider_->num_channels()) {
    DCHECK(provider_);
    DCHECK_GT(sample_rate_, 0);
    DCHECK_GT(num_channels_, 0u);
    DCHECK_LE(num_channels_, kMaxChannels);

    resample_buffer_ =
        ::media::AudioBus::Create(num_channels_, kDefaultResampleBufferFrames);
    resampler_ = std::make_unique<::media::MultiChannelResampler>(
        num_channels_, 1.0, ::media::SincResampler::kSmallRequestSize,
        base::BindRepeating(&AudioClockSimulatorImpl::ResamplerReadCallback,
                            base::Unretained(this)));
    resampler_->PrimeWithSilence();
  }

  ~AudioClockSimulatorImpl() override = default;

  AudioClockSimulatorImpl(const AudioClockSimulatorImpl&) = delete;
  AudioClockSimulatorImpl& operator=(const AudioClockSimulatorImpl&) = delete;

  // AudioClockSimulator implementation:
  double SetRate(double rate) override {
    if (in_fill_) {
      pending_rate_ = rate;
      return rate;
    }
    if (rate != clock_rate_) {
      clock_rate_ = rate;
      resampler_->SetRatio(clock_rate_);
    }
    return rate;
  }

  double DelayFrames() const override {
    return resampler_->BufferedFrames() + resampler_->KernelSize() / 2;
  }

  void SetSampleRate(int sample_rate) override { sample_rate_ = sample_rate; }

  void SetPlaybackRate(double playback_rate) override {
    playback_rate_ = playback_rate;
  }

  int FillFrames(int num_frames,
                 int64_t playout_timestamp,
                 float* const* channel_data) override {
    if (num_frames > resample_buffer_->frames()) {
      resample_buffer_ =
          ::media::AudioBus::Create(num_channels_, num_frames * 2);
    }
    request_timestamp_ = playout_timestamp;
    // resampler_->BufferedFrames() gives incorrect values in the read callback,
    // so track the number of buffered frames ourselves.
    resampler_buffered_frames_ = DelayFrames();
    in_fill_ = true;
    resampler_->Resample(num_frames, resample_buffer_.get());
    in_fill_ = false;
    if (pending_rate_.has_value()) {
      SetRate(pending_rate_.value());
      pending_rate_.reset();
    }
    for (size_t c = 0; c < num_channels_; ++c) {
      std::copy_n(resample_buffer_->channel(c), num_frames, channel_data[c]);
    }
    return num_frames;
  }

  size_t num_channels() const override { return num_channels_; }

  int sample_rate() const override { return sample_rate_; }

 private:
  void ResamplerReadCallback(int frame_delay, ::media::AudioBus* output) {
    float* channels[kMaxChannels];
    for (size_t c = 0; c < num_channels_; ++c) {
      channels[c] = output->channel(c);
    }

    int64_t timestamp =
        request_timestamp_ + FramesToMicroseconds(resampler_buffered_frames_);
    const int needed_frames = output->frames();
    int filled = provider_->FillFrames(output->frames(), timestamp, channels);
    if (filled < needed_frames) {
      output->ZeroFramesPartial(filled, needed_frames - filled);
    }
    resampler_buffered_frames_ += output->frames();
  }

  int64_t FramesToMicroseconds(double frames) {
    return std::round(frames * 1000000 / (sample_rate_ * playback_rate_));
  }

  AudioProvider* const provider_;
  int sample_rate_;
  const size_t num_channels_;
  double playback_rate_ = 1.0;

  double clock_rate_ = 1.0;

  std::unique_ptr<::media::AudioBus> resample_buffer_;
  std::unique_ptr<::media::MultiChannelResampler> resampler_;

  int64_t request_timestamp_ = 0;
  double resampler_buffered_frames_ = 0.0;
  bool in_fill_ = false;
  std::optional<double> pending_rate_;
};

}  // namespace

// static
std::unique_ptr<AudioClockSimulator> AudioClockSimulator::Create(
    AudioProvider* provider) {
  return std::make_unique<AudioClockSimulatorImpl>(provider);
}

}  // namespace media
}  // namespace chromecast
