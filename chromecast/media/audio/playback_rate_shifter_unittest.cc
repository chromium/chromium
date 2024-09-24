// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/playback_rate_shifter.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/types/fixed_array.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {
constexpr int kSampleRate = 48000;
constexpr int64_t kMaxTimestampError = 50;
constexpr int kReadSize = 512;
}  // namespace

class PlaybackRateShifterTest : public testing::Test, public AudioProvider {
 public:
  PlaybackRateShifterTest()
      : rate_shifter_(this,
                      ::media::CHANNEL_LAYOUT_MONO,
                      1,
                      kSampleRate,
                      kReadSize) {
    srand(12345);
  }

  ~PlaybackRateShifterTest() override = default;

  int FillFrames(int num_frames,
                 int64_t playout_timestamp,
                 float* const* channel_data) override {
    if (expected_playout_timestamp_ != INT64_MIN) {
      int64_t error = playout_timestamp - expected_playout_timestamp_;
      EXPECT_LT(std::abs(error), kMaxTimestampError)
          << "Playout timestamp error from BufferedFrames() is " << error;
      expected_playout_timestamp_ = INT64_MIN;
    }

    if (next_playout_timestamp_ != INT64_MIN) {
      int64_t error = playout_timestamp - next_playout_timestamp_;
      EXPECT_LT(std::abs(error), kMaxTimestampError)
          << "Playout timestamp error from FillFrames() is " << error;
    }

    next_playout_timestamp_ =
        playout_timestamp +
        FramesToTime(num_frames) / rate_shifter_.playback_rate();
    float* channel = channel_data[0];
    for (int i = 0; i < num_frames; ++i) {
      channel[i] = base::RandDouble();
    }
    filled_ += num_frames;
    return num_frames;
  }

  size_t num_channels() const override { return 1; }
  int sample_rate() const override { return kSampleRate; }

  void Read(int num_frames) {
    int64_t request_timestamp = FramesToTime(num_read_);
    double buffered = rate_shifter_.BufferedFrames();
    int64_t filled_before = filled_;
    // No more than 50ms of buffer (plus 1 read).
    EXPECT_LT(buffered,
              (kSampleRate / 20 + kReadSize) / rate_shifter_.playback_rate());

    expected_playout_timestamp_ = request_timestamp + FramesToTime(buffered);
    base::FixedArray<float> buffer(num_frames);
    float* data[1] = {buffer.data()};
    int read = rate_shifter_.FillFrames(num_frames, request_timestamp, data);
    EXPECT_EQ(read, num_frames);
    num_read_ += read;

    int64_t filled_during_read = filled_ - filled_before;
    int64_t expected_next_playout_timestamp =
        request_timestamp +
        FramesToTime(buffered +
                     filled_during_read / rate_shifter_.playback_rate());
    int64_t next_playout_timestamp =
        FramesToTime(num_read_ + rate_shifter_.BufferedFrames());
    int64_t error = expected_next_playout_timestamp - next_playout_timestamp;
    EXPECT_LT(std::abs(error), kMaxTimestampError)
        << "Playout timestamp error before/after read is " << error;
  }

  void ReadAll(int total_frames) {
    int read = 0;
    while (read < total_frames) {
      int frames = (rand() % 32 + 8) * 16;
      Read(frames);
      read += frames;
    }
  }

  int64_t FramesToTime(int64_t frames) {
    return frames * 1000000 / kSampleRate;
  }

  void SetPlaybackRate(double rate) {
    LOG(INFO) << "Set rate to " << rate;
    rate_shifter_.SetPlaybackRate(rate);
    next_playout_timestamp_ = INT64_MIN;
  }

 protected:
  PlaybackRateShifter rate_shifter_;

  int64_t next_playout_timestamp_ = INT64_MIN;
  int64_t num_read_ = 0;
  int64_t filled_ = 0;

  int64_t expected_playout_timestamp_ = INT64_MIN;
  int64_t next_expected_playout_timestamp_ = INT64_MIN;
};

TEST_F(PlaybackRateShifterTest, Normal) {
  SetPlaybackRate(1.0);
  ReadAll(kSampleRate);
}

TEST_F(PlaybackRateShifterTest, Slower) {
  SetPlaybackRate(0.7);
  ReadAll(kSampleRate);
}

TEST_F(PlaybackRateShifterTest, Slower2) {
  SetPlaybackRate(0.5);
  ReadAll(kSampleRate);
}

TEST_F(PlaybackRateShifterTest, Faster) {
  SetPlaybackRate(1.5);
  ReadAll(kSampleRate);
}

TEST_F(PlaybackRateShifterTest, Faster2) {
  SetPlaybackRate(2.0);
  ReadAll(kSampleRate);
}

TEST_F(PlaybackRateShifterTest, IncreaseTo1) {
  SetPlaybackRate(0.7);
  ReadAll(kSampleRate);
  SetPlaybackRate(1.0);
  ReadAll(kSampleRate / 2);
}

TEST_F(PlaybackRateShifterTest, IncreaseFrom1) {
  SetPlaybackRate(1.0);
  ReadAll(kSampleRate);
  SetPlaybackRate(1.5);
  ReadAll(kSampleRate / 2);
}

TEST_F(PlaybackRateShifterTest, DecreaseTo1) {
  SetPlaybackRate(1.3);
  ReadAll(kSampleRate);
  SetPlaybackRate(1.0);
  ReadAll(kSampleRate / 2);
}

TEST_F(PlaybackRateShifterTest, DecreaseFrom1) {
  SetPlaybackRate(1.0);
  ReadAll(kSampleRate);
  SetPlaybackRate(0.6);
  ReadAll(kSampleRate / 2);
}

TEST_F(PlaybackRateShifterTest, Increasing) {
  SetPlaybackRate(0.5);
  ReadAll(kSampleRate);
  SetPlaybackRate(0.7);
  ReadAll(kSampleRate);
  SetPlaybackRate(0.99);
  ReadAll(kSampleRate);
  SetPlaybackRate(1.0);
  ReadAll(kSampleRate);
  SetPlaybackRate(1.01);
  ReadAll(kSampleRate);
  SetPlaybackRate(1.4);
  ReadAll(kSampleRate);
  SetPlaybackRate(2.0);
  ReadAll(kSampleRate);
}

TEST_F(PlaybackRateShifterTest, Decreasing) {
  SetPlaybackRate(2.0);
  ReadAll(kSampleRate);
  SetPlaybackRate(1.6);
  ReadAll(kSampleRate);
  SetPlaybackRate(1.01);
  ReadAll(kSampleRate);
  SetPlaybackRate(1.0);
  ReadAll(kSampleRate);
  SetPlaybackRate(0.99);
  ReadAll(kSampleRate);
  SetPlaybackRate(0.64);
  ReadAll(kSampleRate);
  SetPlaybackRate(0.5);
  ReadAll(kSampleRate);
}

}  // namespace media
}  // namespace chromecast
