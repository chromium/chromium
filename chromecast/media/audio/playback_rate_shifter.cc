// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/playback_rate_shifter.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/check.h"
#include "base/time/time.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/filters/audio_renderer_algorithm.h"

namespace chromecast {
namespace media {

namespace {
constexpr int kMaxChannels = 32;
constexpr double kPlaybackRateEpsilon = 0.005;
constexpr int kOutputBufferSize = 4096;
}  // namespace

PlaybackRateShifter::PlaybackRateShifter(AudioProvider* provider,
                                         ::media::ChannelLayout channel_layout,
                                         int num_channels,
                                         int sample_rate,
                                         int request_size)
    : provider_(provider),
      channel_layout_(channel_layout),
      num_channels_(num_channels),
      sample_rate_(sample_rate),
      request_size_(request_size),
      audio_buffer_pool_(
          base::MakeRefCounted<::media::AudioBufferMemoryPool>()) {
  DCHECK(provider_);
}

PlaybackRateShifter::~PlaybackRateShifter() = default;

double PlaybackRateShifter::BufferedFrames() const {
  if (rate_shifter_) {
    return rate_shifter_->DelayInFrames(playback_rate_);
  }
  return 0;
}

void PlaybackRateShifter::SetPlaybackRate(double rate) {
  if (std::fabs(rate - 1.0) < kPlaybackRateEpsilon) {
    rate = 1.0;
  }

  if (rate == playback_rate_) {
    return;
  }

  if (rate != 1.0) {
    if (!rate_shifter_) {
      rate_shifter_ =
          std::make_unique<::media::AudioRendererAlgorithm>(&media_log_);
      ::media::AudioParameters parameters(
          ::media::AudioParameters::AUDIO_PCM_LINEAR,
          {channel_layout_, static_cast<int>(num_channels_)}, sample_rate_,
          request_size_);
      rate_shifter_->Initialize(parameters, false /* is_encrypted */);
    }

    if (!rate_shifter_output_) {
      rate_shifter_output_ =
          ::media::AudioBus::Create(num_channels_, kOutputBufferSize);
    }
  }

  playback_rate_ = rate;
}

int PlaybackRateShifter::FillFrames(int num_frames,
                                    int64_t playout_timestamp,
                                    float* const* channel_data) {
  if (!rate_shifter_ ||
      (playback_rate_ == 1.0 && rate_shifter_->BufferedFrames() == 0)) {
    return provider_->FillFrames(num_frames, playout_timestamp, channel_data);
  }

  if (playback_rate_ == 1.0) {
    return DrainBufferedData(num_frames, playout_timestamp, channel_data);
  }

  int total_filled = 0;
  while (total_filled < num_frames) {
    int amount = std::min(num_frames - total_filled, kOutputBufferSize);
    int filled = rate_shifter_->FillBuffer(rate_shifter_output_.get(), 0,
                                           amount, playback_rate_);

    for (size_t c = 0; c < num_channels_; ++c) {
      std::copy_n(rate_shifter_output_->channel(c), amount,
                  channel_data[c] + total_filled);
    }
    total_filled += filled;

    if (filled != amount) {
      // Get more data and queue it in the rate shifter.
      auto buffer = ::media::AudioBuffer::CreateBuffer(
          ::media::SampleFormat::kSampleFormatPlanarF32, channel_layout_,
          num_channels_, sample_rate_, request_size_, audio_buffer_pool_);
      int new_fill = provider_->FillFrames(
          request_size_,
          playout_timestamp +
              FramesToMicroseconds(total_filled + BufferedFrames()),
          const_cast<float**>(
              reinterpret_cast<float* const*>(buffer->channel_data().data())));
      if (new_fill == 0) {
        break;
      }
      buffer->TrimEnd(request_size_ - new_fill);
      rate_shifter_->EnqueueBuffer(std::move(buffer));
    }
  }
  return total_filled;
}

int PlaybackRateShifter::DrainBufferedData(int num_frames,
                                           int64_t playout_timestamp,
                                           float* const* channel_data) {
  // Drain buffered data from rate shifter.
  DCHECK_EQ(playback_rate_, 1.0);

  int filled = 0;
  while (filled < num_frames) {
    int amount = std::min(num_frames - filled, kOutputBufferSize);
    int to_copy = rate_shifter_->FillBuffer(rate_shifter_output_.get(), 0,
                                            amount, playback_rate_);
    for (size_t c = 0; c < num_channels_; ++c) {
      std::copy_n(rate_shifter_output_->channel(c), to_copy,
                  channel_data[c] + filled);
    }
    filled += to_copy;

    if (to_copy < amount) {
      break;
    }
  }

  if (filled < num_frames) {
    // Now there is no data buffered in the rate shifter.
    float* fill_channel_data[kMaxChannels];
    for (size_t c = 0; c < num_channels_; ++c) {
      fill_channel_data[c] = channel_data[c] + filled;
    }
    int64_t timestamp = playout_timestamp + FramesToMicroseconds(filled);
    filled += provider_->FillFrames(num_frames - filled, timestamp,
                                    fill_channel_data);
  }
  return filled;
}

size_t PlaybackRateShifter::num_channels() const {
  return num_channels_;
}

int PlaybackRateShifter::sample_rate() const {
  return sample_rate_;
}

int64_t PlaybackRateShifter::FramesToMicroseconds(double frames) {
  return frames * base::Time::kMicrosecondsPerSecond / sample_rate_;
}

}  // namespace media
}  // namespace chromecast
