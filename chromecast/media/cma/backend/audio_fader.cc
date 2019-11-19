// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_fader.h"

#include <algorithm>

#include "base/bits.h"
#include "base/logging.h"
#include "media/base/audio_bus.h"

namespace chromecast {
namespace media {

AudioFader::AudioFader(Source* source,
                       base::TimeDelta fade_time,
                       int num_channels,
                       int sample_rate,
                       double playback_rate)
    : AudioFader(
          source,
          std::round(fade_time.InSecondsF() * sample_rate * playback_rate),
          num_channels,
          sample_rate,
          playback_rate) {}

AudioFader::AudioFader(Source* source,
                       int fade_frames,
                       int num_channels,
                       int sample_rate,
                       double playback_rate)
    : source_(source),
      // Ensure that fade_frames_ is a multiple of 4 to keep correct alignment.
      fade_frames_(base::bits::Align(fade_frames, 4)),
      num_channels_(num_channels),
      sample_rate_(sample_rate),
      playback_rate_(playback_rate) {
  DCHECK(source_);
  DCHECK_GT(fade_frames_, 0);
  DCHECK_GT(num_channels_, 0);
  DCHECK_GT(sample_rate_, 0);

  fade_buffer_ = ::media::AudioBus::Create(num_channels, fade_frames_);
  fade_buffer_->Zero();
}

AudioFader::~AudioFader() = default;

int AudioFader::FramesNeededFromSource(int num_fill_frames) const {
  DCHECK_GE(num_fill_frames, 0);
  DCHECK_GE(fade_frames_, buffered_frames_);
  return num_fill_frames + fade_frames_ - buffered_frames_;
}

int64_t AudioFader::FramesToMicroseconds(int64_t frames) {
  return frames * base::Time::kMicrosecondsPerSecond /
         (sample_rate_ * playback_rate_);
}

int AudioFader::FillFrames(int num_frames,
                           RenderingDelay rendering_delay,
                           float* const* channel_data) {
  DCHECK(channel_data);

  int filled_frames = std::min(buffered_frames_, num_frames);
  if (filled_frames > 0) {
    for (int c = 0; c < num_channels_; ++c) {
      float* fade_channel = fade_buffer_->channel(c);
      // First, copy data from buffered_frames_.
      std::copy_n(fade_channel, filled_frames, channel_data[c]);
      // Move data in fade_buffer_ to start.
      std::copy(fade_channel + filled_frames, fade_channel + buffered_frames_,
                fade_channel);
    }

    buffered_frames_ -= filled_frames;
    num_frames -= filled_frames;
  }

  float* fill_channel_data[num_channels_];
  if (num_frames > 0) {
    // Still need more frames; ask source to fill.
    for (int c = 0; c < num_channels_; ++c) {
      fill_channel_data[c] = channel_data[c] + filled_frames;
    }
    RenderingDelay delay = rendering_delay;
    delay.delay_microseconds +=
        FramesToMicroseconds(filled_frames + buffered_frames_);
    int filled = source_->FillFaderFrames(num_frames, delay, fill_channel_data);
    filled_frames += filled;
    num_frames -= filled;
  }
  // Refill fade_buffer_ from source.
  for (int c = 0; c < num_channels_; ++c) {
    fill_channel_data[c] = fade_buffer_->channel(c) + buffered_frames_;
  }
  RenderingDelay delay = rendering_delay;
  delay.delay_microseconds +=
      FramesToMicroseconds(filled_frames + buffered_frames_);
  buffered_frames_ += source_->FillFaderFrames(fade_frames_ - buffered_frames_,
                                               delay, fill_channel_data);

  const bool complete = (num_frames == 0 && buffered_frames_ == fade_frames_);
  if (complete) {
    CompleteFill(channel_data, filled_frames);
  } else {
    IncompleteFill(channel_data, filled_frames);
  }

  return filled_frames;
}

void AudioFader::CompleteFill(float* const* channel_data, int filled_frames) {
  switch (state_) {
    case State::kSilent:
      // Fade in.
      state_ = State::kFadingIn;
      fade_frames_remaining_ = fade_frames_;
      break;
    case State::kFadingIn:
      // Continue fading in.
      break;
    case State::kPlaying:
      // Nothing to do in this case.
      return;
    case State::kFadingOut:
      // Fade back in.
      state_ = State::kFadingIn;
      fade_frames_remaining_ =
          std::max(0, fade_frames_ - fade_frames_remaining_ - 1);
      break;
  }
  FadeIn(channel_data, filled_frames);
}

void AudioFader::IncompleteFill(float* const* channel_data, int filled_frames) {
  switch (state_) {
    case State::kSilent:
      // Remain silent.
      buffered_frames_ = 0;
      for (int c = 0; c < num_channels_; ++c) {
        std::fill_n(channel_data[c], filled_frames, 0);
      }
      return;
    case State::kFadingIn:
      // Fade back out.
      state_ = State::kFadingOut;
      fade_frames_remaining_ =
          std::max(0, fade_frames_ - fade_frames_remaining_ - 1);
      break;
    case State::kPlaying:
      // Fade out.
      state_ = State::kFadingOut;
      fade_frames_remaining_ = fade_frames_;
      break;
    case State::kFadingOut:
      // Continue fading out.
      break;
  }
  FadeOut(channel_data, filled_frames);
}

void AudioFader::FadeIn(float* const* channel_data, int filled_frames) {
  DCHECK(state_ == State::kFadingIn);

  FadeInHelper(channel_data, num_channels_, filled_frames, fade_frames_,
               fade_frames_remaining_);
  fade_frames_remaining_ = std::max(0, fade_frames_remaining_ - filled_frames);

  if (fade_frames_remaining_ == 0) {
    state_ = State::kPlaying;
  }
}

// static
void AudioFader::FadeInHelper(float* const* channel_data,
                              int num_channels,
                              int filled_frames,
                              int fade_frames,
                              int fade_frames_remaining) {
  const float inverse_fade_frames = 1.0f / static_cast<float>(fade_frames);
  const int fade_limit = std::min(filled_frames, fade_frames_remaining + 1);

  for (int c = 0; c < num_channels; ++c) {
    float* channel = channel_data[c];
    for (int f = 0; f < fade_limit; ++f) {
      const float fade_multiplier =
          1.0 - (fade_frames_remaining - f) * inverse_fade_frames;
      channel[f] *= fade_multiplier;
    }
  }
}

void AudioFader::FadeOut(float* const* channel_data, int filled_frames) {
  DCHECK(state_ == State::kFadingOut);

  FadeOutHelper(channel_data, num_channels_, filled_frames, fade_frames_,
                fade_frames_remaining_);
  fade_frames_remaining_ = std::max(0, fade_frames_remaining_ - filled_frames);

  if (fade_frames_remaining_ == 0) {
    state_ = State::kSilent;
    buffered_frames_ = 0;
  }
}

// static
void AudioFader::FadeOutHelper(float* const* channel_data,
                               int num_channels,
                               int filled_frames,
                               int fade_frames,
                               int fade_frames_remaining) {
  const float inverse_fade_frames = 1.0f / static_cast<float>(fade_frames);
  const int fade_limit = std::min(filled_frames, fade_frames_remaining + 1);

  for (int c = 0; c < num_channels; ++c) {
    float* channel = channel_data[c];
    for (int f = 0; f < fade_limit; ++f) {
      const float fade_multiplier =
          (fade_frames_remaining - f) * inverse_fade_frames;
      channel[f] *= fade_multiplier;
    }
  }
  if (filled_frames > fade_frames_remaining) {
    for (int c = 0; c < num_channels; ++c) {
      std::fill_n(channel_data[c] + fade_frames_remaining,
                  filled_frames - fade_frames_remaining, 0);
    }
  }
}

}  // namespace media
}  // namespace chromecast
