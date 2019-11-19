// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_resampler.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "media/base/decoder_buffer.h"

namespace chromecast {

namespace {
constexpr size_t kMaxChannels = 8;
}  // namespace

AudioResampler::AudioResampler(size_t channel_count)
    : channel_count_(channel_count) {
  DCHECK_LE(channel_count_, kMaxChannels);
}

scoped_refptr<media::DecoderBufferBase> AudioResampler::ResampleBuffer(
    scoped_refptr<media::DecoderBufferBase> buffer) {
  DCHECK(buffer);
  const int num_frames = buffer->data_size() / (channel_count_ * sizeof(float));
  input_frames_for_clock_rate_ += num_frames;
  int64_t expected_output_frames = output_frames_for_clock_rate_ + num_frames;
  int64_t desired_output_frames =
      input_frames_for_clock_rate_ / media_clock_rate_;

  if (expected_output_frames > desired_output_frames) {
    output_frames_for_clock_rate_ += num_frames - 1;
    return ShortenBuffer(std::move(buffer));
  } else if (expected_output_frames < desired_output_frames) {
    output_frames_for_clock_rate_ += num_frames + 1;
    return LengthenBuffer(std::move(buffer));
  }
  output_frames_for_clock_rate_ += num_frames;
  return buffer;
}

double AudioResampler::SetMediaClockRate(double rate) {
  // We are only allowed to deviate from 1.0x playback rate by this much,
  // because we only add/remove 1 frame each buffer. The buffers are typically
  // 1024 frames, and even if they're not, we limit to this rate anyway,
  // because bigger changes may start being perceptible.
  double max_deviation = 1.0 / 1024.0;

  rate = std::min(rate, 1.0 + max_deviation);
  rate = std::max(rate, 1.0 - max_deviation);

  media_clock_rate_ = rate;
  input_frames_for_clock_rate_ = 0;
  output_frames_for_clock_rate_ = 0;
  return rate;
}

scoped_refptr<media::DecoderBufferBase> AudioResampler::LengthenBuffer(
    scoped_refptr<media::DecoderBufferBase> buffer) {
  const int num_frames = buffer->data_size() / (channel_count_ * sizeof(float));
  const int new_num_frames = num_frames + 1;
  auto delayed_buffer = base::MakeRefCounted<::media::DecoderBuffer>(
      new_num_frames * (channel_count_ * sizeof(float)));

  delayed_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(buffer->timestamp()));

  const float* old_channels[kMaxChannels];
  float* new_channels[kMaxChannels];
  for (size_t c = 0; c < channel_count_; ++c) {
    old_channels[c] =
        reinterpret_cast<const float*>(buffer->data()) + c * num_frames;
    new_channels[c] =
        reinterpret_cast<float*>(delayed_buffer->writable_data()) +
        c * new_num_frames;
  }

  for (size_t c = 0; c < channel_count_; ++c) {
    new_channels[c][0] = old_channels[c][0];
    // Linearly interpolate between all n input samples to produce (n - 1)
    // samples, plus the first and last sample = (n + 1) output samples.
    for (int s = 1; s < num_frames; ++s) {
      new_channels[c][s] =
          (old_channels[c][s - 1] * s + old_channels[c][s] * (num_frames - s)) /
          num_frames;
    }
    new_channels[c][num_frames] = old_channels[c][num_frames - 1];
  }

  return base::MakeRefCounted<media::DecoderBufferAdapter>(delayed_buffer);
}

scoped_refptr<media::DecoderBufferBase> AudioResampler::ShortenBuffer(
    scoped_refptr<media::DecoderBufferBase> buffer) {
  const int num_frames = buffer->data_size() / (channel_count_ * sizeof(float));

  const int new_num_frames = num_frames - 1;
  auto cut_buffer = base::MakeRefCounted<::media::DecoderBuffer>(
      new_num_frames * (channel_count_ * sizeof(float)));

  cut_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(buffer->timestamp()));

  const float* old_channels[kMaxChannels];
  float* new_channels[kMaxChannels];
  for (size_t c = 0; c < channel_count_; ++c) {
    old_channels[c] =
        reinterpret_cast<const float*>(buffer->data()) + c * num_frames;
    new_channels[c] = reinterpret_cast<float*>(cut_buffer->writable_data()) +
                      c * new_num_frames;
  }

  DCHECK_GE(num_frames, 2);
  for (size_t c = 0; c < channel_count_; ++c) {
    // Linearly interpolate between all n input samples to produce (n - 1)
    // output samples.
    for (int s = 0; s < new_num_frames; ++s) {
      new_channels[c][s] = (old_channels[c][s] * (num_frames - (s + 1)) +
                            old_channels[c][s + 1] * (s + 1)) /
                           num_frames;
    }
  }
  return base::MakeRefCounted<media::DecoderBufferAdapter>(cut_buffer);
}

}  // namespace chromecast
