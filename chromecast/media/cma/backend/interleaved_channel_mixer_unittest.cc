// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <string>
#include <tuple>

#include "base/logging.h"
#include "chromecast/media/cma/backend/interleaved_channel_mixer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "media/base/channel_mixer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {
const int kNumFrames = 32;
}  // namespace

using TestParams = std::tuple<::media::ChannelLayout /* input layout */,
                              ::media::ChannelLayout /* output layout */>;

class InterleavedChannelMixerTest : public testing::TestWithParam<TestParams> {
 public:
  InterleavedChannelMixerTest() = default;
  ~InterleavedChannelMixerTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(InterleavedChannelMixerTest);
};

TEST_P(InterleavedChannelMixerTest, Transform) {
  const TestParams& params = GetParam();
  const ::media::ChannelLayout input_layout = testing::get<0>(params);
  const ::media::ChannelLayout output_layout = testing::get<1>(params);
  const int num_input_channels =
      ::media::ChannelLayoutToChannelCount(input_layout);
  const int num_output_channels =
      ::media::ChannelLayoutToChannelCount(output_layout);

  auto original = ::media::AudioBus::Create(num_input_channels, kNumFrames);
  for (int c = 0; c < num_input_channels; ++c) {
    for (int f = 0; f < kNumFrames; ++f) {
      original->channel(c)[f] = std::pow(-1, f + c) * 0.01 +
                                c / static_cast<float>(num_input_channels * 10);
    }
  }

  auto transformed = ::media::AudioBus::Create(num_output_channels, kNumFrames);
  transformed->Zero();

  // Check that the output of upstream ChannelMixer + interleave is the same
  // as the output of interleave + InterleavedChannelMixer.
  ::media::ChannelMixer channel_mixer(input_layout, output_layout);
  channel_mixer.Transform(original.get(), transformed.get());

  std::vector<float> original_interleaved(num_input_channels * kNumFrames);
  original->ToInterleaved<::media::Float32SampleTypeTraits>(
      kNumFrames, original_interleaved.data());

  InterleavedChannelMixer interleaved_mixer(input_layout, output_layout,
                                            kNumFrames);
  float* interleaved_mixed =
      interleaved_mixer.Transform(original_interleaved.data(), kNumFrames);

  std::vector<float> transformed_interleaved(num_output_channels * kNumFrames);
  transformed->ToInterleaved<::media::Float32SampleTypeTraits>(
      kNumFrames, transformed_interleaved.data());

  for (int f = 0; f < kNumFrames; ++f) {
    for (int c = 0; c < num_output_channels; ++c) {
      EXPECT_FLOAT_EQ(interleaved_mixed[f * num_output_channels + c],
                      transformed_interleaved[f * num_output_channels + c])
          << "at frame " << f << ", channel " << c;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    CommonLayouts,
    InterleavedChannelMixerTest,
    testing::Combine(
        ::testing::Values(::media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                          ::media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                          ::media::ChannelLayout::CHANNEL_LAYOUT_5_1),
        ::testing::Values(::media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                          ::media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                          ::media::ChannelLayout::CHANNEL_LAYOUT_5_1)));

}  // namespace media
}  // namespace chromecast
