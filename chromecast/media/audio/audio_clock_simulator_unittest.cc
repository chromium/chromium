// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <tuple>

#include "base/check_op.h"
#include "base/logging.h"
#include "chromecast/media/audio/audio_clock_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace chromecast {
namespace media {

namespace {

constexpr int kSampleRate = 48000;
constexpr size_t kDefaultChannels = 1;

constexpr int kBufferSize = 4096;

int64_t FramesToTime(int64_t frames, int sample_rate) {
  return frames * 1000000 / sample_rate;
}

class FakeAudioProvider : public AudioProvider {
 public:
  explicit FakeAudioProvider(size_t num_channels)
      : num_channels_(num_channels) {
    DCHECK_GT(num_channels_, 0u);
    ON_CALL(*this, FillFrames)
        .WillByDefault(Invoke(this, &FakeAudioProvider::FillFramesImpl));
  }

  // AudioProvider implementation:
  MOCK_METHOD(int, FillFrames, (int, int64_t, float* const*));
  size_t num_channels() const override { return num_channels_; }
  int sample_rate() const override { return kSampleRate; }

  int FillFramesImpl(int num_frames,
                     int64_t playout_timestamp,
                     float* const* channel_data) {
    for (int f = 0; f < num_frames; ++f) {
      for (size_t c = 0; c < num_channels_; ++c) {
        channel_data[c][f] = static_cast<float>(next_ + f);
      }
    }
    next_ += num_frames;
    return num_frames;
  }

  int consumed() { return next_; }

 private:
  const size_t num_channels_;
  int next_ = 0;
};

using TestParams =
    std::tuple<int /* input request_size */, double /* clock_rate */>;

}  // namespace

class AudioClockSimulatorTest : public testing::TestWithParam<TestParams> {
 public:
  AudioClockSimulatorTest() = default;
  ~AudioClockSimulatorTest() override = default;
};

TEST_P(AudioClockSimulatorTest, Fill) {
  const TestParams& params = GetParam();
  const int request_size = testing::get<0>(params);
  const double clock_rate = testing::get<1>(params);
  LOG(INFO) << "Request size = " << request_size
            << ", clock rate = " << clock_rate;
  NiceMock<FakeAudioProvider> provider(kDefaultChannels);
  AudioClockSimulator clock(&provider);
  if (request_size > kBufferSize) {
    return;
  }

  EXPECT_EQ(clock.SetRate(clock_rate), clock_rate);

  float output[kBufferSize];
  std::fill_n(output, kBufferSize, 0);
  float* test_data[1] = {output};
  int i;
  for (i = 0; i + request_size <= kBufferSize; i += request_size) {
    test_data[0] = output + i;
    int64_t timestamp = FramesToTime(i, kSampleRate);

    EXPECT_CALL(provider, FillFrames(_, _, _)).Times(testing::AnyNumber());
    // Timestamp for requests to provider should not be before current fill
    // timestamp.
    EXPECT_CALL(provider, FillFrames(_, testing::Lt(timestamp), _)).Times(0);
    int provided = clock.FillFrames(request_size, timestamp, test_data);
    EXPECT_EQ(provided, request_size);

    int delay = clock.DelayFrames();
    EXPECT_GE(delay, 0);
    EXPECT_LE(delay, 1);
    testing::Mock::VerifyAndClearExpectations(&provider);
  }
  int leftover = kBufferSize - i;
  if (leftover > 0) {
    test_data[0] = output + kBufferSize - leftover;
    int64_t timestamp = FramesToTime(i, kSampleRate);

    EXPECT_CALL(provider, FillFrames(_, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(provider, FillFrames(_, testing::Lt(timestamp), _)).Times(0);
    int provided = clock.FillFrames(leftover, timestamp, test_data);
    EXPECT_EQ(provided, leftover);

    int delay = clock.DelayFrames();
    EXPECT_GE(delay, 0);
    EXPECT_LE(delay, 1);
  }

  if (clock_rate < 1.0) {
    EXPECT_LE(provider.consumed(), kBufferSize);
    if (clock_rate == AudioClockSimulator::kMinRate) {
      int windows = kBufferSize / (AudioClockSimulator::kInterpolateWindow + 1);
      int extra = kBufferSize % (AudioClockSimulator::kInterpolateWindow + 1);
      EXPECT_EQ(provider.consumed(),
                windows * AudioClockSimulator::kInterpolateWindow + extra);
    }
  } else if (clock_rate == 1.0) {
    EXPECT_EQ(provider.consumed(), kBufferSize);
  } else {
    EXPECT_GE(provider.consumed(), kBufferSize);
    if (clock_rate == AudioClockSimulator::kMaxRate) {
      int windows = kBufferSize / AudioClockSimulator::kInterpolateWindow;
      int extra = kBufferSize % AudioClockSimulator::kInterpolateWindow;
      EXPECT_EQ(
          provider.consumed(),
          windows * (AudioClockSimulator::kInterpolateWindow + 1) + extra);
    }
  }

  for (int f = 0; f < kBufferSize - 1; ++f) {
    EXPECT_LT(output[f], output[f + 1]);
    float diff = output[f + 1] - output[f];
    EXPECT_GE(diff, 1.0 - 1.0 / AudioClockSimulator::kInterpolateWindow);
    EXPECT_LE(diff, 1.0 + 1.0 / AudioClockSimulator::kInterpolateWindow);
  }
}

INSTANTIATE_TEST_SUITE_P(
    RequestSizes,
    AudioClockSimulatorTest,
    testing::Combine(
        ::testing::Values(1,
                          2,
                          100,
                          AudioClockSimulator::kInterpolateWindow - 1,
                          AudioClockSimulator::kInterpolateWindow,
                          AudioClockSimulator::kInterpolateWindow + 1,
                          AudioClockSimulator::kInterpolateWindow + 100),
        ::testing::Values(1.0,
                          AudioClockSimulator::kMinRate,
                          AudioClockSimulator::kMaxRate,
                          (1.0 + AudioClockSimulator::kMinRate) / 2,
                          (1.0 + AudioClockSimulator::kMaxRate) / 2)));

TEST(AudioClockSimulatorTest2, RateChange) {
  NiceMock<FakeAudioProvider> provider(kDefaultChannels);
  AudioClockSimulator clock(&provider);

  float output[kBufferSize];
  std::fill_n(output, kBufferSize, 0);
  int index = 0;
  float* test_data[1] = {output};

  // First, some passthrough data.
  int consumed = 0;
  int requested = 100;
  int provided = clock.FillFrames(requested, 0, test_data);
  EXPECT_EQ(provided, requested);
  EXPECT_EQ(provider.consumed() - consumed, requested);
  consumed = provider.consumed();
  index += requested;

  // Change clock rate. When switching from passthrough, the rate change takes
  // effect immediately.
  clock.SetRate(AudioClockSimulator::kMinRate);

  test_data[0] = output + index;
  requested = 100;
  provided = clock.FillFrames(requested, 0, test_data);
  EXPECT_EQ(provided, requested);
  index += requested;

  // Change clock rate again. The new rate doesn't take effect until the
  // interpolation window is complete.
  clock.SetRate(AudioClockSimulator::kMaxRate);
  test_data[0] = output + index;
  requested = AudioClockSimulator::kInterpolateWindow + 1 - 100;
  provided = clock.FillFrames(requested, 0, test_data);
  EXPECT_EQ(provided, requested);
  // Consume 1 less sample than requested over the entire interpolation window.
  EXPECT_EQ(provider.consumed() - consumed,
            AudioClockSimulator::kInterpolateWindow);
  consumed = provider.consumed();
  index += requested;

  // Interpolation window should now be complete, start on new clock rate.
  test_data[0] = output + index;
  requested = 100;
  provided = clock.FillFrames(requested, 0, test_data);
  EXPECT_EQ(provided, requested);
  index += requested;

  // Change clock rate again.
  clock.SetRate(1.0);

  test_data[0] = output + index;
  requested = AudioClockSimulator::kInterpolateWindow - 100;
  provided = clock.FillFrames(requested, 0, test_data);
  EXPECT_EQ(provided, requested);
  // Consume 1 more sample than requested over the entire interpolation window.
  EXPECT_EQ(provider.consumed() - consumed,
            AudioClockSimulator::kInterpolateWindow + 1);
  index += requested;

  for (int f = 0; f < index - 1; ++f) {
    EXPECT_LT(output[f], output[f + 1]);
    float diff = output[f + 1] - output[f];
    EXPECT_GE(diff, 1.0 - 1.0 / AudioClockSimulator::kInterpolateWindow);
    EXPECT_LE(diff, 1.0 + 1.0 / AudioClockSimulator::kInterpolateWindow);
  }
}

class AudioClockSimulatorLongRunningTest
    : public testing::TestWithParam<double> {
 public:
  AudioClockSimulatorLongRunningTest() = default;
  ~AudioClockSimulatorLongRunningTest() override = default;
};

TEST_P(AudioClockSimulatorLongRunningTest, Run) {
  double rate = GetParam();
  LOG(INFO) << "Rate = " << rate;
  NiceMock<FakeAudioProvider> provider(kDefaultChannels);
  AudioClockSimulator clock(&provider);
  clock.SetRate(rate);

  const int kRequestSize = 1000;
  const int kIterations = 1000;

  float output[kRequestSize];
  float* test_data[1] = {output};
  for (int i = 0; i < kIterations; ++i) {
    int provided = clock.FillFrames(kRequestSize, 0, test_data);
    EXPECT_EQ(provided, kRequestSize);
  }

  int input_frames = provider.consumed();
  int output_frames = kRequestSize * kIterations;

  EXPECT_GE(input_frames, std::floor(rate * output_frames));
  EXPECT_LE(input_frames, std::ceil(rate * output_frames));
}

INSTANTIATE_TEST_SUITE_P(
    Rates,
    AudioClockSimulatorLongRunningTest,
    ::testing::Values(1.0,
                      AudioClockSimulator::kMinRate,
                      AudioClockSimulator::kMaxRate,
                      (1.0 + AudioClockSimulator::kMinRate) / 2,
                      (1.0 + AudioClockSimulator::kMaxRate) / 2));

}  // namespace media
}  // namespace chromecast
