// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/audio_clock_simulator.h"

#include <algorithm>
#include <cmath>

#include "base/check_op.h"
#include "base/cxx17_backports.h"

namespace chromecast {
namespace media {

constexpr int AudioClockSimulator::kInterpolateWindow;
constexpr double AudioClockSimulator::kMaxRate;
constexpr double AudioClockSimulator::kMinRate;
constexpr size_t AudioClockSimulator::kMaxChannels;

AudioClockSimulator::AudioClockSimulator(AudioProvider* provider)
    : provider_(provider),
      sample_rate_(provider_->sample_rate()),
      num_channels_(provider_->num_channels()),
      scratch_buffer_(
          CastAudioBus::Create(num_channels_, kInterpolateWindow + 1)) {
  DCHECK(provider_);
  DCHECK_GT(sample_rate_, 0);
  DCHECK_GT(num_channels_, 0u);
  DCHECK_LE(num_channels_, kMaxChannels);

  scratch_buffer_->Zero();
}

AudioClockSimulator::~AudioClockSimulator() = default;

size_t AudioClockSimulator::num_channels() const {
  return num_channels_;
}

int AudioClockSimulator::sample_rate() const {
  return sample_rate_;
}

double AudioClockSimulator::SetRate(double rate) {
  rate = base::clamp(rate, kMinRate, kMaxRate);

  if (clock_rate_ != rate) {
    clock_rate_ = rate;
    input_frames_ = 0;
    output_frames_ = 0;
  }
  return rate;
}

int AudioClockSimulator::DelayFrames() const {
  if (state_ == State::kLengthening && first_frame_filled_) {
    return 1;
  }
  return 0;
}

void AudioClockSimulator::SetSampleRate(int sample_rate) {
  sample_rate_ = sample_rate;
}

void AudioClockSimulator::SetPlaybackRate(double playback_rate) {
  playback_rate_ = playback_rate;
}

int AudioClockSimulator::FillFrames(int num_frames,
                                    int64_t playout_timestamp,
                                    float* const* channel_data) {
  int filled = 0;
  while (filled < num_frames) {
    if (state_ == State::kLengthening) {
      auto result =
          FillDataLengthen(num_frames, playout_timestamp, channel_data, filled);
      filled += result.filled;
      output_frames_ += result.filled;
      if (!result.complete) {
        break;
      }
      continue;
    }

    if (state_ == State::kShortening) {
      auto result =
          FillDataShorten(num_frames, playout_timestamp, channel_data, filled);
      filled += result.filled;
      output_frames_ += result.filled;
      if (!result.complete) {
        break;
      }
      continue;
    }

    int64_t end_input_frames = input_frames_ + kInterpolateWindow;
    int64_t end_output_frames = output_frames_ + kInterpolateWindow;
    int64_t desired_output_frames = std::round(end_input_frames / clock_rate_);

    if (end_output_frames > desired_output_frames) {
      state_ = State::kShortening;
      continue;
    } else if (end_output_frames < desired_output_frames) {
      state_ = State::kLengthening;
      continue;
    }

    DCHECK_EQ(state_, State::kPassthrough);
    float* channels[kMaxChannels];
    for (size_t c = 0; c < num_channels_; ++c) {
      channels[c] = channel_data[c] + filled;
    }
    int64_t timestamp = playout_timestamp + FramesToMicroseconds(filled);
    int desired = std::min(num_frames - filled, kInterpolateWindow);
    int provided = provider_->FillFrames(desired, timestamp, channels);
    input_frames_ += provided;
    output_frames_ += provided;
    filled += provided;

    if (provided < desired) {
      break;
    }
  }

  return filled;
}

AudioClockSimulator::FillResult AudioClockSimulator::FillDataLengthen(
    int num_frames,
    int64_t playout_timestamp,
    float* const* channel_data,
    int offset) {
  DCHECK_EQ(state_, State::kLengthening);
  DCHECK_LT(interpolate_position_, kInterpolateWindow);
  DCHECK_GE(interpolate_position_, 0);

  if (first_frame_filled_) {
    for (size_t c = 0; c < num_channels_; ++c) {
      channel_data[c][offset] = scratch_buffer_->channel(c)[0];
    }
    first_frame_filled_ = false;
    state_ = State::kPassthrough;  // Finished current interpolation window.
    return {true, 1};
  }

  int frames_for_window = kInterpolateWindow - interpolate_position_;
  int desired_fill = std::min((num_frames - offset), frames_for_window);
  float* channels[kMaxChannels];
  for (size_t c = 0; c < num_channels_; ++c) {
    // The first sample from last call is still needed, so fill starting at
    // index 1.
    channels[c] = scratch_buffer_->channel(c) + 1;
  }
  int64_t timestamp = playout_timestamp + FramesToMicroseconds(offset);
  int provided = provider_->FillFrames(desired_fill, timestamp, channels);
  if (provided == 0) {
    return {false, 0};
  }

  input_frames_ += provided;
  InterpolateLonger(provided, channel_data, offset);

  return {(provided == desired_fill), provided};
}

void AudioClockSimulator::InterpolateLonger(int num_frames,
                                            float* const* dest,
                                            int offset) {
  DCHECK_GT(num_frames, 0);
  DCHECK_LE(interpolate_position_ + num_frames, kInterpolateWindow);
  // When lengthening, we only set |first_frame_filled_| when we have finished
  // the interpolation window (indicating the extra frame is available in
  // the start of the scratch buffer).
  DCHECK(!first_frame_filled_);

  for (size_t c = 0; c < num_channels_; ++c) {
    float* source_channel = scratch_buffer_->channel(c);
    float* dest_channel = dest[c] + offset;

    for (int s = 0; s < num_frames; ++s) {
      int interpolate_point = interpolate_position_ + s;
      dest_channel[s] =
          (source_channel[s] * interpolate_point +
           source_channel[s + 1] * (kInterpolateWindow - interpolate_point)) /
          kInterpolateWindow;
    }

    source_channel[0] = source_channel[num_frames];
  }

  interpolate_position_ += num_frames;
  if (interpolate_position_ == kInterpolateWindow) {
    interpolate_position_ = 0;
    first_frame_filled_ = true;
    // Don't set state to passthrough yet, because we still need to consume the
    // last frame from the scratch buffer.
  }
}

AudioClockSimulator::FillResult AudioClockSimulator::FillDataShorten(
    int num_frames,
    int64_t playout_timestamp,
    float* const* channel_data,
    int offset) {
  DCHECK_EQ(state_, State::kShortening);
  DCHECK_LT(interpolate_position_, kInterpolateWindow);
  DCHECK_GE(interpolate_position_, 0);

  int frames_for_window = kInterpolateWindow - interpolate_position_;
  int desired_fill = std::min((num_frames - offset), frames_for_window);
  int fill_offset = 1;  // Leave the last frame of the previous step untouched.
  if (!first_frame_filled_) {
    DCHECK_EQ(interpolate_position_, 0);
    // Fill an extra frame for the first step of interpolation.
    desired_fill += 1;
    fill_offset = 0;
  }

  float* channels[kMaxChannels];
  for (size_t c = 0; c < num_channels_; ++c) {
    channels[c] = scratch_buffer_->channel(c) + fill_offset;
  }
  int64_t timestamp = playout_timestamp + FramesToMicroseconds(offset);
  int provided = provider_->FillFrames(desired_fill, timestamp, channels);
  if (provided == 0) {
    return {false, 0};
  }

  first_frame_filled_ = true;
  input_frames_ += provided;
  int output_frames = provided + fill_offset - 1;
  InterpolateShorter(output_frames, channel_data, offset);

  return {(provided == desired_fill), output_frames};
}

void AudioClockSimulator::InterpolateShorter(int num_frames,
                                             float* const* dest,
                                             int offset) {
  DCHECK_GT(num_frames, 0);
  DCHECK_LE(interpolate_position_ + num_frames, kInterpolateWindow);
  // When shortening, we must ensure that the first frame in the scratch buffer
  // is filled with valid data in all cases (including when we start the
  // interpolation window).
  DCHECK(first_frame_filled_);

  for (size_t c = 0; c < num_channels_; ++c) {
    float* source_channel = scratch_buffer_->channel(c);
    float* dest_channel = dest[c] + offset;

    for (int s = 0; s < num_frames; ++s) {
      int interpolate_point = interpolate_position_ + s + 1;
      dest_channel[s] =
          (source_channel[s] * (kInterpolateWindow - interpolate_point) +
           source_channel[s + 1] * interpolate_point) /
          kInterpolateWindow;
    }

    source_channel[0] = source_channel[num_frames];
  }

  interpolate_position_ += num_frames;
  if (interpolate_position_ == kInterpolateWindow) {
    interpolate_position_ = 0;
    first_frame_filled_ = false;
    state_ = State::kPassthrough;  // Finished current interpolation window.
  }
}

int64_t AudioClockSimulator::FramesToMicroseconds(int64_t frames) {
  return frames * 1000000 / (sample_rate_ * playback_rate_);
}

}  // namespace media
}  // namespace chromecast
