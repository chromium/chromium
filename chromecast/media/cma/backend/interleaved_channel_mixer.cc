// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/interleaved_channel_mixer.h"

#include "base/logging.h"
#include "media/base/channel_mixing_matrix.h"

namespace chromecast {
namespace media {

InterleavedChannelMixer::InterleavedChannelMixer(
    ::media::ChannelLayout input_layout,
    ::media::ChannelLayout output_layout,
    int max_frames)
    : input_layout_(input_layout),
      input_channel_count_(::media::ChannelLayoutToChannelCount(input_layout_)),
      output_layout_(output_layout),
      output_channel_count_(
          ::media::ChannelLayoutToChannelCount(output_layout_)),
      max_frames_(max_frames) {
  if (input_layout_ == output_layout_) {
    return;
  }

  buffer_.resize(max_frames * output_channel_count_);

  std::vector<std::vector<float>> matrix;
  ::media::ChannelMixingMatrix matrix_builder(
      input_layout_, input_channel_count_, output_layout_,
      output_channel_count_);
  matrix_builder.CreateTransformationMatrix(&matrix);

  transform_.reserve(input_channel_count_ * output_channel_count_);
  for (const std::vector<float>& output_channel : matrix) {
    transform_.insert(transform_.end(), output_channel.begin(),
                      output_channel.end());
  }
}

InterleavedChannelMixer::~InterleavedChannelMixer() = default;

float* InterleavedChannelMixer::Transform(const float* input, int num_frames) {
  if (input_layout_ == output_layout_) {
    return const_cast<float*>(input);
  }

  DCHECK_LE(num_frames, max_frames_);
  // TODO(kmackay) Could use Eigen, but it's not available in public Chromium.
  float* output = buffer_.data();
  for (int f = 0; f < num_frames; ++f) {
    // For each frame, multiply the row-major transform matrix by the column-
    // major interleaved input.
    float* t = transform_.data();
    for (int out_c = 0; out_c < output_channel_count_; ++out_c) {
      // Each channel of the output frame is the current transform row times
      // the input frame.
      float result = 0;
      for (int in_c = 0; in_c < input_channel_count_; ++in_c) {
        result += *t * input[in_c];
        ++t;
      }
      *output = result;
      ++output;
    }
    // Move to next input frame.
    input += input_channel_count_;
  }

  return buffer_.data();
}

}  // namespace media
}  // namespace chromecast
