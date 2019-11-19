// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/post_processors/post_processor_unittest.h"
#include "chromecast/media/cma/backend/mixer/post_processors/post_processor_wrapper.h"

#include <time.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "base/logging.h"

namespace chromecast {
namespace media {
namespace post_processor_test {

namespace {

const float kEpsilon = std::numeric_limits<float>::epsilon();

// Benchmark parameters.
const float kTestDurationSec = 1.0;

}  // namespace

using Status = AudioPostProcessor2::Status;

AudioPostProcessor2::Config MakeProcessorConfig(int sample_rate_hz) {
  AudioPostProcessor2::Config config;
  config.output_sample_rate = sample_rate_hz;
  config.system_output_sample_rate = sample_rate_hz;
  config.output_frames_per_write = kBufSizeFrames;
  return config;
}

AlignedBuffer<float> LinearChirp(int frames,
                                 const std::vector<double>& start_frequencies,
                                 const std::vector<double>& end_frequencies) {
  DCHECK_EQ(start_frequencies.size(), end_frequencies.size());
  AlignedBuffer<float> chirp(frames * start_frequencies.size());
  for (size_t ch = 0; ch < start_frequencies.size(); ++ch) {
    double angle = 0.0;
    for (int f = 0; f < frames; ++f) {
      angle +=
          start_frequencies[ch] +
          (end_frequencies[ch] - start_frequencies[ch]) * f * M_PI / frames;
      chirp[ch + f * start_frequencies.size()] = sin(angle);
    }
  }
  return chirp;
}

AlignedBuffer<float> GetStereoChirp(int frames,
                                    float start_frequency_left,
                                    float end_frequency_left,
                                    float start_frequency_right,
                                    float end_frequency_right) {
  std::vector<double> start_frequencies(2);
  std::vector<double> end_frequencies(2);
  start_frequencies[0] = start_frequency_left;
  start_frequencies[1] = start_frequency_right;
  end_frequencies[0] = end_frequency_left;
  end_frequencies[1] = end_frequency_right;

  return LinearChirp(frames, start_frequencies, end_frequencies);
}

void TestDelay(AudioPostProcessor2* pp,
               int output_sample_rate,
               int num_input_channels) {
  ASSERT_TRUE(pp->SetConfig(MakeProcessorConfig(output_sample_rate)));

  const Status& status = pp->GetStatus();
  ASSERT_GT(status.input_sample_rate, 0);
  ASSERT_GT(status.output_channels, 0);
  ASSERT_GE(status.rendering_delay_frames, 0);
  ASSERT_GE(status.ringing_time_frames, 0);

  const int input_size_frames = kBufSizeFrames * 100;
  const int resample_factor = output_sample_rate / status.input_sample_rate;
  const int output_size_frames = input_size_frames * resample_factor;

  AlignedBuffer<float> data_in = LinearChirp(
      input_size_frames, std::vector<double>(num_input_channels, 0.0),
      std::vector<double>(num_input_channels, 1.0));

  AlignedBuffer<float> data_expected = LinearChirp(
      output_size_frames, std::vector<double>(num_input_channels, 0.0),
      std::vector<double>(num_input_channels, 1.0 / resample_factor));

  AlignedBuffer<float> data_out(data_in.size() * resample_factor);
  const int output_buf_size = kBufSizeFrames * resample_factor *
                              status.output_channels * sizeof(data_out[0]);
  for (int i = 0; i < input_size_frames; i += kBufSizeFrames) {
    pp->ProcessFrames(&data_in[i * num_input_channels], kBufSizeFrames, 1.0,
                      0.0);
    std::memcpy(&data_out[i * status.output_channels * resample_factor],
                status.output_buffer, output_buf_size);
  }

  double max_sum = 0;
  int max_idx = -1;  // index (offset), corresponding to maximum x-correlation.
  // Find the offset of maximum x-correlation of in/out.
  // Search range should be larger than post-processor's expected delay.
  int search_range =
      status.rendering_delay_frames * resample_factor + kBufSizeFrames;
  for (int offset = 0; offset < search_range; ++offset) {
    double sum = 0.0;
    int upper_search_limit_frames = output_size_frames - search_range;
    for (int f = 0; f < upper_search_limit_frames; ++f) {
      for (int ch = 0; ch < status.output_channels; ++ch) {
        sum += data_expected[f * num_input_channels] *
               data_out[(f + offset) * status.output_channels + ch];
      }
    }

    // No need to normalize because every dot product is the same length.
    if (sum > max_sum) {
      max_sum = sum;
      max_idx = offset;
    }
  }
  EXPECT_EQ(max_idx / resample_factor, status.rendering_delay_frames);
}

void TestRingingTime(AudioPostProcessor2* pp,
                     int sample_rate,
                     int num_input_channels) {
  ASSERT_TRUE(pp->SetConfig(MakeProcessorConfig(sample_rate)));

  const Status& status = pp->GetStatus();
  ASSERT_GT(status.input_sample_rate, 0);
  ASSERT_GT(status.output_channels, 0);
  ASSERT_GE(status.rendering_delay_frames, 0);
  ASSERT_GE(status.ringing_time_frames, 0);

  const int kNumFrames = kBufSizeFrames;
  const int kSinFreq = 2000;

  // Send a second of data to excite the filter.
  for (int i = 0; i < sample_rate; i += kNumFrames) {
    AlignedBuffer<float> data =
        GetSineData(kNumFrames, kSinFreq, sample_rate, num_input_channels);
    pp->ProcessFrames(data.data(), kNumFrames, 1.0, 0.0);
  }
  AlignedBuffer<float> data =
      GetSineData(kNumFrames, kSinFreq, sample_rate, num_input_channels);
  pp->ProcessFrames(data.data(), kNumFrames, 1.0, 0.0);

  // Compute the amplitude of the last buffer
  ASSERT_NE(status.output_buffer, nullptr);
  float original_amplitude =
      SineAmplitude(status.output_buffer, status.output_channels * kNumFrames);

  EXPECT_GE(original_amplitude, 0)
      << "Output of nonzero data is 0; cannot test ringing";

  // Feed |ringing_time_frames| of silence.
  int frames_remaining = status.ringing_time_frames;
  int frames_to_process = std::min(frames_remaining, kNumFrames);
  while (frames_remaining > 0) {
    frames_to_process = std::min(frames_to_process, frames_remaining);
    data.assign(frames_to_process * num_input_channels, 0);
    pp->ProcessFrames(data.data(), frames_to_process, 1.0, 0.0);
    frames_remaining -= frames_to_process;
  }

  // Send a little more data and ensure the amplitude is < 1% the original.
  data.assign(kNumFrames * num_input_channels, 0);
  pp->ProcessFrames(data.data(), kNumFrames, 1.0, 0.0);

  // Only look at the amplitude of the first few frames.
  EXPECT_LE(SineAmplitude(status.output_buffer, 10 * status.output_channels) /
                original_amplitude,
            0.01)
      << "Output level after " << status.ringing_time_frames
      << " is more than 1%.";
}

void TestPassthrough(AudioPostProcessor2* pp,
                     int sample_rate,
                     int num_input_channels) {
  ASSERT_TRUE(pp->SetConfig(MakeProcessorConfig(sample_rate)));

  const Status& status = pp->GetStatus();
  ASSERT_GT(status.input_sample_rate, 0);
  ASSERT_GT(status.output_channels, 0);
  ASSERT_GE(status.rendering_delay_frames, 0);

  ASSERT_EQ(status.output_channels, num_input_channels)
      << "\"Passthrough\" is not well defined for "
      << "num_input_channels != num_output_channels";

  ASSERT_EQ(status.input_sample_rate, sample_rate);

  const int kNumFrames = kBufSizeFrames;
  const int kSinFreq = 2000;

  AlignedBuffer<float> data =
      GetSineData(kNumFrames, kSinFreq, sample_rate, num_input_channels);
  AlignedBuffer<float> expected(data);

  pp->ProcessFrames(data.data(), kNumFrames, 1.0, 0.0);
  int delayed_frames = 0;

  while (status.rendering_delay_frames >= delayed_frames + kNumFrames) {
    delayed_frames += kNumFrames;
    for (int i = 0; i < kNumFrames * num_input_channels; ++i) {
      EXPECT_EQ(0.0f, data[i]) << i;
    }
    data = expected;
    pp->ProcessFrames(data.data(), kNumFrames, 1.0, 0.0);

    ASSERT_GE(status.rendering_delay_frames, delayed_frames);
  }

  int delay_samples =
      (status.rendering_delay_frames - delayed_frames) * status.output_channels;
  ASSERT_LE(delay_samples, status.output_channels * kNumFrames);

  CheckArraysEqual(expected.data(), status.output_buffer + delay_samples,
                   data.size() - delay_samples);
}

void AudioProcessorBenchmark(AudioPostProcessor2* pp,
                             int sample_rate,
                             int num_input_channels) {
  ASSERT_TRUE(pp->SetConfig(MakeProcessorConfig(sample_rate)));

  int test_size_frames = kTestDurationSec * pp->GetStatus().input_sample_rate;
  // Make test_size multiple of kBufSizeFrames and calculate effective
  // duration.
  test_size_frames -= test_size_frames % kBufSizeFrames;
  float effective_duration = static_cast<float>(test_size_frames) / sample_rate;
  AlignedBuffer<float> data_in = LinearChirp(
      test_size_frames, std::vector<double>(num_input_channels, 0.0),
      std::vector<double>(num_input_channels, 1.0));
  clock_t start_clock = clock();
  for (int i = 0; i < test_size_frames; i += kBufSizeFrames * kNumChannels) {
    pp->ProcessFrames(&data_in[i], kBufSizeFrames, 1.0, 0.0);
  }
  clock_t stop_clock = clock();
  LOG(INFO) << "At " << sample_rate
            << " frames per second CPU usage: " << std::defaultfloat
            << 100.0 * (stop_clock - start_clock) /
                   (CLOCKS_PER_SEC * effective_duration)
            << "%";
}

void AudioProcessorBenchmark(AudioPostProcessor* pp, int sample_rate) {
  AudioPostProcessorWrapper wrapper(pp, kNumChannels);
  AudioProcessorBenchmark(&wrapper, sample_rate, kNumChannels);
}

template <typename T>
void CheckArraysEqual(const T* expected, const T* actual, size_t size) {
  std::vector<int> differing_values = CompareArray(expected, actual, size);
  if (differing_values.empty()) {
    return;
  }

  size_t size_to_print =
      std::min(static_cast<size_t>(differing_values[0] + 8), size);
  EXPECT_EQ(differing_values.size(), 0u)
      << "Arrays differ at indices "
      << ::testing::PrintToString(differing_values)
      << "\n  Expected: " << ArrayToString(expected, size_to_print)
      << "\n  Actual:   " << ArrayToString(actual, size_to_print);
}

template <typename T>
std::vector<int> CompareArray(const T* expected, const T* actual, size_t size) {
  std::vector<int> diffs;
  for (size_t i = 0; i < size; ++i) {
    if (std::abs(expected[i] - actual[i]) > kEpsilon) {
      diffs.push_back(i);
    }
  }
  return diffs;
}

template <typename T>
std::string ArrayToString(const T* array, size_t size) {
  std::string result;
  for (size_t i = 0; i < size; ++i) {
    result += ::testing::PrintToString(array[i]) + " ";
  }
  return result;
}

float SineAmplitude(const float* data, int num_frames) {
  double power = 0;
  for (int i = 0; i < num_frames; ++i) {
    power += std::pow(data[i], 2);
  }
  return std::sqrt(power / num_frames) * sqrt(2);
}

AlignedBuffer<float> GetSineData(int frames,
                                 float frequency,
                                 int sample_rate,
                                 int num_channels) {
  AlignedBuffer<float> sine(frames * num_channels);
  for (int f = 0; f < frames; ++f) {
    for (int ch = 0; ch < num_channels; ++ch) {
      // Offset by a little so that first value is non-zero
      sine[f * num_channels + ch] =
          sin(static_cast<double>(f + ch) * frequency * 2 * M_PI / sample_rate);
    }
  }
  return sine;
}

void TestDelay(AudioPostProcessor* pp, int sample_rate) {
  AudioPostProcessorWrapper ppw(pp, kNumChannels);
  TestDelay(&ppw, sample_rate, kNumChannels);
}

void TestRingingTime(AudioPostProcessor* pp, int sample_rate) {
  AudioPostProcessorWrapper ppw(pp, kNumChannels);
  TestRingingTime(&ppw, sample_rate, kNumChannels);
}
void TestPassthrough(AudioPostProcessor* pp, int sample_rate) {
  AudioPostProcessorWrapper ppw(pp, kNumChannels);
  TestPassthrough(&ppw, sample_rate, kNumChannels);
}

PostProcessorTest::PostProcessorTest() : sample_rate_(GetParam()) {}
PostProcessorTest::~PostProcessorTest() = default;

INSTANTIATE_TEST_SUITE_P(SampleRates,
                         PostProcessorTest,
                         ::testing::Values(44100, 48000));

}  // namespace post_processor_test
}  // namespace media
}  // namespace chromecast
