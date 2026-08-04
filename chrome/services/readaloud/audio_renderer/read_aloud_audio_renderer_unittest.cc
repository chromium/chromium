// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/audio_renderer/read_aloud_audio_renderer.h"

#include <memory>

#include "chrome/services/readaloud/audio_segment_queue.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

class ReadAloudAudioRendererTest : public testing::Test {
 protected:
  void SetUp() override {
    queue_ = std::make_unique<AudioSegmentQueue>();
    renderer_ = std::make_unique<ReadAloudAudioRenderer>();
  }

  std::unique_ptr<AudioSegmentQueue> queue_;
  std::unique_ptr<ReadAloudAudioRenderer> renderer_;
};

TEST_F(ReadAloudAudioRendererTest, LifecycleInitializeValid) {
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
  EXPECT_TRUE(params.IsValid());
  EXPECT_TRUE(renderer_->Initialize(params, queue_.get()));
}

TEST_F(ReadAloudAudioRendererTest, LifecycleInitializeInvalid) {
  // Invalid sample rate
  media::AudioParameters invalid_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 0, 480);
  EXPECT_FALSE(invalid_params.IsValid());
  EXPECT_FALSE(renderer_->Initialize(invalid_params, queue_.get()));

  // Null queue
  media::AudioParameters valid_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 480);
  EXPECT_TRUE(valid_params.IsValid());
  EXPECT_FALSE(renderer_->Initialize(valid_params, nullptr));
}

TEST_F(ReadAloudAudioRendererTest, RenderReturnsSilenceWhenQueueIsEmpty) {
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));

  auto dest = media::AudioBus::Create(params);
  // Pre-fill destination with non-zero values to ensure we can verify it gets
  // zeroed.
  for (int c = 0; c < dest->channels(); ++c) {
    for (int i = 0; i < dest->frames(); ++i) {
      dest->channel(c)[i] = 1.0f;
    }
  }

  int frames_rendered = renderer_->Render(
      /*delay=*/base::TimeDelta(),
      /*delay_timestamp=*/base::TimeTicks::Now(),
      /*glitch_info=*/media::AudioGlitchInfo(), dest.get());

  EXPECT_EQ(frames_rendered, 0);

  // Verify the destination buffer was zeroed out.
  EXPECT_TRUE(dest->AreFramesZero());
}

TEST_F(ReadAloudAudioRendererTest, RenderWithoutInitializeZeroesBuffer) {
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
  auto dest = media::AudioBus::Create(params);
  // Pre-fill destination with non-zero values.
  for (int c = 0; c < dest->channels(); ++c) {
    for (int i = 0; i < dest->frames(); ++i) {
      dest->channel(c)[i] = 1.0f;
    }
  }

  int frames_rendered = renderer_->Render(
      /*delay=*/base::TimeDelta(),
      /*delay_timestamp=*/base::TimeTicks::Now(),
      /*glitch_info=*/media::AudioGlitchInfo(), dest.get());

  EXPECT_EQ(frames_rendered, 0);

  // Verify the destination buffer was zeroed out because it is not initialized.
  EXPECT_TRUE(dest->AreFramesZero());
}

}  // namespace readaloud
