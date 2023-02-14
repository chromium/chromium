// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <tuple>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromecast/media/api/audio_clock_simulator.h"
#include "media/base/sinc_resampler.h"
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
  return std::round(frames * 1000000.0 / sample_rate);
}

class FakeAudioProvider : public AudioProvider {
 public:
  explicit FakeAudioProvider(size_t num_channels)
      : num_channels_(num_channels) {
    DCHECK_GT(num_channels_, 0u);
    ON_CALL(*this, FillFrames)
        .WillByDefault(Invoke(this, &FakeAudioProvider::FillFramesImpl));
  }

  void SetFillCallback(base::RepeatingCallback<void()> callback) {
    fill_callback_ = std::move(callback);
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

    if (fill_callback_) {
      fill_callback_.Run();
    }
    return num_frames;
  }

  int consumed() { return next_; }

 private:
  const size_t num_channels_;
  int next_ = 0;

  base::RepeatingCallback<void()> fill_callback_;
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
  auto clock = AudioClockSimulator::Create(&provider);
  if (request_size > kBufferSize) {
    return;
  }

  EXPECT_EQ(clock->SetRate(clock_rate), clock_rate);

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
    int provided = clock->FillFrames(request_size, timestamp, test_data);
    EXPECT_EQ(provided, request_size);

    double delay = clock->DelayFrames();
    EXPECT_GE(delay, 0);
    testing::Mock::VerifyAndClearExpectations(&provider);
  }
}

TEST(AudioClockSimulatorTest, ChangeRateDuringFill) {
  NiceMock<FakeAudioProvider> provider(2);
  auto clock = AudioClockSimulator::Create(&provider);

  double rates[] = {0.9999, 1.0001, 0.9998, 1.0002, 1.0};
  int rate_index = 0;
  provider.SetFillCallback(base::BindRepeating(
      [](AudioClockSimulator* clock, double* rates, int* rate_index) {
        if (*rate_index >= 5) {
          return;
        }
        clock->SetRate(rates[*rate_index]);
        *rate_index += 1;
      },
      clock.get(), rates, &rate_index));

  float output1[kBufferSize];
  float output2[kBufferSize];
  std::fill_n(output1, kBufferSize, 0);
  std::fill_n(output2, kBufferSize, 0);
  float* test_data[2] = {output1, output2};
  int requested_frames = 0;
  while (true) {
    int64_t timestamp = FramesToTime(requested_frames, kSampleRate);
    EXPECT_CALL(provider, FillFrames(_, _, _)).Times(testing::AnyNumber());
    int provided = clock->FillFrames(kBufferSize, timestamp, test_data);
    EXPECT_EQ(provided, kBufferSize);
    testing::Mock::VerifyAndClearExpectations(&provider);
    if (rate_index >= 5) {
      return;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    RequestSizes,
    AudioClockSimulatorTest,
    testing::Combine(::testing::Values(1, 2, 31, 63, 64, 65, 1000),
                     ::testing::Values(1.0, 0.999, 1.001, 0.9995, 1.0005)));

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
  auto clock = AudioClockSimulator::Create(&provider);
  clock->SetRate(rate);

  const int kRequestSize = 1000;
  const int kIterations = 1000;

  float output[kRequestSize];
  float* test_data[1] = {output};
  for (int i = 0; i < kIterations; ++i) {
    int provided = clock->FillFrames(kRequestSize, 0, test_data);
    EXPECT_EQ(provided, kRequestSize);
  }

  int input_frames = provider.consumed();
  int output_frames = kRequestSize * kIterations;

  EXPECT_GE(input_frames, std::floor(rate * output_frames));
  EXPECT_LE(input_frames, std::ceil(rate * output_frames) +
                              ::media::SincResampler::kSmallRequestSize);
}

INSTANTIATE_TEST_SUITE_P(Rates,
                         AudioClockSimulatorLongRunningTest,
                         ::testing::Values(1.0, 0.999, 1.001, 0.9995, 1.0005));

}  // namespace media
}  // namespace chromecast
