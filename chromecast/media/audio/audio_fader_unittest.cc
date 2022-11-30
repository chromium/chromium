// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>
#include <memory>

#include "chromecast/media/audio/audio_fader.h"
#include "media/base/audio_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chromecast {
namespace media {

namespace {

const int kNumChannels = 2;
const int kFadeFrames = 128;
const int kSampleRate = 48000;

std::unique_ptr<::media::AudioBus> CreateAudioBus(int num_frames) {
  auto buffer = ::media::AudioBus::Create(kNumChannels, num_frames);
  // Fill with invalid values.
  for (int c = 0; c < buffer->channels(); ++c) {
    float* channel_data = buffer->channel(c);
    std::fill_n(channel_data, num_frames, -2.0f);
  }
  return buffer;
}

class TestFaderSource : public AudioProvider {
 public:
  TestFaderSource()
      : max_fill_frames_(std::numeric_limits<int>::max()),
        total_requested_frames_(0),
        last_requested_frames_(0),
        last_filled_frames_(0) {}

  TestFaderSource(const TestFaderSource&) = delete;
  TestFaderSource& operator=(const TestFaderSource&) = delete;

  // AudioProvider implementation:
  int FillFrames(int num_frames,
                 int64_t playout_timestamp,
                 float* const* channel_data) override {
    last_requested_frames_ = num_frames;
    total_requested_frames_ += num_frames;

    int count = std::min(num_frames, max_fill_frames_);
    last_filled_frames_ = count;

    for (int c = 0; c < kNumChannels; ++c) {
      std::fill_n(channel_data[c], count, 1.0f);
    }

    return count;
  }
  size_t num_channels() const override { return kNumChannels; }
  int sample_rate() const override { return kSampleRate; }

  void set_max_fill_frames(int frames) { max_fill_frames_ = frames; }

  int total_requested_frames() const { return total_requested_frames_; }
  int last_requested_frames() const { return last_requested_frames_; }
  int last_filled_frames() const { return last_filled_frames_; }

 private:
  int max_fill_frames_;
  int total_requested_frames_;
  int last_requested_frames_;
  int last_filled_frames_;
};

}  // namespace

TEST(AudioFaderTest, Startup) {
  TestFaderSource source;
  AudioFader fader(&source, kFadeFrames, 1.0);

  // Fader has no buffered frames initially.
  EXPECT_EQ(fader.buffered_frames(), 0);

  const int kFillSize = kFadeFrames * 2;
  int frames_needed = fader.FramesNeededFromSource(kFillSize);
  // The fader should fill its internal buffer, plus the size of the request.
  EXPECT_EQ(frames_needed, kFadeFrames + kFillSize);

  auto dest = CreateAudioBus(kFillSize);
  float* channels[kNumChannels];
  for (int c = 0; c < kNumChannels; ++c) {
    channels[c] = dest->channel(c);
  }
  EXPECT_EQ(fader.FillFrames(kFillSize, 0, channels), kFillSize);

  // Test that FramesNeededFromSource() works correctly.
  EXPECT_EQ(source.total_requested_frames(), frames_needed);

  // Fader's internal buffer should be full.
  EXPECT_EQ(fader.buffered_frames(), kFadeFrames);

  // Data should be faded in.
  EXPECT_EQ(dest->channel(0)[0], 0.0f);
  EXPECT_EQ(dest->channel(0)[kFadeFrames], 1.0f);
}

TEST(AudioFaderTest, FadeInOver2Buffers) {
  TestFaderSource source;
  AudioFader fader(&source, kFadeFrames, 1.0);

  // Fader has no buffered frames initially.
  EXPECT_EQ(fader.buffered_frames(), 0);

  const int kFillSize = kFadeFrames * 2 / 3;
  int frames_needed = fader.FramesNeededFromSource(kFillSize);
  auto dest = CreateAudioBus(kFillSize);
  float* channels[kNumChannels];
  for (int c = 0; c < kNumChannels; ++c) {
    channels[c] = dest->channel(c);
  }
  EXPECT_EQ(fader.FillFrames(kFillSize, 0, channels), kFillSize);

  // Fader's internal buffer should be full.
  EXPECT_EQ(fader.buffered_frames(), kFadeFrames);
  // Data should be partially faded in.
  EXPECT_EQ(dest->channel(0)[0], 0.0f);
  EXPECT_GT(dest->channel(0)[kFillSize - 1], 0.0f);
  EXPECT_LT(dest->channel(0)[kFillSize - 1], 1.0f);

  // Fill more data.
  frames_needed += fader.FramesNeededFromSource(kFillSize);
  EXPECT_EQ(fader.FillFrames(kFillSize, 0, channels), kFillSize);
  EXPECT_EQ(fader.buffered_frames(), kFadeFrames);

  // Test that FramesNeededFromSource() works correctly.
  EXPECT_EQ(source.total_requested_frames(), frames_needed);

  // Fader's internal buffer should be full.
  EXPECT_EQ(fader.buffered_frames(), kFadeFrames);

  // Data should be faded in.
  EXPECT_EQ(dest->channel(0)[kFillSize - 1], 1.0f);
}

TEST(AudioFaderTest, ContinuePlaying) {
  TestFaderSource source;
  AudioFader fader(&source, kFadeFrames, 1.0);

  // Fader has no buffered frames initially.
  EXPECT_EQ(fader.buffered_frames(), 0);

  const int kFillSize = kFadeFrames * 2;
  auto dest = CreateAudioBus(kFillSize);

  int frames_needed = fader.FramesNeededFromSource(kFillSize);
  float* channels[kNumChannels];
  for (int c = 0; c < kNumChannels; ++c) {
    channels[c] = dest->channel(c);
  }
  EXPECT_EQ(fader.FillFrames(kFillSize, 0, channels), kFillSize);

  // Data should be faded in.
  EXPECT_EQ(dest->channel(0)[kFadeFrames], 1.0f);

  // Now request more data. Data should remain fully faded in.
  frames_needed += fader.FramesNeededFromSource(kFillSize);
  EXPECT_EQ(fader.FillFrames(kFillSize, 0, channels), kFillSize);
  EXPECT_EQ(dest->channel(0)[0], 1.0f);

  // Test that FramesNeededFromSource() works correctly.
  EXPECT_EQ(source.total_requested_frames(), frames_needed);

  // Fader's internal buffer should be full.
  EXPECT_EQ(fader.buffered_frames(), kFadeFrames);
}

TEST(AudioFaderTest, FadeOut) {
  TestFaderSource source;
  AudioFader fader(&source, kFadeFrames, 1.0);

  // Fader has no buffered frames initially.
  EXPECT_EQ(fader.buffered_frames(), 0);

  const int kFillSize = kFadeFrames * 2;
  auto dest = CreateAudioBus(kFillSize);

  int frames_needed = fader.FramesNeededFromSource(kFillSize);
  float* channels[kNumChannels];
  for (int c = 0; c < kNumChannels; ++c) {
    channels[c] = dest->channel(c);
  }
  EXPECT_EQ(fader.FillFrames(kFillSize, 0, channels), kFillSize);

  // Data should be faded in.
  EXPECT_EQ(dest->channel(0)[kFadeFrames], 1.0f);

  // Now request more data. Data should remain fully faded in.
  frames_needed += fader.FramesNeededFromSource(kFillSize);
  EXPECT_EQ(fader.FillFrames(kFillSize, 0, channels), kFillSize);
  EXPECT_EQ(dest->channel(0)[0], 1.0f);

  // Now make the source not provide enough data.
  EXPECT_GT(fader.FramesNeededFromSource(kFillSize), 0);
  source.set_max_fill_frames(0);
  frames_needed += fader.FramesNeededFromSource(kFillSize);
  int filled = fader.FillFrames(kFillSize, 0, channels);
  EXPECT_EQ(filled, kFadeFrames);

  // Test that FramesNeededFromSource() works correctly.
  EXPECT_EQ(source.total_requested_frames(), frames_needed);

  // Data should be faded out.
  EXPECT_EQ(dest->channel(0)[0], 1.0f);
  EXPECT_LT(dest->channel(0)[filled - 1], 0.1f);
  EXPECT_GE(dest->channel(0)[filled - 1], 0.0f);

  // Fader's internal buffer should be empty since we are fully faded out.
  EXPECT_EQ(fader.buffered_frames(), 0);
}

TEST(AudioFaderTest, FadeOutPartially) {
  TestFaderSource source;
  AudioFader fader(&source, kFadeFrames, 1.0);

  // Fader has no buffered frames initially.
  EXPECT_EQ(fader.buffered_frames(), 0);

  const int kFillSize = kFadeFrames * 2;
  auto dest = CreateAudioBus(kFillSize);

  int frames_needed = fader.FramesNeededFromSource(kFillSize);
  float* channels[kNumChannels];
  for (int c = 0; c < kNumChannels; ++c) {
    channels[c] = dest->channel(c);
  }
  EXPECT_EQ(fader.FillFrames(kFillSize, 0, channels), kFillSize);

  // Data should be faded in.
  EXPECT_EQ(dest->channel(0)[kFadeFrames], 1.0f);

  // Now request more data. Data should remain fully faded in.
  frames_needed += fader.FramesNeededFromSource(kFillSize);
  EXPECT_EQ(fader.FillFrames(kFillSize, 0, channels), kFillSize);
  EXPECT_EQ(dest->channel(0)[0], 1.0f);

  // Now make the source not provide enough data.
  EXPECT_GT(fader.FramesNeededFromSource(kFillSize), 0);
  source.set_max_fill_frames(0);
  frames_needed += fader.FramesNeededFromSource(kFadeFrames / 3);
  int filled = fader.FillFrames(kFadeFrames / 3, 0, channels);
  EXPECT_EQ(filled, kFadeFrames / 3);

  // Data should be partially faded out.
  EXPECT_EQ(dest->channel(0)[0], 1.0f);
  EXPECT_LT(dest->channel(0)[filled - 1], 1.0f);
  EXPECT_GE(dest->channel(0)[filled - 1], 0.0f);
  float fade_min = dest->channel(0)[filled - 1];

  // Fader's internal buffer should be partially full.
  EXPECT_LT(fader.buffered_frames(), kFadeFrames);

  // Now let the source provide data again.
  source.set_max_fill_frames(std::numeric_limits<int>::max());
  frames_needed += fader.FramesNeededFromSource(kFillSize);
  EXPECT_EQ(fader.FillFrames(kFillSize, 0, channels), kFillSize);
  // Data should fade back in from the point it faded out to.
  EXPECT_GE(dest->channel(0)[0], fade_min);
  EXPECT_EQ(dest->channel(0)[kFillSize - 1], 1.0f);

  // Test that FramesNeededFromSource() works correctly.
  EXPECT_EQ(source.total_requested_frames(), frames_needed);

  // Fader's internal buffer should be full.
  EXPECT_EQ(fader.buffered_frames(), kFadeFrames);
}

TEST(AudioFaderTest, IncompleteFadeIn) {
  TestFaderSource source;
  AudioFader fader(&source, kFadeFrames, 1.0);

  // Fader has no buffered frames initially.
  EXPECT_EQ(fader.buffered_frames(), 0);

  const int kFillSize = kFadeFrames * 2;
  int frames_needed = fader.FramesNeededFromSource(kFillSize);

  // The source only partially fills the fader request. Since we're fading in
  // from silence, the fader should output silence.
  auto dest = CreateAudioBus(kFillSize);
  source.set_max_fill_frames(10);
  float* channels[kNumChannels];
  for (int c = 0; c < kNumChannels; ++c) {
    channels[c] = dest->channel(c);
  }
  int filled = fader.FillFrames(kFillSize, 0, channels);

  // Test that FramesNeededFromSource() works correctly.
  EXPECT_EQ(source.total_requested_frames(), frames_needed);

  // Fader's internal buffer should be empty.
  EXPECT_EQ(fader.buffered_frames(), 0);

  // Data should be silent.
  for (int i = 0; i < filled; ++i) {
    EXPECT_EQ(dest->channel(0)[i], 0.0f);
  }
}

TEST(AudioFaderTest, FadeInPartially) {
  TestFaderSource source;
  AudioFader fader(&source, kFadeFrames, 1.0);

  // Fader has no buffered frames initially.
  EXPECT_EQ(fader.buffered_frames(), 0);

  const int kFillSize = kFadeFrames * 2 / 3;

  int frames_needed = fader.FramesNeededFromSource(kFillSize);
  auto dest = CreateAudioBus(kFillSize);
  float* channels[kNumChannels];
  for (int c = 0; c < kNumChannels; ++c) {
    channels[c] = dest->channel(c);
  }
  EXPECT_EQ(fader.FillFrames(kFillSize, 0, channels), kFillSize);

  // Fader's internal buffer should be full.
  EXPECT_EQ(fader.buffered_frames(), kFadeFrames);

  // Data should be partially faded in.
  EXPECT_EQ(dest->channel(0)[0], 0.0f);
  EXPECT_GT(dest->channel(0)[kFillSize - 1], 0.0f);
  EXPECT_LT(dest->channel(0)[kFillSize - 1], 1.0f);
  float fade_max = dest->channel(0)[kFillSize - 1];

  // Now tell the source not to provide any data. The fader output should fade
  // back out to silence.
  source.set_max_fill_frames(0);
  frames_needed += fader.FramesNeededFromSource(kFillSize);
  int filled = fader.FillFrames(kFillSize, 0, channels);

  // Data should be faded out.
  EXPECT_LE(dest->channel(0)[0], fade_max);
  EXPECT_GE(dest->channel(0)[0], 0.0f);
  EXPECT_EQ(dest->channel(0)[filled - 1], 0.0f);

  // Test that FramesNeededFromSource() works correctly.
  EXPECT_EQ(source.total_requested_frames(), frames_needed);

  // Fader's internal buffer should be empty.
  EXPECT_EQ(fader.buffered_frames(), 0);
}

}  // namespace media
}  // namespace chromecast
