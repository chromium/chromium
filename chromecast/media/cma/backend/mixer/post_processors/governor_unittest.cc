// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/post_processors/governor.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "chromecast/media/base/aligned_buffer.h"
#include "chromecast/media/cma/backend/mixer/post_processors/post_processor_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace post_processor_test {

namespace {

constexpr char kConfigTemplate[] =
    R"config({"onset_volume": %f, "clamp_multiplier": %f})config";

const float kDefaultClamp = 0.6f;
const int kNumFrames = 100;
const int kSampleRate = 44100;

std::string MakeConfigString(float onset_volume, float clamp_multiplier) {
  return base::StringPrintf(kConfigTemplate, onset_volume, clamp_multiplier);
}

void ScaleData(float* data, int frames, float scale) {
  for (int f = 0; f < frames; ++f) {
    data[f] *= scale;
  }
}

}  // namespace

class GovernorTest : public ::testing::TestWithParam<float> {
 public:
  GovernorTest(const GovernorTest&) = delete;
  GovernorTest& operator=(const GovernorTest&) = delete;

 protected:
  GovernorTest()
      : clamp_(kDefaultClamp),
        onset_volume_(GetParam()),
        governor_(
            std::make_unique<Governor>(MakeConfigString(onset_volume_, clamp_),
                                       kNumChannels)),
        data_(LinearChirp(kNumFrames,
                          std::vector<double>(kNumChannels, 0.0),
                          std::vector<double>(kNumChannels, 1.0))),
        expected_(data_) {}

  ~GovernorTest() = default;
  void SetUp() override {
    governor_->SetSlewTimeMsForTest(0);
    governor_->SetConfig({kSampleRate});
  }

  void ProcessFrames(float volume) {
    AudioPostProcessor2::Metadata metadata = {0, 0, volume};
    governor_->ProcessFrames(data_.data(), kNumFrames, &metadata);
  }

  void CompareBuffers() {
    CheckArraysEqual(expected_.data(), data_.data(), expected_.size());
  }

  float clamp_;
  float onset_volume_;
  std::unique_ptr<Governor> governor_;
  AlignedBuffer<float> data_;
  AlignedBuffer<float> expected_;
};

TEST_P(GovernorTest, ZeroVolume) {
  ProcessFrames(0.0f);
  CompareBuffers();
}

TEST_P(GovernorTest, EpsilonBelowOnset) {
  // Approximately equaling is inclusive, thus needs more than one epsilon to
  // make sure triggering volume change.
  float volume = onset_volume_ - 2 * std::numeric_limits<float>::epsilon();
  ProcessFrames(volume);
  CompareBuffers();
}

TEST_P(GovernorTest, MaxVolume) {
  ProcessFrames(1.0);
  if (onset_volume_ <= 1.0) {
    ScaleData(expected_.data(), kNumFrames * kNumChannels, clamp_);
  }
  CompareBuffers();
}

INSTANTIATE_TEST_SUITE_P(GovernorClampVolumeTest,
                         GovernorTest,
                         ::testing::Values(0.0f, 0.1f, 0.5f));

// Default tests from post_processor_test
TEST_P(PostProcessorTest, GovernorDelay) {
  std::string config = MakeConfigString(0.8, 0.9);
  auto pp = std::make_unique<Governor>(config, kNumChannels);
  TestDelay(pp.get(), sample_rate_);
}

TEST_P(PostProcessorTest, GovernorRinging) {
  std::string config = MakeConfigString(0.8, 0.9);
  auto pp = std::make_unique<Governor>(config, kNumChannels);
  TestRingingTime(pp.get(), sample_rate_);
}

TEST_P(PostProcessorTest, GovernorBenchmark) {
  std::string config = MakeConfigString(0.8, 0.9);
  auto pp = std::make_unique<Governor>(config, kNumChannels);
  AudioProcessorBenchmark(pp.get(), sample_rate_);
}

}  // namespace post_processor_test
}  // namespace media
}  // namespace chromecast

/*
Benchmark results:
Device: Google Home Max, test audio duration: 1 sec.
Benchmark           Sample Rate    CPU(%)
----------------------------------------------------
GovernorBenchmark   44100          0.0013%
GovernorBenchmark   48000          0.0015%
*/
